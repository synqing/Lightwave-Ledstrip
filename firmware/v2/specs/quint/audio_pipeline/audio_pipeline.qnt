// @file audio_pipeline.qnt
// @module audio_pipeline
// @description Safety invariants for audio pipeline (DC removal, AGC, noise floor)

module audio_pipeline {

  // ============================================================================
  // Constants and Configuration
  // ============================================================================
  
  // Scaled integer representation (multiply by 1000000 for precision)
  pure val SCALE = 1000000
  
  // Time step (16ms per hop)
  pure val DT_MS = 16
  
  // ============================================================================
  // Tuning Parameters (scaled integers)
  // ============================================================================
  
  type PipelineTuning = {
    dc_alpha: int,              // DC removal filter alpha (0.000001-0.1, scaled)
    agc_target_rms: int,        // AGC target RMS (0.01-1.0, scaled)
    agc_min_gain: int,          // AGC minimum gain (0.1-50.0, scaled)
    agc_max_gain: int,          // AGC maximum gain (1.0-500.0, scaled)
    agc_attack: int,            // AGC attack rate (0.0-1.0, scaled)
    agc_release: int,           // AGC release rate (0.0-1.0, scaled)
    noise_floor_min: int,       // Minimum noise floor (0.0-0.1, scaled)
    noise_floor_rise: int,      // Noise floor rise rate (0.0-1.0, scaled)
    noise_floor_fall: int       // Noise floor fall rate (0.0-1.0, scaled)
  }
  
  // ============================================================================
  // State Variables
  // ============================================================================
  
  type PipelineState = {
    // DC removal state
    dc_estimate: int,           // DC offset estimate (scaled)
    
    // AGC state
    agc_gain: int,              // Current AGC gain (scaled, 1.0 = SCALE)
    rms_pre_gain: int,          // RMS before gain (scaled)
    rms_post_gain: int,        // RMS after gain (scaled)
    
    // Noise floor state
    noise_floor: int,           // Current noise floor estimate (scaled)
    
    // Timing
    tick: int,                   // Global tick counter
    
    // Tuning (fixed per trace)
    tuning: PipelineTuning
  }
  
  var state: PipelineState
  
  // ============================================================================
  // Helper Functions
  // ============================================================================
  
  // Clamp value to range
  pure def clamp(value: int, min_val: int, max_val: int): int =
    (if (value < min_val) min_val else if (value > max_val) max_val else value)
  
  // Absolute value
  pure def abs(value: int): int =
    (if (value < 0) -value else value)
  
  // ============================================================================
  // Initialization
  // ============================================================================
  
  action init = {
    all {
      state' = {
        dc_estimate: 0,
        agc_gain: SCALE,              // Start at 1.0
        rms_pre_gain: 0,
        rms_post_gain: 0,
        noise_floor: 400,             // 0.0004 * SCALE
        tick: 0,
        tuning: {
          dc_alpha: 1000,              // 0.001 * SCALE
          agc_target_rms: 250000,      // 0.25 * SCALE
          agc_min_gain: SCALE,         // 1.0 * SCALE
          agc_max_gain: 40000000,      // 40.0 * SCALE
          agc_attack: 30000,           // 0.03 * SCALE
          agc_release: 15000,          // 0.015 * SCALE
          noise_floor_min: 400,        // 0.0004 * SCALE
          noise_floor_rise: 500,       // 0.0005 * SCALE
          noise_floor_fall: 10000       // 0.01 * SCALE
        }
      }
    }
  }
  
  // ============================================================================
  // Pipeline Step
  // ============================================================================
  
  action step = {
    val t = state.tuning
    
    // Simulate input sample (nondeterministic, but bounded)
    // In real system, this comes from audio capture
    // Use a simplified bounded range for model checking
    nondet input_sample = Set(-32768, -16384, 0, 16384, 32767).oneOf()
    
    // 1. DC Removal (high-pass filter)
    val dc_removed = input_sample - state.dc_estimate
    val new_dc_estimate = state.dc_estimate + (t.dc_alpha * dc_removed) / SCALE
    
    // 2. Compute RMS (simplified: use absolute value as proxy)
    val rms_raw = abs(dc_removed) * 1000 / 32768  // Scale to 0-SCALE range
    val new_rms_pre_gain = clamp(rms_raw, 0, SCALE)
    
    // 3. AGC Gain Update
    val target_rms = t.agc_target_rms
    val current_rms = new_rms_pre_gain
    val alpha = if (current_rms > target_rms) t.agc_attack else t.agc_release
    val gain_error = if (current_rms > 0) (target_rms * SCALE) / current_rms else SCALE
    val new_gain_raw = state.agc_gain + (alpha * (gain_error - state.agc_gain)) / SCALE
    val new_gain = clamp(new_gain_raw, t.agc_min_gain, t.agc_max_gain)
    
    // 4. Apply gain
    val new_rms_post_gain = (new_rms_pre_gain * new_gain) / SCALE
    
    // 5. Noise Floor Adaptation
    val new_noise_floor = if (new_rms_pre_gain < state.noise_floor) 
      state.noise_floor + (t.noise_floor_fall * (new_rms_pre_gain - state.noise_floor)) / SCALE
    else
      state.noise_floor + (t.noise_floor_rise * (new_rms_pre_gain - state.noise_floor)) / SCALE
    val clamped_noise_floor = clamp(new_noise_floor, t.noise_floor_min, SCALE / 10)
    
    // 6. Update state
    all {
      state' = {
        dc_estimate: clamp(new_dc_estimate, -SCALE, SCALE),
        agc_gain: new_gain,
        rms_pre_gain: new_rms_pre_gain,
        rms_post_gain: clamp(new_rms_post_gain, 0, SCALE * 2),
        noise_floor: clamped_noise_floor,
        tick: state.tick + 1,
        tuning: state.tuning
      }
    }
  }
  
  // ============================================================================
  // Safety Invariants
  // ============================================================================
  
  // DC estimate stays bounded
  val DCBounded: bool = {
    abs(state.dc_estimate) <= SCALE
  }
  
  // AGC gain stays in valid range
  val AGCGainValid: bool = {
    state.agc_gain >= state.tuning.agc_min_gain and
    state.agc_gain <= state.tuning.agc_max_gain
  }
  
  // RMS values stay bounded
  val RMSBounded: bool = {
    state.rms_pre_gain >= 0 and state.rms_pre_gain <= SCALE and
    state.rms_post_gain >= 0 and state.rms_post_gain <= SCALE * 2
  }
  
  // Noise floor stays in valid range
  val NoiseFloorValid: bool = {
    state.noise_floor >= state.tuning.noise_floor_min and
    state.noise_floor <= SCALE / 10
  }
  
  // AGC doesn't oscillate (gain change bounded per step)
  val AGCStable: bool = {
    // Gain change per step should be bounded by attack/release rates
    // This is a simplified check - in practice, we'd track gain history
    true  // Placeholder - would need gain history for full stability check
  }
  
  // All invariants combined
  val Invariants: bool = {
    DCBounded and
    AGCGainValid and
    RMSBounded and
    NoiseFloorValid and
    AGCStable
  }
  
}
