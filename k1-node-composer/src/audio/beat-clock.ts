/**
 * @file beat-clock.ts
 * @brief Renderer-domain beat clock — interpolates beat phase for smooth 60fps animation.
 *
 * Ported from firmware-v3/src/audio/backends/esv11/EsBeatClock.cpp.
 *
 * The AudioAdapter runs at 125 Hz (hop rate). Effects render at 60 fps.
 * This clock bridges the gap: it accepts BPM and beat-tick events from the
 * adapter at 125 Hz and advances beat phase at `bpm / 60 * dt` every
 * render frame, producing jitter-free phase for animations.
 *
 * Key behaviours (matching firmware):
 *   - Phase advances at bpm/60 beats per second
 *   - Beat tick fires when phase wraps past 1.0
 *   - Beat-in-bar tracks modulo 4 (4/4 time)
 *   - Beat strength decays exponentially from tick (tau ~0.30s)
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Default BPM when no tempo information is available. */
const DEFAULT_BPM = 120.0;

/** Exponential decay time constant for beat strength (seconds). */
const BEAT_STRENGTH_TAU = 0.30;

/** Number of beats per bar (4/4 time signature). */
const BEATS_PER_BAR = 4;

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

function clamp01(x: number): number {
  if (x < 0) return 0;
  if (x > 1) return 1;
  return x;
}

// ---------------------------------------------------------------------------
// BeatClock
// ---------------------------------------------------------------------------

/**
 * Renderer-domain beat clock for smooth 60fps beat phase interpolation.
 *
 * Usage:
 *   1. Call `onAudioFrame()` each time a new ControlBusFrame arrives from
 *      the AudioAdapter (~125 Hz). This synchronises BPM, phase, and ticks.
 *   2. Call `tick(dt)` every render frame (60fps). This advances phase
 *      smoothly between audio updates.
 *   3. Read `beatPhase`, `beatTick`, `beatStrength`, etc. for animation.
 */
export class BeatClock {
  // Phase and tempo state
  private phase01 = 0.0;
  private bpm = DEFAULT_BPM;
  private confidence = 0.0;

  // Beat-in-bar tracking
  private _beatInBar = 0;

  // Per-frame tick flags (reset each tick)
  private _beatTick = false;
  private _downbeatTick = false;

  // Beat strength (exponential decay from last tick)
  private _beatStrength = 0.0;

  // Synchronisation state
  private hasBase = false;

  // -----------------------------------------------------------------------
  // Public API — audio-rate input (125 Hz)
  // -----------------------------------------------------------------------

  /**
   * Synchronise the clock to a fresh audio frame from the AudioAdapter.
   *
   * Call this once per hop (~125 Hz) when new DSP data arrives.
   * The clock anchors its phase to the audio observation and will
   * interpolate forward from here at render cadence.
   *
   * @param audioBpm      Current BPM estimate from tempo processor
   * @param audioPhase01  Beat phase [0, 1) at the audio observation time
   * @param audioBeatTick True if a beat tick occurred this hop
   * @param audioBeatStrength  Beat strength [0, 1] at the tick
   * @param audioConfidence    Tempo confidence [0, 1]
   */
  onAudioFrame(
    audioBpm: number,
    audioPhase01: number,
    audioBeatTick: boolean,
    audioBeatStrength: number,
    audioConfidence: number,
  ): void {
    if (!this.hasBase) {
      this.hasBase = true;
      this.phase01 = clamp01(audioPhase01);
      this.bpm = audioBpm > 1.0 ? audioBpm : DEFAULT_BPM;
      this.confidence = clamp01(audioConfidence);
      return;
    }

    // Resynchronise to audio observation
    this.bpm = audioBpm > 1.0 ? audioBpm : this.bpm;
    this.confidence = clamp01(audioConfidence);
    this.phase01 = clamp01(audioPhase01);

    if (audioBeatTick) {
      // Hard-align to beat boundary (matching firmware behaviour)
      this.phase01 = 0.0;
      this._beatTick = true;
      this._beatStrength = clamp01(audioBeatStrength);
      this._beatInBar = (this._beatInBar + 1) % BEATS_PER_BAR;
      this._downbeatTick = this._beatInBar === 0;
    }
  }

  // -----------------------------------------------------------------------
  // Public API — render-rate tick (60 fps)
  // -----------------------------------------------------------------------

  /**
   * Advance the beat clock by one render frame.
   *
   * Call this at 60fps from the main animation loop. The clock integrates
   * phase forward at the current BPM, providing smooth sub-hop resolution.
   *
   * @param dt  Time since last render frame in seconds (typically ~0.0167)
   */
  tick(dt: number): void {
    // Reset per-frame tick flags
    this._beatTick = false;
    this._downbeatTick = false;

    if (!this.hasBase) return;

    // Integrate phase: advance by (bpm / 60) * dt beats
    const bps = this.bpm / 60.0;
    const phaseAdvance = dt * bps;
    this.phase01 += phaseAdvance;

    // Detect phase wrap (beat tick from interpolation)
    if (this.phase01 >= 1.0) {
      this.phase01 -= Math.floor(this.phase01);
      this._beatTick = true;
      this._beatStrength = 1.0;
      this._beatInBar = (this._beatInBar + 1) % BEATS_PER_BAR;
      this._downbeatTick = this._beatInBar === 0;
    }

    // Exponential decay of beat strength
    const decay = BEAT_STRENGTH_TAU > 0 ? Math.exp(-dt / BEAT_STRENGTH_TAU) : 0;
    this._beatStrength *= decay;
  }

  // -----------------------------------------------------------------------
  // Public API — read-only accessors
  // -----------------------------------------------------------------------

  /** Beat phase in [0, 1). Advances smoothly at 60fps. */
  get beatPhase(): number {
    return clamp01(this.phase01);
  }

  /** True for one render frame when a beat tick fires. */
  get beatTick(): boolean {
    return this._beatTick;
  }

  /** Beat strength [0, 1]. Set to 1.0 on tick, decays exponentially (tau ~0.30s). */
  get beatStrength(): number {
    return clamp01(this._beatStrength);
  }

  /** True for one render frame on a downbeat (beat 0 of bar). */
  get downbeatTick(): boolean {
    return this._downbeatTick;
  }

  /** Current beat position within a 4-beat bar (0..3). */
  get beatInBar(): number {
    return this._beatInBar;
  }

  /** Current BPM estimate. */
  get currentBpm(): number {
    return this.bpm;
  }

  /** Current tempo confidence [0, 1]. */
  get tempoConfidence(): number {
    return this.confidence;
  }

  // -----------------------------------------------------------------------
  // Reset
  // -----------------------------------------------------------------------

  /** Reset all state to initial values. */
  reset(): void {
    this.phase01 = 0.0;
    this.bpm = DEFAULT_BPM;
    this.confidence = 0.0;
    this._beatInBar = 0;
    this._beatTick = false;
    this._downbeatTick = false;
    this._beatStrength = 0.0;
    this.hasBase = false;
  }
}
