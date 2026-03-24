/**
 * VU Processor — exact TypeScript port of firmware ESV11 vu.h
 *
 * Vendored from Emotiscope v1.1_320 VU meter (DSP-only; unchanged logic).
 * Uses 32kHz constants per EsV11_32kHz_Shim.h overrides.
 *
 * Algorithm:
 *   1. Compute max squared amplitude over the most recent CHUNK_SIZE samples
 *   2. Maintain a rolling noise floor estimate (log buffer, 250ms update interval)
 *   3. Subtract noise floor from amplitude
 *   4. Adaptive cap follower (auto-scaling to dynamic range)
 *   5. Rolling average smoothing for final VU level
 */

import {
  SAMPLE_RATE,
  CHUNK_SIZE,
} from '../constants';

// ── VU-specific constants (match firmware vu.h #defines) ─────────────────────

const NUM_VU_LOG_SAMPLES = 20;
const NUM_VU_SMOOTH_SAMPLES = 12;

// Cap follower alpha, retuned against baseline chunk rate (12.8k/64 = 200 Hz).
const BASELINE_CHUNK_RATE_HZ = 12800.0 / 64.0; // 200 Hz
const CURRENT_CHUNK_RATE_HZ = SAMPLE_RATE / CHUNK_SIZE;
const CAP_ALPHA = 1.0 - Math.pow(0.9, BASELINE_CHUNK_RATE_HZ / CURRENT_CHUNK_RATE_HZ);

// ── Utility ──────────────────────────────────────────────────────────────────

/** Clamp a float to [0, 1] — mirrors firmware clip_float(). */
function clipFloat(input: number): number {
  if (input < 0.0) return 0.0;
  if (input > 1.0) return 1.0;
  return input;
}

// ── VU Processor class ───────────────────────────────────────────────────────

export class VuProcessor {
  // Rolling log buffer for noise floor estimation (updated every 250ms)
  private vuLog: Float32Array;
  private vuLogIndex: number = 0;

  // Smoothing buffer for final VU output
  private vuSmooth: Float32Array;
  private vuSmoothIndex: number = 0;

  // Output values
  private vuLevelRaw: number = 0.0;
  private vuLevel: number = 0.0;
  private vuMax: number = 0.0;
  private vuFloor: number = 0.0;

  // Cap follower state (static in firmware; member here)
  private maxAmplitudeCap: number = 0.0000001;

  // Timestamp of last noise floor log update (milliseconds)
  private lastVuLog: number = 0;

  constructor() {
    this.vuLog = new Float32Array(NUM_VU_LOG_SAMPLES);
    this.vuSmooth = new Float32Array(NUM_VU_SMOOTH_SAMPLES);
  }

  /** Initialise all VU state — mirrors firmware init_vu(). */
  init(): void {
    this.vuLog.fill(0.0);
    this.vuLogIndex = 0;

    this.vuSmooth.fill(0.0);
    this.vuSmoothIndex = 0;

    this.vuLevelRaw = 0.0;
    this.vuLevel = 0.0;
    this.vuMax = 0.0;
    this.vuFloor = 0.0;

    this.maxAmplitudeCap = 0.0000001;
    this.lastVuLog = 0;
  }

  /**
   * Run one VU update cycle — mirrors firmware run_vu().
   *
   * @param sampleHistory   - The full sample history ring buffer
   * @param sampleHistoryLength - Number of valid samples in sampleHistory
   * @param currentTimeMs   - Current time in milliseconds (analogous to firmware t_now_ms)
   */
  update(
    sampleHistory: Float32Array,
    sampleHistoryLength: number,
    currentTimeMs: number,
  ): void {
    // ── CALCULATE AMPLITUDE ──────────────────────────────────────────────
    // Read the most recent CHUNK_SIZE samples from the end of the history
    const samplesOffset = (sampleHistoryLength - 1) - CHUNK_SIZE;

    let maxAmplitudeNow = 0.000001;
    for (let i = 0; i < CHUNK_SIZE; i++) {
      const sample = sampleHistory[samplesOffset + i] ?? 0;
      const sampleAbs = Math.abs(sample);

      maxAmplitudeNow = Math.max(maxAmplitudeNow, sampleAbs * sampleAbs);
    }
    maxAmplitudeNow = clipFloat(maxAmplitudeNow);

    // ── LOG AMPLITUDE FOR NOISE REMOVAL ──────────────────────────────────
    if (currentTimeMs < 2000) {
      // Fill log with current amplitude at boot for fast convergence
      for (let i = 0; i < NUM_VU_LOG_SAMPLES; i++) {
        this.vuLog[i] = maxAmplitudeNow;
      }
    } else if (currentTimeMs - this.lastVuLog >= 250) {
      this.lastVuLog = currentTimeMs;
      this.vuLog[this.vuLogIndex] = maxAmplitudeNow;
      this.vuLogIndex = (this.vuLogIndex + 1) % NUM_VU_LOG_SAMPLES;

      let vuSum = 0.0;
      for (let i = 0; i < NUM_VU_LOG_SAMPLES; i++) {
        vuSum += this.vuLog[i] ?? 0;
      }
      this.vuFloor = vuSum / NUM_VU_LOG_SAMPLES;

      this.vuFloor *= 0.90;
    }

    // ── SCALE OUTPUT ─────────────────────────────────────────────────────
    maxAmplitudeNow = Math.max(maxAmplitudeNow - this.vuFloor, 0.0);

    // Cap follower — tracks the dynamic range envelope
    if (maxAmplitudeNow > this.maxAmplitudeCap) {
      const distance = maxAmplitudeNow - this.maxAmplitudeCap;
      this.maxAmplitudeCap += distance * CAP_ALPHA;
    } else if (this.maxAmplitudeCap > maxAmplitudeNow) {
      const distance = this.maxAmplitudeCap - maxAmplitudeNow;
      this.maxAmplitudeCap -= distance * CAP_ALPHA;
    }
    this.maxAmplitudeCap = clipFloat(this.maxAmplitudeCap);

    if (this.maxAmplitudeCap < 0.000025) {
      this.maxAmplitudeCap = 0.000025;
    }

    const autoScale = 1.0 / Math.max(this.maxAmplitudeCap, 0.00001);

    this.vuLevelRaw = clipFloat(maxAmplitudeNow * autoScale);

    // ── SMOOTHING ────────────────────────────────────────────────────────
    this.vuSmooth[this.vuSmoothIndex] = this.vuLevelRaw;
    this.vuSmoothIndex = (this.vuSmoothIndex + 1) % NUM_VU_SMOOTH_SAMPLES;

    let vuSum = 0.0;
    for (let i = 0; i < NUM_VU_SMOOTH_SAMPLES; i++) {
      vuSum += this.vuSmooth[i] ?? 0;
    }
    this.vuLevel = vuSum / NUM_VU_SMOOTH_SAMPLES;

    // ── MAX VALUE ────────────────────────────────────────────────────────
    this.vuMax = Math.max(this.vuMax, this.vuLevel);
  }

  /** Return the smoothed VU level (normalised 0–1). */
  getVuLevel(): number {
    return this.vuLevel;
  }

  /** Return the raw VU level before smoothing (normalised 0–1). */
  getVuLevelRaw(): number {
    return this.vuLevelRaw;
  }

  /** Return the peak VU level ever observed. */
  getVuMax(): number {
    return this.vuMax;
  }

  /** Return the current noise floor estimate. */
  getVuFloor(): number {
    return this.vuFloor;
  }
}
