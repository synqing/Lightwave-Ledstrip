/**
 * @file EffectContext.h
 * @brief Dependency injection container for effect rendering
 *
 * EffectContext replaces the 15+ global variables from v1 with a single
 * structured container. Effects receive this context in render() and
 * should use ONLY this for accessing LEDs, palettes, and parameters.
 *
 * Key differences from v1:
 * - No global leds[] - use ctx.leds
 * - No global currentPalette - use ctx.palette
 * - No global gHue - use ctx.gHue
 * - No hardcoded 320 - use ctx.ledCount
 * - No hardcoded 80 - use ctx.centerPoint
 *
 * CENTER ORIGIN: Use getDistanceFromCenter(i) for position-based effects.
 * This returns 0.0 at center (LED 79/80) and 1.0 at edges (LED 0/159).
 */

#pragma once

#include <cstdint>
#include <cmath>

// Forward declare FastLED types for native builds
#ifdef NATIVE_BUILD
#include "../../../test/unit/mocks/fastled_mock.h"
#else
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace plugins {

/**
 * @brief Palette wrapper for portable color lookups
 */
class PaletteRef {
public:
    PaletteRef() : m_palette(nullptr) {}

#ifndef NATIVE_BUILD
    explicit PaletteRef(const CRGBPalette16* palette) : m_palette(palette) {}

    /**
     * @brief Get color from palette
     * @param index Position in palette (0-255)
     * @param brightness Optional brightness scaling (0-255)
     * @return Color from palette
     */
    CRGB getColor(uint8_t index, uint8_t brightness = 255) const {
        if (!m_palette) return CRGB::Black;
        return ColorFromPalette(*m_palette, index, brightness, LINEARBLEND);
    }
#else
    explicit PaletteRef(const void* palette) : m_palette(palette) {}

    CRGB getColor(uint8_t index, uint8_t brightness = 255) const {
        (void)brightness;
        // Mock implementation for testing
        return CRGB(index, index, index);
    }
#endif

    bool isValid() const { return m_palette != nullptr; }

private:
#ifndef NATIVE_BUILD
    const CRGBPalette16* m_palette;
#else
    const void* m_palette;
#endif
};

/**
 * @brief Effect rendering context with all dependencies
 *
 * This is the single source of truth for effect rendering. All effect
 * implementations receive this context and should NOT access any other
 * global state.
 */
struct EffectContext {
    //--------------------------------------------------------------------------
    // LED Buffer (WRITE TARGET)
    //--------------------------------------------------------------------------

    CRGB* leds;                 ///< LED buffer to write to
    uint16_t ledCount;          ///< Total LED count (320 for standard config)
    uint16_t centerPoint;       ///< CENTER ORIGIN point (80 for standard config)

    //--------------------------------------------------------------------------
    // Palette
    //--------------------------------------------------------------------------

    PaletteRef palette;         ///< Current palette for color lookups

    //--------------------------------------------------------------------------
    // Global Animation Parameters
    //--------------------------------------------------------------------------

    uint8_t brightness;         ///< Master brightness (0-255)
    uint8_t speed;              ///< Animation speed (1-50)
    uint8_t gHue;               ///< Auto-incrementing hue (0-255)

    //--------------------------------------------------------------------------
    // Visual Enhancement Parameters (from v1 ColorEngine)
    //--------------------------------------------------------------------------

    uint8_t intensity;          ///< Effect intensity (0-255)
    uint8_t saturation;         ///< Color saturation (0-255)
    uint8_t complexity;         ///< Pattern complexity (0-255)
    uint8_t variation;          ///< Random variation (0-255)

    //--------------------------------------------------------------------------
    // Timing
    //--------------------------------------------------------------------------

    uint32_t deltaTimeMs;       ///< Time since last frame (ms)
    uint32_t frameNumber;       ///< Frame counter (wraps at 2^32)
    uint32_t totalTimeMs;       ///< Total effect runtime (ms)

    //--------------------------------------------------------------------------
    // Zone Information (when rendering a zone)
    //--------------------------------------------------------------------------

    uint8_t zoneId;             ///< Current zone ID (0-3, or 0xFF if global)
    uint16_t zoneStart;         ///< Zone start index in global buffer
    uint16_t zoneLength;        ///< Zone length

    //--------------------------------------------------------------------------
    // Helper Methods
    //--------------------------------------------------------------------------

    /**
     * @brief Calculate normalized distance from center (CENTER ORIGIN pattern)
     * @param index LED index (0 to ledCount-1)
     * @return Distance from center: 0.0 at center, 1.0 at edges
     *
     * This is the core method for CENTER ORIGIN compliance. Effects should
     * use this instead of raw index for position-based calculations.
     *
     * Example:
     * @code
     * for (uint16_t i = 0; i < ctx.ledCount; i++) {
     *     float dist = ctx.getDistanceFromCenter(i);
     *     uint8_t heat = 255 * (1.0f - dist);  // Hotter at center
     *     ctx.leds[i] = ctx.palette.getColor(heat);
     * }
     * @endcode
     */
    float getDistanceFromCenter(uint16_t index) const {
        if (ledCount == 0 || centerPoint == 0) return 0.0f;

        int16_t distanceFromCenter = abs((int16_t)index - (int16_t)centerPoint);
        float maxDistance = (float)centerPoint;  // Distance to edge

        return (float)distanceFromCenter / maxDistance;
    }

    /**
     * @brief Get signed position from center (-1.0 to +1.0)
     * @param index LED index
     * @return -1.0 at start, 0.0 at center, +1.0 at end
     *
     * Useful for effects that need to know which "side" of center an LED is on.
     */
    float getSignedPosition(uint16_t index) const {
        if (ledCount == 0 || centerPoint == 0) return 0.0f;

        int16_t offset = (int16_t)index - (int16_t)centerPoint;
        float maxOffset = (float)centerPoint;

        return (float)offset / maxOffset;
    }

    /**
     * @brief Map strip index to mirror position (for symmetric effects)
     * @param index LED index on one side
     * @return Corresponding index on the other side
     *
     * For a 320-LED strip with center at 80:
     * - mirrorIndex(0) returns 159
     * - mirrorIndex(79) returns 80
     * - mirrorIndex(80) returns 79
     */
    uint16_t mirrorIndex(uint16_t index) const {
        if (index >= ledCount) return 0;

        if (index < centerPoint) {
            // Left side -> right side
            return centerPoint + (centerPoint - 1 - index);
        } else {
            // Right side -> left side
            return centerPoint - 1 - (index - centerPoint);
        }
    }

    /**
     * @brief Get time-based phase for smooth animations
     * @param frequencyHz Oscillation frequency
     * @return Phase value (0.0 to 1.0)
     */
    float getPhase(float frequencyHz) const {
        float period = 1000.0f / frequencyHz;
        return fmodf((float)totalTimeMs, period) / period;
    }

    /**
     * @brief Get sine wave value based on time
     * @param frequencyHz Oscillation frequency
     * @return Sine value (-1.0 to +1.0)
     */
    float getSineWave(float frequencyHz) const {
        float phase = getPhase(frequencyHz);
        return sinf(phase * 2.0f * 3.14159265f);
    }

    /**
     * @brief Check if this is a zone render (not full strip)
     * @return true if rendering to a zone
     */
    bool isZoneRender() const {
        return zoneId != 0xFF;
    }

    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------

    EffectContext()
        : leds(nullptr)
        , ledCount(0)
        , centerPoint(0)
        , palette()
        , brightness(255)
        , speed(15)
        , gHue(0)
        , intensity(128)
        , saturation(255)
        , complexity(128)
        , variation(64)
        , deltaTimeMs(8)
        , frameNumber(0)
        , totalTimeMs(0)
        , zoneId(0xFF)
        , zoneStart(0)
        , zoneLength(0)
    {}
};

} // namespace plugins
} // namespace lightwaveos
