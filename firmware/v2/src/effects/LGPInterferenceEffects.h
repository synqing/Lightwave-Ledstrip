/**
 * @file LGPInterferenceEffects.h
 * @brief LGP Interference pattern effects for Light Guide Plate displays
 *
 * These effects exploit optical waveguide properties to create interference patterns.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== CENTER PAIR Constants ====================
// Imported from CoreEffects.h for consistency
constexpr uint16_t LGP_CENTER_LEFT = 79;
constexpr uint16_t LGP_CENTER_RIGHT = 80;
constexpr uint16_t LGP_HALF_LENGTH = 80;
constexpr uint16_t LGP_STRIP_LENGTH = 160;

// ==================== LGP Interference Effects ====================

void effectBoxWave(RenderContext& ctx);
void effectHolographic(RenderContext& ctx);
void effectModalResonance(RenderContext& ctx);
void effectInterferenceScanner(RenderContext& ctx);
void effectWaveCollision(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Interference effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPInterferenceEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
