/**
 * @file adapter.ts
 * @brief AudioAdapter — maps raw Goertzel/VU/Tempo DSP outputs into ControlBusFrame fields.
 *
 * Ported from firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp.
 * Every field mapping and state variable is preserved 1:1 from the C++ implementation.
 *
 * The adapter receives raw DSP outputs (spectrogram, chromagram, VU level, tempo,
 * waveform) and produces a fully populated ControlBusFrame suitable for effect rendering.
 */

import type { ControlBusFrame } from '../engine/types';
import { HOP_RATE_HZ } from './constants';
import { retunedAlpha } from './audio-math';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Number of Goertzel frequency bins. */
const BINS_64_COUNT = 64;

/** Number of octave bands (sub-bass through treble). */
const NUM_BANDS = 8;

/** Number of chroma pitch classes. */
const NUM_CHROMA = 12;

/** Number of waveform samples per frame. */
const WAVEFORM_N = 128;

/** Below this VU level, treat as near-silence (disables autorange). */
const ACTIVE_VU_THRESHOLD = 0.01;

/** One-frame onset energy delta threshold for percussion triggers. */
const ONSET_THRESHOLD = 0.08;

/** Snare energy minimum for trigger to fire. */
const SNARE_MIN_ENERGY = 0.10;

/** Hi-hat energy minimum for trigger to fire. */
const HIHAT_MIN_ENERGY = 0.05;

/** Sensory Bridge waveform history depth (frames). */
const SB_WAVEFORM_HISTORY = 4;

/** Sensory Bridge waveform "sweet spot" minimum level offset. */
const SB_SWEET_SPOT_MIN = 750.0;

// ---------------------------------------------------------------------------
// Pre-computed retuned alphas (matching firmware `static const` values)
//
// In the firmware, these are declared as `static const` inside buildFrame()
// and computed once from retunedAlpha(alphaAt50Hz, 50.0, HOP_RATE_HZ).
// The firmware's HOP_RATE_HZ is 50 Hz (12800/256), but retunedAlpha
// preserves the time constant: the alpha is retuned FROM 50 Hz TO whatever
// the current hop rate is.
//
// In the node-composer, HOP_RATE_HZ = 125 Hz (32000/256). The retunedAlpha
// calls below use 50 as refHz and HOP_RATE_HZ (125) as targetHz, producing
// identical perceptual smoothing behaviour.
// ---------------------------------------------------------------------------

/** bins64 autorange follower — decay alpha (slow release). */
const BINS_DECAY = 1.0 - retunedAlpha(1.0 - 0.995, 50.0, HOP_RATE_HZ);

/** bins64 autorange follower — rise alpha (fast attack). */
const BINS_RISE = retunedAlpha(0.25, 50.0, HOP_RATE_HZ);

/** bins64 autorange follower — minimum follower value. */
const BINS_FLOOR = 0.05;

/** Chroma autorange follower — decay alpha (slow release). */
const CHROMA_DECAY = 1.0 - retunedAlpha(1.0 - 0.995, 50.0, HOP_RATE_HZ);

/** Chroma autorange follower — rise alpha (fast attack). */
const CHROMA_RISE = retunedAlpha(0.35, 50.0, HOP_RATE_HZ);

/** Chroma autorange follower — minimum follower value. */
const CHROMA_FLOOR = 0.08;

/** Heavy smoothing (slow envelope) alpha for bands and chroma. */
const HEAVY_ALPHA = retunedAlpha(0.05, 50.0, HOP_RATE_HZ);

/** SB waveform peak follower — attack alpha. */
const SB_PEAK_ATTACK = retunedAlpha(0.25, 50.0, HOP_RATE_HZ);

/** SB waveform peak follower — release alpha. */
const SB_PEAK_RELEASE = retunedAlpha(0.005, 50.0, HOP_RATE_HZ);

/** SB waveform scaled peak — attack alpha. */
const SB_SCALED_ATTACK = retunedAlpha(0.25, 50.0, HOP_RATE_HZ);

/** SB waveform scaled peak — release alpha. */
const SB_SCALED_RELEASE = retunedAlpha(0.25, 50.0, HOP_RATE_HZ);

/** SB waveform peak-scaled-last EMA alpha. */
const SB_LAST_ALPHA = retunedAlpha(0.05, 50.0, HOP_RATE_HZ);

// ---------------------------------------------------------------------------
// Raw DSP output interface (matches firmware EsV11Outputs)
// ---------------------------------------------------------------------------

/**
 * Raw DSP outputs from the Goertzel/VU/Tempo pipeline.
 * This is the input to the adapter — produced by the worklet processors.
 */
export interface RawDspOutputs {
  /** Smoothed spectrogram magnitudes (64 bins, 0..1 range). */
  spectrogramSmooth: Float32Array; // [64]

  /** Chromagram pitch-class energies (12 bins, 0..1 range). */
  chromagram: Float32Array; // [12]

  /** VU level (0..1 linear energy). */
  vuLevel: number;

  /** Normalised novelty curve sample (0..1). */
  noveltyNormLast: number;

  /** Top BPM estimate. */
  topBpm: number;

  /** Tempo confidence (0..1). */
  tempoConfidence: number;

  /** Beat phase in radians [-pi, pi]. */
  phaseRadians: number;

  /** True on beat-tick event (phase wrap). */
  beatTick: boolean;

  /** Beat strength (0..1). */
  beatStrength: number;

  /** Time-domain waveform samples (128 int16 values). */
  waveform: Int16Array; // [128]
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

function clamp01(x: number): number {
  if (x < 0) return 0;
  if (x > 1) return 1;
  return x;
}

// ---------------------------------------------------------------------------
// AudioAdapter
// ---------------------------------------------------------------------------

/**
 * Maps raw Goertzel/VU/Tempo DSP outputs into ControlBusFrame fields.
 *
 * This is a stateful class — it maintains 14 persistent state variables
 * across frames for autorange followers, heavy smoothing, percussion
 * onset detection, Sensory Bridge waveform processing, and beat-in-bar
 * tracking.
 *
 * Call `buildFrame()` once per hop (125 Hz) with the latest raw DSP outputs.
 */
export class AudioAdapter {
  // -----------------------------------------------------------------------
  // Persistent state variables (14 total — all survive across frames)
  // -----------------------------------------------------------------------

  /** Adaptive normalisation follower for bins64. */
  private binsMaxFollower = 0.1;

  /** Adaptive normalisation follower for chroma. */
  private chromaMaxFollower = 0.2;

  /** Heavy smoothing state for octave bands (slow envelope). */
  private heavyBands = new Float32Array(NUM_BANDS); // init 0

  /** Heavy smoothing state for chroma (slow envelope). */
  private heavyChroma = new Float32Array(NUM_CHROMA); // init 0

  /** Current beat position within a 4-beat bar (0..3). */
  private beatInBar = 0;

  /** Previous frame snare energy (for onset delta detection). */
  private prevSnareEnergy = 0;

  /** Previous frame hi-hat energy (for onset delta detection). */
  private prevHihatEnergy = 0;

  /** Sensory Bridge waveform history ring buffer (4 frames x 128 samples). */
  private sbWaveformHistory: Int16Array[];

  /** Current write index into the waveform history ring buffer. */
  private sbWaveformHistoryIndex = 0;

  /** SB peak follower value (sweet spot scaling, init 750). */
  private sbMaxWaveformValFollower = SB_SWEET_SPOT_MIN;

  /** SB scaled waveform peak (smoothed). */
  private sbWaveformPeakScaled = 0;

  /** SB scaled waveform peak — last frame (double-smoothed). */
  private sbWaveformPeakScaledLast = 0;

  /** SB note chromagram (octave-aggregated from bins64Adaptive). */
  private sbNoteChroma = new Float32Array(NUM_CHROMA); // init 0

  /** SB chromagram maximum value (for normalisation). */
  private sbChromaMaxVal = 0.0001;

  constructor() {
    this.sbWaveformHistory = [];
    for (let i = 0; i < SB_WAVEFORM_HISTORY; i++) {
      this.sbWaveformHistory.push(new Int16Array(WAVEFORM_N));
    }
  }

  /**
   * Reset all state to initial values.
   * Call this when the audio pipeline restarts or is re-initialised.
   */
  reset(): void {
    this.binsMaxFollower = 0.1;
    this.chromaMaxFollower = 0.2;
    this.heavyBands.fill(0);
    this.heavyChroma.fill(0);
    this.beatInBar = 0;
    this.prevSnareEnergy = 0;
    this.prevHihatEnergy = 0;

    for (let i = 0; i < SB_WAVEFORM_HISTORY; i++) {
      (this.sbWaveformHistory[i] ?? new Int16Array(0)).fill(0);
    }
    this.sbWaveformHistoryIndex = 0;
    this.sbMaxWaveformValFollower = SB_SWEET_SPOT_MIN;
    this.sbWaveformPeakScaled = 0;
    this.sbWaveformPeakScaledLast = 0;
    this.sbNoteChroma.fill(0);
    this.sbChromaMaxVal = 0.0001;
  }

  /**
   * Build a ControlBusFrame from the latest raw DSP outputs.
   *
   * @param es   Raw DSP outputs from the Goertzel/VU/Tempo worklets
   * @param hopSeq  Monotonic hop sequence number
   * @returns Fully populated ControlBusFrame
   */
  buildFrame(es: RawDspOutputs, hopSeq: number): ControlBusFrame {
    // -----------------------------------------------------------------
    // Allocate output arrays
    // -----------------------------------------------------------------
    const bins64 = new Float32Array(BINS_64_COUNT);
    const bands = new Float32Array(NUM_BANDS);
    const heavyBandsOut = new Float32Array(NUM_BANDS);
    const chroma = new Float32Array(NUM_CHROMA);
    const heavyChromaOut = new Float32Array(NUM_CHROMA);

    // -----------------------------------------------------------------
    // Core energy / novelty proxy
    // -----------------------------------------------------------------
    // ES vu_level tends to be a low-range linear energy; map to LWLS
    // contract range expected by existing effects (0..1, perceptually expanded).
    const rms = clamp01(Math.sqrt(Math.max(0, es.vuLevel)) * 1.25);
    const flux = clamp01(es.noveltyNormLast);

    // -----------------------------------------------------------------
    // bins64: clamp raw ES spectrogram
    // -----------------------------------------------------------------
    const isActive = es.vuLevel >= ACTIVE_VU_THRESHOLD;
    const rawBins = new Float32Array(BINS_64_COUNT);

    for (let i = 0; i < BINS_64_COUNT; i++) {
      rawBins[i] = clamp01(es.spectrogramSmooth[i] ?? 0);
    }

    // -----------------------------------------------------------------
    // bins64Adaptive: autorange follower (simple max follower)
    // -----------------------------------------------------------------
    let currentMax = 0.00001;
    for (let i = 0; i < BINS_64_COUNT; i++) {
      const v = rawBins[i] ?? 0;
      if (v > currentMax) currentMax = v;
    }

    // Decay + rise behaviour (retuned for current hop rate)
    let decayed = this.binsMaxFollower * BINS_DECAY;
    if (currentMax > decayed) {
      const delta = currentMax - decayed;
      this.binsMaxFollower = decayed + delta * BINS_RISE;
    } else {
      this.binsMaxFollower = decayed;
    }
    if (this.binsMaxFollower < BINS_FLOOR) {
      this.binsMaxFollower = BINS_FLOOR;
    }

    const inv = isActive ? (1.0 / this.binsMaxFollower) : 1.0;
    for (let i = 0; i < BINS_64_COUNT; i++) {
      const v = clamp01((rawBins[i] ?? 0) * inv);
      bins64[i] = v;
      // bins64 and bins64Adaptive share the same value in the ES adapter
    }

    // -----------------------------------------------------------------
    // Aggregate 8 bands from 64 bins (mean of each 8-bin block)
    // Mapping: band 0 = bins 0-7 (sub-bass), band 7 = bins 56-63 (treble)
    // -----------------------------------------------------------------
    for (let band = 0; band < NUM_BANDS; band++) {
      const start = band * 8;
      let sum = 0;
      for (let i = 0; i < 8; i++) {
        sum += bins64[start + i] ?? 0;
      }
      bands[band] = clamp01(sum / 8.0);
    }

    // -----------------------------------------------------------------
    // Chroma: clamp + autorange follower
    // -----------------------------------------------------------------
    const rawChroma = new Float32Array(NUM_CHROMA);
    let chromaMax = 0.00001;
    for (let i = 0; i < NUM_CHROMA; i++) {
      rawChroma[i] = clamp01(es.chromagram[i] ?? 0);
      if ((rawChroma[i] ?? 0) > chromaMax) chromaMax = rawChroma[i] ?? 0;
    }

    // Similar autorange follower for chroma magnitudes, gated by activity
    let chromaDecayed = this.chromaMaxFollower * CHROMA_DECAY;
    if (chromaMax > chromaDecayed) {
      const delta = chromaMax - chromaDecayed;
      this.chromaMaxFollower = chromaDecayed + delta * CHROMA_RISE;
    } else {
      this.chromaMaxFollower = chromaDecayed;
    }
    if (this.chromaMaxFollower < CHROMA_FLOOR) {
      this.chromaMaxFollower = CHROMA_FLOOR;
    }

    const chromaInv = isActive ? (1.0 / this.chromaMaxFollower) : 1.0;
    for (let i = 0; i < NUM_CHROMA; i++) {
      chroma[i] = clamp01((rawChroma[i] ?? 0) * chromaInv);
    }

    // -----------------------------------------------------------------
    // Heavy smoothing (slow envelope) — purely within adapter
    // -----------------------------------------------------------------
    for (let i = 0; i < NUM_BANDS; i++) {
      const hb = this.heavyBands[i] ?? 0;
      const b = bands[i] ?? 0;
      this.heavyBands[i] = hb * (1.0 - HEAVY_ALPHA) + b * HEAVY_ALPHA;
      heavyBandsOut[i] = clamp01(this.heavyBands[i] ?? 0);
    }
    for (let i = 0; i < NUM_CHROMA; i++) {
      const hc = this.heavyChroma[i] ?? 0;
      const c = chroma[i] ?? 0;
      this.heavyChroma[i] = hc * (1.0 - HEAVY_ALPHA) + c * HEAVY_ALPHA;
      heavyChromaOut[i] = clamp01(this.heavyChroma[i] ?? 0);
    }

    // -----------------------------------------------------------------
    // Sensory Bridge parity: waveform history ring buffer
    // -----------------------------------------------------------------
    const historyBuf = this.sbWaveformHistory[this.sbWaveformHistoryIndex] ?? new Int16Array(WAVEFORM_N);
    for (let i = 0; i < WAVEFORM_N; i++) {
      const sample = es.waveform[i] ?? 0;
      historyBuf[i] = sample;
    }
    this.sbWaveformHistoryIndex++;
    if (this.sbWaveformHistoryIndex >= SB_WAVEFORM_HISTORY) {
      this.sbWaveformHistoryIndex = 0;
    }

    // -----------------------------------------------------------------
    // SB waveform peak follower (sweet spot scaling; matches SB 3.1.0)
    // -----------------------------------------------------------------
    let maxWaveformValRaw = 0;
    for (let i = 0; i < WAVEFORM_N; i++) {
      const sample = es.waveform[i] ?? 0;
      const absSample = sample < 0 ? -sample : sample;
      if (absSample > maxWaveformValRaw) {
        maxWaveformValRaw = absSample;
      }
    }

    // Sweet spot min level offset
    let maxWaveformVal = maxWaveformValRaw - SB_SWEET_SPOT_MIN;
    if (maxWaveformVal < 0) maxWaveformVal = 0;

    // Asymmetric follower (fast attack, slow release)
    if (maxWaveformVal > this.sbMaxWaveformValFollower) {
      const delta = maxWaveformVal - this.sbMaxWaveformValFollower;
      this.sbMaxWaveformValFollower += delta * SB_PEAK_ATTACK;
    } else if (maxWaveformVal < this.sbMaxWaveformValFollower) {
      const delta = this.sbMaxWaveformValFollower - maxWaveformVal;
      this.sbMaxWaveformValFollower -= delta * SB_PEAK_RELEASE;
      if (this.sbMaxWaveformValFollower < SB_SWEET_SPOT_MIN) {
        this.sbMaxWaveformValFollower = SB_SWEET_SPOT_MIN;
      }
    }

    // Scaled peak
    let waveformPeakScaledRaw = 0;
    if (this.sbMaxWaveformValFollower > 0) {
      waveformPeakScaledRaw = maxWaveformVal / this.sbMaxWaveformValFollower;
    }

    // Symmetric smoothing on the scaled value
    if (waveformPeakScaledRaw > this.sbWaveformPeakScaled) {
      const delta = waveformPeakScaledRaw - this.sbWaveformPeakScaled;
      this.sbWaveformPeakScaled += delta * SB_SCALED_ATTACK;
    } else if (waveformPeakScaledRaw < this.sbWaveformPeakScaled) {
      const delta = this.sbWaveformPeakScaled - waveformPeakScaledRaw;
      this.sbWaveformPeakScaled -= delta * SB_SCALED_RELEASE;
    }

    // 3.1.0 waveform peak follower used by waveform/VU modes
    this.sbWaveformPeakScaledLast =
      this.sbWaveformPeakScaled * SB_LAST_ALPHA +
      this.sbWaveformPeakScaledLast * (1.0 - SB_LAST_ALPHA);

    // -----------------------------------------------------------------
    // SB note chromagram: octave aggregation of bins64Adaptive
    // 6 octaves x 12 notes, clamped to [0, 1]
    // -----------------------------------------------------------------
    this.sbChromaMaxVal = 0;
    this.sbNoteChroma.fill(0);

    for (let octave = 0; octave < 6; octave++) {
      for (let note = 0; note < NUM_CHROMA; note++) {
        const noteIndex = 12 * octave + note;
        if (noteIndex < BINS_64_COUNT) {
          // bins64Adaptive and bins64 share the same value in this adapter
          const val = bins64[noteIndex] ?? 0;
          this.sbNoteChroma[note] = (this.sbNoteChroma[note] ?? 0) + val;
          if ((this.sbNoteChroma[note] ?? 0) > 1.0) {
            this.sbNoteChroma[note] = 1.0;
          }
          if ((this.sbNoteChroma[note] ?? 0) > this.sbChromaMaxVal) {
            this.sbChromaMaxVal = this.sbNoteChroma[note] ?? 0;
          }
        }
      }
    }
    if (this.sbChromaMaxVal < 0.0001) {
      this.sbChromaMaxVal = 0.0001;
    }

    // -----------------------------------------------------------------
    // Lightweight onset detection from 64-bin spectrum
    // Snare: bins 5-10 (~150-300 Hz), Hi-hat: bins 50-60 (~6-12 kHz)
    // -----------------------------------------------------------------
    let snareSum = 0;
    for (let i = 5; i <= 10 && i < BINS_64_COUNT; i++) {
      snareSum += bins64[i] ?? 0;
    }
    const snareEnergy = clamp01(snareSum / 6.0);

    let hihatSum = 0;
    for (let i = 50; i <= 60 && i < BINS_64_COUNT; i++) {
      hihatSum += bins64[i] ?? 0;
    }
    const hihatEnergy = clamp01(hihatSum / 11.0);

    // One-frame onset pulse: trigger when energy jumps above previous + threshold
    const snareTrigger =
      snareEnergy > this.prevSnareEnergy + ONSET_THRESHOLD &&
      snareEnergy > SNARE_MIN_ENERGY;
    const hihatTrigger =
      hihatEnergy > this.prevHihatEnergy + ONSET_THRESHOLD &&
      hihatEnergy > HIHAT_MIN_ENERGY;

    this.prevSnareEnergy = snareEnergy;
    this.prevHihatEnergy = hihatEnergy;

    // -----------------------------------------------------------------
    // Phase conversion: ES phase in radians [-pi, pi] -> [0, 1)
    // -----------------------------------------------------------------
    let phase01 = (es.phaseRadians + Math.PI) / (2.0 * Math.PI);
    // Guard wrap
    phase01 -= Math.floor(phase01);
    const beatPhase = clamp01(phase01);

    // -----------------------------------------------------------------
    // Beat-in-bar tracking (mod 4)
    // -----------------------------------------------------------------
    if (es.beatTick) {
      this.beatInBar = (this.beatInBar + 1) % 4;
    }
    const downbeatTick = es.beatTick && this.beatInBar === 0;

    // -----------------------------------------------------------------
    // Assemble the ControlBusFrame
    // -----------------------------------------------------------------
    const frame: ControlBusFrame = {
      // Core scalars
      rms,
      fastRms: rms,
      flux,
      fastFlux: flux,
      liveliness: 0, // Not computed in ES adapter (default 0)

      // Octave bands (8)
      bands,
      heavyBands: heavyBandsOut,

      // Chroma (12)
      chroma,
      heavyChroma: heavyChromaOut,

      // Spectrum (64)
      bins64,

      // Beat / tempo
      beatPhase,
      beatStrength: clamp01(es.beatStrength),
      beatTick: es.beatTick,
      downbeatTick,
      bpm: es.topBpm,
      tempoConfidence: clamp01(es.tempoConfidence),

      // Percussion
      snareEnergy,
      hihatEnergy,
      snareTrigger,
      hihatTrigger,

      // Silence (not computed in ES adapter — defaults)
      silentScale: 1.0,
      isSilent: false,

      // Hop sequencing
      hopSeq,
    };

    return frame;
  }

  // -----------------------------------------------------------------------
  // State accessors (useful for debugging / visualisation)
  // -----------------------------------------------------------------------

  /** Current bins64 autorange follower value. */
  getBinsMaxFollower(): number {
    return this.binsMaxFollower;
  }

  /** Current chroma autorange follower value. */
  getChromaMaxFollower(): number {
    return this.chromaMaxFollower;
  }

  /** Current beat-in-bar position (0..3). */
  getBeatInBar(): number {
    return this.beatInBar;
  }

  /** SB waveform peak scaled value. */
  getSbWaveformPeakScaled(): number {
    return this.sbWaveformPeakScaled;
  }

  /** SB waveform peak scaled last (double-smoothed). */
  getSbWaveformPeakScaledLast(): number {
    return this.sbWaveformPeakScaledLast;
  }

  /** SB note chromagram (12 values, octave-aggregated). */
  getSbNoteChromagram(): Float32Array {
    return new Float32Array(this.sbNoteChroma);
  }

  /** SB chromagram maximum value. */
  getSbChromaMaxVal(): number {
    return this.sbChromaMaxVal;
  }
}
