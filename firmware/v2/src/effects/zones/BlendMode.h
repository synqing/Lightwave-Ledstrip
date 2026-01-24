/**
 * @file BlendMode.h
 * @brief Pixel blending modes for zone compositing
 *
 * LightwaveOS v2 - Zone System
 *
 * Provides 8 blend modes for compositing multiple zones.
 */

#pragma once

#include <FastLED.h>

namespace lightwaveos {
namespace zones {

// ==================== Blend Mode Enum ====================

enum class BlendMode : uint8_t {
    OVERWRITE = 0,      // Replace: pixel = new
    ADDITIVE = 1,       // Add: pixel += new (light accumulation)
    MULTIPLY = 2,       // Multiply: pixel = (pixel * new) / 255
    SCREEN = 3,         // Screen: inverse multiply (lighten)
    OVERLAY = 4,        // Overlay: multiply if dark, screen if light
    ALPHA = 5,          // Alpha blend: 50/50 mix
    LIGHTEN = 6,        // Lighten: take brighter pixel
    DARKEN = 7,         // Darken: take darker pixel

    MODE_COUNT = 8
};

// ==================== Blend Names ====================

inline const char* getBlendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::OVERWRITE: return "Overwrite";
        case BlendMode::ADDITIVE:  return "Additive";
        case BlendMode::MULTIPLY:  return "Multiply";
        case BlendMode::SCREEN:    return "Screen";
        case BlendMode::OVERLAY:   return "Overlay";
        case BlendMode::ALPHA:     return "Alpha";
        case BlendMode::LIGHTEN:   return "Lighten";
        case BlendMode::DARKEN:    return "Darken";
        default:                   return "Unknown";
    }
}

// Overload for uint8_t (convenience for codec usage)
inline const char* getBlendModeName(uint8_t mode) {
    return getBlendModeName(static_cast<BlendMode>(mode));
}

// ==================== Blend Functions ====================

/**
 * @brief Blend two pixels using the specified mode
 * @param base The existing pixel (destination)
 * @param blend The new pixel (source)
 * @param mode The blend mode to use
 * @return The blended result
 */
inline CRGB blendPixels(const CRGB& base, const CRGB& blend, BlendMode mode) {
    switch (mode) {
        case BlendMode::OVERWRITE:
            return blend;

        case BlendMode::ADDITIVE: {
            // PRE-SCALE both inputs to prevent white saturation when blending
            // Two full-bright pixels would saturate all channels to 255 (white)
            // Scale by ~70% (180/255) to leave headroom for accumulation
            constexpr uint8_t ADDITIVE_SCALE = 180;
            return CRGB(
                qadd8(scale8(base.r, ADDITIVE_SCALE), scale8(blend.r, ADDITIVE_SCALE)),
                qadd8(scale8(base.g, ADDITIVE_SCALE), scale8(blend.g, ADDITIVE_SCALE)),
                qadd8(scale8(base.b, ADDITIVE_SCALE), scale8(blend.b, ADDITIVE_SCALE))
            );
        }

        case BlendMode::MULTIPLY:
            return CRGB(
                scale8(base.r, blend.r),
                scale8(base.g, blend.g),
                scale8(base.b, blend.b)
            );

        case BlendMode::SCREEN:
            // Screen: 1 - (1-a)(1-b) = a + b - ab
            return CRGB(
                255 - scale8(255 - base.r, 255 - blend.r),
                255 - scale8(255 - base.g, 255 - blend.g),
                255 - scale8(255 - base.b, 255 - blend.b)
            );

        case BlendMode::OVERLAY:
            // Overlay: multiply if base < 128, screen otherwise
            return CRGB(
                (base.r < 128) ? scale8(base.r * 2, blend.r)
                               : 255 - scale8((255 - base.r) * 2, 255 - blend.r),
                (base.g < 128) ? scale8(base.g * 2, blend.g)
                               : 255 - scale8((255 - base.g) * 2, 255 - blend.g),
                (base.b < 128) ? scale8(base.b * 2, blend.b)
                               : 255 - scale8((255 - base.b) * 2, 255 - blend.b)
            );

        case BlendMode::ALPHA:
            // 50/50 blend
            return CRGB(
                (base.r + blend.r) / 2,
                (base.g + blend.g) / 2,
                (base.b + blend.b) / 2
            );

        case BlendMode::LIGHTEN:
            return CRGB(
                max(base.r, blend.r),
                max(base.g, blend.g),
                max(base.b, blend.b)
            );

        case BlendMode::DARKEN:
            return CRGB(
                min(base.r, blend.r),
                min(base.g, blend.g),
                min(base.b, blend.b)
            );

        default:
            return blend;
    }
}

} // namespace zones
} // namespace lightwaveos
