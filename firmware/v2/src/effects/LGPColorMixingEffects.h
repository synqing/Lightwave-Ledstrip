/**
 * @file LGPColorMixingEffects.h
 * @brief LGP Color Mixing effects for Light Guide Plate displays
 *
 * Exploiting opposing light channels for unprecedented color phenomena.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== LGP Color Mixing Effects ====================

void effectColorTemperature(RenderContext& ctx);
void effectRGBPrism(RenderContext& ctx);
void effectComplementaryMixing(RenderContext& ctx);
void effectQuantumColors(RenderContext& ctx);
void effectDopplerShift(RenderContext& ctx);
void effectColorAccelerator(RenderContext& ctx);
void effectDNAHelix(RenderContext& ctx);
void effectPhaseTransition(RenderContext& ctx);
void effectChromaticAberration(RenderContext& ctx);
void effectPerceptualBlend(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Color Mixing effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPColorMixingEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
