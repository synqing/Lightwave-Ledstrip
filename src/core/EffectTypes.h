#ifndef EFFECT_TYPES_H
#define EFFECT_TYPES_H

#include <Arduino.h>

// Universal visual parameters for encoders 4-7
struct VisualParams {
    uint8_t intensity = 128;      // Effect intensity/amplitude (0-255)
    uint8_t saturation = 255;     // Color saturation (0-255)
    uint8_t complexity = 128;     // Effect complexity/detail (0-255)
    uint8_t variation = 0;        // Effect variation/mode (0-255)
    
    // Helper functions for normalized access
    float getIntensityNorm() const { return intensity / 255.0f; }
    float getSaturationNorm() const { return saturation / 255.0f; }
    float getComplexityNorm() const { return complexity / 255.0f; }
    float getVariationNorm() const { return variation / 255.0f; }
};

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
inline float clamp01(float t) { return constrain(t, 0.0f, 1.0f); }

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

inline float arc(
    float t,
    float buildPortion,
    float holdPortion,
    float releasePortion,
    EasingCurve buildCurve = EASE_IN_OUT_CUBIC,
    EasingCurve releaseCurve = EASE_IN_OUT_CUBIC
) {
    t = clamp01(t);
    float sum = buildPortion + holdPortion + releasePortion;
    if (sum <= 0.0f) return 0.0f;
    float buildEnd = buildPortion / sum;
    float holdEnd = (buildPortion + holdPortion) / sum;

    if (t <= buildEnd && buildEnd > 0.0f) {
        return ease(t / buildEnd, buildCurve);
    }
    if (t <= holdEnd) {
        return 1.0f;
    }
    float releaseDen = 1.0f - holdEnd;
    if (releaseDen <= 0.0f) return 0.0f;
    float rt = (t - holdEnd) / releaseDen;
    return 1.0f - ease(rt, releaseCurve);
}
}

// ============== NARRATIVE CYCLE ==============
// Multi-frame dramatic arc: BUILD → HOLD → RELEASE → REST → repeat
// Creates cinematic pacing for effects without audio input

enum NarrativePhase : uint8_t {
    PHASE_BUILD,    // Tension/approach - intensity rising
    PHASE_HOLD,     // The "beat" / hero moment - peak intensity
    PHASE_RELEASE,  // Resolution - intensity falling
    PHASE_REST      // Cooldown before next cycle - minimum intensity
};

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
    float snapAmount = 0.0f;       // 0-1: tanh compression at transitions (Snapwave-style punch)
    float durationVariance = 0.0f; // 0-1: randomizes total cycle length
    
    // Runtime state (managed internally)
    NarrativePhase phase = PHASE_BUILD;
    uint32_t phaseStartMs = 0;
    uint32_t cycleStartMs = 0;
    bool initialized = false;
    float currentCycleDuration = 0.0f;  // Includes variance if enabled
    
    // Get total cycle duration
    float getTotalDuration() const {
        return buildDuration + holdDuration + releaseDuration + restDuration;
    }
    
    // Initialize/reset the cycle
    void reset() {
        phase = PHASE_BUILD;
        phaseStartMs = millis();
        cycleStartMs = phaseStartMs;
        initialized = true;
        
        // Apply duration variance if enabled
        if (durationVariance > 0.0f) {
            float variance = (random(1000) / 1000.0f - 0.5f) * 2.0f * durationVariance;
            currentCycleDuration = getTotalDuration() * (1.0f + variance);
        } else {
            currentCycleDuration = getTotalDuration();
        }
    }
    
    // Get current phase duration (with variance scaling)
    float getPhaseDuration(NarrativePhase p) const {
        float base = 0.0f;
        switch (p) {
            case PHASE_BUILD:   base = buildDuration; break;
            case PHASE_HOLD:    base = holdDuration; break;
            case PHASE_RELEASE: base = releaseDuration; break;
            case PHASE_REST:    base = restDuration; break;
        }
        // Scale by variance ratio
        float totalBase = getTotalDuration();
        if (totalBase <= 0.0f) return base;
        return base * (currentCycleDuration / totalBase);
    }
    
    // Update state machine - call every frame
    void update() {
        if (!initialized) {
            reset();
        }
        
        uint32_t now = millis();
        float elapsed = (now - phaseStartMs) / 1000.0f;
        float phaseDur = getPhaseDuration(phase);
        
        // Check for phase transition
        if (elapsed >= phaseDur) {
            // Advance to next phase
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
                    // Cycle complete - restart
                    reset();
                    return;
            }
            phaseStartMs = now;
        }
    }
    
    // Get 0-1 progress within current phase
    float getPhaseT() const {
        uint32_t now = millis();
        float elapsed = (now - phaseStartMs) / 1000.0f;
        float phaseDur = getPhaseDuration(phase);
        if (phaseDur <= 0.0f) return 1.0f;
        return Easing::clamp01(elapsed / phaseDur);
    }
    
    // Apply Snapwave-style tanh compression for "punch"
    float applySnap(float t) const {
        if (snapAmount <= 0.0f) return t;
        float scaled = (t - 0.5f) * (2.0f + snapAmount * 4.0f);
        return (tanh(scaled) + 1.0f) * 0.5f;
    }
    
    // Apply subtle oscillation during hold
    float applyBreathe(float t) const {
        if (holdBreathe <= 0.0f) return 1.0f;
        // Sine wave pulse during hold
        float breathe = sin(t * PI * 2.0f) * holdBreathe;
        return 1.0f + breathe * 0.1f;  // ±10% at max breathe
    }
    
    // Get eased intensity (main output) - returns 0-1
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
        
        // Apply snap compression if enabled
        if (snapAmount > 0.0f && (phase == PHASE_BUILD || phase == PHASE_RELEASE)) {
            intensity = applySnap(intensity);
        }
        
        return Easing::clamp01(intensity);
    }
    
    // Query current phase
    NarrativePhase getPhase() const { return phase; }
    bool isIn(NarrativePhase p) const { return phase == p; }
    
    // Manual trigger - skip REST and force BUILD
    void trigger() {
        phase = PHASE_BUILD;
        phaseStartMs = millis();
        cycleStartMs = phaseStartMs;
    }
};

#endif // EFFECT_TYPES_H
