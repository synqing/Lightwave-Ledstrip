/**
 * @file LGPNovelPhysicsEffects.h
 * @brief LGP Novel Physics effects for Light Guide Plate displays
 *
 * Advanced effects exploiting dual-edge optical interference properties.
 * These effects are IMPOSSIBLE on single LED strips - they require two
 * coherent light sources creating real interference patterns.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererNode.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::nodes;

// ==================== LGP Novel Physics Effects ====================

void effectChladniHarmonics(RenderContext& ctx);
void effectGravitationalWaveChirp(RenderContext& ctx);
void effectQuantumEntanglementCollapse(RenderContext& ctx);
void effectMycelialNetwork(RenderContext& ctx);
void effectRileyDissonance(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Novel Physics effects with RendererNode
 * @param renderer Pointer to RendererNode
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPNovelPhysicsEffects(RendererNode* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
