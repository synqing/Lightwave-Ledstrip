// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPGeometricEffects.cpp
 * @brief LGP Geometric pattern effects implementation
 *
 * Advanced shapes and patterns leveraging Light Guide Plate optics.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPGeometricEffects.h"
#include "CoreEffects.h"
#include <FastLED.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// NOTE: Effects migrated to IEffect classes; legacy stubs kept for reference.

// ==================== DIAMOND LATTICE ====================
void effectDiamondLattice(RenderContext& ctx) {
    // MIGRATED TO LGPDiamondLatticeEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== HEXAGONAL GRID ====================
void effectHexagonalGrid(RenderContext& ctx) {
    // MIGRATED TO LGPHexagonalGridEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== SPIRAL VORTEX ====================
void effectSpiralVortex(RenderContext& ctx) {
    // MIGRATED TO LGPSpiralVortexEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== SIERPINSKI TRIANGLES ====================
void effectSierpinskiTriangles(RenderContext& ctx) {
    // MIGRATED TO LGPSierpinskiEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CHEVRON WAVES ====================
void effectChevronWaves(RenderContext& ctx) {
    // MIGRATED TO ChevronWavesEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CONCENTRIC RINGS ====================
void effectConcentricRings(RenderContext& ctx) {
    // MIGRATED TO LGPConcentricRingsEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== STAR BURST ====================
void effectStarBurst(RenderContext& ctx) {
    // MIGRATED TO LGPStarBurstEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== MESH NETWORK ====================
void effectMeshNetwork(RenderContext& ctx) {
    // MIGRATED TO LGPMeshNetworkEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPGeometricEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Diamond Lattice (ID 18) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Hexagonal Grid (ID 19) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Spiral Vortex (ID 20) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Sierpinski (ID 21) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Chevron Waves (ID 22) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Concentric Rings (ID 23) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Star Burst (ID 24) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Mesh Network (ID 25) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
