// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPGeometricEffects.h
 * @brief LGP Geometric pattern effects for Light Guide Plate displays
 *
 * Advanced shapes and patterns leveraging Light Guide Plate optics.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#pragma once

#include "../core/actors/RendererActor.h"

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== LGP Geometric Effects ====================

void effectDiamondLattice(RenderContext& ctx);
void effectHexagonalGrid(RenderContext& ctx);
void effectSpiralVortex(RenderContext& ctx);
void effectSierpinskiTriangles(RenderContext& ctx);
void effectChevronWaves(RenderContext& ctx);
void effectConcentricRings(RenderContext& ctx);
void effectStarBurst(RenderContext& ctx);
void effectMeshNetwork(RenderContext& ctx);

// ==================== Effect Registration ====================

/**
 * @brief Register all LGP Geometric effects with RendererActor
 * @param renderer Pointer to RendererActor
 * @param startId Starting effect ID
 * @return Number of effects registered
 */
uint8_t registerLGPGeometricEffects(RendererActor* renderer, uint8_t startId);

} // namespace effects
} // namespace lightwaveos
