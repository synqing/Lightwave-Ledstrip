/**
 * @file SmoothingEngine.h
 * @brief Centralized smoothing primitives for ultra-smooth audio-reactive rendering
 *
 * ## Architecture (based on Sensory Bridge 4.1.0)
 *
 * This module provides mathematically correct smoothing that is:
 * - Frame-rate INDEPENDENT (uses true exponential decay, not approximation)
 * - Configurable via MOOD knob (reactive vs smooth)
 * - Physics-based (optional spring dynamics)
 * - Subpixel-aware (anti-aliased LED positioning)
 *
 * ## Critical Formula
 *
 * WRONG (approximation - frame-rate dependent):
 *   alpha = dt / (tau + dt);
 *
 * CORRECT (true exponential decay - frame-rate independent):
 *   alpha = 1.0f - expf(-lambda * dt);
 *   // where lambda = 1/tau (convergence rate)
 *
 * Reference: https://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
 */

#pragma once

#include <cmath>
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace enhancement {

// ============================================================================
// ExpDecay - True Exponential Smoothing
// ============================================================================
// The foundation of smooth rendering. Uses mathematically correct formula
// that produces identical results at ANY frame rate.
//
// Usage:
//   ExpDecay smoother{0.0f, 5.0f};  // initial value, lambda (higher = faster)
//   float smoothed = smoother.update(target, dt);
// ============================================================================

struct ExpDecay {
    float value = 0.0f;
    float lambda = 5.0f;  // Convergence rate: higher = faster response

    /**
     * Update with TRUE exponential decay
     * @param target Target value to approach
     * @param dt Delta time in seconds
     * @return Smoothed value
     */
    float update(float target, float dt) {
        // TRUE frame-rate independent formula
        float alpha = 1.0f - expf(-lambda * dt);
        value += (target - value) * alpha;
        return value;
    }

    /**
     * Factory: Create from time constant (seconds to reach ~63% of target)
     * @param tauSeconds Time constant in seconds
     */
    static ExpDecay withTimeConstant(float tauSeconds) {
        return ExpDecay{0.0f, 1.0f / tauSeconds};
    }

    /**
     * Reset to specific value
     */
    void reset(float newValue = 0.0f) {
        value = newValue;
    }
};

// ============================================================================
// Spring - Critically Damped Physics
// ============================================================================
// For natural motion with momentum. Critically damped = fastest approach
// without overshoot (perfect for UI/visual elements).
//
// Usage:
//   Spring spring;
//   spring.init(100.0f, 1.0f);  // stiffness, mass
//   float pos = spring.update(target, dt);
// ============================================================================

struct Spring {
    float position = 0.0f;
    float velocity = 0.0f;
    float stiffness = 100.0f;
    float damping = 20.0f;
    float mass = 1.0f;

    /**
     * Initialize with critical damping (no overshoot)
     * @param stiffnessVal Spring stiffness (higher = snappier)
     * @param massVal Mass (higher = more inertia)
     */
    void init(float stiffnessVal = 100.0f, float massVal = 1.0f) {
        stiffness = stiffnessVal;
        mass = massVal;
        // Critical damping formula: damping = 2 * sqrt(stiffness * mass)
        damping = 2.0f * sqrtf(stiffness * mass);
    }

    /**
     * Update spring physics
     * @param target Target position
     * @param dt Delta time in seconds
     * @return Current position
     */
    float update(float target, float dt) {
        float displacement = position - target;
        float acceleration = (-stiffness * displacement - damping * velocity) / mass;
        velocity += acceleration * dt;
        position += velocity * dt;
        return position;
    }

    /**
     * Reset spring to position with zero velocity
     */
    void reset(float newPosition = 0.0f) {
        position = newPosition;
        velocity = 0.0f;
    }
};

// ============================================================================
// AsymmetricFollower - Sensory Bridge Pattern
// ============================================================================
// Different time constants for rising vs falling - essential for audio
// visualization where attacks should be fast but decays should be smooth.
//
// Usage:
//   AsymmetricFollower follower{0.0f, 0.05f, 0.30f};  // value, riseTau, fallTau
//   float smoothed = follower.update(target, dt);
//   float smoothed = follower.updateWithMood(target, dt, moodNorm);
// ============================================================================

struct AsymmetricFollower {
    float value = 0.0f;
    float riseTau = 0.05f;   // Rise time constant (seconds) - fast attack
    float fallTau = 0.30f;   // Fall time constant (seconds) - slow release

    // Default constructor
    AsymmetricFollower() = default;

    // Constructor with initial value and time constants
    AsymmetricFollower(float initialValue, float rise, float fall)
        : value(initialValue), riseTau(rise), fallTau(fall) {}

    /**
     * Update with asymmetric smoothing
     * @param target Target value
     * @param dt Delta time in seconds
     * @return Smoothed value
     */
    float update(float target, float dt) {
        float tau = (target > value) ? riseTau : fallTau;
        // TRUE exponential decay (not approximation!)
        float alpha = 1.0f - expf(-dt / tau);
        value += (target - value) * alpha;
        return value;
    }

    /**
     * Update with MOOD-adjusted time constants
     * mood=0 (reactive): fast rise, fast fall
     * mood=1 (smooth): slow rise, slow fall
     *
     * @param target Target value
     * @param dt Delta time in seconds
     * @param moodNorm Normalized mood (0.0-1.0)
     * @return Smoothed value
     */
    float updateWithMood(float target, float dt, float moodNorm) {
        // Mood adjusts time constants:
        // Reactive (mood=0): riseTau*1.0, fallTau*0.5
        // Smooth (mood=1):   riseTau*2.0, fallTau*1.0
        float adjRiseTau = riseTau * (1.0f + moodNorm);
        float adjFallTau = fallTau * (0.5f + 0.5f * moodNorm);

        float tau = (target > value) ? adjRiseTau : adjFallTau;
        float alpha = 1.0f - expf(-dt / tau);
        value += (target - value) * alpha;
        return value;
    }

    /**
     * Reset to specific value
     */
    void reset(float newValue = 0.0f) {
        value = newValue;
    }
};

// ============================================================================
// SubpixelRenderer Pattern
// ============================================================================
// Renders points at fractional LED positions by distributing brightness
// between adjacent LEDs. Essential for smooth motion at low speeds.
//
// Usage:
//   SubpixelRenderer::renderPoint(leds, 160, 45.7f, CRGB::Red, 255);
// ============================================================================

struct SubpixelRenderer {
    /**
     * Render a point at fractional position with anti-aliasing
     * @param buffer LED buffer
     * @param bufferSize Size of LED buffer
     * @param position Fractional position (e.g., 45.7)
     * @param color Color to render
     * @param brightness Overall brightness (0-255)
     */
    static void renderPoint(CRGB* buffer, uint16_t bufferSize,
                           float position, CRGB color, uint8_t brightness) {
        if (position < 0.0f || position >= bufferSize - 1) return;

        int idx = (int)position;
        float frac = position - idx;

        // Distribute brightness between adjacent LEDs based on fractional position
        uint8_t bright0 = scale8(brightness, (uint8_t)((1.0f - frac) * 255.0f));
        uint8_t bright1 = scale8(brightness, (uint8_t)(frac * 255.0f));

        // Add to existing LED values (don't overwrite)
        buffer[idx].r = qadd8(buffer[idx].r, scale8(color.r, bright0));
        buffer[idx].g = qadd8(buffer[idx].g, scale8(color.g, bright0));
        buffer[idx].b = qadd8(buffer[idx].b, scale8(color.b, bright0));

        if (idx + 1 < bufferSize) {
            buffer[idx + 1].r = qadd8(buffer[idx + 1].r, scale8(color.r, bright1));
            buffer[idx + 1].g = qadd8(buffer[idx + 1].g, scale8(color.g, bright1));
            buffer[idx + 1].b = qadd8(buffer[idx + 1].b, scale8(color.b, bright1));
        }
    }

    /**
     * Render a line between two fractional positions
     * @param buffer LED buffer
     * @param bufferSize Size of LED buffer
     * @param start Start position (fractional)
     * @param end End position (fractional)
     * @param color Color to render
     * @param brightness Overall brightness (0-255)
     */
    static void renderLine(CRGB* buffer, uint16_t bufferSize,
                          float start, float end, CRGB color, uint8_t brightness) {
        if (start > end) {
            float tmp = start;
            start = end;
            end = tmp;
        }

        // Clamp to buffer bounds
        if (start < 0.0f) start = 0.0f;
        if (end >= bufferSize) end = bufferSize - 0.001f;
        if (start >= end) return;

        int startIdx = (int)start;
        int endIdx = (int)end;
        float startFrac = start - startIdx;
        float endFrac = end - endIdx;

        // First partial LED
        if (startIdx < bufferSize) {
            uint8_t partialBright = scale8(brightness, (uint8_t)((1.0f - startFrac) * 255.0f));
            buffer[startIdx].r = qadd8(buffer[startIdx].r, scale8(color.r, partialBright));
            buffer[startIdx].g = qadd8(buffer[startIdx].g, scale8(color.g, partialBright));
            buffer[startIdx].b = qadd8(buffer[startIdx].b, scale8(color.b, partialBright));
        }

        // Full LEDs in between
        for (int i = startIdx + 1; i < endIdx && i < bufferSize; i++) {
            buffer[i].r = qadd8(buffer[i].r, scale8(color.r, brightness));
            buffer[i].g = qadd8(buffer[i].g, scale8(color.g, brightness));
            buffer[i].b = qadd8(buffer[i].b, scale8(color.b, brightness));
        }

        // Last partial LED (if different from first)
        if (endIdx > startIdx && endIdx < bufferSize) {
            uint8_t partialBright = scale8(brightness, (uint8_t)(endFrac * 255.0f));
            buffer[endIdx].r = qadd8(buffer[endIdx].r, scale8(color.r, partialBright));
            buffer[endIdx].g = qadd8(buffer[endIdx].g, scale8(color.g, partialBright));
            buffer[endIdx].b = qadd8(buffer[endIdx].b, scale8(color.b, partialBright));
        }
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get safe delta time (clamped to prevent physics explosion)
 * @param deltaTimeMs Delta time in milliseconds
 * @return Delta time in seconds, clamped to [0.001, 0.05]
 */
inline float getSafeDeltaSeconds(float deltaTimeMs) {
    float dt = deltaTimeMs * 0.001f;
    if (dt < 0.001f) dt = 0.001f;   // Minimum 1ms
    if (dt > 0.05f) dt = 0.05f;     // Maximum 50ms (20 FPS floor)
    return dt;
}

/**
 * Convert time constant (tau) to lambda (convergence rate)
 * @param tauSeconds Time constant in seconds
 * @return Lambda value for ExpDecay
 */
inline float tauToLambda(float tauSeconds) {
    return 1.0f / tauSeconds;
}

/**
 * Convert lambda to time constant
 * @param lambda Convergence rate
 * @return Time constant in seconds
 */
inline float lambdaToTau(float lambda) {
    return 1.0f / lambda;
}

} // namespace enhancement
} // namespace effects
} // namespace lightwaveos
