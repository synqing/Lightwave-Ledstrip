/**
 * @file tempo-processor.ts
 * @brief Complete port of the ESV11 tempo/beat detection pipeline to TypeScript.
 *
 * Sources:
 *   firmware-v3/src/audio/backends/esv11/vendor/tempo.h
 *   firmware-v3/src/audio/backends/esv11/vendor/global_defines.h
 *   firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h
 *   firmware-v3/src/audio/backends/esv11/vendor/types_min.h
 *   firmware-v3/src/audio/backends/esv11/EsV11Backend.h
 *   firmware-v3/src/audio/backends/esv11/EsV11Backend.cpp
 *
 * Algorithm: 96-bin Goertzel bank across 48-144 BPM applied to a novelty curve
 * derived from spectral flux. Octave-aware post-selection with hysteresis and
 * time-based bin stabilisation prevents half-tempo lock and transient jitter.
 *
 * All constants, thresholds, and logic paths are exact matches of the firmware.
 * British English in comments throughout.
 */

import {
  NUM_FREQS,
  NUM_TEMPI,
  TEMPO_LOW,
  TEMPO_HIGH,
  BEAT_SHIFT_PERCENT,
  REFERENCE_FPS,
  NOVELTY_LOG_HZ,
  NOVELTY_HISTORY_LENGTH,
  WINDOW_LOOKUP_SIZE,
  TEMPO_ALPHA,
} from '../constants';

// ============================================================================
// Tempo resonator state (mirrors struct tempo from types_min.h)
// ============================================================================

interface TempoResonator {
  targetTempoHz: number;
  coeff: number;
  sine: number;
  cosine: number;
  windowStep: number;
  phase: number;
  phaseTarget: number;
  phaseInverted: boolean;
  phaseRadiansPerReferenceFrame: number;
  beat: number;
  magnitude: number;
  magnitudeFullScale: number;
  blockSize: number;
}

// ============================================================================
// Runtime-tuneable bin stabilisation parameters (mirrors TempoParams)
// ============================================================================

interface TempoParams {
  gateBase: number;          // Minimum magnitude ratio for generic bin switch
  gateScale: number;         // Additional gate from stability (max gate = base+scale)
  gateTau: number;           // Seconds for gate to reach halfway to max
  confFloor: number;         // Below this raw conf, freeze bin switches
  validationThr: number;     // Raw conf must exceed this to validate a bin
  stabilityTau: number;      // Seconds for stability confidence to reach 0.5
  holdMs: number;            // Minimum hold time before allowing switch (ms)
  octaveRuns: number;        // Consecutive hops for octave promotion
  decayFloor: number;        // Magnitude below this = "decayed" (escape hatch)
  octRatioLo: number;        // Lower bound for octave detection (candidate/stable)
  octRatioHi: number;        // Upper bound for octave detection
  wsSepFloor: number;        // Winner separation denominator floor
  confDecay: number;         // Confidence smoothing decay factor (per 1/60s)
  genericPersistMs: number;  // Generic candidate must persist this long (ms)
}

const DEFAULT_TEMPO_PARAMS: TempoParams = {
  gateBase: 1.2,
  gateScale: 0.8,
  gateTau: 4.0,
  confFloor: 0.10,
  validationThr: 0.08,
  stabilityTau: 4.0,
  holdMs: 200,             // 200000 us in firmware
  octaveRuns: 4,
  decayFloor: 0.005,
  octRatioLo: 1.9,
  octRatioHi: 2.1,
  wsSepFloor: 0.001,
  confDecay: 0.994,
  genericPersistMs: 1000,  // 1000000 us in firmware
};

// ============================================================================
// Utility
// ============================================================================

function clamp01(x: number): number {
  if (x < 0.0) return 0.0;
  if (x > 1.0) return 1.0;
  return x;
}

/** Safe typed-array read — returns 0 for out-of-bounds. */
function f32at(arr: Float32Array, i: number): number {
  return arr[i] ?? 0;
}

// ============================================================================
// TempoProcessor — complete port
// ============================================================================

export class TempoProcessor {
  // --- Tempo resonator bank ---
  private tempi: TempoResonator[];
  private tempiBpmValuesHz: Float32Array;
  private tempiSmooth: Float32Array;
  private tempiPowerSum: number;

  // --- Novelty curve state ---
  private noveltyCurve: Float32Array;
  private noveltyCurveNormalised: Float32Array;
  private vuCurve: Float32Array;
  private vuCurveNormalised: Float32Array;

  // --- Novelty computation state (per-bin spectral flux) ---
  private magnitudeLast: Float32Array;

  // --- Silence detection ---
  private silenceDetected: boolean;
  private silenceLevel: number;

  // --- Tempo confidence ---
  private tempoConfidence: number;

  // --- Autoranging ---
  private maxTempoRange: number;

  // --- Window lookup (Gaussian, 4096 points) ---
  private windowLookup: Float32Array;

  // --- VU tracking for log_vu ---
  private lastVuInput: number;

  // --- Octave-aware selection persistence ---
  private octaveDoublePersist: number;

  // --- Incremental calculation state (update_tempo round-robin) ---
  private updateTempoIter: number;
  private calcBin: number;

  // --- Novelty rate limiter ---
  private noveltyAccumulatorMs: number;
  private noveltyIntervalMs: number;

  // --- Bin stabilisation state (from EsV11Backend) ---
  private tp: TempoParams;
  private stableTopBin: number;
  private stableBinLockedMs: number;
  private octaveCandBin: number;
  private octaveCandRuns: number;
  private genericCandBin: number;
  private genericCandFirstMs: number;
  private stableBinValidated: boolean;

  // --- Beat phase accumulator (free-running, from EsV11Backend) ---
  private beatPhase: number;
  private lastRefreshMs: number;

  // --- Output state ---
  private outputBeatTick: boolean;
  private outputBeatStrength: number;
  private outputPhaseRadians: number;
  private outputTempoConfidence: number;

  constructor() {
    this.tempi = [];
    this.tempiBpmValuesHz = new Float32Array(NUM_TEMPI);
    this.tempiSmooth = new Float32Array(NUM_TEMPI);
    this.tempiPowerSum = 0.00000001;

    this.noveltyCurve = new Float32Array(NOVELTY_HISTORY_LENGTH);
    this.noveltyCurveNormalised = new Float32Array(NOVELTY_HISTORY_LENGTH);
    this.vuCurve = new Float32Array(NOVELTY_HISTORY_LENGTH);
    this.vuCurveNormalised = new Float32Array(NOVELTY_HISTORY_LENGTH);

    this.magnitudeLast = new Float32Array(NUM_FREQS);

    this.silenceDetected = false;
    this.silenceLevel = 0.0;
    this.tempoConfidence = 0.0;
    this.maxTempoRange = 1.0;

    this.windowLookup = new Float32Array(WINDOW_LOOKUP_SIZE);

    this.lastVuInput = 0.0;
    this.octaveDoublePersist = 0;
    this.updateTempoIter = 0;
    this.calcBin = 0;

    this.noveltyIntervalMs = 1000.0 / NOVELTY_LOG_HZ;
    this.noveltyAccumulatorMs = 0.0;

    this.tp = { ...DEFAULT_TEMPO_PARAMS };
    this.stableTopBin = 0;
    this.stableBinLockedMs = 0;
    this.octaveCandBin = 0;
    this.octaveCandRuns = 0;
    this.genericCandBin = 0;
    this.genericCandFirstMs = 0;
    this.stableBinValidated = false;

    this.beatPhase = 0.0;
    this.lastRefreshMs = 0;

    this.outputBeatTick = false;
    this.outputBeatStrength = 0.0;
    this.outputPhaseRadians = 0.0;
    this.outputTempoConfidence = 0.0;
  }

  // ==========================================================================
  // Initialisation
  // ==========================================================================

  /**
   * Initialise all state including Goertzel constants and window lookup.
   * Must be called before any processing.
   */
  init(): void {
    this.initWindowLookup();
    this.initTempoGoertzelConstants();

    // Zero all history buffers
    this.noveltyCurve.fill(0);
    this.noveltyCurveNormalised.fill(0);
    this.vuCurve.fill(0);
    this.vuCurveNormalised.fill(0);
    this.tempiSmooth.fill(0);
    this.magnitudeLast.fill(0);

    this.silenceDetected = false;
    this.silenceLevel = 0.0;
    this.tempoConfidence = 0.0;
    this.maxTempoRange = 1.0;
    this.lastVuInput = 0.0;
    this.octaveDoublePersist = 0;
    this.updateTempoIter = 0;
    this.calcBin = 0;
    this.tempiPowerSum = 0.00000001;
    this.noveltyAccumulatorMs = 0.0;

    this.stableTopBin = 0;
    this.stableBinLockedMs = 0;
    this.octaveCandBin = 0;
    this.octaveCandRuns = 0;
    this.genericCandBin = 0;
    this.genericCandFirstMs = 0;
    this.stableBinValidated = false;
    this.beatPhase = 0.0;
    this.lastRefreshMs = 0;

    this.outputBeatTick = false;
    this.outputBeatStrength = 0.0;
    this.outputPhaseRadians = 0.0;
    this.outputTempoConfidence = 0.0;
  }

  /**
   * Gaussian window lookup table (4096 points, sigma=0.8).
   * Exact port of init_window_lookup() from goertzel.h.
   */
  private initWindowLookup(): void {
    const sigma = 0.8;
    for (let i = 0; i < 2048; i++) {
      const nMinusHalfN = i - 2048 / 2;
      const gaussianWeight = Math.exp(
        -0.5 * Math.pow(nMinusHalfN / (sigma * 2048 / 2), 2),
      );
      this.windowLookup[i] = gaussianWeight;
      this.windowLookup[4095 - i] = gaussianWeight;
    }
  }

  /**
   * Precompute Goertzel constants for 96 tempo bins (48-144 BPM).
   * Exact port of init_tempo_goertzel_constants() from tempo.h.
   */
  private initTempoGoertzelConstants(): void {
    // Compute BPM-to-Hz mapping for each bin
    for (let i = 0; i < NUM_TEMPI; i++) {
      const progress = i / NUM_TEMPI;
      const tempiRange = TEMPO_HIGH - TEMPO_LOW;
      const tempoBpm = tempiRange * progress + TEMPO_LOW;
      this.tempiBpmValuesHz[i] = tempoBpm / 60.0;
    }

    // Allocate resonator structs
    this.tempi = [];
    for (let i = 0; i < NUM_TEMPI; i++) {
      const targetTempoHz = f32at(this.tempiBpmValuesHz, i);

      let neighbourLeft: number;
      let neighbourRight: number;
      if (i === 0) {
        neighbourLeft = f32at(this.tempiBpmValuesHz, i);
        neighbourRight = f32at(this.tempiBpmValuesHz, i + 1);
      } else if (i === NUM_TEMPI - 1) {
        neighbourLeft = f32at(this.tempiBpmValuesHz, i - 1);
        neighbourRight = f32at(this.tempiBpmValuesHz, i);
      } else {
        neighbourLeft = f32at(this.tempiBpmValuesHz, i - 1);
        neighbourRight = f32at(this.tempiBpmValuesHz, i + 1);
      }

      const neighbourLeftDistHz = Math.abs(neighbourLeft - targetTempoHz);
      const neighbourRightDistHz = Math.abs(neighbourRight - targetTempoHz);
      const maxDistHz = Math.max(neighbourLeftDistHz, neighbourRightDistHz);

      let blockSize = Math.floor(NOVELTY_LOG_HZ / (maxDistHz * 0.5));
      if (blockSize > NOVELTY_HISTORY_LENGTH) {
        blockSize = NOVELTY_HISTORY_LENGTH;
      }

      const k = Math.round((blockSize * targetTempoHz) / NOVELTY_LOG_HZ);
      const w = (2.0 * Math.PI * k) / blockSize;
      const cosine = Math.cos(w);
      const sine = Math.sin(w);
      const coeff = 2.0 * cosine;

      const windowStep = 4096.0 / blockSize;
      const phaseRadiansPerRefFrame =
        (2.0 * Math.PI * targetTempoHz) / REFERENCE_FPS;

      this.tempi.push({
        targetTempoHz,
        coeff,
        sine,
        cosine,
        windowStep,
        phase: 0,
        phaseTarget: 0,
        phaseInverted: false,
        phaseRadiansPerReferenceFrame: phaseRadiansPerRefFrame,
        beat: 0,
        magnitude: 0,
        magnitudeFullScale: 0,
        blockSize,
      });
    }
  }

  // ==========================================================================
  // Novelty Curve
  // ==========================================================================

  /**
   * Compute novelty from spectral flux and update the novelty curve.
   * Combines update_novelty(), check_silence(), log_novelty(), and log_vu().
   *
   * In the firmware this is rate-limited to NOVELTY_LOG_HZ (50 Hz).
   * The caller should invoke this at the chunk rate; the internal accumulator
   * handles the rate limiting.
   *
   * @param spectrogram - Current smoothed spectrogram (NUM_FREQS bins, 0..1)
   * @param vuLevel - Current VU level (0..1)
   * @param chunkDtMs - Time since last chunk in ms (e.g. CHUNK_SIZE/SAMPLE_RATE*1000)
   */
  updateNovelty(spectrogram: Float32Array, vuLevel: number, chunkDtMs: number): void {
    this.noveltyAccumulatorMs += chunkDtMs;
    if (this.noveltyAccumulatorMs < this.noveltyIntervalMs) {
      return;
    }
    this.noveltyAccumulatorMs -= this.noveltyIntervalMs;

    // Spectral flux: half-wave rectified difference across all bins
    let currentNovelty = 0.0;
    for (let i = 0; i < NUM_FREQS; i++) {
      const newMag = f32at(spectrogram, i);
      const novelty = Math.max(0.0, newMag - f32at(this.magnitudeLast, i));
      currentNovelty += novelty;
      this.magnitudeLast[i] = newMag;
    }
    currentNovelty /= NUM_FREQS;

    // Silence detection (check_silence)
    this.checkSilence();

    // Log novelty: shift left, append log1p(mean flux)
    this.shiftArrayLeft(this.noveltyCurve, 1);
    this.noveltyCurve[NOVELTY_HISTORY_LENGTH - 1] = Math.log1p(currentNovelty);

    // Log VU: positive difference only
    const positiveDiff = Math.max(vuLevel - this.lastVuInput, 0.0);
    this.shiftArrayLeft(this.vuCurve, 1);
    this.vuCurve[NOVELTY_HISTORY_LENGTH - 1] = positiveDiff;
    this.lastVuInput = vuLevel;
  }

  // ==========================================================================
  // Tempo Calculation (Goertzel bank on novelty curve)
  // ==========================================================================

  /**
   * Run the incremental tempo calculation.
   * Exact port of update_tempo(): normalises novelty, computes 2 Goertzel
   * bins per call in round-robin fashion.
   */
  calculateTempi(): void {
    this.updateTempoIter++;

    this.normaliseNoveltyCurve();

    const maxBin = Math.floor((NUM_TEMPI - 1) * this.maxTempoRange);

    if (this.updateTempoIter % 2 === 0) {
      this.calculateTempiMagnitudes(this.calcBin + 0);
    } else {
      this.calculateTempiMagnitudes(this.calcBin + 1);
    }

    this.calcBin += 2;
    if (this.calcBin >= maxBin) {
      this.calcBin = 0;
    }
  }

  /**
   * Update phase for all tempi bins and compute tempo confidence.
   * Exact port of update_tempi_phase().
   *
   * @param delta - ES-style delta (elapsed_us / ideal_us_interval at REFERENCE_FPS)
   */
  updateTempiPhase(delta: number): void {
    this.tempiPowerSum = 0.00000001;

    for (let bin = 0; bin < NUM_TEMPI; bin++) {
      const t = this.tempi[bin]!;
      const tempiMagnitude = t.magnitude;

      // Tempo EMA: retune alpha against baseline chunk rate
      const prev = f32at(this.tempiSmooth, bin);
      const smoothed = prev * (1.0 - TEMPO_ALPHA) + tempiMagnitude * TEMPO_ALPHA;
      this.tempiSmooth[bin] = smoothed;
      this.tempiPowerSum += smoothed;

      this.syncBeatPhaseVendor(bin, delta);
    }

    // Confidence: max contribution of any single bin
    let maxContribution = 0.000001;
    for (let bin = 0; bin < NUM_TEMPI; bin++) {
      maxContribution = Math.max(
        f32at(this.tempiSmooth, bin) / this.tempiPowerSum,
        maxContribution,
      );
    }
    this.tempoConfidence = maxContribution;
  }

  // ==========================================================================
  // Octave-Aware Tempo Selection
  // ==========================================================================

  /**
   * Pick the top tempo bin using the full octave-aware selection logic.
   * Exact port of esv11_pick_top_tempo_bin_octave_aware() from tempo.h.
   *
   * Includes:
   * - octave_double_persist counter (50 calls ~200ms)
   * - Double gate: raw_bpm < 80 AND confidence > 0.06
   * - Ratio thresholds: 0.56 at BPM <= 72, 0.72 at BPM > 72
   * - Edge rebound: raw_bpm >= 138 AND confidence < 0.35
   * - 210-BPM alias rescue: 128 <= raw_bpm <= 136 AND confidence < 0.32
   * - Half-time rescue: raw_bpm >= 132, half_mag >= selected * 0.92
   */
  pickTopTempo(): number {
    const rawBin = this.pickTopTempoBinRaw();
    const rawMag = Math.max(f32at(this.tempiSmooth, rawBin), 1e-6);
    const rawBpm = TEMPO_LOW + rawBin;

    let selectedBin = rawBin;
    let selectedScore = rawMag;

    // --- Octave doubling for sub-80 BPM ---
    const doubleBin = this.bpmToBin(rawBpm * 2.0);
    if (doubleBin >= 0) {
      // Persistence counter: require 50 consecutive calls (~200ms @ 250Hz)
      // in the doubling zone before committing.
      const wantDouble = rawBpm < 80.0 && this.tempoConfidence > 0.06;
      if (wantDouble) {
        if (this.octaveDoublePersist < 255) this.octaveDoublePersist++;
      } else {
        this.octaveDoublePersist = 0;
      }
      if (this.octaveDoublePersist >= 50) {
        selectedBin = doubleBin;
        selectedScore = this.pickLocalBinMagnitude(doubleBin, 1);
      }

      const doubleMag = this.pickLocalBinMagnitude(doubleBin, 1);
      const ratio = doubleMag / rawMag;
      const ratioThreshold = rawBpm <= 72.0 ? 0.56 : 0.72;
      if (ratio >= ratioThreshold && doubleMag > selectedScore) {
        selectedBin = doubleBin;
        selectedScore = doubleMag;
      }
    }

    // --- Edge rebound: raw pinned near bank ceiling ---
    if (rawBpm >= 138.0 && this.tempoConfidence < 0.35) {
      const result = this.findPeakBinInBpmWindow(76.0, 84.0, 1);
      if (result.mag >= rawMag * 0.70) {
        selectedBin = result.bin;
        selectedScore = result.mag;
      }
    }

    // --- 210-BPM alias rescue ---
    if (rawBpm >= 128.0 && rawBpm <= 136.0 && this.tempoConfidence < 0.32) {
      const result = this.findPeakBinInBpmWindow(102.0, 108.0, 1);
      if (
        result.mag >= 0.09 &&
        result.mag >= rawMag * 0.10 &&
        result.mag > selectedScore * 0.80
      ) {
        selectedBin = result.bin;
        selectedScore = result.mag;
      }
    }

    // --- Half-time rescue ---
    if (rawBpm >= 132.0) {
      const halfBin = this.bpmToBin(rawBpm * 0.5);
      if (halfBin >= 0) {
        const halfMag = this.pickLocalBinMagnitude(halfBin, 1);
        if (halfMag >= selectedScore * 0.92) {
          selectedBin = halfBin;
          selectedScore = halfMag;
        }
      }
    }

    return selectedBin;
  }

  /**
   * Full bin stabilisation and output refresh.
   * Exact port of EsV11Backend::refreshOutputs() tempo section.
   *
   * Call this after calculateTempi() + updateTempiPhase() each frame.
   *
   * @param nowMs - Current time in milliseconds (e.g. performance.now())
   */
  stabiliseAndRefresh(nowMs: number): void {
    const candidateI = this.pickTopTempo();
    const rawI = this.pickTopTempoBinRaw();

    // Octave-partner surrogate for BOTH sides of the magnitude comparison
    let candidateMag = f32at(this.tempiSmooth, candidateI);
    if (candidateI !== rawI && rawI < NUM_TEMPI) {
      candidateMag = Math.max(candidateMag, f32at(this.tempiSmooth, rawI));
    }
    let currentMag = f32at(this.tempiSmooth, this.stableTopBin);
    {
      const stabBpmI = TEMPO_LOW + this.stableTopBin;
      const halfBpm = Math.floor(stabBpmI / 2);
      if (halfBpm >= TEMPO_LOW) {
        const halfIdx = halfBpm - TEMPO_LOW;
        if (halfIdx < NUM_TEMPI) {
          currentMag = Math.max(currentMag, f32at(this.tempiSmooth, halfIdx));
        }
      }
    }

    // Time-based hold
    const holdExpired = (nowMs - this.stableBinLockedMs) >= this.tp.holdMs;

    // Confidence-proportional gate
    const holdSec = (nowMs - this.stableBinLockedMs) / 1000.0;
    const gateStability = this.stableBinValidated
      ? holdSec / (holdSec + this.tp.gateTau)
      : 0.0;
    const kGenericGate = this.tp.gateBase + gateStability * this.tp.gateScale;

    // Low-confidence freeze
    const confidenceFreeze =
      this.tempoConfidence < this.tp.confFloor &&
      currentMag >= this.tp.decayFloor;

    // Octave-family check
    const stabBpm = TEMPO_LOW + this.stableTopBin;
    const candBpm = TEMPO_LOW + candidateI;
    const ratio = stabBpm > 0.0 ? candBpm / stabBpm : 0.0;
    const isOctaveUp =
      ratio > this.tp.octRatioLo && ratio < this.tp.octRatioHi;

    if (candidateI !== this.stableTopBin && !confidenceFreeze) {
      // Track octave candidate persistence
      if (isOctaveUp) {
        if (candidateI === this.octaveCandBin) {
          if (this.octaveCandRuns < 255) this.octaveCandRuns++;
        } else {
          this.octaveCandBin = candidateI;
          this.octaveCandRuns = 1;
        }
      } else {
        this.octaveCandRuns = 0;
      }

      // Track generic candidate persistence (time-based)
      if (candidateI === this.genericCandBin) {
        // Same candidate persisting -- timer keeps running
      } else {
        this.genericCandBin = candidateI;
        this.genericCandFirstMs = nowMs;
      }
      const genericPersisted =
        (nowMs - this.genericCandFirstMs) >= this.tp.genericPersistMs;

      if (holdExpired) {
        const octavePromotion =
          isOctaveUp && this.octaveCandRuns >= this.tp.octaveRuns;
        const genericPromotion =
          genericPersisted &&
          (candidateMag > currentMag * kGenericGate ||
            currentMag < this.tp.decayFloor);

        if (octavePromotion || genericPromotion) {
          this.stableTopBin = candidateI;
          this.stableBinLockedMs = nowMs;
          this.octaveCandRuns = 0;
          this.genericCandBin = 0;
          this.genericCandFirstMs = 0;
          this.stableBinValidated = false;
        }
      }
    } else {
      // Stable bin matches candidate -- reset all candidate tracking
      this.octaveCandRuns = 0;
      this.genericCandBin = 0;
      this.genericCandFirstMs = 0;
    }

    const topI = this.stableTopBin;
    const topMag = f32at(this.tempiSmooth, topI);
    const bpm = TEMPO_LOW + topI;

    // --- Composite confidence ---
    const rawConfidence = clamp01(this.tempoConfidence);

    if (rawConfidence >= this.tp.validationThr) {
      this.stableBinValidated = true;
    }
    const holdSeconds = (nowMs - this.stableBinLockedMs) / 1000.0;
    const stabilityConfidence = this.stableBinValidated
      ? holdSeconds / (holdSeconds + this.tp.stabilityTau)
      : 0.0;

    // Winner separation
    let secondPeak = 0.0;
    for (let i = 0; i < NUM_TEMPI; i++) {
      if (i !== topI) {
        const s = f32at(this.tempiSmooth, i);
        if (s > secondPeak) secondPeak = s;
      }
    }
    const winnerSeparation =
      secondPeak > this.tp.wsSepFloor
        ? clamp01(1.0 - secondPeak / topMag)
        : topMag > this.tp.wsSepFloor
          ? 1.0
          : 0.0;

    this.outputTempoConfidence = Math.max(
      rawConfidence,
      Math.max(stabilityConfidence, winnerSeparation),
    );
    this.outputBeatStrength = clamp01(topMag);

    // --- Beat tick: free-running phase accumulator ---
    const dt =
      this.lastRefreshMs === 0 ? 0.0 : (nowMs - this.lastRefreshMs) / 1000.0;
    this.lastRefreshMs = nowMs;
    const phaseInc = (bpm / 60.0) * dt;
    this.beatPhase += phaseInc;
    let tick = false;
    if (this.beatPhase >= 1.0) {
      this.beatPhase -= Math.floor(this.beatPhase);
      tick = true;
    }
    this.outputBeatTick = tick;
    // Convert 0..1 beat phase to [-PI, PI]
    this.outputPhaseRadians = (this.beatPhase * 2.0 - 1.0) * Math.PI;
  }

  // ==========================================================================
  // Public Getters
  // ==========================================================================

  /** Stabilised BPM (after octave-aware selection + bin stabilisation). */
  getTopBpm(): number {
    return TEMPO_LOW + this.stableTopBin;
  }

  /** Composite tempo confidence (0..1). */
  getTempoConfidence(): number {
    return this.outputTempoConfidence;
  }

  /** Beat tick: true on the frame where the beat phase wraps. */
  getBeatTick(): boolean {
    return this.outputBeatTick;
  }

  /** Beat strength: magnitude of the stable tempo bin (0..1). */
  getBeatStrength(): number {
    return this.outputBeatStrength;
  }

  /** Phase in radians [-PI, PI]. */
  getPhaseRadians(): number {
    return this.outputPhaseRadians;
  }

  /** Raw vendor confidence (peak/sum before stabilisation). */
  getRawTempoConfidence(): number {
    return this.tempoConfidence;
  }

  /** Whether silence is currently detected. */
  getSilenceDetected(): boolean {
    return this.silenceDetected;
  }

  /** Read/write access to runtime-tuneable stabilisation parameters. */
  getTempoParams(): TempoParams {
    return this.tp;
  }

  setTempoParams(params: Partial<TempoParams>): void {
    Object.assign(this.tp, params);
  }

  // ==========================================================================
  // Internal: Novelty Normalisation
  // ==========================================================================

  /**
   * Auto-scale novelty and VU curves into normalised buffers.
   * Exact port of normalize_novelty_curve() from tempo.h.
   */
  private normaliseNoveltyCurve(): void {
    // Novelty curve normalisation
    let maxVal = 0.00001;
    for (let i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
      const v = f32at(this.noveltyCurve, i);
      if (v > maxVal) maxVal = v;
    }
    const autoScale = 1.0 / maxVal;
    for (let i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
      this.noveltyCurveNormalised[i] = f32at(this.noveltyCurve, i) * autoScale;
    }

    // VU curve normalisation
    maxVal = 0.00001;
    for (let i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
      const v = f32at(this.vuCurve, i);
      if (v > maxVal) maxVal = v;
    }
    const vuAutoScale = 1.0 / maxVal;
    for (let i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
      this.vuCurveNormalised[i] = f32at(this.vuCurve, i) * vuAutoScale;
    }
  }

  // ==========================================================================
  // Internal: Silence Detection
  // ==========================================================================

  /**
   * Detect silence from the normalised novelty curve's contrast.
   * Exact port of check_silence() from tempo.h.
   *
   * Silence window: 2.56s regardless of frame rate.
   */
  private checkSilence(): void {
    // Normalise before checking — firmware calls normalize_novelty_curve()
    // at the start of update_tempo(), which runs before check_silence()
    // fires inside update_novelty(). We replicate that ordering.
    this.normaliseNoveltyCurve();

    const kSilenceFrames = Math.floor(2.56 * NOVELTY_LOG_HZ);
    let minVal = 1.0;
    let maxVal = 0.0;

    for (let i = 0; i < kSilenceFrames; i++) {
      let recentNovelty = f32at(
        this.noveltyCurveNormalised,
        NOVELTY_HISTORY_LENGTH - 1 - kSilenceFrames + i,
      );
      recentNovelty = Math.min(0.5, recentNovelty) * 2.0;
      const scaledValue = Math.sqrt(recentNovelty);
      maxVal = Math.max(maxVal, scaledValue);
      minVal = Math.min(minVal, scaledValue);
    }

    const noveltyContrast = Math.abs(maxVal - minVal);
    const silenceLevelRaw = 1.0 - noveltyContrast;

    this.silenceLevel = Math.max(0.0, silenceLevelRaw - 0.5) * 2.0;
    if (silenceLevelRaw > 0.5) {
      this.silenceDetected = true;
      this.reduceTempoHistory(this.silenceLevel * 0.10);
    } else {
      this.silenceLevel = 0.0;
      this.silenceDetected = false;
    }
  }

  /**
   * Decay both novelty and VU history buffers.
   * Exact port of reduce_tempo_history() from tempo.h.
   */
  private reduceTempoHistory(reductionAmount: number): void {
    const reductionAmountInv = 1.0 - reductionAmount;
    for (let i = 0; i < NOVELTY_HISTORY_LENGTH; i++) {
      this.noveltyCurve[i] = Math.max(
        f32at(this.noveltyCurve, i) * reductionAmountInv,
        0.00001,
      );
      this.vuCurve[i] = Math.max(
        f32at(this.vuCurve, i) * reductionAmountInv,
        0.00001,
      );
    }
  }

  // ==========================================================================
  // Internal: Goertzel Magnitude Computation
  // ==========================================================================

  /**
   * Compute the Goertzel magnitude for a single tempo bin.
   * Exact port of calculate_magnitude_of_tempo() from tempo.h.
   */
  private calculateMagnitudeOfTempo(tempoBin: number): number {
    const t = this.tempi[tempoBin]!;
    const blockSize = t.blockSize;
    let q1 = 0.0;
    let q2 = 0.0;
    let windowPos = 0.0;

    for (let i = 0; i < blockSize; i++) {
      const sampleNovelty = f32at(
        this.noveltyCurveNormalised,
        NOVELTY_HISTORY_LENGTH - 1 - blockSize + i,
      );
      const q0 = t.coeff * q1 - q2
        + sampleNovelty * f32at(this.windowLookup, Math.floor(windowPos));
      q2 = q1;
      q1 = q0;
      windowPos += t.windowStep;
    }

    const real = q1 - q2 * t.cosine;
    const imag = q2 * t.sine;

    // Phase computation with beat shift
    t.phase = Math.atan2(imag, real) + Math.PI * BEAT_SHIFT_PERCENT;
    if (t.phase > Math.PI) {
      t.phase -= 2 * Math.PI;
      t.phaseInverted = !t.phaseInverted;
    } else if (t.phase < -Math.PI) {
      t.phase += 2 * Math.PI;
      t.phaseInverted = !t.phaseInverted;
    }

    const magnitudeSquared = q1 * q1 + q2 * q2 - q1 * q2 * t.coeff;
    const magnitude = Math.sqrt(magnitudeSquared);
    return magnitude / (blockSize / 2.0);
  }

  /**
   * Calculate tempi magnitudes for a single bin (or all if -1).
   * Exact port of calculate_tempi_magnitudes() from tempo.h.
   *
   * Includes autoranging (minimum max_val 0.02) and magnitude sharpening (mag^3).
   */
  private calculateTempiMagnitudes(singleBin: number = -1): void {
    let maxVal = 0.0;

    for (let i = 0; i < NUM_TEMPI; i++) {
      const t = this.tempi[i]!;
      if (singleBin !== -1) {
        if (i === singleBin) {
          t.magnitudeFullScale = this.calculateMagnitudeOfTempo(singleBin);
        }
      } else {
        t.magnitudeFullScale = this.calculateMagnitudeOfTempo(i);
      }
      if (t.magnitudeFullScale > maxVal) {
        maxVal = t.magnitudeFullScale;
      }
    }

    // Autoranging: minimum floor of 0.02
    if (maxVal < 0.02) maxVal = 0.02;
    const autorangerScale = 1.0 / maxVal;

    // Magnitude sharpening: cube after normalisation
    for (let i = 0; i < NUM_TEMPI; i++) {
      const t = this.tempi[i]!;
      let scaledMag = t.magnitudeFullScale * autorangerScale;
      if (scaledMag < 0.0) scaledMag = 0.0;
      if (scaledMag > 1.0) scaledMag = 1.0;
      t.magnitude = scaledMag * scaledMag * scaledMag;
    }
  }

  // ==========================================================================
  // Internal: Beat Phase Synchronisation (vendor Goertzel phase)
  // ==========================================================================

  /**
   * Advance the vendor Goertzel phase for a single tempo bin.
   * Exact port of sync_beat_phase() from tempo.h.
   *
   * Note: the stable beat tick uses a separate free-running accumulator
   * (beatPhase in stabiliseAndRefresh). This vendor phase is used internally
   * by the Goertzel bank.
   */
  private syncBeatPhaseVendor(tempoBin: number, delta: number): void {
    const t = this.tempi[tempoBin]!;
    const push = t.phaseRadiansPerReferenceFrame * delta;
    t.phase += push;

    if (t.phase > Math.PI) {
      t.phase -= 2 * Math.PI;
      t.phaseInverted = !t.phaseInverted;
    } else if (t.phase < -Math.PI) {
      t.phase += 2 * Math.PI;
      t.phaseInverted = !t.phaseInverted;
    }

    t.beat = Math.sin(t.phase);
  }

  // ==========================================================================
  // Internal: Raw Bin Selection Helpers
  // ==========================================================================

  /**
   * Find the bin with the highest tempi_smooth value.
   * Exact port of esv11_pick_top_tempo_bin_raw().
   */
  private pickTopTempoBinRaw(): number {
    let topBin = 0;
    let topMag = 0.0;
    for (let i = 0; i < NUM_TEMPI; i++) {
      const m = f32at(this.tempiSmooth, i);
      if (m > topMag) {
        topMag = m;
        topBin = i;
      }
    }
    return topBin;
  }

  /**
   * Convert BPM to bin index. Returns -1 if out of range.
   * Exact port of esv11_bpm_to_bin().
   */
  private bpmToBin(bpm: number): number {
    const idx = Math.round(bpm - TEMPO_LOW);
    if (idx < 0 || idx >= NUM_TEMPI) {
      return -1;
    }
    return idx;
  }

  /**
   * Peak magnitude within +/-radius of a centre bin.
   * Exact port of esv11_pick_local_bin_magnitude().
   */
  private pickLocalBinMagnitude(centreBin: number, radius: number = 1): number {
    if (centreBin < 0 || centreBin >= NUM_TEMPI) {
      return 0.0;
    }
    let best = 0.0;
    for (let o = -radius; o <= radius; o++) {
      const idx = centreBin + o;
      if (idx >= 0 && idx < NUM_TEMPI) {
        best = Math.max(best, f32at(this.tempiSmooth, idx));
      }
    }
    return best;
  }

  /**
   * Find the peak bin within a BPM window, returning both bin and magnitude.
   * Exact port of esv11_find_peak_bin_in_bpm_window().
   */
  private findPeakBinInBpmWindow(
    bpmMin: number,
    bpmMax: number,
    radius: number,
  ): { bin: number; mag: number } {
    let startBin = this.bpmToBin(bpmMin);
    let endBin = this.bpmToBin(bpmMax);

    if (startBin < 0) startBin = 0;
    if (endBin < 0) endBin = NUM_TEMPI - 1;
    if (endBin >= NUM_TEMPI) endBin = NUM_TEMPI - 1;
    if (endBin < startBin) {
      const tmp = startBin;
      startBin = endBin;
      endBin = tmp;
    }

    let bestBin = startBin;
    let bestMag = 0.0;
    for (let bin = startBin; bin <= endBin; bin++) {
      const mag = this.pickLocalBinMagnitude(bin, radius);
      if (mag > bestMag) {
        bestMag = mag;
        bestBin = bin;
      }
    }
    return { bin: bestBin, mag: bestMag };
  }

  // ==========================================================================
  // Internal: Array Utilities
  // ==========================================================================

  /**
   * Shift a Float32Array left by `amount` positions, zeroing the tail.
   * Exact port of shift_array_left() from utilities_min.h.
   */
  private shiftArrayLeft(array: Float32Array, amount: number): void {
    if (amount >= array.length) {
      array.fill(0);
      return;
    }
    array.copyWithin(0, amount);
    array.fill(0, array.length - amount);
  }
}

export type { TempoParams, TempoResonator };
