/**
 * @file LGPAdvancedEffects.h
 * @brief LGP Advanced optical effects for Light Guide Plate displays
 *
 * Implementation of advanced optical phenomena including Moire patterns,
 * Fresnel zones, and photonic crystal effects.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== LGP Advanced Effects ====================

void effectMoireCurtains(RenderContext& ctx);
void effectRadialRipple(RenderContext& ctx);
void effectHolographicVortex(RenderContext& ctx);
void effectEvanescentDrift(RenderContext& ctx);
void effectChromaticShear(RenderContext& ctx);
void effectModalCavity(RenderContext& ctx);
void effectFresnelZones(RenderContext& ctx);
void effectPhotonicCrystal(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Advanced effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPAdvancedEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
