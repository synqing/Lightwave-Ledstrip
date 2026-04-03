/**
 * @file GradientTypes.h
 * @brief Core types for the K1 gradient rendering system
 *
 * This header defines the fundamental types used by the gradient kernel:
 * stops, repeat modes, interpolation modes, and blend modes.
 *
 * All types are value types suitable for stack allocation. No heap
 * allocation is required or permitted in the render path.
 *
 * Coordinate model:
 * - u_center: 0.0 at centre pair (79/80), 1.0 at edge — primary radial basis
 * - u_signed: -1.0 to +1.0 around centre pair — for asymmetry/shear
 * - u_local:  0.0 to 1.0 along single strip (0-159) — for linear sweeps
 * - edge_id:  0=EDGE_A (strip1), 1=EDGE_B (strip2) — for dual-edge differentiation
 * - u_half:   0-79 half-strip index — for mirrored kernels
 *
 * See GradientCoord.h for coordinate conversion helpers.
 */

#pragma once

#include <cstdint>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/test_native/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace gradient {

// ============================================================================
// Constants
// ============================================================================

/// Maximum colour stops per gradient ramp (static allocation)
static constexpr uint8_t kMaxStops = 8;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief How the gradient behaves beyond its defined range [0-255]
 */
enum class RepeatMode : uint8_t {
    CLAMP = 0,    ///< Hold edge colours beyond range
    REPEAT,       ///< Tile the pattern (sawtooth wrap)
    MIRROR        ///< Ping-pong repeat (triangle wrap)
};

/**
 * @brief Interpolation method between adjacent stops
 */
enum class InterpolationMode : uint8_t {
    LINEAR = 0,   ///< Linear RGB interpolation (lerp8)
    HARD_STOP,    ///< No interpolation — nearest stop boundary
    EASED         ///< Cosine-eased interpolation (smoother perceptual transitions)
};

/**
 * @brief How a gradient layer composites onto existing LED data
 */
enum class BlendMode : uint8_t {
    REPLACE = 0,  ///< Overwrite destination completely
    ADD,          ///< Additive blend (clamped at 255)
    SCREEN,       ///< Screen blend: 255 - ((255-a) * (255-b) >> 8)
    MULTIPLY      ///< Multiply blend: (a * b) >> 8
};

// ============================================================================
// Core Data Types
// ============================================================================

/**
 * @brief A single colour stop in a gradient ramp
 *
 * Position is fixed-point [0-255] mapping to [0.0-1.0] along the ramp.
 * Colour is a standard FastLED CRGB value.
 */
struct GradientStop {
    uint8_t position;   ///< Fixed-point position along ramp [0-255]
    CRGB colour;        ///< Colour at this stop

    constexpr GradientStop()
        : position(0), colour(CRGB::Black) {}

    constexpr GradientStop(uint8_t pos, CRGB col)
        : position(pos), colour(col) {}

    constexpr GradientStop(uint8_t pos, uint8_t r, uint8_t g, uint8_t b)
        : position(pos), colour(r, g, b) {}
};

} // namespace gradient
} // namespace effects
} // namespace lightwaveos
