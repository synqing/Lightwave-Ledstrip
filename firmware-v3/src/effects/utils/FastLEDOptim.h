// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FastLEDOptim.h
 * @brief FastLED optimization utility functions for effect development
 * 
 * Centralized wrapper functions for FastLED's optimized math operations.
 * Reduces code duplication and provides consistent optimization patterns
 * across all effects.
 * 
 * Based on recommendations from:
 * - b4. LGP_PATTERN_DEVELOPMENT_PLAYBOOK.md (lines 420-450)
 * - c3. IMPLEMENTATION_PLAYBOOK_LIGHT_PATTERNS.md
 * 
 * Usage Examples:
 * 
 * // sin16 wrapper - normalized float result
 * float wave = fastled_sin16_normalized(angle * 256);
 * 
 * // beatsin16 wrapper - oscillating distance from center
 * int dist = fastled_beatsin16(13, 0, HALF_LENGTH);
 * 
 * // scale8 wrapper - brightness scaling
 * uint8_t scaled = fastled_scale8(color.r, brightness);
 */

#ifndef FASTLED_OPTIM_H
#define FASTLED_OPTIM_H

#include <FastLED.h>
#include <math.h>
#include <stdint.h>

namespace lightwaveos {
namespace effects {
namespace utils {

// ============================================================================
// sin16/cos16 Wrappers (Normalized Float Results)
// ============================================================================

/**
 * @brief FastLED sin16 wrapper returning normalized float (-1.0 to 1.0)
 * 
 * FastLED's sin16() returns int16_t (-32768 to 32767).
 * This wrapper normalizes to float for easier math operations.
 * 
 * @param angle Angle in FastLED units (0-65535 maps to 0-2π)
 * @return Normalized sine value (-1.0 to 1.0)
 * 
 * Example:
 *   float v = fastled_sin16_normalized(angle * 256 + time);
 */
inline float fastled_sin16_normalized(uint16_t angle) {
    return sin16(angle) / 32768.0f;
}

/**
 * @brief FastLED cos16 wrapper returning normalized float (-1.0 to 1.0)
 * 
 * @param angle Angle in FastLED units (0-65535 maps to 0-2π)
 * @return Normalized cosine value (-1.0 to 1.0)
 * 
 * Example:
 *   float v = fastled_cos16_normalized(angle * 256);
 */
inline float fastled_cos16_normalized(uint16_t angle) {
    return cos16(angle) / 32768.0f;
}

// ============================================================================
// scale8/qadd8/qsub8 Helper Functions
// ============================================================================

/**
 * @brief FastLED scale8 wrapper for brightness scaling
 * 
 * Scales a color component by brightness factor (0-255).
 * More efficient than float multiplication.
 * 
 * @param value Color component (0-255)
 * @param scale Brightness scale factor (0-255)
 * @return Scaled value (0-255)
 * 
 * Example:
 *   uint8_t r = fastled_scale8(color.r, brightness);
 */
inline uint8_t fastled_scale8(uint8_t value, uint8_t scale) {
    return scale8(value, scale);
}

/**
 * @brief FastLED qadd8 wrapper for saturating addition
 * 
 * Adds two values with saturation (max 255).
 * Faster than min(value1 + value2, 255).
 * 
 * @param value1 First value (0-255)
 * @param value2 Second value (0-255)
 * @return Sum clamped to 255
 * 
 * Example:
 *   uint8_t sum = fastled_qadd8(color1.r, color2.r);
 */
inline uint8_t fastled_qadd8(uint8_t value1, uint8_t value2) {
    return qadd8(value1, value2);
}

/**
 * @brief FastLED qsub8 wrapper for saturating subtraction
 * 
 * Subtracts two values with saturation (min 0).
 * Faster than max(value1 - value2, 0).
 * 
 * @param value1 First value (0-255)
 * @param value2 Second value (0-255)
 * @return Difference clamped to 0
 * 
 * Example:
 *   fireHeat[i] = fastled_qsub8(fireHeat[i], cooling);
 */
inline uint8_t fastled_qsub8(uint8_t value1, uint8_t value2) {
    return qsub8(value1, value2);
}

// ============================================================================
// beatsin8/beatsin16 Timing Utilities
// ============================================================================

/**
 * @brief FastLED beatsin16 wrapper for oscillating values
 * 
 * Returns a value that oscillates between min and max using a sine wave.
 * Useful for pulsing effects, distance oscillations, etc.
 * 
 * @param beatsPerMinute Oscillation rate (BPM)
 * @param min Minimum value
 * @param max Maximum value
 * @return Oscillating value between min and max
 * 
 * Example:
 *   int distFromCenter = fastled_beatsin16(13, 0, HALF_LENGTH);
 */
inline int fastled_beatsin16(uint8_t beatsPerMinute, int min, int max) {
    return beatsin16(beatsPerMinute, min, max);
}

/**
 * @brief FastLED beatsin8 wrapper for oscillating byte values
 * 
 * Returns a byte value (0-255) that oscillates between min and max.
 * 
 * @param beatsPerMinute Oscillation rate (BPM)
 * @param min Minimum value (0-255)
 * @param max Maximum value (0-255)
 * @return Oscillating value between min and max
 * 
 * Example:
 *   uint8_t brightness = fastled_beatsin8(60, 128, 255);
 */
inline uint8_t fastled_beatsin8(uint8_t beatsPerMinute, uint8_t min, uint8_t max) {
    return beatsin8(beatsPerMinute, min, max);
}

// ============================================================================
// Hue Wrapping Utilities (No-Rainbows Rule Compliance)
// ============================================================================

/**
 * @brief Wrap hue value to prevent rainbow cycling (no-rainbows rule)
 * 
 * Prevents hue from cycling through the full spectrum by wrapping within
 * a maximum range from the base hue. Ensures compliance with no-rainbows
 * rule (< 60° hue range).
 * 
 * @param hue Base hue (0-255)
 * @param offset Hue offset to add
 * @param maxRange Maximum hue range from base (default 60 for no-rainbows rule)
 * @return Wrapped hue value within acceptable range
 * 
 * Example:
 *   uint8_t safeHue = fastled_wrap_hue_safe(ctx.hue, offset, 60);
 */
inline uint8_t fastled_wrap_hue_safe(uint8_t hue, int16_t offset, uint8_t maxRange = 60) {
    int16_t result = (int16_t)hue + offset;
    // Wrap to keep within 0-255 range
    while (result < 0) result += 256;
    while (result >= 256) result -= 256;
    // Clamp to maxRange if needed (no-rainbows rule)
    int16_t diff = result - hue;
    if (diff > maxRange && diff < 256 - maxRange) {
        result = hue + maxRange;
    } else if (diff < -maxRange && diff > -(256 - maxRange)) {
        result = hue - maxRange;
    }
    return (uint8_t)result;
}

// ============================================================================
// Combined Helper Functions
// ============================================================================

/**
 * @brief Calculate normalized distance from center with sin16
 * 
 * Common pattern: calculate distance from center, apply sin16 wave.
 * This combines the calculation for convenience.
 * 
 * @param position LED position (0 to STRIP_LENGTH-1)
 * @param center Center position (typically CENTER_LEFT or CENTER_RIGHT)
 * @param halfLength Half the strip length for normalization
 * @param frequency Wave frequency multiplier
 * @param phase Phase offset
 * @return Normalized sine wave value (-1.0 to 1.0)
 * 
 * Example:
 *   float wave = fastled_center_sin16(i, CENTER_LEFT, HALF_LENGTH, 8.0f, plasmaTime);
 */
inline float fastled_center_sin16(int position, int center, float halfLength, 
                                   float frequency, uint16_t phase) {
    float distA = fabsf((float)position - (float)center);
    float distB = fabsf((float)position - (float)(center + 1));
    float distFromCenter = fminf(distA, distB);
    float normalizedDist = distFromCenter / halfLength;
    uint16_t angle = (uint16_t)(normalizedDist * frequency * 256.0f + phase);
    return fastled_sin16_normalized(angle);
}

} // namespace utils
} // namespace effects
} // namespace lightwaveos

#endif // FASTLED_OPTIM_H
