/**
 * @file goertzel-processor.ts
 * @brief Goertzel spectral analysis — exact port from firmware ESV11 vendor code.
 *
 * Sources:
 *   firmware-v3/src/audio/backends/esv11/vendor/goertzel.h
 *   firmware-v3/src/audio/backends/esv11/vendor/utilities_min.h
 *
 * This is a faithful line-by-line port. The algorithm, constants, buffer sizes,
 * interlacing strategy, noise floor estimation, and autoranging are all preserved
 * exactly as they appear in the C firmware. Comments reference the original C code
 * where the mapping is non-obvious.
 *
 * All float arrays use Float32Array to match ESP32 single-precision behaviour.
 */

import {
    SAMPLE_RATE,
    SAMPLE_HISTORY_LENGTH,
    NUM_FREQS,
    BOTTOM_NOTE,
    NOTE_STEP,
    NUM_AVERAGE_SAMPLES,
    NUM_SPECTROGRAM_AVERAGE_SAMPLES,
    NOISE_HISTORY_SAMPLES,
    WINDOW_LOOKUP_SIZE,
    NOISE_ALPHA,
    AUTORANGER_ALPHA,
    NOTES,
} from '../constants';

// ============================================================================
// Frequency bin state — mirrors C struct `freq` from types_min.h
// ============================================================================

interface FreqBin {
    targetFreq: number;
    coeff: number;
    windowStep: number;
    magnitude: number;
    magnitudeFullScale: number;
    magnitudeLast: number;
    novelty: number;
    blockSize: number;
}

// ============================================================================
// Utility functions — exact ports from utilities_min.h
// ============================================================================

/**
 * Clamp a float to [0, 1].
 * C original: clip_float() in utilities_min.h
 */
function clipFloat(input: number): number {
    if (input < 0.0) return 0.0;
    if (input > 1.0) return 1.0;
    return input;
}

/**
 * Shift array left and append new data.
 * C original: shift_and_copy_arrays() in utilities_min.h
 *
 * memmove(history, history + newSize, (historySize - newSize) * sizeof(float));
 * memcpy(history + historySize - newSize, newArray, newSize * sizeof(float));
 */
function shiftAndCopyArrays(
    historyArray: Float32Array,
    historySize: number,
    newArray: Float32Array,
    newSize: number,
): void {
    // Shift existing data left by newSize
    historyArray.copyWithin(0, newSize, historySize);
    // Copy new data into the vacated tail
    historyArray.set(newArray, historySize - newSize);
}

// ============================================================================
// GoertzelProcessor
// ============================================================================

export class GoertzelProcessor {
    // -- Frequency bin bank (C: freq frequencies_musical[NUM_FREQS]) --
    private readonly frequenciesMusical: FreqBin[];

    // -- Window lookup (C: float window_lookup[4096]) --
    private readonly windowLookup: Float32Array;

    // -- Sample history ring buffer (C: float sample_history[SAMPLE_HISTORY_LENGTH]) --
    private readonly sampleHistory: Float32Array;

    // -- Max Goertzel block size across all bins --
    private maxGoertzelBlockSize: number = 0;

    // -- Spectrogram outputs (C: float spectrogram[NUM_FREQS]) --
    private readonly spectrogram: Float32Array;
    private readonly spectrogramSmooth: Float32Array;

    // -- Chromagram output (C: float chromagram[12]) --
    private readonly chromagram: Float32Array;

    // -- Spectrogram averaging ring buffer --
    // C: float spectrogram_average[NUM_SPECTROGRAM_AVERAGE_SAMPLES][NUM_FREQS]
    private readonly spectrogramAverage: Float32Array[];
    private spectrogramAverageIndex: number = 0;

    // -- Noise floor estimation --
    // C: float noise_history[10][NUM_FREQS]
    private readonly noiseHistory: Float32Array[];
    private noiseHistoryIndex: number = 0;
    private lastNoiseSpectrumLog: number = 0;

    // C: float noise_floor[NUM_FREQS]
    private readonly noiseFloor: Float32Array;

    // -- Magnitude pipeline static locals (C: static inside calculate_magnitudes) --
    // C: float magnitudes_raw[NUM_FREQS]
    private readonly magnitudesRaw: Float32Array;
    // C: float magnitudes_noise_filtered[NUM_FREQS]
    private readonly magnitudesNoiseFiltered: Float32Array;
    // C: float magnitudes_avg[NUM_AVERAGE_SAMPLES][NUM_FREQS]
    private readonly magnitudesAvg: Float32Array[];
    // C: float magnitudes_smooth[NUM_FREQS]
    private readonly magnitudesSmooth: Float32Array;
    // C: float max_val_smooth (static inside calculate_magnitudes)
    private maxValSmooth: number = 0.0;

    // -- Interlacing state (C: static bool interlacing_frame_field) --
    private interlacingFrameField: boolean = false;

    // -- Iteration counter (C: static uint32_t iter inside calculate_magnitudes) --
    private iter: number = 0;

    // -- Timing: millisecond clock for noise snapshot logging --
    // In the browser worklet we track elapsed time ourselves.
    private elapsedMs: number = 0;

    constructor() {
        // Allocate frequency bin bank
        this.frequenciesMusical = new Array<FreqBin>(NUM_FREQS);
        for (let i = 0; i < NUM_FREQS; i++) {
            this.frequenciesMusical[i] = {
                targetFreq: 0,
                coeff: 0,
                windowStep: 0,
                magnitude: 0,
                magnitudeFullScale: 0,
                magnitudeLast: 0,
                novelty: 0,
                blockSize: 0,
            };
        }

        // Allocate buffers
        this.windowLookup = new Float32Array(WINDOW_LOOKUP_SIZE);
        this.sampleHistory = new Float32Array(SAMPLE_HISTORY_LENGTH);

        this.spectrogram = new Float32Array(NUM_FREQS);
        this.spectrogramSmooth = new Float32Array(NUM_FREQS);
        this.chromagram = new Float32Array(12);

        // Spectrogram averaging: NUM_SPECTROGRAM_AVERAGE_SAMPLES rows of NUM_FREQS
        this.spectrogramAverage = new Array<Float32Array>(NUM_SPECTROGRAM_AVERAGE_SAMPLES);
        for (let i = 0; i < NUM_SPECTROGRAM_AVERAGE_SAMPLES; i++) {
            this.spectrogramAverage[i] = new Float32Array(NUM_FREQS);
        }

        // Noise history: 10 rows of NUM_FREQS
        this.noiseHistory = new Array<Float32Array>(NOISE_HISTORY_SAMPLES);
        for (let i = 0; i < NOISE_HISTORY_SAMPLES; i++) {
            this.noiseHistory[i] = new Float32Array(NUM_FREQS);
        }

        this.noiseFloor = new Float32Array(NUM_FREQS);
        this.magnitudesRaw = new Float32Array(NUM_FREQS);
        this.magnitudesNoiseFiltered = new Float32Array(NUM_FREQS);

        // magnitudes_avg: NUM_AVERAGE_SAMPLES rows of NUM_FREQS
        this.magnitudesAvg = new Array<Float32Array>(NUM_AVERAGE_SAMPLES);
        for (let i = 0; i < NUM_AVERAGE_SAMPLES; i++) {
            this.magnitudesAvg[i] = new Float32Array(NUM_FREQS);
        }

        this.magnitudesSmooth = new Float32Array(NUM_FREQS);

        // Initialise lookup tables and Goertzel constants
        this.initWindowLookup();
        this.initGoertzelConstants();
    }

    // ========================================================================
    // init_window_lookup() — from goertzel.h lines 119–129
    //
    // Gaussian window, sigma=0.8, symmetrically filled into 4096-point table.
    // The table is indexed by a floating-point position that steps through
    // it at a rate determined by each bin's block_size.
    //
    // C original:
    //   for (uint16_t i = 0; i < 2048; i++) {
    //       float sigma = 0.8f;
    //       float n_minus_halfN = i - 2048 / 2;
    //       float gaussian_weighing_factor = expf(-0.5f * powf((n_minus_halfN / (sigma * 2048 / 2)), 2));
    //       window_lookup[i] = gaussian_weighing_factor;
    //       window_lookup[4095 - i] = gaussian_weighing_factor;
    //   }
    // ========================================================================
    private initWindowLookup(): void {
        for (let i = 0; i < 2048; i++) {
            const sigma = 0.8;
            const nMinusHalfN = i - 2048 / 2;
            const gaussianWeighingFactor = Math.exp(
                -0.5 * Math.pow(nMinusHalfN / (sigma * 2048 / 2), 2),
            );
            const weighingFactor = gaussianWeighingFactor;

            this.windowLookup[i] = weighingFactor;
            this.windowLookup[4095 - i] = weighingFactor;
        }
    }

    // ========================================================================
    // init_goertzel() — from goertzel.h lines 69–88
    //
    // Sets up a single frequency bin: target frequency, block size (rounded
    // down to multiple of 4), clamped to SAMPLE_HISTORY_LENGTH-1, window step,
    // and Goertzel coefficient.
    //
    // C original:
    //   frequencies_musical[slot].block_size = (uint16_t)(SAMPLE_RATE / bandwidth);
    //   while (block_size % 4 != 0) block_size -= 1;
    //   if (block_size > SAMPLE_HISTORY_LENGTH - 1) block_size = SAMPLE_HISTORY_LENGTH - 1;
    //   window_step = 4096.0f / block_size;
    //   float k = (int)(0.5f + ((block_size * target_freq) / SAMPLE_RATE));
    //   float w = (2.0f * PI * k) / block_size;
    //   coeff = 2.0f * cosf(w);
    // ========================================================================
    private initGoertzel(frequencySlot: number, frequency: number, bandwidth: number): void {
        const bin = this.frequenciesMusical[frequencySlot]!;
        bin.targetFreq = frequency;

        // C: (uint16_t)(SAMPLE_RATE / bandwidth)
        bin.blockSize = Math.trunc(SAMPLE_RATE / bandwidth);

        // Round down to multiple of 4
        // C: while (block_size % 4 != 0) block_size -= 1;
        while (bin.blockSize % 4 !== 0) {
            bin.blockSize -= 1;
        }

        // Clamp to buffer size
        // C: if (block_size > SAMPLE_HISTORY_LENGTH - 1) block_size = SAMPLE_HISTORY_LENGTH - 1;
        if (bin.blockSize > SAMPLE_HISTORY_LENGTH - 1) {
            bin.blockSize = SAMPLE_HISTORY_LENGTH - 1;
        }

        this.maxGoertzelBlockSize = Math.max(this.maxGoertzelBlockSize, bin.blockSize);

        // C: window_step = 4096.0f / block_size
        bin.windowStep = Math.fround(4096.0 / bin.blockSize);

        // C: float k = (int)(0.5f + ((block_size * target_freq) / SAMPLE_RATE));
        const k = Math.trunc(0.5 + (bin.blockSize * bin.targetFreq) / SAMPLE_RATE);
        // C: float w = (2.0f * M_PI * k) / block_size;
        const w = (2.0 * Math.PI * k) / bin.blockSize;
        // C: coeff = 2.0f * cosf(w);
        bin.coeff = Math.fround(2.0 * Math.cos(w));
    }

    // ========================================================================
    // init_goertzel_constants() — from goertzel.h lines 90–117
    //
    // For each of the 64 bins, looks up the target frequency from the notes[]
    // table using BOTTOM_NOTE + i*NOTE_STEP, calculates bandwidth from
    // neighbouring notes, and calls init_goertzel with bandwidth * 4.
    //
    // C original:
    //   uint16_t note = BOTTOM_NOTE + (i * NOTE_STEP);
    //   ... neighbor distance ...
    //   init_goertzel(i, target_freq, neighbor_distance_hz * 4.0f);
    // ========================================================================
    private initGoertzelConstants(): void {
        for (let i = 0; i < NUM_FREQS; i++) {
            const note = BOTTOM_NOTE + i * NOTE_STEP;
            const bin = this.frequenciesMusical[i]!;
            bin.targetFreq = NOTES[note]!;

            let neighborLeft: number;
            let neighborRight: number;

            // C: boundary conditions — note == 0 or note == NUM_FREQS - 1
            if (note === 0) {
                neighborLeft = NOTES[note]!;
                neighborRight = NOTES[note + 1]!;
            } else if (note === NUM_FREQS - 1) {
                neighborLeft = NOTES[note - 1]!;
                neighborRight = NOTES[note]!;
            } else {
                neighborLeft = NOTES[note - 1]!;
                neighborRight = NOTES[note + 1]!;
            }

            const neighborDistanceHz = Math.max(
                Math.abs(bin.targetFreq - neighborLeft),
                Math.abs(bin.targetFreq - neighborRight),
            );

            // C: init_goertzel(i, target_freq, neighbor_distance_hz * 4.0f)
            this.initGoertzel(i, bin.targetFreq, neighborDistanceHz * 4.0);
        }
    }

    // ========================================================================
    // processSamples() — feed audio chunk into sample_history ring buffer
    //
    // Mirrors the firmware's shift_and_copy_arrays() call that slides
    // sample_history left by CHUNK_SIZE and appends new samples at the tail.
    //
    // C original (from EsV11Backend processAudio path):
    //   shift_and_copy_arrays(sample_history, SAMPLE_HISTORY_LENGTH, chunk, CHUNK_SIZE);
    // ========================================================================
    processSamples(samples: Float32Array): void {
        // Advance the elapsed time clock (CHUNK_SIZE samples at SAMPLE_RATE).
        // C: t_now_ms is maintained externally; here we derive it from sample count.
        this.elapsedMs += (samples.length / SAMPLE_RATE) * 1000.0;

        shiftAndCopyArrays(this.sampleHistory, SAMPLE_HISTORY_LENGTH, samples, samples.length);
    }

    // ========================================================================
    // calculate_magnitude_of_bin() — from goertzel.h lines 131–166
    //
    // Runs the Goertzel algorithm on a single frequency bin using the windowed
    // sample history. Returns sqrt(normalised_magnitude * frequency_scale).
    //
    // C original (critical loop):
    //   for (uint16_t i = 0; i < block_size; i++) {
    //       float windowed_sample = sample_ptr[i] * window_lookup[(uint32_t)window_pos];
    //       float q0 = coeff * q1 - q2 + windowed_sample;
    //       q2 = q1; q1 = q0;
    //       window_pos += window_step;
    //   }
    //   magnitude_squared = (q1*q1) + (q2*q2) - q1*q2*coeff;
    //   normalized_magnitude = magnitude_squared / (block_size / 2.0f);
    //   progress = float(bin) / NUM_FREQS;  progress^4
    //   scale = (progress * 0.9975f) + 0.0025f;
    //   return sqrtf(normalized_magnitude * scale);
    // ========================================================================
    private calculateMagnitudeOfBin(binNumber: number): number {
        let q1 = 0.0;
        let q2 = 0.0;
        let windowPos = 0.0;

        const bin = this.frequenciesMusical[binNumber]!;
        const blockSize = bin.blockSize;
        const coeff = bin.coeff;
        const windowStep = bin.windowStep;

        // C: float* sample_ptr = &sample_history[(SAMPLE_HISTORY_LENGTH - 1) - block_size];
        const sampleOffset = (SAMPLE_HISTORY_LENGTH - 1) - blockSize;

        for (let i = 0; i < blockSize; i++) {
            // C: float windowed_sample = sample_ptr[i] * window_lookup[(uint32_t)window_pos];
            const windowedSample =
                this.sampleHistory[sampleOffset + i]! *
                this.windowLookup[Math.trunc(windowPos)]!;

            // C: float q0 = coeff * q1 - q2 + windowed_sample;
            const q0 = coeff * q1 - q2 + windowedSample;
            q2 = q1;
            q1 = q0;

            windowPos += windowStep;
        }

        // C: magnitude_squared = (q1*q1) + (q2*q2) - q1*q2*coeff;
        const magnitudeSquared = q1 * q1 + q2 * q2 - q1 * q2 * coeff;

        // C: normalized_magnitude = magnitude_squared / (block_size / 2.0f);
        const normalizedMagnitude = magnitudeSquared / (blockSize / 2.0);

        // C: float progress = float(bin_number) / NUM_FREQS;
        //    progress *= progress; progress *= progress;  // progress^4
        //    scale = (progress * 0.9975f) + 0.0025f;
        let progress = binNumber / NUM_FREQS;
        progress *= progress;
        progress *= progress;
        const scale = progress * 0.9975 + 0.0025;

        // C: return sqrtf(normalized_magnitude * scale);
        return Math.sqrt(normalizedMagnitude * scale);
    }

    // ========================================================================
    // calculateMagnitudes() — from goertzel.h lines 168–280
    //
    // The main spectral analysis pass. Runs Goertzel on all 64 bins with
    // odd/even interlacing (only half the bins are recomputed each frame),
    // applies noise floor subtraction, magnitude averaging, autoranging,
    // and spectrogram smoothing.
    //
    // IMPORTANT: This method uses the same interlacing strategy as the firmware —
    // even-indexed bins are computed on one frame, odd-indexed on the next.
    // This halves per-frame computation at the cost of one frame latency on
    // each bin.
    // ========================================================================
    calculateMagnitudes(): void {
        const tNowMs = this.elapsedMs;

        this.iter++;

        // -- Noise history snapshot (every 1000ms) --
        // C: if(t_now_ms - last_noise_spectrum_log >= 1000) { ... }
        if (tNowMs - this.lastNoiseSpectrumLog >= 1000) {
            this.lastNoiseSpectrumLog = tNowMs;
            this.noiseHistoryIndex++;
            if (this.noiseHistoryIndex >= NOISE_HISTORY_SAMPLES) {
                this.noiseHistoryIndex = 0;
            }
            // C: memcpy(noise_history[noise_history_index], magnitudes_raw, sizeof(float)*NUM_FREQS)
            this.noiseHistory[this.noiseHistoryIndex]!.set(this.magnitudesRaw);
        }

        // -- Interlacing: alternate between even and odd bins --
        // C: static bool interlacing_frame_field = 0;
        //    interlacing_frame_field = !interlacing_frame_field;
        this.interlacingFrameField = !this.interlacingFrameField;

        let maxVal = 0.0;

        for (let i = 0; i < NUM_FREQS; i++) {
            const bin = this.frequenciesMusical[i]!;

            // C: bool interlace_field_now = ((i % 2) == 0);
            //    if (interlace_field_now == interlacing_frame_field) { ... }
            const interlaceFieldNow = (i % 2) === 0;
            if (interlaceFieldNow === this.interlacingFrameField) {
                // Recompute this bin's raw magnitude
                this.magnitudesRaw[i] = this.calculateMagnitudeOfBin(i);

                // -- Noise floor: average of 10 history snapshots, scaled by 0.90 --
                // C: float avg_val = 0.0f;
                //    for(uint8_t j = 0; j < 10; j++) avg_val += noise_history[j][i];
                //    avg_val /= 10.0f;  avg_val *= 0.90f;
                let avgVal = 0.0;
                for (let j = 0; j < NOISE_HISTORY_SAMPLES; j++) {
                    avgVal += this.noiseHistory[j]![i]!;
                }
                avgVal /= 10.0;
                avgVal *= 0.90;

                // -- Noise floor EMA --
                // C: noise_floor[i] = noise_floor[i] * (1.0f - kNoiseAlpha) + avg_val * kNoiseAlpha;
                this.noiseFloor[i] = this.noiseFloor[i]! * (1.0 - NOISE_ALPHA) + avgVal * NOISE_ALPHA;

                // C: magnitudes_noise_filtered[i] = fmaxf(magnitudes_raw[i] - noise_floor[i], 0.0f);
                this.magnitudesNoiseFiltered[i] = Math.max(
                    this.magnitudesRaw[i]! - this.noiseFloor[i]!,
                    0.0,
                );
            }

            // -- Store full-scale magnitude (before autoranging) --
            // C: frequencies_musical[i].magnitude_full_scale = magnitudes_noise_filtered[i];
            bin.magnitudeFullScale = this.magnitudesNoiseFiltered[i]!;

            // -- Rolling average over NUM_AVERAGE_SAMPLES frames --
            // C: magnitudes_avg[iter % NUM_AVERAGE_SAMPLES][i] = magnitudes_noise_filtered[i];
            this.magnitudesAvg[this.iter % NUM_AVERAGE_SAMPLES]![i] =
                this.magnitudesNoiseFiltered[i]!;

            // C: float magnitudes_avg_result = 0.0f;
            //    for (uint8_t a = 0; a < NUM_AVERAGE_SAMPLES; a++)
            //        magnitudes_avg_result += magnitudes_avg[a][i];
            //    magnitudes_avg_result /= NUM_AVERAGE_SAMPLES;
            let magnitudesAvgResult = 0.0;
            for (let a = 0; a < NUM_AVERAGE_SAMPLES; a++) {
                magnitudesAvgResult += this.magnitudesAvg[a]![i]!;
            }
            magnitudesAvgResult /= NUM_AVERAGE_SAMPLES;

            this.magnitudesSmooth[i] = magnitudesAvgResult;

            if (this.magnitudesSmooth[i]! > maxVal) {
                maxVal = this.magnitudesSmooth[i]!;
            }
        }

        // -- Autoranging: smooth max_val with asymmetric EMA --
        // C: if (max_val > max_val_smooth) {
        //        float delta = max_val - max_val_smooth;
        //        max_val_smooth += delta * kAutorangerAlpha;
        //    }
        //    if (max_val < max_val_smooth) {
        //        float delta = max_val_smooth - max_val;
        //        max_val_smooth -= delta * kAutorangerAlpha;
        //    }
        if (maxVal > this.maxValSmooth) {
            const delta = maxVal - this.maxValSmooth;
            this.maxValSmooth += delta * AUTORANGER_ALPHA;
        }
        if (maxVal < this.maxValSmooth) {
            const delta = this.maxValSmooth - maxVal;
            this.maxValSmooth -= delta * AUTORANGER_ALPHA;
        }

        // C: if (max_val_smooth < 0.0025f) max_val_smooth = 0.0025f;
        if (this.maxValSmooth < 0.0025) {
            this.maxValSmooth = 0.0025;
        }

        // C: float autoranger_scale = 1.0f / max_val_smooth;
        const autorangerScale = 1.0 / this.maxValSmooth;

        // -- Apply autoranging to produce normalised spectrogram --
        for (let i = 0; i < NUM_FREQS; i++) {
            const bin = this.frequenciesMusical[i]!;
            // C: frequencies_musical[i].magnitude = clip_float(magnitudes_smooth[i] * autoranger_scale);
            bin.magnitude = clipFloat(this.magnitudesSmooth[i]! * autorangerScale);
            // C: spectrogram[i] = frequencies_musical[i].magnitude;
            this.spectrogram[i] = bin.magnitude;
        }

        // -- Spectrogram smoothing: rolling average over NUM_SPECTROGRAM_AVERAGE_SAMPLES --
        // C: spectrogram_average_index++;
        //    if(spectrogram_average_index >= NUM_SPECTROGRAM_AVERAGE_SAMPLES)
        //        spectrogram_average_index = 0;
        this.spectrogramAverageIndex++;
        if (this.spectrogramAverageIndex >= NUM_SPECTROGRAM_AVERAGE_SAMPLES) {
            this.spectrogramAverageIndex = 0;
        }

        // C: for(uint16_t i = 0; i < NUM_FREQS; i++) {
        //        spectrogram_average[spectrogram_average_index][i] = spectrogram[i];
        //        spectrogram_smooth[i] = 0;
        //        for(uint16_t a = 0; a < NUM_SPECTROGRAM_AVERAGE_SAMPLES; a++)
        //            spectrogram_smooth[i] += spectrogram_average[a][i];
        //        spectrogram_smooth[i] /= float(NUM_SPECTROGRAM_AVERAGE_SAMPLES);
        //    }
        for (let i = 0; i < NUM_FREQS; i++) {
            this.spectrogramAverage[this.spectrogramAverageIndex]![i] = this.spectrogram[i]!;

            let smoothAccum = 0.0;
            for (let a = 0; a < NUM_SPECTROGRAM_AVERAGE_SAMPLES; a++) {
                smoothAccum += this.spectrogramAverage[a]![i]!;
            }
            this.spectrogramSmooth[i] = smoothAccum / NUM_SPECTROGRAM_AVERAGE_SAMPLES;
        }
    }

    // ========================================================================
    // getSpectrogram() — return the smoothed spectrogram (64 bins, 0–1 range)
    // ========================================================================
    getSpectrogram(): Float32Array {
        return this.spectrogramSmooth;
    }

    // ========================================================================
    // getRawSpectrogram() — return the per-frame spectrogram (before smoothing)
    // ========================================================================
    getRawSpectrogram(): Float32Array {
        return this.spectrogram;
    }

    // ========================================================================
    // getChromagram() — from goertzel.h lines 282–297
    //
    // Folds 60 spectrogram bins into 12 chroma buckets (C, C#, D, ... B)
    // by summing bin i into chromagram[i % 12], then auto-scales.
    //
    // C original:
    //   memset(chromagram, 0, sizeof(float)*12);
    //   float max_val = 0.2f;
    //   for(uint16_t i = 0; i < 60; i++){
    //       chromagram[ i % 12 ] += (spectrogram_smooth[i] / 5.0f);
    //       max_val = fmaxf(max_val, chromagram[ i % 12 ]);
    //   }
    //   float auto_scale = 1.0f / max_val;
    //   for(uint16_t i = 0; i < 12; i++) chromagram[i] *= auto_scale;
    // ========================================================================
    getChromagram(): Float32Array {
        // C: memset(chromagram, 0, sizeof(float)*12)
        this.chromagram.fill(0);

        // C: float max_val = 0.2f;
        let maxVal = 0.2;

        // C: for(uint16_t i = 0; i < 60; i++) { ... }
        // Note: only 60 of the 64 bins are folded (5 octaves of 12 half-steps)
        for (let i = 0; i < 60; i++) {
            const chromaIdx = i % 12;
            this.chromagram[chromaIdx] = this.chromagram[chromaIdx]! + this.spectrogramSmooth[i]! / 5.0;
            maxVal = Math.max(maxVal, this.chromagram[chromaIdx]!);
        }

        // C: float auto_scale = 1.0f / max_val;
        const autoScale = 1.0 / maxVal;
        for (let i = 0; i < 12; i++) {
            this.chromagram[i] = this.chromagram[i]! * autoScale;
        }

        return this.chromagram;
    }

    // ========================================================================
    // Accessor: per-bin frequency, magnitude, and novelty
    // ========================================================================

    /** Get the target frequency of a specific bin. */
    getBinFrequency(bin: number): number {
        return this.frequenciesMusical[bin]!.targetFreq;
    }

    /** Get the autoranged magnitude (0–1) of a specific bin. */
    getBinMagnitude(bin: number): number {
        return this.frequenciesMusical[bin]!.magnitude;
    }

    /** Get the full-scale (pre-autoranging) magnitude of a specific bin. */
    getBinMagnitudeFullScale(bin: number): number {
        return this.frequenciesMusical[bin]!.magnitudeFullScale;
    }

    /**
     * Get the novelty value for a bin (positive magnitude delta since last frame).
     * Updated externally by the novelty/tempo pipeline; exposed here for completeness.
     */
    getBinNovelty(bin: number): number {
        return this.frequenciesMusical[bin]!.novelty;
    }

    /**
     * Update novelty for all bins — called by the tempo pipeline.
     * C original (from tempo.h update_novelty):
     *   for (uint16_t i = 0; i < NUM_FREQS; i++) {
     *       float new_mag = spectrogram_smooth[i];
     *       frequencies_musical[i].novelty = fmaxf(0.0f, new_mag - frequencies_musical[i].magnitude_last);
     *       current_novelty += frequencies_musical[i].novelty;
     *       frequencies_musical[i].magnitude_last = new_mag;
     *   }
     */
    computeNovelty(): number {
        let currentNovelty = 0.0;
        for (let i = 0; i < NUM_FREQS; i++) {
            const bin = this.frequenciesMusical[i]!;
            const newMag = this.spectrogramSmooth[i]!;
            bin.novelty = Math.max(0.0, newMag - bin.magnitudeLast);
            currentNovelty += bin.novelty;
            bin.magnitudeLast = newMag;
        }
        // C: current_novelty /= float(NUM_FREQS);
        currentNovelty /= NUM_FREQS;
        return currentNovelty;
    }

    /** Expose sample history for VU processor (reads from end of ring buffer). */
    getSampleHistory(): Float32Array {
        return this.sampleHistory;
    }

    /** Expose sample history length for VU processor. */
    getSampleHistoryLength(): number {
        return this.sampleHistory.length;
    }

    /** Get the current autoranging scale ceiling. */
    getAutorangeMax(): number {
        return this.maxValSmooth;
    }

    /** Get the max Goertzel block size across all bins (for diagnostics). */
    getMaxBlockSize(): number {
        return this.maxGoertzelBlockSize;
    }

    /** Get the Goertzel block size for a specific bin. */
    getBinBlockSize(bin: number): number {
        return this.frequenciesMusical[bin]!.blockSize;
    }

    /** Get the Goertzel coefficient for a specific bin. */
    getBinCoeff(bin: number): number {
        return this.frequenciesMusical[bin]!.coeff;
    }

    /**
     * Reset all state to initial conditions.
     * Useful when switching audio sources or after a long silence.
     */
    reset(): void {
        this.sampleHistory.fill(0);
        this.spectrogram.fill(0);
        this.spectrogramSmooth.fill(0);
        this.chromagram.fill(0);
        this.magnitudesRaw.fill(0);
        this.magnitudesNoiseFiltered.fill(0);
        this.magnitudesSmooth.fill(0);
        this.noiseFloor.fill(0);

        for (let i = 0; i < NUM_SPECTROGRAM_AVERAGE_SAMPLES; i++) {
            this.spectrogramAverage[i]!.fill(0);
        }
        for (let i = 0; i < NOISE_HISTORY_SAMPLES; i++) {
            this.noiseHistory[i]!.fill(0);
        }
        for (let i = 0; i < NUM_AVERAGE_SAMPLES; i++) {
            this.magnitudesAvg[i]!.fill(0);
        }

        this.spectrogramAverageIndex = 0;
        this.noiseHistoryIndex = 0;
        this.lastNoiseSpectrumLog = 0;
        this.maxValSmooth = 0.0;
        this.interlacingFrameField = false;
        this.iter = 0;
        this.elapsedMs = 0;

        for (let i = 0; i < NUM_FREQS; i++) {
            const bin = this.frequenciesMusical[i]!;
            bin.magnitude = 0;
            bin.magnitudeFullScale = 0;
            bin.magnitudeLast = 0;
            bin.novelty = 0;
        }
    }
}
