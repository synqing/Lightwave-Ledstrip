/**
 * @file EffectTypes.h
 * @brief Common types for effects, easing, and narrative timing
 *
 * LightwaveOS v2 - Core Types
 *
 * Provides:
 * - VisualParams: Universal effect parameters
 * - EasingCurve: 15 easing functions
 * - NarrativePhase/Cycle: Dramatic timing arc
 */

#pragma once

#include <Arduino.h>

namespace lightwaveos {
namespace effects {

// ============================================================================
// Visual Parameters
// ============================================================================

/**
 * @brief Universal visual parameters for effects
 */
struct VisualParams {
    uint8_t intensity = 128;      // Effect intensity/amplitude (0-255)
    uint8_t saturation = 255;     // Color saturation (0-255)
    uint8_t complexity = 128;     // Effect complexity/detail (0-255)
    uint8_t variation = 0;        // Effect variation/mode (0-255)

    float getIntensityNorm() const { return intensity / 255.0f; }
    float getSaturationNorm() const { return saturation / 255.0f; }
    float getComplexityNorm() const { return complexity / 255.0f; }
    float getVariationNorm() const { return variation / 255.0f; }
};

// ============================================================================
// Easing Curves
// ============================================================================

enum EasingCurve : uint8_t {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_ELASTIC,
    EASE_OUT_ELASTIC,
    EASE_IN_OUT_ELASTIC,
    EASE_IN_BOUNCE,
    EASE_OUT_BOUNCE,
    EASE_IN_BACK,
    EASE_OUT_BACK,
    EASE_IN_OUT_BACK
};

namespace Easing {

inline float clamp01(float t) {
    return constrain(t, 0.0f, 1.0f);
}

inline float ease(float t, EasingCurve curve) {
    t = clamp01(t);
    switch (curve) {
        case EASE_LINEAR:
            return t;
        case EASE_IN_QUAD:
            return t * t;
        case EASE_OUT_QUAD:
            return t * (2 - t);
        case EASE_IN_OUT_QUAD:
            return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case EASE_IN_CUBIC:
            return t * t * t;
        case EASE_OUT_CUBIC:
            t -= 1.0f;
            return t * t * t + 1.0f;
        case EASE_IN_OUT_CUBIC:
            return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
        case EASE_IN_ELASTIC:
            return t == 0 ? 0 : t == 1 ? 1 : -pow(2, 10 * (t - 1)) * sin((t - 1.1f) * 5 * PI);
        case EASE_OUT_ELASTIC:
            return t == 0 ? 0 : t == 1 ? 1 : pow(2, -10 * t) * sin((t - 0.1f) * 5 * PI) + 1;
        case EASE_IN_OUT_ELASTIC:
            if (t == 0) return 0;
            if (t == 1) return 1;
            t *= 2;
            if (t < 1) return -0.5f * pow(2, 10 * (t - 1)) * sin((t - 1.1f) * 5 * PI);
            return 0.5f * pow(2, -10 * (t - 1)) * sin((t - 1.1f) * 5 * PI) + 1;
        case EASE_IN_BOUNCE:
            return 1 - ease(1 - t, EASE_OUT_BOUNCE);
        case EASE_OUT_BOUNCE:
            if (t < 1 / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2 / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5 / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        case EASE_IN_BACK:
            return t * t * (2.70158f * t - 1.70158f);
        case EASE_OUT_BACK:
            t -= 1.0f;
            return 1 + t * t * (2.70158f * t + 1.70158f);
        case EASE_IN_OUT_BACK:
            t *= 2;
            if (t < 1) return 0.5f * t * t * (3.5949095f * t - 2.5949095f);
            t -= 2;
            return 0.5f * (t * t * (3.5949095f * t + 2.5949095f) + 2);
        default:
            return t;
    }
}

} // namespace Easing

// ============================================================================
// Narrative Timing (BUILD → HOLD → RELEASE → REST)
// ============================================================================

/**
 * @brief Narrative phase states for dramatic timing
 */
enum NarrativePhase : uint8_t {
    PHASE_BUILD,    // Tension/approach - intensity rising
    PHASE_HOLD,     // Peak intensity / "hero moment"
    PHASE_RELEASE,  // Resolution - intensity falling
    PHASE_REST      // Cooldown before next cycle
};

/**
 * @brief Multi-phase dramatic arc timing
 */
struct NarrativeCycle {
    // Phase durations in seconds
    float buildDuration = 1.5f;
    float holdDuration = 0.4f;
    float releaseDuration = 1.0f;
    float restDuration = 0.5f;

    // Easing curves for transitions
    EasingCurve buildCurve = EASE_IN_QUAD;
    EasingCurve releaseCurve = EASE_OUT_QUAD;

    // Optional behaviors
    float holdBreathe = 0.0f;      // 0-1: oscillation amplitude during hold
    float snapAmount = 0.0f;       // 0-1: tanh compression at transitions
    float durationVariance = 0.0f; // 0-1: randomizes total cycle length

    // Runtime state
    NarrativePhase phase = PHASE_BUILD;
    uint32_t phaseStartMs = 0;
    uint32_t cycleStartMs = 0;
    bool initialized = false;
    float currentCycleDuration = 0.0f;

    float getTotalDuration() const {
        return buildDuration + holdDuration + releaseDuration + restDuration;
    }

    void reset() {
        phase = PHASE_BUILD;
        phaseStartMs = millis();
        cycleStartMs = phaseStartMs;
        initialized = true;

        if (durationVariance > 0.0f) {
            float variance = (random(1000) / 1000.0f - 0.5f) * 2.0f * durationVariance;
            currentCycleDuration = getTotalDuration() * (1.0f + variance);
        } else {
            currentCycleDuration = getTotalDuration();
        }
    }

    float getPhaseDuration(NarrativePhase p) const {
        float base = 0.0f;
        switch (p) {
            case PHASE_BUILD:   base = buildDuration; break;
            case PHASE_HOLD:    base = holdDuration; break;
            case PHASE_RELEASE: base = releaseDuration; break;
            case PHASE_REST:    base = restDuration; break;
        }
        float totalBase = getTotalDuration();
        if (totalBase <= 0.0f) return base;
        return base * (currentCycleDuration / totalBase);
    }

    void update() {
        if (!initialized) {
            reset();
        }

        uint32_t now = millis();
        float elapsed = (now - phaseStartMs) / 1000.0f;
        float phaseDur = getPhaseDuration(phase);

        if (elapsed >= phaseDur) {
            switch (phase) {
                case PHASE_BUILD:
                    phase = PHASE_HOLD;
                    break;
                case PHASE_HOLD:
                    phase = PHASE_RELEASE;
                    break;
                case PHASE_RELEASE:
                    phase = PHASE_REST;
                    break;
                case PHASE_REST:
                    reset();
                    return;
            }
            phaseStartMs = now;
        }
    }

    float getPhaseT() const {
        uint32_t now = millis();
        float elapsed = (now - phaseStartMs) / 1000.0f;
        float phaseDur = getPhaseDuration(phase);
        if (phaseDur <= 0.0f) return 1.0f;
        return Easing::clamp01(elapsed / phaseDur);
    }

    float applySnap(float t) const {
        if (snapAmount <= 0.0f) return t;
        float scaled = (t - 0.5f) * (2.0f + snapAmount * 4.0f);
        return (tanh(scaled) + 1.0f) * 0.5f;
    }

    float applyBreathe(float t) const {
        if (holdBreathe <= 0.0f) return 1.0f;
        float breathe = sin(t * PI * 2.0f) * holdBreathe;
        return 1.0f + breathe * 0.1f;
    }

    float getIntensity() const {
        float t = getPhaseT();
        float intensity = 0.0f;

        switch (phase) {
            case PHASE_BUILD:
                intensity = Easing::ease(t, buildCurve);
                break;
            case PHASE_HOLD:
                intensity = applyBreathe(t);
                break;
            case PHASE_RELEASE:
                intensity = 1.0f - Easing::ease(t, releaseCurve);
                break;
            case PHASE_REST:
                intensity = 0.0f;
                break;
        }

        if (snapAmount > 0.0f && (phase == PHASE_BUILD || phase == PHASE_RELEASE)) {
            intensity = applySnap(intensity);
        }

        return Easing::clamp01(intensity);
    }

    NarrativePhase getPhase() const { return phase; }
    bool isIn(NarrativePhase p) const { return phase == p; }

    void trigger() {
        phase = PHASE_BUILD;
        phaseStartMs = millis();
        cycleStartMs = phaseStartMs;
    }
};

} // namespace effects
} // namespace lightwaveos
