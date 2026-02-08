/**
 * @file BeatPulseRenderUtils.h
 * @brief Shared utilities for Beat Pulse effect family
 *
 * Provides:
 * - RingProfile: Gaussian, tent, and other ring shape functions
 * - ColourUtil: Saturating colour operations
 * - Conversion helpers: floatToByte, scaleBrightness, clamp01
 *
 * All functions are inline/constexpr for zero overhead.
 *
 * IMPORTANT: Include CoreEffects.h (which includes FastLED) BEFORE this header.
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace lightwaveos::effects::ieffect {

// ============================================================================
// Basic Utilities
// ============================================================================

/**
 * @brief Clamp float to [0, 1] range
 */
static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/**
 * @brief Linear interpolation between two values
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor [0, 1]
 * @return Interpolated value
 */
static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * @brief Convert float [0, 1] to uint8_t [0, 255]
 */
static inline uint8_t floatToByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

/**
 * @brief Scale brightness by a factor
 * @param baseBrightness Original brightness [0, 255]
 * @param factor Scale factor [0, 1]
 * @return Scaled brightness [0, 255]
 */
static inline uint8_t scaleBrightness(uint8_t baseBrightness, float factor) {
    const float scaled = static_cast<float>(baseBrightness) * clamp01(factor);
    if (scaled <= 0.0f) return 0;
    if (scaled >= 255.0f) return 255;
    return static_cast<uint8_t>(scaled + 0.5f);
}

// ============================================================================
// Ring Profile Functions
// ============================================================================

namespace RingProfile {

/**
 * @brief Gaussian ring profile (soft, natural falloff)
 * @param distance Distance from ring centre
 * @param sigma Gaussian sigma (controls ring spread)
 * @return Intensity [0, 1]
 *
 * Formula: exp(-0.5 * (distance / sigma)^2)
 * At distance = sigma, intensity is ~0.6
 * At distance = 2*sigma, intensity is ~0.13
 * At distance = 3*sigma, intensity is ~0.01
 */
static inline float gaussian(float distance, float sigma) {
    const float ratio = distance / sigma;
    return expf(-0.5f * ratio * ratio);
}

/**
 * @brief Tent (linear) ring profile
 * @param distance Distance from ring centre
 * @param width Half-width of the tent
 * @return Intensity [0, 1]
 */
static inline float tent(float distance, float width) {
    if (distance >= width) return 0.0f;
    return 1.0f - (distance / width);
}

/**
 * @brief Cosine ring profile (smooth falloff, zero derivative at edges)
 * @param distance Distance from ring centre
 * @param width Half-width of the ring
 * @return Intensity [0, 1]
 */
static inline float cosine(float distance, float width) {
    if (distance >= width) return 0.0f;
    return 0.5f * (1.0f + cosf(3.14159265f * distance / width));
}

/**
 * @brief Glow ring profile (bright core + soft halo) for water-like spread
 * @param distance Distance from ring centre
 * @param coreWidth Width of bright core
 * @param haloWidth Width of soft halo beyond core
 * @return Intensity [0, 1]
 *
 * Creates a brighter core with smooth quadratic falloff in the halo.
 * Ideal for water ripple or light diffusion effects.
 */
static inline float glow(float distance, float coreWidth, float haloWidth) {
    // Core: high intensity with slight rolloff at edge
    if (distance <= coreWidth) {
        const float t = distance / coreWidth;
        return 1.0f - (t * t * 0.2f);  // 1.0 at centre, 0.8 at core edge
    }

    // Halo: smooth quadratic decay beyond core
    const float haloPos = distance - coreWidth;
    if (haloPos >= haloWidth) return 0.0f;

    const float t = haloPos / haloWidth;
    return 0.8f * (1.0f - t) * (1.0f - t);  // Quadratic falloff from 0.8 to 0
}

/**
 * @brief Hard-edged ring with minimal soft boundary for pressure wave fronts
 * @param diff Absolute distance from ring centre
 * @param width Ring half-width
 * @param softness Anti-aliasing transition width
 * @return Intensity [0, 1]
 *
 * Creates a sharp pressure wave front with subtle anti-aliasing at edges.
 * Ideal for shockwave and detonation effects.
 */
static inline float hardEdge(float diff, float width, float softness) {
    if (diff >= width + softness) return 0.0f;
    if (diff <= width - softness) return 1.0f;
    // Smooth transition at edge
    return 1.0f - (diff - (width - softness)) / (2.0f * softness);
}

} // namespace RingProfile

// ============================================================================
// Colour Utilities (require FastLED CRGB type)
// ============================================================================

#ifndef NATIVE_BUILD

namespace ColourUtil {

/**
 * @brief Add white to colour with saturation (no overflow)
 * @param c Colour to modify (in-place)
 * @param w White amount [0, 255]
 */
static inline void addWhiteSaturating(CRGB& c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r) + w;
    uint16_t g = static_cast<uint16_t>(c.g) + w;
    uint16_t b = static_cast<uint16_t>(c.b) + w;
    c.r = (r > 255) ? 255 : static_cast<uint8_t>(r);
    c.g = (g > 255) ? 255 : static_cast<uint8_t>(g);
    c.b = (b > 255) ? 255 : static_cast<uint8_t>(b);
}

/**
 * @brief Additive blend (saturating) - both colours visible simultaneously
 * @param base Base colour
 * @param overlay Overlay colour to add
 * @return Blended colour (saturating addition)
 *
 * Creates a screen-like blend where both layers are visible.
 * Ideal for layering attack and body rings.
 */
static inline CRGB additive(const CRGB& base, const CRGB& overlay) {
    uint16_t r = static_cast<uint16_t>(base.r) + static_cast<uint16_t>(overlay.r);
    uint16_t g = static_cast<uint16_t>(base.g) + static_cast<uint16_t>(overlay.g);
    uint16_t b = static_cast<uint16_t>(base.b) + static_cast<uint16_t>(overlay.b);
    return CRGB(
        (r > 255) ? 255 : static_cast<uint8_t>(r),
        (g > 255) ? 255 : static_cast<uint8_t>(g),
        (b > 255) ? 255 : static_cast<uint8_t>(b)
    );
}

} // namespace ColourUtil

#endif // NATIVE_BUILD

// ============================================================================
// Blend Mode Functions
// ============================================================================

namespace BlendMode {

/**
 * @brief Soft accumulation for graceful multi-layer handling
 * @param accumulated Sum of all layer contributions
 * @param knee Softness of curve (higher = softer saturation)
 * @return Blended intensity [0, 1)
 *
 * Maps [0, inf) to [0, 1) with configurable knee. Multiple overlapping
 * rings accumulate without harsh clipping. Formula: x / (x + knee).
 */
static inline float softAccumulate(float accumulated, float knee = 1.5f) {
    if (accumulated <= 0.0f) return 0.0f;
    return accumulated / (accumulated + knee);
}

/**
 * @brief Screen blend: graceful additive that avoids clipping
 * @param a First value [0, 1]
 * @param b Second value [0, 1]
 * @return Blended intensity [0, 1]
 *
 * Formula: 1 - (1 - a) * (1 - b)
 * Multiple overlapping layers blend gracefully without harsh saturation.
 */
static inline float screen(float a, float b) {
    const float ca = clamp01(a);
    const float cb = clamp01(b);
    return 1.0f - (1.0f - ca) * (1.0f - cb);
}

} // namespace BlendMode

// ============================================================================
// HTML Beat Pulse Parity Core (from led-preview-stack.html)
// ============================================================================
//
// This namespace locks the exact maths used by the HTML Beat Pulse demo.
//
// Reference (HTML):
//   - On beat: beatIntensity = 1.0
//   - Per frame: beatIntensity *= 0.94  (at ~60 FPS)
//   - wavePos   = beatIntensity * 1.2
//   - waveHit   = 1 - min(1, abs(dist01 - wavePos*0.5) * 3)
//   - intensity = max(0, waveHit) * beatIntensity
//   - brightness = 0.5 + intensity * 0.5
//   - whiteMix   = intensity * 0.3
//
// Notes:
// - dist01 is centre-origin distance in [0..1], where 0 is the centre and 1 is the edges.
// - All functions here are dt-correct so visual timing is stable at 60/120/etc FPS.
//
namespace BeatPulseHTML {

/**
 * @brief dt-correct multiplier to match beatIntensity *= 0.94 at ~60 FPS.
 */
static inline float decayMul(float dtSeconds) {
    // Equivalent to repeatedly multiplying by 0.94 once per 1/60s frame.
    return powf(0.94f, dtSeconds * 60.0f);
}

/**
 * @brief Update beatIntensity in-place using the exact HTML behaviour.
 * @param beatIntensity (in/out) Current intensity state [0..1]
 * @param beatTick True on beat (slam to 1.0)
 * @param dtSeconds Delta time in seconds
 */
static inline void updateBeatIntensity(float& beatIntensity, bool beatTick, float dtSeconds) {
    if (beatTick) {
        beatIntensity = 1.0f;
    }
    beatIntensity *= decayMul(dtSeconds);
    if (beatIntensity < 0.0005f) {
        beatIntensity = 0.0f;
    }
}

/**
 * @brief Ring centre position in dist01 units (0..1) per HTML maths.
 * wavePos = beatIntensity * 1.2, centre = wavePos * 0.5 = beatIntensity * 0.6
 */
static inline float ringCentre01(float beatIntensity) {
    return beatIntensity * 0.6f;
}

/**
 * @brief Compute per-LED pulse intensity at a given centre-distance.
 * @param dist01 Centre-origin distance [0..1]
 * @param beatIntensity Global beat intensity state [0..1]
 * @return intensity [0..1]
 *
 * HTML formula: waveHit = 1 - min(1, abs(dist - wavePos*0.5) * 3)
 *               intensity = max(0, waveHit) * beatIntensity
 */
static inline float intensityAtDist(float dist01, float beatIntensity) {
    const float centre = ringCentre01(beatIntensity);
    const float waveHit = 1.0f - fminf(1.0f, fabsf(dist01 - centre) * 3.0f);
    const float hit = fmaxf(0.0f, waveHit);
    return hit * beatIntensity;
}

/**
 * @brief Brightness factor per HTML maths.
 * @param intensity [0..1]
 * @return brightnessFactor [0.5..1.0]
 *
 * HTML formula: brightness = 0.5 + intensity * 0.5
 */
static inline float brightnessFactor(float intensity) {
    return 0.5f + clamp01(intensity) * 0.5f;
}

/**
 * @brief White mix per HTML maths.
 * @param intensity [0..1]
 * @return whiteMix [0..0.3]
 *
 * HTML formula: whiteMix = intensity * 0.3
 */
static inline float whiteMix(float intensity) {
    return clamp01(intensity) * 0.3f;
}

} // namespace BeatPulseHTML

} // namespace lightwaveos::effects::ieffect
