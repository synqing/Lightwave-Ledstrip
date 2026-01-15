/**
 * @file EmotiscopeAudio.h
 * @brief Emotiscope Audio Processing Pipeline
 *
 * Ported from Emotiscope.HIL v1.1 audio pipeline.
 * Implements 64-bin Goertzel spectral analysis, VU metering,
 * chromagram, novelty detection, and tempo/beat tracking.
 *
 * Usage:
 *   EmotiscopeAudio audio;
 *   audio.init();
 *   // Each audio hop:
 *   audio.process(samples, sample_count);
 *   const EmotiscopeOutput& out = audio.getOutput();
 */

#pragma once

#include "config.h"
#include "types.h"
#include "utilities.h"

#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace audio {
namespace emotiscope {

class EmotiscopeAudio {
public:
    EmotiscopeAudio() = default;

    /**
     * @brief Initialize all DSP components
     *
     * Must be called once before processing.
     * Initializes Goertzel coefficients, window lookup, tempo constants.
     */
    void init() {
        initWindowLookup();
        initGoertzelConstants();
        initTempoConstants();
        initVU();
        m_initialized = true;
    }

    /**
     * @brief Process a chunk of audio samples
     *
     * Call this each audio hop with new samples.
     * Updates spectrogram, VU, chromagram, novelty, and tempo.
     *
     * @param samples  Array of normalized float samples (-1.0 to 1.0)
     * @param count    Number of samples (typically CHUNK_SIZE = 64)
     */
    void process(const float* samples, uint16_t count) {
        if (!m_initialized) return;

        // Add samples to history buffer
        shiftAndCopyArrays(m_sampleHistory, SAMPLE_HISTORY_LENGTH, samples, count);

        // Run DSP pipeline
        calculateMagnitudes();
        runVU();
        getChromagram();
        updateTempo();

        // Increment sequence number for freshness detection
        m_output.hop_seq++;
    }

    /**
     * @brief Update tempo phase (call from GPU/render loop)
     *
     * @param delta Time delta in reference frames (1.0 = 1/REFERENCE_FPS seconds)
     */
    void updateTempiPhase(float delta) {
        if (!m_initialized) return;

        m_tempiPowerSum = 0.00000001f;

        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            // Smooth magnitude
            float mag = m_tempi[i].magnitude;
            m_tempiSmooth[i] = m_tempiSmooth[i] * 0.975f + mag * 0.025f;
            m_tempiPowerSum += m_tempiSmooth[i];

            // Advance phase
            syncBeatPhase(i, delta);

            // Export to output
            m_output.tempi_magnitude[i] = m_tempi[i].magnitude;
            m_output.tempi_phase[i] = m_tempi[i].phase;
            m_output.tempi_beat[i] = m_tempi[i].beat;
        }

        // Calculate confidence (max contribution ratio)
        float maxContribution = 0.000001f;
        uint8_t topIndex = 0;
        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            float contribution = m_tempiSmooth[i] / m_tempiPowerSum;
            if (contribution > maxContribution) {
                maxContribution = contribution;
                topIndex = i;
            }
        }

        m_output.tempo_confidence = maxContribution;
        m_output.top_bpm_index = topIndex;
        m_output.top_bpm_magnitude = m_tempi[topIndex].magnitude;
    }

    /**
     * @brief Update novelty curve (call from GPU loop at NOVELTY_LOG_HZ)
     */
    void updateNovelty() {
        // Calculate spectral flux
        float currentNovelty = 0.0f;
        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            float newMag = m_output.spectrogram_smooth[i];
            m_freqs[i].novelty = std::max(0.0f, newMag - m_freqs[i].magnitude_last);
            currentNovelty += m_freqs[i].novelty;
            m_freqs[i].magnitude_last = newMag;
        }
        currentNovelty /= static_cast<float>(NUM_FREQS);

        // Check for silence
        checkSilence(currentNovelty);

        // Log to novelty curve
        logNovelty(std::log1p(currentNovelty));
        logVU(m_output.vu_max);
        m_output.vu_max = 0.000001f;

        m_output.current_novelty = currentNovelty;
    }

    /**
     * @brief Get current output state
     */
    const EmotiscopeOutput& getOutput() const { return m_output; }

    /**
     * @brief Get mutable output (for direct modification)
     */
    EmotiscopeOutput& getOutputMutable() { return m_output; }

private:
    // ========================================================================
    // Initialization
    // ========================================================================

    void initWindowLookup() {
        // Gaussian window (sigma = 0.8)
        for (uint16_t i = 0; i < 2048; i++) {
            float n_minus_halfN = i - 2048.0f / 2.0f;
            float gaussian = std::exp(-0.5f * std::pow(n_minus_halfN / (GAUSSIAN_SIGMA * 2048.0f / 2.0f), 2.0f));
            m_windowLookup[i] = gaussian;
            m_windowLookup[4095 - i] = gaussian;
        }
    }

    void initGoertzelConstants() {
        m_maxGoertzelBlockSize = 0;

        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            // Calculate target frequency from quarter-step table
            uint16_t note = BOTTOM_NOTE + (i * NOTE_STEP);
            m_freqs[i].target_freq = NOTES[note];

            // Calculate bandwidth from neighbors
            float neighborLeft = (note > 0) ? NOTES[note - 1] : NOTES[note];
            float neighborRight = (note < 255) ? NOTES[note + 1] : NOTES[note];
            float neighborDistance = std::max(
                std::fabs(m_freqs[i].target_freq - neighborLeft),
                std::fabs(m_freqs[i].target_freq - neighborRight)
            );

            // Initialize Goertzel for this bin
            initGoertzelBin(i, m_freqs[i].target_freq, neighborDistance * 4.0f);
        }
    }

    void initGoertzelBin(uint16_t slot, float frequency, float bandwidth) {
        // Calculate block size from bandwidth
        m_freqs[slot].block_size = SAMPLE_RATE / bandwidth;

        // Align to multiple of 4
        while (m_freqs[slot].block_size % 4 != 0) {
            m_freqs[slot].block_size--;
        }

        // Limit to sample history length
        if (m_freqs[slot].block_size > SAMPLE_HISTORY_LENGTH - 1) {
            m_freqs[slot].block_size = SAMPLE_HISTORY_LENGTH - 1;
        }

        m_maxGoertzelBlockSize = std::max(m_maxGoertzelBlockSize, m_freqs[slot].block_size);

        // Window step
        m_freqs[slot].window_step = 4096.0f / m_freqs[slot].block_size;

        // Goertzel coefficient
        float k = static_cast<int>(0.5f + ((m_freqs[slot].block_size * m_freqs[slot].target_freq) / SAMPLE_RATE));
        float w = (2.0f * M_PI * k) / m_freqs[slot].block_size;
        m_freqs[slot].coeff = 2.0f * std::cos(w);
    }

    void initTempoConstants() {
        // Calculate tempo Hz values
        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            float progress = static_cast<float>(i) / NUM_TEMPI;
            float tempiRange = TEMPO_HIGH - TEMPO_LOW;
            float tempo = tempiRange * progress + TEMPO_LOW;
            m_tempiBpmHz[i] = tempo / 60.0f;
        }

        // Initialize tempo Goertzel bins
        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            m_tempi[i].target_tempo_hz = m_tempiBpmHz[i];

            // Calculate bandwidth from neighbors
            float neighborLeft = (i > 0) ? m_tempiBpmHz[i - 1] : m_tempiBpmHz[i];
            float neighborRight = (i < NUM_TEMPI - 1) ? m_tempiBpmHz[i + 1] : m_tempiBpmHz[i];
            float maxDistance = std::max(
                std::fabs(neighborLeft - m_tempi[i].target_tempo_hz),
                std::fabs(neighborRight - m_tempi[i].target_tempo_hz)
            );

            // Block size
            m_tempi[i].block_size = static_cast<uint32_t>(NOVELTY_LOG_HZ / (maxDistance * 0.5f));
            if (m_tempi[i].block_size > NOVELTY_HISTORY_LENGTH) {
                m_tempi[i].block_size = NOVELTY_HISTORY_LENGTH;
            }

            // Goertzel coefficients
            float k = static_cast<int>(0.5f + ((m_tempi[i].block_size * m_tempi[i].target_tempo_hz) / NOVELTY_LOG_HZ));
            float w = (2.0f * M_PI * k) / m_tempi[i].block_size;
            m_tempi[i].cosine = std::cos(w);
            m_tempi[i].sine = std::sin(w);
            m_tempi[i].coeff = 2.0f * m_tempi[i].cosine;
            m_tempi[i].window_step = 4096.0f / m_tempi[i].block_size;

            // Phase advance per reference frame
            m_tempi[i].phase_radians_per_frame = (2.0f * M_PI * m_tempi[i].target_tempo_hz) / REFERENCE_FPS;
            m_tempi[i].phase_inverted = false;
        }
    }

    void initVU() {
        memset(m_vuLog, 0, sizeof(m_vuLog));
        memset(m_vuSmooth, 0, sizeof(m_vuSmooth));
        m_vuLogIndex = 0;
        m_vuSmoothIndex = 0;
        m_output.vu_level = 0.0f;
        m_output.vu_max = 0.0f;
        m_output.vu_floor = 0.0f;
    }

    // ========================================================================
    // Goertzel Magnitude Calculation
    // ========================================================================

    float calculateMagnitudeOfBin(uint16_t bin) {
        float q1 = 0.0f, q2 = 0.0f;
        float windowPos = 0.0f;

        const uint16_t blockSize = m_freqs[bin].block_size;
        const float coeff = m_freqs[bin].coeff;
        const float windowStep = m_freqs[bin].window_step;

        float* samplePtr = &m_sampleHistory[(SAMPLE_HISTORY_LENGTH - 1) - blockSize];

        for (uint16_t i = 0; i < blockSize; i++) {
            float windowedSample = samplePtr[i] * m_windowLookup[static_cast<uint32_t>(windowPos)];
            float q0 = coeff * q1 - q2 + windowedSample;
            q2 = q1;
            q1 = q0;
            windowPos += windowStep;
        }

        float magnitudeSquared = (q1 * q1) + (q2 * q2) - q1 * q2 * coeff;
        float normalizedMagnitude = magnitudeSquared / (blockSize / 2.0f);

        // Progressive scaling (higher frequencies get more boost)
        float progress = static_cast<float>(bin) / NUM_FREQS;
        progress *= progress;
        progress *= progress;
        float scale = (progress * 0.9975f) + 0.0025f;

        return std::sqrt(normalizedMagnitude * scale);
    }

    void calculateMagnitudes() {
        // Interlaced processing (alternate bins each frame)
        m_interlacingField = !m_interlacingField;

        float maxVal = 0.0f;

        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            bool interlaceNow = ((i % 2) == 0);
            if (interlaceNow == m_interlacingField) {
                m_magnitudesRaw[i] = calculateMagnitudeOfBin(i);

                // Noise filtering
                float avgVal = 0.0f;
                for (uint8_t j = 0; j < 10; j++) {
                    avgVal += m_noiseHistory[j][i];
                }
                avgVal = (avgVal / 10.0f) * 0.90f;

                m_noiseFloor[i] = m_noiseFloor[i] * 0.99f + avgVal * 0.01f;
                m_magnitudesNoiseFiltered[i] = std::max(m_magnitudesRaw[i] - m_noiseFloor[i], 0.0f);
            }

            m_freqs[i].magnitude_full_scale = m_magnitudesNoiseFiltered[i];

            // Moving average
            m_magnitudesAvg[m_iter % 2][i] = m_magnitudesNoiseFiltered[i];
            float avgResult = (m_magnitudesAvg[0][i] + m_magnitudesAvg[1][i]) / 2.0f;
            m_magnitudesSmooth[i] = avgResult;

            if (m_magnitudesSmooth[i] > maxVal) {
                maxVal = m_magnitudesSmooth[i];
            }
        }

        // Update noise history periodically
        m_iter++;
        if (m_iter % 50 == 0) {  // ~1 second at 50Hz
            m_noiseHistoryIndex = (m_noiseHistoryIndex + 1) % 10;
            memcpy(m_noiseHistory[m_noiseHistoryIndex], m_magnitudesRaw, sizeof(float) * NUM_FREQS);
        }

        // Auto-ranging
        if (maxVal > m_maxValSmooth) {
            m_maxValSmooth += (maxVal - m_maxValSmooth) * 0.005f;
        } else {
            m_maxValSmooth -= (m_maxValSmooth - maxVal) * 0.005f;
        }
        if (m_maxValSmooth < 0.0025f) {
            m_maxValSmooth = 0.0025f;
        }

        float autoScale = 1.0f / m_maxValSmooth;

        // Apply scaling and update output
        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            m_freqs[i].magnitude = clipFloat(m_magnitudesSmooth[i] * autoScale);
            m_output.spectrogram[i] = m_freqs[i].magnitude;
        }

        // Smooth spectrogram (12-sample average)
        m_spectrogramAvgIndex = (m_spectrogramAvgIndex + 1) % NUM_SPECTROGRAM_AVERAGE_SAMPLES;
        for (uint16_t i = 0; i < NUM_FREQS; i++) {
            m_spectrogramAverage[m_spectrogramAvgIndex][i] = m_output.spectrogram[i];

            float sum = 0.0f;
            for (uint8_t a = 0; a < NUM_SPECTROGRAM_AVERAGE_SAMPLES; a++) {
                sum += m_spectrogramAverage[a][i];
            }
            m_output.spectrogram_smooth[i] = sum / static_cast<float>(NUM_SPECTROGRAM_AVERAGE_SAMPLES);
        }
    }

    // ========================================================================
    // VU Meter
    // ========================================================================

    void runVU() {
        // Calculate max amplitude from recent samples
        float* samples = &m_sampleHistory[(SAMPLE_HISTORY_LENGTH - 1) - CHUNK_SIZE];

        float maxAmplitude = 0.000001f;
        for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
            float absVal = std::fabs(samples[i]);
            maxAmplitude = std::max(maxAmplitude, absVal * absVal);
        }
        maxAmplitude = clipFloat(maxAmplitude);

        // Periodically update noise floor
        m_vuFrameCount++;
        if (m_vuFrameCount >= 50) {  // ~250ms at 200Hz hop rate
            m_vuFrameCount = 0;
            m_vuLog[m_vuLogIndex] = maxAmplitude;
            m_vuLogIndex = (m_vuLogIndex + 1) % NUM_VU_LOG_SAMPLES;

            float vuSum = 0.0f;
            for (uint16_t i = 0; i < NUM_VU_LOG_SAMPLES; i++) {
                vuSum += m_vuLog[i];
            }
            m_output.vu_floor = (vuSum / NUM_VU_LOG_SAMPLES) * 0.90f;
        }

        // Remove noise floor
        maxAmplitude = std::max(maxAmplitude - m_output.vu_floor, 0.0f);

        // Auto-ranging
        if (maxAmplitude > m_maxAmplitudeCap) {
            m_maxAmplitudeCap += (maxAmplitude - m_maxAmplitudeCap) * 0.1f;
        } else {
            m_maxAmplitudeCap -= (m_maxAmplitudeCap - maxAmplitude) * 0.1f;
        }
        m_maxAmplitudeCap = std::max(clipFloat(m_maxAmplitudeCap), 0.000025f);

        float autoScale = 1.0f / std::max(m_maxAmplitudeCap, 0.00001f);
        float vuLevelRaw = clipFloat(maxAmplitude * autoScale);

        // Smoothing
        m_vuSmooth[m_vuSmoothIndex] = vuLevelRaw;
        m_vuSmoothIndex = (m_vuSmoothIndex + 1) % NUM_VU_SMOOTH_SAMPLES;

        float vuSum = 0.0f;
        for (uint8_t i = 0; i < NUM_VU_SMOOTH_SAMPLES; i++) {
            vuSum += m_vuSmooth[i];
        }
        m_output.vu_level = vuSum / NUM_VU_SMOOTH_SAMPLES;

        // Track max
        m_output.vu_max = std::max(m_output.vu_max, m_output.vu_level);
    }

    // ========================================================================
    // Chromagram
    // ========================================================================

    void getChromagram() {
        memset(m_output.chromagram, 0, sizeof(m_output.chromagram));

        float maxVal = 0.2f;
        for (uint16_t i = 0; i < 60; i++) {  // First 60 bins (5 octaves)
            m_output.chromagram[i % 12] += (m_output.spectrogram_smooth[i] / 5.0f);
            maxVal = std::max(maxVal, m_output.chromagram[i % 12]);
        }

        // Auto-scale
        float autoScale = 1.0f / maxVal;
        for (uint8_t i = 0; i < 12; i++) {
            m_output.chromagram[i] *= autoScale;
        }
    }

    // ========================================================================
    // Tempo Detection
    // ========================================================================

    float calculateMagnitudeOfTempo(uint16_t bin) {
        uint32_t blockSize = m_tempi[bin].block_size;

        float q1 = 0.0f, q2 = 0.0f;
        float windowPos = 0.0f;

        for (uint32_t i = 0; i < blockSize; i++) {
            float sampleNovelty = m_noveltyCurveNormalized[((NOVELTY_HISTORY_LENGTH - 1) - blockSize) + i];
            float q0 = m_tempi[bin].coeff * q1 - q2 + (sampleNovelty * m_windowLookup[static_cast<uint32_t>(windowPos)]);
            q2 = q1;
            q1 = q0;
            windowPos += m_tempi[bin].window_step;
        }

        // Calculate phase
        float real = q1 - q2 * m_tempi[bin].cosine;
        float imag = q2 * m_tempi[bin].sine;
        m_tempi[bin].phase = std::atan2(imag, real) + (M_PI * BEAT_SHIFT_PERCENT);

        // Wrap phase
        if (m_tempi[bin].phase > M_PI) {
            m_tempi[bin].phase -= 2.0f * M_PI;
            m_tempi[bin].phase_inverted = !m_tempi[bin].phase_inverted;
        } else if (m_tempi[bin].phase < -M_PI) {
            m_tempi[bin].phase += 2.0f * M_PI;
            m_tempi[bin].phase_inverted = !m_tempi[bin].phase_inverted;
        }

        float magnitudeSquared = (q1 * q1) + (q2 * q2) - q1 * q2 * m_tempi[bin].coeff;
        float magnitude = std::sqrt(magnitudeSquared);
        return magnitude / (blockSize / 2.0f);
    }

    void updateTempo() {
        // Normalize novelty curve
        normalizeNoveltyCurve();

        // Calculate tempo magnitudes (interlaced)
        static uint16_t calcBin = 0;
        uint16_t maxBin = NUM_TEMPI - 1;

        if (m_iter % 2 == 0) {
            calculateTempiMagnitudes(calcBin);
        } else {
            calculateTempiMagnitudes(calcBin + 1);
        }

        calcBin += 2;
        if (calcBin >= maxBin) {
            calcBin = 0;
        }
    }

    void calculateTempiMagnitudes(int16_t singleBin) {
        float maxVal = 0.0f;

        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            if (singleBin >= 0) {
                if (i == singleBin) {
                    m_tempi[i].magnitude_full_scale = calculateMagnitudeOfTempo(singleBin);
                }
            } else {
                m_tempi[i].magnitude_full_scale = calculateMagnitudeOfTempo(i);
            }

            if (m_tempi[i].magnitude_full_scale > maxVal) {
                maxVal = m_tempi[i].magnitude_full_scale;
            }
        }

        if (maxVal < 0.02f) maxVal = 0.02f;

        float autoScale = 1.0f / maxVal;

        for (uint16_t i = 0; i < NUM_TEMPI; i++) {
            float scaled = clipFloat(m_tempi[i].magnitude_full_scale * autoScale);
            m_tempi[i].magnitude = scaled * scaled * scaled;  // Cube for contrast
        }
    }

    void normalizeNoveltyCurve() {
        static float maxVal = 0.00001f;

        maxVal *= 0.99f;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i += 4) {
            maxVal = std::max(maxVal, m_noveltyCurve[i]);
            maxVal = std::max(maxVal, m_noveltyCurve[i + 1]);
            maxVal = std::max(maxVal, m_noveltyCurve[i + 2]);
            maxVal = std::max(maxVal, m_noveltyCurve[i + 3]);
        }
        maxVal = std::max(0.1f, maxVal);

        float autoScale = 1.0f / maxVal;
        multiplyByScalar(m_noveltyCurve, m_noveltyCurveNormalized, NOVELTY_HISTORY_LENGTH, autoScale);

        // Copy to output
        memcpy(m_output.novelty_curve, m_noveltyCurve, sizeof(m_noveltyCurve));
        memcpy(m_output.novelty_normalized, m_noveltyCurveNormalized, sizeof(m_noveltyCurveNormalized));
    }

    void logNovelty(float input) {
        shiftArrayLeft(m_noveltyCurve, NOVELTY_HISTORY_LENGTH, 1);
        m_noveltyCurve[NOVELTY_HISTORY_LENGTH - 1] = input;
    }

    void logVU(float input) {
        static float lastInput = 0.0f;
        float positiveDiff = std::max(input - lastInput, 0.0f);
        shiftArrayLeft(m_vuCurve, NOVELTY_HISTORY_LENGTH, 1);
        m_vuCurve[NOVELTY_HISTORY_LENGTH - 1] = positiveDiff;
        lastInput = input;
    }

    void syncBeatPhase(uint16_t bin, float delta) {
        float push = m_tempi[bin].phase_radians_per_frame * delta;
        m_tempi[bin].phase += push;

        if (m_tempi[bin].phase > M_PI) {
            m_tempi[bin].phase -= 2.0f * M_PI;
            m_tempi[bin].phase_inverted = !m_tempi[bin].phase_inverted;
        } else if (m_tempi[bin].phase < -M_PI) {
            m_tempi[bin].phase += 2.0f * M_PI;
            m_tempi[bin].phase_inverted = !m_tempi[bin].phase_inverted;
        }

        m_tempi[bin].beat = std::sin(m_tempi[bin].phase);
    }

    void checkSilence(float currentNovelty) {
        float minVal = 1.0f, maxVal = 0.0f;

        for (uint16_t i = 0; i < 128; i++) {
            float recent = m_noveltyCurveNormalized[(NOVELTY_HISTORY_LENGTH - 1 - 128) + i];
            recent = std::min(0.5f, recent) * 2.0f;
            float scaled = std::sqrt(recent);
            maxVal = std::max(maxVal, scaled);
            minVal = std::min(minVal, scaled);
        }

        float noveltyContrast = std::fabs(maxVal - minVal);
        float silenceLevelRaw = 1.0f - noveltyContrast;

        m_output.silence_level = std::max(0.0f, silenceLevelRaw - 0.5f) * 2.0f;

        if (silenceLevelRaw > 0.5f) {
            m_output.silence_detected = true;
            reduceTempoHistory(m_output.silence_level * 0.10f);
        } else {
            m_output.silence_level = 0.0f;
            m_output.silence_detected = false;
        }
    }

    void reduceTempoHistory(float reduction) {
        float factor = 1.0f - reduction;
        for (uint16_t i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
            m_noveltyCurve[i] = std::max(m_noveltyCurve[i] * factor, 0.00001f);
            m_vuCurve[i] = std::max(m_vuCurve[i] * factor, 0.00001f);
        }
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    bool m_initialized = false;

    // Sample history
    float m_sampleHistory[SAMPLE_HISTORY_LENGTH] = {0};

    // Window lookup (Gaussian)
    float m_windowLookup[WINDOW_LOOKUP_SIZE] = {0};

    // Frequency bins
    FreqBin m_freqs[NUM_FREQS];
    uint16_t m_maxGoertzelBlockSize = 0;

    // Magnitude processing
    float m_magnitudesRaw[NUM_FREQS] = {0};
    float m_magnitudesNoiseFiltered[NUM_FREQS] = {0};
    float m_magnitudesAvg[2][NUM_FREQS] = {{0}};
    float m_magnitudesSmooth[NUM_FREQS] = {0};
    float m_noiseHistory[10][NUM_FREQS] = {{0}};
    float m_noiseFloor[NUM_FREQS] = {0};
    uint8_t m_noiseHistoryIndex = 0;
    float m_maxValSmooth = 0.0f;
    bool m_interlacingField = false;
    uint32_t m_iter = 0;

    // Spectrogram averaging
    float m_spectrogramAverage[NUM_SPECTROGRAM_AVERAGE_SAMPLES][NUM_FREQS] = {{0}};
    uint8_t m_spectrogramAvgIndex = 0;

    // VU meter
    float m_vuLog[NUM_VU_LOG_SAMPLES] = {0};
    float m_vuSmooth[NUM_VU_SMOOTH_SAMPLES] = {0};
    uint16_t m_vuLogIndex = 0;
    uint16_t m_vuSmoothIndex = 0;
    uint16_t m_vuFrameCount = 0;
    float m_maxAmplitudeCap = 0.0000001f;

    // Tempo detection
    TempoBin m_tempi[NUM_TEMPI];
    float m_tempiBpmHz[NUM_TEMPI] = {0};
    float m_tempiSmooth[NUM_TEMPI] = {0};
    float m_tempiPowerSum = 0.0f;

    // Novelty curves
    float m_noveltyCurve[NOVELTY_HISTORY_LENGTH] = {0};
    float m_noveltyCurveNormalized[NOVELTY_HISTORY_LENGTH] = {0};
    float m_vuCurve[NOVELTY_HISTORY_LENGTH] = {0};

    // Output structure
    EmotiscopeOutput m_output;
};

} // namespace emotiscope
} // namespace audio
} // namespace lightwaveos
