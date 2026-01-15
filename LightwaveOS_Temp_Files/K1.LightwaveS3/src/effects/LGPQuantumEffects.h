/**
 * @file LGPQuantumEffects.h
 * @brief LGP Quantum-inspired effects for Light Guide Plate displays
 *
 * Mind-bending optical effects based on quantum mechanics and exotic physics.
 * Designed to exploit Light Guide Plate interference for otherworldly visuals.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererNode.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::nodes;

// ==================== LGP Quantum Effects ====================

void effectQuantumTunneling(RenderContext& ctx);
void effectGravitationalLensing(RenderContext& ctx);
void effectTimeCrystal(RenderContext& ctx);
void effectSolitonWaves(RenderContext& ctx);
void effectMetamaterialCloaking(RenderContext& ctx);
void effectGrinCloak(RenderContext& ctx);
void effectCausticFan(RenderContext& ctx);
void effectBirefringentShear(RenderContext& ctx);
void effectAnisotropicCloak(RenderContext& ctx);
void effectEvanescentSkin(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Quantum effects with RendererNode
 * @param renderer Pointer to RendererNode
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPQuantumEffects(RendererNode* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
