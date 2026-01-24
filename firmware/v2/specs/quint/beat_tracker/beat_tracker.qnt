// @file beat_tracker.qnt
// @module beat_tracker
// @description Control-layer model of beat tracker for tuning sweep

module beat_tracker {

  // ============================================================================
  // Constants and Configuration
  // ============================================================================
  
  pure val DT_MS = 20              // Time step in milliseconds
  pure val BPM_MIN = 60            // Minimum valid BPM
  pure val BPM_MAX = 200           // Maximum valid BPM
  pure val BPM_BUCKET_STEP = 2     // BPM discretization step
  pure val CONF_BUCKETS = 11       // Confidence buckets (0.0-1.0 step 0.1)
  pure val PHASE_BUCKETS = 50      // Phase buckets (0.0-1.0 step 0.02)
  pure val IOI_HIST_LEN = 10       // IOI history buffer length
  pure val MAX_TIME_TICKS = 100    // Max time since onset (2000ms)
  pure val INVALID_BPM = 255       // Marker for invalid IOI slot
  
  // Tuning parameters (nondeterministic choices in MBT mode)
  type TuningParams = {
    refractory_ticks: int,        // 5, 7, or 10
    conf_gate: int,               // 5, 7, or 8 (maps to 0.5, 0.7, 0.8)
    alpha_attack: int,            // 10, 15, 20, or 25 (maps to 0.10, 0.15, 0.20, 0.25)
    alpha_release: int,           // 2, 5, or 10 (maps to 0.02, 0.05, 0.10)
    hold_ticks: int,              // 0, 25, or 50
    octave_mode: str,             // "off", "conservative", "aggressive"
    phase_nudge: int              // 0, 5, or 10 (maps to 0.0, 0.05, 0.10)
  }
  
  // Environment parameters (nondeterministic choices)
  type EnvParams = {
    true_bpm: int,                // Ground truth BPM (80, 100, 120, 140, 160, 180)
    jitter_ms: int,               // Jitter std dev (0, 5, 10, 20)
    p_miss_pct: int,              // Miss probability * 100 (0, 5, 10)
    p_false_pct: int              // False positive probability * 1000 (0, 10, 20)
  }
  
  // ============================================================================
  // State Variables
  // ============================================================================
  
  type BeatTrackerState = {
    // Core beat tracker state
    bpm_hat: int,                 // BPM bucket (0-70, maps to 60-200 step 2)
    conf: int,                    // Confidence bucket (0-10, maps to 0.0-1.0 step 0.1)
    phase: int,                   // Phase bucket (0-49, maps to 0.0-1.0 step 0.02)
    time_since_onset: int,        // Ticks since last onset (0-100)
    
    // IOI history (circular buffer)
    ioi_history: List[int],       // List of BPM buckets or INVALID_BPM (length 10)
    ioi_idx: int,                 // Write index (0-9)
    
    // Lock state
    locked: bool,                 // True when confidence above gate for hold_ticks
    hold_timer: int,              // Consecutive ticks above gate (0-hold_ticks)
    
    // Smoothing state
    bpm_smooth_raw: int,          // Pre-smoothed BPM (for attack/release logic)
    
    // Timing
    tick: int,                    // Global tick counter
    last_onset_tick: int,         // Tick of last detected onset
    
    // Tuning (fixed per trace)
    tuning: TuningParams,
    
    // Environment (fixed per trace, except tempo_step events)
    env: EnvParams,
    
    // Metrics (accumulated)
    metrics: {
      locked_ticks: int,
      first_lock_tick: int,        // -1 if never locked
      bpm_error_sum: int,          // Sum of |bpm_hat - true_bpm|
      thrash_count: int,           // BPM jumps > 10
      double_trigger_count: int,   // Onsets within refractory
      false_lock_ticks: int        // Locked with |bpm_hat - true_bpm| > 10
    }
  }
  
  var state: BeatTrackerState
  
  // ============================================================================
  // Helper Functions
  // ============================================================================
  
  // Convert BPM bucket to actual BPM
  pure def bucket_to_bpm(bucket: int): int =
    BPM_MIN + BPM_BUCKET_STEP * bucket
  
  // Convert actual BPM to bucket (with clamping)
  pure def bpm_to_bucket(bpm: int): int =
    (if ((bpm - BPM_MIN) / BPM_BUCKET_STEP < 0) 0 else if ((bpm - BPM_MIN) / BPM_BUCKET_STEP > 70) 70 else (bpm - BPM_MIN) / BPM_BUCKET_STEP)
  
  // Convert confidence bucket to float (0.0-1.0)
  pure def bucket_to_conf(bucket: int): int =
    bucket  // For display, multiply by 10 to get percentage
  
  // Clamp value to range
  pure def clamp(value: int, min_val: int, max_val: int): int =
    (if (value < min_val) min_val else if (value > max_val) max_val else value)
  
  // Absolute value
  pure def abs(value: int): int =
    (if (value < 0) -value else value)
  
  // Compute median of valid BPMs in IOI history
  pure def compute_median_bpm(history: List[int]): int =
    // Simplified: just use first valid BPM (median calculation complex without recursion)
    (if (history.select(x => x != INVALID_BPM).length() == 0) 0 else history.select(x => x != INVALID_BPM).head())
  
  // Count valid BPMs in IOI history
  pure def count_valid_bpms(history: List[int]): int =
    history.select(x => x != INVALID_BPM).length()
  
  // Apply octave correction based on mode - split into helper for complex nested logic
  pure def apply_octave_aggressive(bpm: int): int =
    (if ((if (bpm < BPM_MIN) bpm * 2 else bpm) > BPM_MAX) (if (bpm < BPM_MIN) bpm * 2 else bpm) / 2 else (if (bpm < BPM_MIN) bpm * 2 else bpm))
  
  pure def apply_octave_conservative_helper(bpm: int, last3: List[int]): int =
    (if (last3.foldl(true, (acc, b) => acc and bucket_to_bpm(b) < BPM_MIN)) bpm * 2 else if (last3.foldl(true, (acc, b) => acc and bucket_to_bpm(b) > BPM_MAX)) bpm / 2 else bpm)
  
  pure def apply_octave_conservative(bpm: int, valid: List[int]): int =
    (if (valid.length() < 3) bpm else apply_octave_conservative_helper(bpm, valid.slice(valid.length() - 3, valid.length())))
  
  pure def apply_octave_correction(bpm_bucket: int, mode: str, history: List[int]): int =
    (if (mode == "off") bpm_bucket else if (mode == "aggressive") bpm_to_bucket(apply_octave_aggressive(bucket_to_bpm(bpm_bucket))) else bpm_to_bucket(apply_octave_conservative(bucket_to_bpm(bpm_bucket), history.select(x => x != INVALID_BPM))))
  
  // Apply EMA smoothing with attack/release
  // Integer EMA: new = (alpha * target + (100 - alpha) * current) / 100
  pure def apply_smoothing(current: int, target: int, alpha_attack_pct: int, alpha_release_pct: int): int =
    bpm_to_bucket(((if (bucket_to_bpm(target) > bucket_to_bpm(current)) alpha_attack_pct else alpha_release_pct) * bucket_to_bpm(target) + (100 - (if (bucket_to_bpm(target) > bucket_to_bpm(current)) alpha_attack_pct else alpha_release_pct)) * bucket_to_bpm(current)) / 100)
  
  // Apply phase nudge (PLL correction)
  pure def apply_phase_nudge_helper(current_phase: int, nudge_pct: int): int =
    (if (current_phase - (current_phase * nudge_pct) / 100 < 0) 0 else current_phase - (current_phase * nudge_pct) / 100)
  
  pure def apply_phase_nudge(current_phase: int, nudge_pct: int): int =
    (if (nudge_pct == 0) current_phase else apply_phase_nudge_helper(current_phase, nudge_pct))
  
  // ============================================================================
  // Nondeterministic Choices (for MBT)
  // ============================================================================
  
  // Select tuning parameters nondeterministically
  // Expanded grid with 8 tunings covering different refractory periods
  pure val TUNING_OPTIONS: Set[TuningParams] = Set(
    // refractory_ticks: 5 (100ms) - aggressive, higher double-trigger risk
    { refractory_ticks: 5, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
    { refractory_ticks: 5, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
    
    // refractory_ticks: 7 (140ms) - moderate
    { refractory_ticks: 7, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
    { refractory_ticks: 7, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
    { refractory_ticks: 7, conf_gate: 8, alpha_attack: 20, alpha_release: 10, hold_ticks: 50, octave_mode: "aggressive", phase_nudge: 10 },
    
    // refractory_ticks: 10 (200ms) - conservative, should reduce double triggers
    { refractory_ticks: 10, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
    { refractory_ticks: 10, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
    { refractory_ticks: 10, conf_gate: 8, alpha_attack: 20, alpha_release: 10, hold_ticks: 50, octave_mode: "aggressive", phase_nudge: 10 }
  )
  
  // Select environment parameters nondeterministically
  pure val ENV_OPTIONS: Set[EnvParams] = Set(
    { true_bpm: 120, jitter_ms: 0, p_miss_pct: 0, p_false_pct: 0 },
    { true_bpm: 120, jitter_ms: 10, p_miss_pct: 5, p_false_pct: 10 },
    { true_bpm: 140, jitter_ms: 20, p_miss_pct: 10, p_false_pct: 20 }
  )
  
  // Decide if an onset should occur this tick (based on ground truth + jitter)
  pure def should_onset_occur(tick: int, true_bpm: int, jitter_ms: int): bool =
    (tick >= ((tick / ((60000 / true_bpm) / DT_MS)) * ((60000 / true_bpm) / DT_MS)) - (jitter_ms / DT_MS)) and (tick <= ((tick / ((60000 / true_bpm) / DT_MS)) * ((60000 / true_bpm) / DT_MS)) + (jitter_ms / DT_MS))
  
  // Decide if this onset is missed (probability)
  // Simplified: use tick % 100 as pseudo-random
  pure def is_onset_missed(p_miss_pct: int, tick: int): bool =
    (tick % 100) < p_miss_pct
  
  // Decide if a false positive occurs this tick
  // Simplified: use (tick * 7) % 1000 as pseudo-random
  pure def is_false_positive(p_false_pct: int, tick: int): bool =
    ((tick * 7) % 1000) < p_false_pct
  
  // ============================================================================
  // Initialization
  // ============================================================================
  
  // Common initialization logic (shared by all init actions)
  pure def init_state(tuning_val: TuningParams, env_val: EnvParams): BeatTrackerState = {
    bpm_hat: 0,
    conf: 0,
    phase: 0,
    time_since_onset: 100,
    ioi_history: List(
      INVALID_BPM, INVALID_BPM, INVALID_BPM, INVALID_BPM, INVALID_BPM,
      INVALID_BPM, INVALID_BPM, INVALID_BPM, INVALID_BPM, INVALID_BPM
    ),
    ioi_idx: 0,
    locked: false,
    hold_timer: 0,
    bpm_smooth_raw: 0,
    tick: 0,
    last_onset_tick: -100,
    tuning: tuning_val,
    env: env_val,
    metrics: {
      locked_ticks: 0,
      first_lock_tick: -1,
      bpm_error_sum: 0,
      thrash_count: 0,
      double_trigger_count: 0,
      false_lock_ticks: 0
    }
  }
  
  // Default init (nondeterministic tuning selection)
  action init = {
    nondet tuning_choice = TUNING_OPTIONS.oneOf()
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(tuning_choice, env_choice)
    }
  }
  
  // Deterministic init actions for per-tuning sweeps (init_tuning_0 through init_tuning_7)
  // Each sets a specific tuning from TUNING_OPTIONS and keeps env selection as nondet
  
  action init_tuning_0 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 5, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
        env_choice
      )
    }
  }
  
  action init_tuning_1 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 5, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
        env_choice
      )
    }
  }
  
  action init_tuning_2 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 7, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
        env_choice
      )
    }
  }
  
  action init_tuning_3 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 7, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
        env_choice
      )
    }
  }
  
  action init_tuning_4 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 7, conf_gate: 8, alpha_attack: 20, alpha_release: 10, hold_ticks: 50, octave_mode: "aggressive", phase_nudge: 10 },
        env_choice
      )
    }
  }
  
  action init_tuning_5 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 10, conf_gate: 5, alpha_attack: 10, alpha_release: 2, hold_ticks: 0, octave_mode: "off", phase_nudge: 0 },
        env_choice
      )
    }
  }
  
  action init_tuning_6 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 10, conf_gate: 7, alpha_attack: 15, alpha_release: 5, hold_ticks: 25, octave_mode: "conservative", phase_nudge: 5 },
        env_choice
      )
    }
  }
  
  action init_tuning_7 = {
    nondet env_choice = ENV_OPTIONS.oneOf()
    all {
      state' = init_state(
        { refractory_ticks: 10, conf_gate: 8, alpha_attack: 20, alpha_release: 10, hold_ticks: 50, octave_mode: "aggressive", phase_nudge: 10 },
        env_choice
      )
    }
  }
  
  // ============================================================================
  // Onset Detection and State Update
  // ============================================================================
  
  action step = {
    // State reads
    val t = state.tuning
    val e = state.env
    val ticks_since_last = state.tick - state.last_onset_tick
    
    // 1. Determine if an onset occurs this tick
    val expected_onset = should_onset_occur(state.tick, e.true_bpm, e.jitter_ms)
    val missed = is_onset_missed(e.p_miss_pct, state.tick)
    val false_pos = is_false_positive(e.p_false_pct, state.tick)
    val onset_occurs = (expected_onset and not(missed)) or false_pos
    
    // 2. Check refractory period
    val past_refractory = ticks_since_last >= t.refractory_ticks
    val onset_detected = onset_occurs and past_refractory
    
    // 3. Update metrics: detect double trigger
    val double_trigger = onset_occurs and not(past_refractory)
    
    // 4. If onset detected, update IOI history
    // Compute IOI values
    val ioi_ticks_computed = ticks_since_last
    val ioi_ms_computed = ioi_ticks_computed * DT_MS
    // BPM = 60000ms / IOI_ms (e.g., 500ms IOI = 120 BPM)
    val ioi_bpm_computed = if (ioi_ms_computed > 0) 60000 / ioi_ms_computed else 0
    val ioi_bucket_raw_computed = bpm_to_bucket(ioi_bpm_computed)
    val ioi_bucket_corrected_computed = apply_octave_correction(ioi_bucket_raw_computed, t.octave_mode, state.ioi_history)
    
    // State reads for IOI history update
    val new_ioi_history = if (onset_detected) state.ioi_history.replaceAt(state.ioi_idx, ioi_bucket_corrected_computed) else state.ioi_history
    val new_ioi_idx = if (onset_detected) (state.ioi_idx + 1) % IOI_HIST_LEN else state.ioi_idx
    
    // 5. Compute median BPM and confidence
    val median_bpm_bucket = compute_median_bpm(new_ioi_history)
    val valid_count = count_valid_bpms(new_ioi_history)
    val conf_bucket_raw = (valid_count * 10) / IOI_HIST_LEN  // Map 0-10 to 0-10
    val conf_bucket = clamp(conf_bucket_raw, 0, 10)
    
    // 6. Apply smoothing to BPM
    val bpm_smoothed = apply_smoothing(state.bpm_hat, median_bpm_bucket, t.alpha_attack, t.alpha_release)
    
    // 7. Update hold timer and locked state
    val conf_above_gate = conf_bucket >= t.conf_gate
    val new_hold_timer = if (conf_above_gate) clamp(state.hold_timer + 1, 0, t.hold_ticks + 1) else 0
    val new_locked = new_hold_timer >= t.hold_ticks
    
    // 8. Update phase
    val bpm_actual = bucket_to_bpm(bpm_smoothed)
    val period_ticks = if (bpm_actual > 0) (60000 / bpm_actual) / DT_MS else 1000
    val phase_increment = PHASE_BUCKETS / period_ticks  // Increment per tick
    val phase_raw = (state.phase + phase_increment) % PHASE_BUCKETS
    
    val phase_corrected = if (onset_detected) apply_phase_nudge(phase_raw, t.phase_nudge) else phase_raw
    
    // 9. Update metrics
    val true_bpm_bucket = bpm_to_bucket(e.true_bpm)
    val bpm_error = abs(bpm_smoothed - true_bpm_bucket)
    val is_thrash = abs(bpm_smoothed - state.bpm_hat) > 5  // >10 BPM change
    val is_false_lock = new_locked and (bpm_error > 5)     // >10 BPM error
    
    val new_metrics = {
      locked_ticks: state.metrics.locked_ticks + (if (new_locked) 1 else 0),
      first_lock_tick: if (state.metrics.first_lock_tick == -1 and new_locked) state.tick else state.metrics.first_lock_tick,
      bpm_error_sum: state.metrics.bpm_error_sum + bpm_error * BPM_BUCKET_STEP,
      thrash_count: state.metrics.thrash_count + (if (is_thrash) 1 else 0),
      double_trigger_count: state.metrics.double_trigger_count + (if (double_trigger) 1 else 0),
      false_lock_ticks: state.metrics.false_lock_ticks + (if (is_false_lock) 1 else 0)
    }
    
    // 10. Update state (wrap in all { } block)
    all {
      state' = {
        bpm_hat: bpm_smoothed,
        conf: conf_bucket,
        phase: phase_corrected,
        time_since_onset: if (onset_detected) 0 else clamp(state.time_since_onset + 1, 0, MAX_TIME_TICKS),
        ioi_history: new_ioi_history,
        ioi_idx: new_ioi_idx,
        locked: new_locked,
        hold_timer: new_hold_timer,
        bpm_smooth_raw: median_bpm_bucket,
        tick: state.tick + 1,
        last_onset_tick: if (onset_detected) state.tick else state.last_onset_tick,
        tuning: state.tuning,
        env: state.env,
        metrics: new_metrics
      }
    }
  }
  
  // ============================================================================
  // Invariants
  // ============================================================================
  
  // No double-trigger: onsets must respect refractory period
  val NoDoubleTrigger: bool = {
    state.metrics.double_trigger_count == 0
  }
  
  // Confidence in valid range
  val ConfidenceValid: bool = {
    state.conf >= 0 and state.conf <= 10
  }
  
  // BPM in valid range
  val BpmValid: bool = {
    state.bpm_hat >= 0 and state.bpm_hat <= 70
  }
  
  // Locked implies confidence above gate
  val LockedImpliesConfident: bool = {
    not(state.locked) or (state.conf >= state.tuning.conf_gate)
  }
  
  // Phase in valid range
  val PhaseValid: bool = {
    state.phase >= 0 and state.phase < PHASE_BUCKETS
  }
  
  // All invariants combined
  val Invariants: bool = {
    NoDoubleTrigger and
    ConfidenceValid and
    BpmValid and
    LockedImpliesConfident and
    PhaseValid
  }
  
  // ============================================================================
  // Witness Properties (Phase 2)
  // ============================================================================
  
  // Witness: Lock achieved within T ticks
  // Use as existence check: if this is ever true, we have a lock witness
  val LockAchieved: bool = state.locked
  
  // Witness: Lock achieved within 250 ticks (5 seconds)
  val LockWithin5Seconds: bool = state.locked and state.tick <= 250
  
  // Witness: Lock achieved with low error (<10 BPM)
  val AccurateLock: bool = {
    val true_bpm_bucket = (state.env.true_bpm - BPM_MIN) / BPM_BUCKET_STEP
    val error = abs(state.bpm_hat - true_bpm_bucket) * BPM_BUCKET_STEP
    state.locked and error <= 10
  }
  
  // Witness: Stable lock (locked for at least 100 ticks = 2 seconds)
  val StableLock: bool = state.metrics.locked_ticks >= 100
  
}
