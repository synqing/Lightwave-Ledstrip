/**
 * @file LGPChromaticEffects.h
 * @brief Physics-accurate chromatic dispersion effects using Cauchy equation
 *
 * Implements chromatic dispersion based on Cauchy equation: n(λ) = A + B/λ² + C/λ⁴
 * Reference: b1. LGP_OPTICAL_PHYSICS_REFERENCE.md lines 380-450
 *
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== Physics-Accurate Chromatic Dispersion ====================

/**
 * @brief Calculate chromatic dispersion using Cauchy equation
 * @param position LED position (0-159 for single strip)
 * @param aberration Aberration strength (0.0-3.0)
 * @param phase Animation phase (0.0-2π)
 * @param intensity Brightness multiplier (0.0-1.0)
 * @return CRGB color with physics-accurate dispersion
 */
// Implementation detail (kept internal to `LGPChromaticEffects.cpp`).
// If dispersion needs to be reused elsewhere, promote it to a shared utility module.

// ==================== Chromatic Effects ====================

/**
 * @brief Chromatic Lens: Static aberration, lens position controlled by speed encoder
 * Physics-accurate dispersion creates realistic lens-like color separation
 */
void effectChromaticLens(RenderContext& ctx);

/**
 * @brief Chromatic Pulse: Aberration sweeps from centre outward, intensity pulse
 * Creates pulsing chromatic separation that expands and contracts
 */
void effectChromaticPulse(RenderContext& ctx);

/**
 * @brief Chromatic Interference: Dual-edge injection with dispersion, interference patterns
 * Combines dispersion with wave interference for complex chromatic patterns
 */
void effectChromaticInterference(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Chromatic effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPChromaticEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos

