/**
 * @file ChromaUtils.h
 * @brief Shared chroma-to-hue utilities for audio-reactive effects
 *
 * Replaces the broken dominantChromaBin12() argmax pattern with a
 * circular weighted mean that varies continuously. Optionally adds
 * circular EMA for frame-to-frame temporal smoothing.
 *
 * The argmax pattern causes jarring hue discontinuities when two
 * chroma bins compete for dominance — a bin flip of 6 semitones
 * shifts the hue by ~128, making dual-strip effects appear to swap
 * colours between strips.
 */

#pragma once

#include <math.h>
#include <stdint.h>

namespace lightwaveos {
namespace effects {
namespace chroma {

// Precomputed sin/cos for 12 chroma bin positions (30-degree steps).
// cos(i * 2π/12) and sin(i * 2π/12) for i = 0..11.
static constexpr float kCos[12] = {
     1.000000f,  0.866025f,  0.500000f,  0.000000f, -0.500000f, -0.866025f,
    -1.000000f, -0.866025f, -0.500000f,  0.000000f,  0.500000f,  0.866025f
};
static constexpr float kSin[12] = {
     0.000000f,  0.500000f,  0.866025f,  1.000000f,  0.866025f,  0.500000f,
     0.000000f, -0.500000f, -0.866025f, -1.000000f, -0.866025f, -0.500000f
};

static constexpr float TWO_PI_F = 6.2831853f;
static constexpr float PI_F     = 3.1415927f;

/**
 * @brief Circular weighted mean of 12 chroma bins → hue (0-255).
 *
 * Instantaneous — no temporal smoothing. Use circularChromaHueSmoothed()
 * for frame-to-frame stability.
 *
 * @param chroma  Array of 12 chroma magnitudes
 * @return uint8_t palette hue (0-255)
 */
static inline uint8_t circularChromaHue(const float chroma[12]) {
    float c = 0.0f, s = 0.0f;
    for (uint8_t i = 0; i < 12; i++) {
        c += chroma[i] * kCos[i];
        s += chroma[i] * kSin[i];
    }
    float a = atan2f(s, c);
    if (a < 0.0f) a += TWO_PI_F;
    return static_cast<uint8_t>(a * (255.0f / TWO_PI_F));
}

/**
 * @brief Circular exponential moving average.
 *
 * Smooths an angle in [0, 2π) by always taking the shortest arc.
 *
 * @param newAngle   New angle in radians [0, 2π)
 * @param prevAngle  Previous smoothed angle (state, modified in place)
 * @param alpha      EMA alpha (0 = no change, 1 = instant snap)
 * @return Smoothed angle in [0, 2π)
 */
static inline float circularEma(float newAngle, float prevAngle, float alpha) {
    float diff = newAngle - prevAngle;
    // Shortest arc: wrap to [-π, π]
    if (diff > PI_F)  diff -= TWO_PI_F;
    if (diff < -PI_F) diff += TWO_PI_F;
    float result = prevAngle + diff * alpha;
    // Wrap to [0, 2π)
    if (result < 0.0f)       result += TWO_PI_F;
    if (result >= TWO_PI_F)  result -= TWO_PI_F;
    return result;
}

/**
 * @brief Circular weighted mean of 12 chroma bins → hue with temporal smoothing.
 *
 * Combines circular weighted mean (eliminates argmax discontinuities)
 * with circular EMA (prevents rapid hue shifts from chroma distribution changes).
 *
 * @param chroma     Array of 12 chroma magnitudes
 * @param prevAngle  Previous smoothed angle in radians — CALLER MUST PERSIST THIS.
 *                   Initialise to 0.0f. Updated in place each call.
 * @param dt         Delta time in seconds (use rawDt for frame-rate independence)
 * @param tau        Time constant in seconds. Higher = slower/smoother.
 *                   0.12f is responsive, 0.25f is smooth, 0.40f is very stable.
 * @return uint8_t palette hue (0-255)
 */
static inline uint8_t circularChromaHueSmoothed(
    const float chroma[12], float& prevAngle, float dt, float tau = 0.20f)
{
    // Compute instantaneous circular mean angle
    float c = 0.0f, s = 0.0f;
    for (uint8_t i = 0; i < 12; i++) {
        c += chroma[i] * kCos[i];
        s += chroma[i] * kSin[i];
    }
    float angle = atan2f(s, c);
    if (angle < 0.0f) angle += TWO_PI_F;

    // Circular EMA for frame-to-frame stability
    float alpha = 1.0f - expf(-dt / tau);
    prevAngle = circularEma(angle, prevAngle, alpha);

    return static_cast<uint8_t>(prevAngle * (255.0f / TWO_PI_F));
}

/**
 * @brief dt-corrected per-frame exponential decay.
 *
 * Replaces bare `value *= rate` with frame-rate-independent version.
 * The rate parameter is the per-frame multiplier at 60fps.
 *
 * @param value      Current value
 * @param rate60fps  Decay rate per frame at 60fps (e.g. 0.88f)
 * @param dt         Delta time in seconds
 * @return Decayed value
 */
static inline float dtDecay(float value, float rate60fps, float dt) {
    return value * powf(rate60fps, dt * 60.0f);
}

} // namespace chroma
} // namespace effects
} // namespace lightwaveos
