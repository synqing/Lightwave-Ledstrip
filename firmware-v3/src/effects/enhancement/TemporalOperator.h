/**
 * @file TemporalOperator.h
 * @brief Parameterized temporal envelope operators for the motion-semantic layer
 *
 * ## Architecture
 *
 * Eight temporal operators that shape how motion-semantic dimensions and LED
 * render parameters change over time. Each operator is a closed-form function
 * executable in <0.05ms on ESP32-S3. Operators chain into composite envelopes
 * (max 4 stages) between the inference engine and the render pipeline.
 *
 * Pipeline position:
 *   dsp_frame_t -> inference -> motion_semantic_frame_t -> TEMPORAL OPERATORS -> render mapping -> LED framebuffer
 *
 * ## Operator Summary
 *
 * | Operator       | Purpose                                  | Dominant cost  |
 * |----------------|------------------------------------------|----------------|
 * | Anticipation   | Counter-movement windup before action     | sinf (~0.005ms)|
 * | Attack         | Onset ramp (lin/exp/log curves)           | expf (~0.003ms)|
 * | Sustain        | Hold with optional flutter                | sinf (~0.003ms)|
 * | Decay          | Fade toward floor value                   | expf (~0.003ms)|
 * | Follow-through | Overshoot with damped oscillation         | expf+cosf      |
 * | Recoil         | Elastic bounce series                    | powf+cosf      |
 * | Ease-in/out    | Power-curve acceleration/deceleration     | powf (~0.003ms)|
 * | Suspension     | Tension hold at peak                     | comparison     |
 *
 * Reference: SPEC - Multi - Temporal Operator Formalization
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace enhancement {

// ============================================================================
// Constants
// ============================================================================

static constexpr float TEMPORAL_PI  = 3.14159265358979323846f;
static constexpr float TEMPORAL_2PI = 6.28318530717958647692f;
static constexpr float TEMPORAL_E   = 2.71828182845904523536f;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Temporal operator types
 *
 * Each type corresponds to a distinct time-dependent shaping function.
 * Maps to parameter slot assignments documented per operator.
 */
enum class TemporalOpType : uint8_t {
    ANTICIPATION    = 0,  ///< Counter-movement windup
    ATTACK          = 1,  ///< Onset ramp
    SUSTAIN         = 2,  ///< Hold with optional flutter
    DECAY           = 3,  ///< Fade toward floor
    FOLLOW_THROUGH  = 4,  ///< Overshoot + damped oscillation
    RECOIL          = 5,  ///< Elastic bounce series
    EASE_IN_OUT     = 6,  ///< Power-curve transitions
    SUSPENSION      = 7   ///< Tension hold at peak
};

/**
 * @brief Curve shapes for Attack, Decay, and Ease operators
 */
enum class CurveType : uint8_t {
    LINEAR      = 0,
    EXPONENTIAL = 1,
    LOGARITHMIC = 2
};

/**
 * @brief Flutter behavior for Sustain operator
 */
enum class FlutterType : uint8_t {
    NONE    = 0,  ///< Static hold
    TREMOLO = 1,  ///< 5 Hz sine modulation
    DRIFT   = 2   ///< Two incommensurate sines (aperiodic)
};

/**
 * @brief Ease direction mode
 */
enum class EaseMode : uint8_t {
    EASE_IN   = 0,  ///< Slow start, fast end (t^n)
    EASE_OUT  = 1,  ///< Fast start, slow end (1-(1-t)^n)
    EASE_BOTH = 2   ///< Symmetric ease (smoothstep variant)
};

// ============================================================================
// TemporalOp - Single Operator Instance
// ============================================================================
// Packs type-specific parameters into a fixed 4-slot array.
// Slot assignments are documented per operator in the eval functions below.
//
// Usage:
//   TemporalOp op;
//   op.type = TemporalOpType::ATTACK;
//   op.params[0] = (float)CurveType::EXPONENTIAL;  // curve
//   op.params[1] = 50.0f;                           // duration_ms
//   op.trigger_ms = now;
//   op.base_value = 0.0f;
//   op.target_value = 1.0f;
//   float val = temporal_op_eval(op, now + 25);
// ============================================================================

struct TemporalOp {
    TemporalOpType type;
    float params[4];        ///< Up to 4 type-specific parameters
    uint32_t trigger_ms;    ///< Timestamp when this operator was triggered
    float base_value;       ///< Value at trigger time
    float target_value;     ///< Target value (for transitions)
};

// ============================================================================
// Per-Operator Evaluation Functions
// ============================================================================
// Each function is self-contained with documented parameter slot assignments.
// All use single-precision math (sinf, cosf, expf, powf) for ESP32 FPU.
// ============================================================================

/**
 * @brief Anticipation: counter-movement before action
 *
 * params[0] = magnitude (0.0 - 0.3, default 0.15) fraction of total change
 * params[1] = duration_ms (50 - 200, default 80)
 *
 * Half-sine arc dips opposite to intended direction, then returns to base.
 */
inline float anticipation_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float magnitude   = op.params[0];
    float duration_ms = op.params[1];

    if (duration_ms <= 0.0f) return op.base_value;
    if (elapsed_ms >= (uint32_t)duration_ms) return op.base_value;

    float t = (float)elapsed_ms / duration_ms;
    float d = op.target_value - op.base_value;
    return op.base_value - magnitude * d * sinf(TEMPORAL_PI * t);
}

/**
 * @brief Attack: onset ramp from base to target
 *
 * params[0] = curve (0=LINEAR, 1=EXPONENTIAL, 2=LOGARITHMIC)
 * params[1] = duration_ms (5 - 500, default 50)
 *
 * Exponential front-loads (percussive). Logarithmic back-loads (crescendo).
 */
inline float attack_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float duration_ms = op.params[1];

    if (duration_ms <= 0.0f) return op.target_value;

    float t = (float)elapsed_ms / duration_ms;
    if (t >= 1.0f) t = 1.0f;

    float shaped;
    uint8_t curve = (uint8_t)op.params[0];

    switch (curve) {
        case (uint8_t)CurveType::EXPONENTIAL:
            shaped = 1.0f - expf(-4.0f * t);
            break;
        case (uint8_t)CurveType::LOGARITHMIC:
            shaped = logf(1.0f + t * (TEMPORAL_E - 1.0f));
            break;
        default: // LINEAR
            shaped = t;
            break;
    }

    return op.base_value + (op.target_value - op.base_value) * shaped;
}

/**
 * @brief Sustain: hold at level with optional flutter
 *
 * params[0] = level (0.0 - 1.0, default 1.0) fraction of peak
 * params[1] = flutter_type (0=NONE, 1=TREMOLO, 2=DRIFT)
 * params[2] = flutter_depth (0.0 - 0.1, default 0.03)
 *
 * Tremolo: 5 Hz sine modulation. Drift: two incommensurate sines.
 * Sustain has no inherent duration -- terminated externally or by envelope.
 */
inline float sustain_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float level         = op.params[0];
    uint8_t flutter     = (uint8_t)op.params[1];
    float flutter_depth = op.params[2];

    float base_level = op.target_value * level;

    switch (flutter) {
        case (uint8_t)FlutterType::TREMOLO: {
            float t_sec = (float)elapsed_ms * 0.001f;
            float mod = sinf(TEMPORAL_2PI * 5.0f * t_sec);
            return base_level * (1.0f + flutter_depth * mod);
        }
        case (uint8_t)FlutterType::DRIFT: {
            float phase = (float)elapsed_ms * 0.007f;
            float mod = sinf(phase) * cosf(phase * 1.7f);
            return base_level * (1.0f + flutter_depth * mod);
        }
        default: // NONE
            return base_level;
    }
}

/**
 * @brief Decay: fade from base toward floor
 *
 * params[0] = curve (0=LINEAR, 1=EXPONENTIAL, 2=LOGARITHMIC)
 * params[1] = time_constant_ms (50 - 5000, default 300)
 * params[2] = floor (0.0 - 1.0, default 0.0)
 *
 * Exponential reaches ~5% at 3x time constant (default 900ms).
 * 300ms default sits within the 200-500ms audiovisual binding window.
 */
inline float decay_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float time_constant_ms = op.params[1];
    float floor_val        = op.params[2];

    if (time_constant_ms <= 0.0f) return floor_val;

    float t = (float)elapsed_ms / time_constant_ms;
    float d = op.base_value - floor_val;

    float shaped;
    uint8_t curve = (uint8_t)op.params[0];

    switch (curve) {
        case (uint8_t)CurveType::EXPONENTIAL:
            shaped = expf(-t);
            break;
        case (uint8_t)CurveType::LOGARITHMIC:
            shaped = 1.0f - logf(1.0f + t * (TEMPORAL_E - 1.0f));
            if (shaped < 0.0f) shaped = 0.0f;
            break;
        default: // LINEAR
            shaped = 1.0f - t;
            if (shaped < 0.0f) shaped = 0.0f;
            break;
    }

    return floor_val + d * shaped;
}

/**
 * @brief Follow-through: overshoot + damped oscillation settling on target
 *
 * params[0] = overshoot_pct (0 - 50, default 20) percent past target
 * params[1] = settle_ms (100 - 1000, default 300) time to 5% residual
 *
 * Damped cosine centered on target. Damping derived from settle time:
 * exp(-damping * settle_sec) = 0.05, so damping = -ln(0.05) / settle_sec.
 * Frequency: one full cycle over settle duration.
 */
inline float followthrough_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float overshoot_pct = op.params[0];
    float settle_ms     = op.params[1];

    if (settle_ms <= 0.0f) return op.target_value;

    float t_sec      = (float)elapsed_ms * 0.001f;
    float settle_sec = settle_ms * 0.001f;

    float overshoot = overshoot_pct * 0.01f;
    float d         = op.target_value - op.base_value;

    // -ln(0.05) = 2.9957
    float damping = 2.9957f / settle_sec;
    float freq    = 1.0f / settle_sec;

    float envelope    = expf(-damping * t_sec);
    float oscillation = cosf(TEMPORAL_2PI * freq * t_sec);

    return op.target_value + overshoot * d * envelope * oscillation;
}

/**
 * @brief Recoil: elastic bounce with geometric amplitude decay
 *
 * params[0] = bounce_count (1 - 5, default 3)
 * params[1] = damping (0.3 - 0.9, default 0.5) energy retained per bounce
 * params[2] = total_ms (100 - 1000, default 400)
 *
 * Continuous formulation: geometric powf decay * absolute cosine bounces.
 * Returns 0 after total_ms elapses.
 */
inline float recoil_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float bounce_count = op.params[0];
    float damping      = op.params[1];
    float total_ms     = op.params[2];

    if (total_ms <= 0.0f) return 0.0f;
    if (elapsed_ms >= (uint32_t)total_ms) return 0.0f;

    float t_norm = (float)elapsed_ms / total_ms;

    // Geometric amplitude decay across bounces
    float amplitude = op.base_value * powf(damping, t_norm * bounce_count);

    // Absolute cosine: peak -> 0 -> peak per half-cycle
    float bounce = fabsf(cosf(TEMPORAL_PI * bounce_count * t_norm));

    return amplitude * bounce;
}

/**
 * @brief Ease-in/out: power-curve transition from base to target
 *
 * params[0] = mode (0=EASE_IN, 1=EASE_OUT, 2=EASE_BOTH)
 * params[1] = exponent (1.0 - 5.0, default 2.0) curve sharpness
 * params[2] = duration_ms (10 - 2000, default 500)
 *
 * EASE_IN:   t^n           (slow start, fast end)
 * EASE_OUT:  1-(1-t)^n     (fast start, slow end)
 * EASE_BOTH: smoothstep variant with symmetric ease
 */
inline float ease_in_out_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float duration_ms = op.params[2];

    if (duration_ms <= 0.0f) return op.target_value;

    float t = (float)elapsed_ms / duration_ms;
    if (t > 1.0f) t = 1.0f;

    float exponent = op.params[1];
    float shaped;
    uint8_t mode = (uint8_t)op.params[0];

    switch (mode) {
        case (uint8_t)EaseMode::EASE_OUT:
            shaped = 1.0f - powf(1.0f - t, exponent);
            break;
        case (uint8_t)EaseMode::EASE_BOTH:
            if (t < 0.5f) {
                shaped = powf(2.0f * t, exponent) * 0.5f;
            } else {
                shaped = 1.0f - powf(2.0f * (1.0f - t), exponent) * 0.5f;
            }
            break;
        default: // EASE_IN
            shaped = powf(t, exponent);
            break;
    }

    return op.base_value + (op.target_value - op.base_value) * shaped;
}

/**
 * @brief Suspension: hold target_value for a fixed duration
 *
 * params[0] = hold_ms (100 - 2000, default 500)
 *
 * Returns target_value while elapsed < hold_ms, then base_value.
 * Power comes from position in a composite envelope -- Suspension between
 * Attack and Decay transforms a standard pulse into a dramatic event.
 */
inline float suspension_eval(const TemporalOp& op, uint32_t elapsed_ms) {
    float hold_ms = op.params[0];

    if (elapsed_ms < (uint32_t)hold_ms) return op.target_value;
    return op.base_value;
}

// ============================================================================
// Dispatcher
// ============================================================================

/**
 * @brief Evaluate a temporal operator at the given timestamp
 * @param op The operator instance (type, params, trigger, base/target)
 * @param now_ms Current time in milliseconds
 * @return Shaped value at this point in time
 */
inline float temporal_op_eval(const TemporalOp& op, uint32_t now_ms) {
    uint32_t elapsed_ms = now_ms - op.trigger_ms;

    switch (op.type) {
        case TemporalOpType::ANTICIPATION:   return anticipation_eval(op, elapsed_ms);
        case TemporalOpType::ATTACK:         return attack_eval(op, elapsed_ms);
        case TemporalOpType::SUSTAIN:        return sustain_eval(op, elapsed_ms);
        case TemporalOpType::DECAY:          return decay_eval(op, elapsed_ms);
        case TemporalOpType::FOLLOW_THROUGH: return followthrough_eval(op, elapsed_ms);
        case TemporalOpType::RECOIL:         return recoil_eval(op, elapsed_ms);
        case TemporalOpType::EASE_IN_OUT:    return ease_in_out_eval(op, elapsed_ms);
        case TemporalOpType::SUSPENSION:     return suspension_eval(op, elapsed_ms);
        default:                             return op.base_value;
    }
}

// ============================================================================
// Stage Duration Helper
// ============================================================================

/**
 * @brief Extract effective duration from operator params
 *
 * Duration slot varies by operator type. Sustain returns UINT32_MAX
 * (externally terminated). Decay returns 3x time constant (~95% complete).
 */
inline uint32_t get_stage_duration_ms(const TemporalOp& op) {
    switch (op.type) {
        case TemporalOpType::ANTICIPATION:   return (uint32_t)op.params[1];
        case TemporalOpType::ATTACK:         return (uint32_t)op.params[1];
        case TemporalOpType::SUSTAIN:        return UINT32_MAX;
        case TemporalOpType::DECAY:          return (uint32_t)(op.params[1] * 3.0f);
        case TemporalOpType::FOLLOW_THROUGH: return (uint32_t)op.params[1];
        case TemporalOpType::RECOIL:         return (uint32_t)op.params[2];
        case TemporalOpType::EASE_IN_OUT:    return (uint32_t)op.params[2];
        case TemporalOpType::SUSPENSION:     return (uint32_t)op.params[0];
        default:                             return 0;
    }
}

// ============================================================================
// TemporalEnvelope - Composite Multi-Stage Envelope
// ============================================================================
// Chains up to 4 operators in sequence. When one stage completes, the next
// begins with base_value set to the output of the previous stage.
//
// Usage:
//   TemporalEnvelope env = makeImpactEnvelope(0.0f, 1.0f);
//   env.trigger(millis());
//   // In render loop:
//   float val = env.eval(millis());
//   if (env.isDone(millis())) { /* envelope complete */ }
// ============================================================================

struct TemporalEnvelope {
    TemporalOp stages[4];
    uint8_t stage_count;
    uint8_t current_stage;
    uint32_t stage_start_ms;

    /**
     * Evaluate the envelope at the given timestamp
     *
     * Advances through stages automatically. When a stage's elapsed time
     * exceeds its duration, the next stage begins with base_value chained
     * from the previous stage's output.
     *
     * @param now_ms Current time in milliseconds
     * @return Shaped value at this point in time
     */
    float eval(uint32_t now_ms) {
        if (stage_count == 0) return 0.0f;

        // Already past final stage
        if (current_stage >= stage_count) {
            const TemporalOp& last = stages[stage_count - 1];
            return temporal_op_eval(last, last.trigger_ms + get_stage_duration_ms(last));
        }

        TemporalOp& current = stages[current_stage];
        uint32_t elapsed = now_ms - stage_start_ms;
        uint32_t duration = get_stage_duration_ms(current);

        // Advance through completed stages
        while (elapsed >= duration && current_stage < stage_count - 1) {
            // Evaluate at completion time for chaining
            float end_value = temporal_op_eval(current, current.trigger_ms + duration);

            current_stage++;
            stage_start_ms += duration;

            // Chain: next stage starts where previous ended
            TemporalOp& next = stages[current_stage];
            next.base_value = end_value;
            next.trigger_ms = stage_start_ms;

            elapsed = now_ms - stage_start_ms;
            duration = get_stage_duration_ms(next);
        }

        // Evaluate current stage
        stages[current_stage].trigger_ms = stage_start_ms;
        return temporal_op_eval(stages[current_stage], now_ms);
    }

    /**
     * Check if all stages have completed
     * @param now_ms Current time in milliseconds
     * @return true if the envelope has finished all stages
     */
    bool isDone(uint32_t now_ms) const {
        if (stage_count == 0) return true;

        // Sum all stage durations
        uint32_t total_ms = 0;
        for (uint8_t i = 0; i < stage_count; i++) {
            uint32_t d = get_stage_duration_ms(stages[i]);
            if (d == UINT32_MAX) return false;  // Sustain never self-completes
            total_ms += d;
        }

        return (now_ms - stages[0].trigger_ms) >= total_ms;
    }

    /**
     * Reset envelope to stage 0 and arm at the given timestamp
     * @param now_ms Current time in milliseconds
     */
    void trigger(uint32_t now_ms) {
        current_stage = 0;
        stage_start_ms = now_ms;
        if (stage_count > 0) {
            stages[0].trigger_ms = now_ms;
        }
    }
};

// ============================================================================
// Preset Factory Functions
// ============================================================================
// Pre-built envelope configurations for common audio-reactive scenarios.
// All presets return a TemporalEnvelope ready to trigger().
// ============================================================================

/**
 * @brief Impact envelope: sharp percussive hit with elastic rebound
 *
 * Attack(10ms, LINEAR) -> Recoil(3 bounces, 0.5 damping, 400ms) -> Decay(300ms, EXP)
 *
 * Use for strong beat onsets (onset_strength > 0.7).
 *
 * @param base Resting value
 * @param peak Impact peak value
 */
inline TemporalEnvelope makeImpactEnvelope(float base, float peak) {
    TemporalEnvelope env = {};
    env.stage_count = 3;
    env.current_stage = 0;
    env.stage_start_ms = 0;

    // Stage 0: Attack - 10ms linear ramp to peak
    env.stages[0].type = TemporalOpType::ATTACK;
    env.stages[0].params[0] = (float)CurveType::LINEAR;
    env.stages[0].params[1] = 10.0f;
    env.stages[0].params[2] = 0.0f;
    env.stages[0].params[3] = 0.0f;
    env.stages[0].base_value = base;
    env.stages[0].target_value = peak;

    // Stage 1: Recoil - 3 bounces, 0.5 damping, 400ms
    env.stages[1].type = TemporalOpType::RECOIL;
    env.stages[1].params[0] = 3.0f;
    env.stages[1].params[1] = 0.5f;
    env.stages[1].params[2] = 400.0f;
    env.stages[1].params[3] = 0.0f;
    env.stages[1].base_value = peak;
    env.stages[1].target_value = base;

    // Stage 2: Decay - 300ms exponential fade
    env.stages[2].type = TemporalOpType::DECAY;
    env.stages[2].params[0] = (float)CurveType::EXPONENTIAL;
    env.stages[2].params[1] = 300.0f;
    env.stages[2].params[2] = 0.0f;
    env.stages[2].params[3] = 0.0f;
    env.stages[2].base_value = peak;
    env.stages[2].target_value = base;

    return env;
}

/**
 * @brief Standard envelope: smooth musical response
 *
 * Attack(50ms, EXP) -> Sustain(1.0, none) -> Decay(500ms, EXP)
 *
 * Use for moderate beat onsets (onset_strength 0.3-0.7).
 *
 * @param base Resting value
 * @param peak Sustain peak value
 */
inline TemporalEnvelope makeStandardEnvelope(float base, float peak) {
    TemporalEnvelope env = {};
    env.stage_count = 3;
    env.current_stage = 0;
    env.stage_start_ms = 0;

    // Stage 0: Attack - 50ms exponential ramp
    env.stages[0].type = TemporalOpType::ATTACK;
    env.stages[0].params[0] = (float)CurveType::EXPONENTIAL;
    env.stages[0].params[1] = 50.0f;
    env.stages[0].params[2] = 0.0f;
    env.stages[0].params[3] = 0.0f;
    env.stages[0].base_value = base;
    env.stages[0].target_value = peak;

    // Stage 1: Sustain - hold at full level, no flutter
    env.stages[1].type = TemporalOpType::SUSTAIN;
    env.stages[1].params[0] = 1.0f;
    env.stages[1].params[1] = (float)FlutterType::NONE;
    env.stages[1].params[2] = 0.0f;
    env.stages[1].params[3] = 0.0f;
    env.stages[1].base_value = peak;
    env.stages[1].target_value = peak;

    // Stage 2: Decay - 500ms exponential fade
    env.stages[2].type = TemporalOpType::DECAY;
    env.stages[2].params[0] = (float)CurveType::EXPONENTIAL;
    env.stages[2].params[1] = 500.0f;
    env.stages[2].params[2] = 0.0f;
    env.stages[2].params[3] = 0.0f;
    env.stages[2].base_value = peak;
    env.stages[2].target_value = base;

    return env;
}

/**
 * @brief Dramatic envelope: anticipation + overshoot momentum
 *
 * Anticipation(0.15, 100ms) -> Attack(30ms, EXP) -> Follow-through(20%, 300ms)
 *
 * Use for section boundaries, bass drops, dramatic entrances.
 *
 * @param base Resting value
 * @param peak Target peak value
 */
inline TemporalEnvelope makeDramaticEnvelope(float base, float peak) {
    TemporalEnvelope env = {};
    env.stage_count = 3;
    env.current_stage = 0;
    env.stage_start_ms = 0;

    // Stage 0: Anticipation - 100ms windup, 15% counter-dip
    env.stages[0].type = TemporalOpType::ANTICIPATION;
    env.stages[0].params[0] = 0.15f;
    env.stages[0].params[1] = 100.0f;
    env.stages[0].params[2] = 0.0f;
    env.stages[0].params[3] = 0.0f;
    env.stages[0].base_value = base;
    env.stages[0].target_value = peak;

    // Stage 1: Attack - 30ms exponential ramp
    env.stages[1].type = TemporalOpType::ATTACK;
    env.stages[1].params[0] = (float)CurveType::EXPONENTIAL;
    env.stages[1].params[1] = 30.0f;
    env.stages[1].params[2] = 0.0f;
    env.stages[1].params[3] = 0.0f;
    env.stages[1].base_value = base;
    env.stages[1].target_value = peak;

    // Stage 2: Follow-through - 20% overshoot, 300ms settle
    env.stages[2].type = TemporalOpType::FOLLOW_THROUGH;
    env.stages[2].params[0] = 20.0f;
    env.stages[2].params[1] = 300.0f;
    env.stages[2].params[2] = 0.0f;
    env.stages[2].params[3] = 0.0f;
    env.stages[2].base_value = base;
    env.stages[2].target_value = peak;

    return env;
}

/**
 * @brief Tension envelope: slow build, dramatic hold, sharp release
 *
 * Attack(200ms, ease-in via EASE_IN_OUT) -> Suspension(1000ms) -> Decay(100ms, LINEAR)
 *
 * Use for tension-before-resolution moments (held notes, silence before drops).
 *
 * @param base Resting value
 * @param peak Tension peak value
 */
inline TemporalEnvelope makeTensionEnvelope(float base, float peak) {
    TemporalEnvelope env = {};
    env.stage_count = 3;
    env.current_stage = 0;
    env.stage_start_ms = 0;

    // Stage 0: Ease-in attack - 200ms quadratic ease-in (slow start, fast end)
    env.stages[0].type = TemporalOpType::EASE_IN_OUT;
    env.stages[0].params[0] = (float)EaseMode::EASE_IN;
    env.stages[0].params[1] = 2.0f;
    env.stages[0].params[2] = 200.0f;
    env.stages[0].params[3] = 0.0f;
    env.stages[0].base_value = base;
    env.stages[0].target_value = peak;

    // Stage 1: Suspension - 1000ms hold at peak
    env.stages[1].type = TemporalOpType::SUSPENSION;
    env.stages[1].params[0] = 1000.0f;
    env.stages[1].params[1] = 0.0f;
    env.stages[1].params[2] = 0.0f;
    env.stages[1].params[3] = 0.0f;
    env.stages[1].base_value = peak;
    env.stages[1].target_value = peak;

    // Stage 2: Decay - 100ms linear drop
    env.stages[2].type = TemporalOpType::DECAY;
    env.stages[2].params[0] = (float)CurveType::LINEAR;
    env.stages[2].params[1] = 100.0f;
    env.stages[2].params[2] = 0.0f;
    env.stages[2].params[3] = 0.0f;
    env.stages[2].base_value = peak;
    env.stages[2].target_value = base;

    return env;
}

} // namespace enhancement
} // namespace effects
} // namespace lightwaveos
