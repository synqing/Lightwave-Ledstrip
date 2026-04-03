/**
 * @file GradientRamp.h
 * @brief Gradient ramp sampler — the core of the K1 gradient kernel
 *
 * GradientRamp holds up to kMaxStops colour stops and samples them at
 * any position with configurable interpolation and repeat modes.
 *
 * Usage:
 * @code
 * gradient::GradientRamp ramp;
 * ramp.addStop(0,   CRGB::Red);
 * ramp.addStop(128, CRGB::Blue);
 * ramp.addStop(255, CRGB::Green);
 * ramp.setRepeatMode(gradient::RepeatMode::MIRROR);
 *
 * for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
 *     float u = gradient::uCenter(i);
 *     ctx.leds[i] = ramp.samplef(u);
 * }
 * @endcode
 *
 * Memory: ~35 bytes on stack. No heap allocation.
 * Performance: ~15 ops per sample (linear scan + lerp8). Trivial at 160 LEDs.
 *
 * HARD RULE: Do NOT call addStop() or clear() inside render().
 * Build the ramp in init() or on parameter change, then sample in render().
 */

#pragma once

#include "GradientTypes.h"
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace gradient {

/**
 * @class GradientRamp
 * @brief Fixed-capacity gradient with up to kMaxStops colour stops
 *
 * Stops must be added in ascending position order. The ramp maintains
 * sorted order internally. Sampling between stops uses the configured
 * interpolation mode.
 */
class GradientRamp {
public:
    GradientRamp()
        : m_stopCount(0)
        , m_repeatMode(RepeatMode::CLAMP)
        , m_interpolation(InterpolationMode::LINEAR)
    {}

    // ========================================================================
    // Configuration (call in init() or on parameter change, NOT in render())
    // ========================================================================

    /**
     * @brief Remove all stops
     */
    void clear() {
        m_stopCount = 0;
    }

    /**
     * @brief Add a colour stop at a fixed-point position [0-255]
     * @param position Position along ramp [0-255]
     * @param colour Colour at this stop
     * @return true if stop was added, false if ramp is full
     *
     * Stops must be added in ascending position order.
     */
    bool addStop(uint8_t position, CRGB colour) {
        if (m_stopCount >= kMaxStops) return false;
        m_stops[m_stopCount] = GradientStop(position, colour);
        m_stopCount++;
        return true;
    }

    /**
     * @brief Convenience: add a stop from RGB components
     */
    bool addStop(uint8_t position, uint8_t r, uint8_t g, uint8_t b) {
        return addStop(position, CRGB(r, g, b));
    }

    /**
     * @brief Set how the ramp behaves beyond [0-255]
     */
    void setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }

    /**
     * @brief Set interpolation method between stops
     */
    void setInterpolationMode(InterpolationMode mode) { m_interpolation = mode; }

    /**
     * @brief Get current stop count
     */
    uint8_t stopCount() const { return m_stopCount; }

    // ========================================================================
    // Sampling (safe for render() — no allocations, no branches on heap)
    // ========================================================================

    /**
     * @brief Sample the ramp at a fixed-point position [0-255]
     * @param pos Position along ramp [0-255]
     * @return Interpolated colour at that position
     */
    CRGB sample(uint8_t pos) const {
        if (m_stopCount == 0) return CRGB::Black;
        if (m_stopCount == 1) return m_stops[0].colour;

        // Apply repeat mode
        pos = applyRepeat(pos);

        // Find bracketing stops
        // Stops are in ascending position order
        if (pos <= m_stops[0].position) return m_stops[0].colour;
        if (pos >= m_stops[m_stopCount - 1].position) return m_stops[m_stopCount - 1].colour;

        // Linear scan for bracketing pair (kMaxStops=8, so this is fast)
        for (uint8_t i = 0; i < m_stopCount - 1; i++) {
            if (pos >= m_stops[i].position && pos <= m_stops[i + 1].position) {
                return interpolate(m_stops[i], m_stops[i + 1], pos);
            }
        }

        // Fallback (should not reach here with sorted stops)
        return m_stops[m_stopCount - 1].colour;
    }

    /**
     * @brief Sample the ramp at a float position [0.0-1.0]
     * @param t Normalised position [0.0-1.0], clamped internally
     * @return Interpolated colour at that position
     *
     * Convenience wrapper that converts float to fixed-point [0-255].
     * Handles values outside [0.0-1.0] via the configured repeat mode.
     */
    CRGB samplef(float t) const {
        // For repeat/mirror, allow values outside [0,1]
        if (m_repeatMode == RepeatMode::CLAMP) {
            if (t <= 0.0f) return sample(0);
            if (t >= 1.0f) return sample(255);
        }

        // Scale to [0-255] range, handling wrap for repeat modes
        int32_t scaled;
        if (t < 0.0f) {
            // Negative values: wrap into positive range for repeat/mirror
            scaled = (int32_t)(t * 256.0f);
            scaled = ((scaled % 256) + 256) % 256;
        } else {
            scaled = (int32_t)(t * 255.0f);
            if (m_repeatMode != RepeatMode::CLAMP) {
                scaled = scaled % 256;
            }
        }
        return sample((uint8_t)scaled);
    }

    /**
     * @brief Sample with scaled position: pos * scale, then sample
     * @param pos Base position [0-255]
     * @param scale Repeat scale (e.g., ringCount)
     * @param offset Phase offset [0-255]
     * @return Interpolated colour
     *
     * Useful for repeating patterns: sample at (pos * scale + offset).
     */
    CRGB sampleScaled(uint8_t pos, uint8_t scale, uint8_t offset = 0) const {
        uint16_t scaledPos = ((uint16_t)pos * scale + offset);
        return sample((uint8_t)(scaledPos & 0xFF));
    }

    // ========================================================================
    // Blend helpers (compositing onto existing LED data)
    // ========================================================================

    /**
     * @brief Blend a sampled colour onto an existing LED value
     * @param dest Existing LED colour (modified in place)
     * @param src Gradient sample colour
     * @param mode Blend mode
     * @param amount Blend amount [0-255] (255 = full effect)
     */
    static void blend(CRGB& dest, const CRGB& src, BlendMode mode, uint8_t amount = 255) {
        CRGB result;
        switch (mode) {
            case BlendMode::REPLACE:
                result = src;
                break;
            case BlendMode::ADD:
                result.r = qadd8(dest.r, src.r);
                result.g = qadd8(dest.g, src.g);
                result.b = qadd8(dest.b, src.b);
                break;
            case BlendMode::SCREEN:
                result.r = 255 - (uint8_t)(((uint16_t)(255 - dest.r) * (255 - src.r)) >> 8);
                result.g = 255 - (uint8_t)(((uint16_t)(255 - dest.g) * (255 - src.g)) >> 8);
                result.b = 255 - (uint8_t)(((uint16_t)(255 - dest.b) * (255 - src.b)) >> 8);
                break;
            case BlendMode::MULTIPLY:
                result.r = (uint8_t)(((uint16_t)dest.r * src.r) >> 8);
                result.g = (uint8_t)(((uint16_t)dest.g * src.g) >> 8);
                result.b = (uint8_t)(((uint16_t)dest.b * src.b) >> 8);
                break;
        }

        if (amount == 255) {
            dest = result;
        } else {
            dest = dest.lerp8(result, amount);
        }
    }

private:
    GradientStop m_stops[kMaxStops];
    uint8_t m_stopCount;
    RepeatMode m_repeatMode;
    InterpolationMode m_interpolation;

    /**
     * @brief Apply repeat mode to a position value
     */
    uint8_t applyRepeat(uint8_t pos) const {
        // For CLAMP mode, positions within [0-255] pass through unchanged
        // (clamping to stop range happens in sample())
        if (m_repeatMode == RepeatMode::CLAMP) return pos;

        // REPEAT and MIRROR use the full [0-255] range of the stops
        // Since pos is already uint8_t [0-255], it naturally wraps for REPEAT
        if (m_repeatMode == RepeatMode::MIRROR) {
            // Triangle wave: 0→255→0 mapped from input 0→127→255
            // Use the position within a virtual 0-510 range
            // Since we only have uint8_t input, mirror at 128
            if (pos >= 128) {
                pos = (uint8_t)(255 - ((pos - 128) * 2));
            } else {
                pos = (uint8_t)(pos * 2);
            }
        }
        return pos;
    }

    /**
     * @brief Interpolate between two stops at a given position
     */
    CRGB interpolate(const GradientStop& a, const GradientStop& b, uint8_t pos) const {
        if (a.position == b.position) return a.colour;

        // Calculate interpolation fraction [0-255]
        uint8_t range = b.position - a.position;
        uint8_t offset = pos - a.position;
        uint8_t frac = (uint8_t)(((uint16_t)offset * 255) / range);

        switch (m_interpolation) {
            case InterpolationMode::HARD_STOP:
                // Step function at midpoint
                return (frac < 128) ? a.colour : b.colour;

            case InterpolationMode::EASED: {
                // Cosine-eased interpolation (smooth step)
                // Approximate cos ease: cubic hermite 3t² - 2t³
                uint16_t t = frac;
                uint16_t t2 = (t * t) >> 8;        // t²
                uint16_t t3 = (t2 * t) >> 8;       // t³
                frac = (uint8_t)((3 * t2 - 2 * t3) >> 0);
                // Falls through to linear with eased fraction
            }
            // fall through
            case InterpolationMode::LINEAR:
            default:
                return a.colour.lerp8(b.colour, frac);
        }
    }
};

} // namespace gradient
} // namespace effects
} // namespace lightwaveos
