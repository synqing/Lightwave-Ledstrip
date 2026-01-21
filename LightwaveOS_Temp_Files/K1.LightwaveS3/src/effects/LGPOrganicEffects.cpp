/**
 * @file LGPOrganicEffects.cpp
 * @brief LGP Organic pattern effects implementation
 *
 * Natural and fluid patterns leveraging Light Guide Plate diffusion.
 * These effects create organic, living visuals through optical blending.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPOrganicEffects.h"
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

using namespace lightwaveos::nodes;

// NOTE: Effects migrated to IEffect classes; legacy stubs kept for reference.

// ==================== AURORA BOREALIS ====================
void effectAuroraBorealis(RenderContext& ctx) {
    // MIGRATED TO LGPAuroraBorealisEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== BIOLUMINESCENT WAVES ====================
void effectBioluminescentWaves(RenderContext& ctx) {
    // MIGRATED TO LGPBioluminescentWavesEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PLASMA MEMBRANE ====================
void effectPlasmaMembrane(RenderContext& ctx) {
    // MIGRATED TO LGPPlasmaMembraneEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== NEURAL NETWORK ====================
void effectNeuralNetwork(RenderContext& ctx) {
    // MIGRATED TO LGPNeuralNetworkEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CRYSTALLINE GROWTH ====================
void effectCrystallineGrowth(RenderContext& ctx) {
    // MIGRATED TO LGPCrystallineGrowthEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== FLUID DYNAMICS ====================
void effectFluidDynamics(RenderContext& ctx) {
    // MIGRATED TO LGPFluidDynamicsEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPOrganicEffects(RendererNode* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Aurora Borealis (ID 34) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Bioluminescent Waves (ID 35) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Plasma Membrane (ID 36) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Neural Network (ID 37) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Crystalline Growth (ID 38) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Fluid Dynamics (ID 39) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
