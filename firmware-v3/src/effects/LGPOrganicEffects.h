/**
 * @file LGPOrganicEffects.h
 * @brief LGP Organic pattern effects for Light Guide Plate displays
 *
 * Natural and fluid patterns leveraging Light Guide Plate diffusion.
 * These effects create organic, living visuals through optical blending.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== LGP Organic Effects ====================

void effectAuroraBorealis(RenderContext& ctx);
void effectBioluminescentWaves(RenderContext& ctx);
void effectPlasmaMembrane(RenderContext& ctx);
void effectNeuralNetwork(RenderContext& ctx);
void effectCrystallineGrowth(RenderContext& ctx);
void effectFluidDynamics(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Organic effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPOrganicEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
