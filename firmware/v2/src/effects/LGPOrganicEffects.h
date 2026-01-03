/**
 * @file LGPOrganicEffects.h
 * @brief LGP Organic pattern effects for Light Guide Plate displays
 *
 * Natural and fluid patterns leveraging Light Guide Plate diffusion.
 * These effects create organic, living visuals through optical blending.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererNode.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::nodes;

// ==================== LGP Organic Effects ====================

void effectAuroraBorealis(RenderContext& ctx);
void effectBioluminescentWaves(RenderContext& ctx);
void effectPlasmaMembrane(RenderContext& ctx);
void effectNeuralNetwork(RenderContext& ctx);
void effectCrystallineGrowth(RenderContext& ctx);
void effectFluidDynamics(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Organic effects with RendererNode
 * @param renderer Pointer to RendererNode
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPOrganicEffects(RendererNode* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
