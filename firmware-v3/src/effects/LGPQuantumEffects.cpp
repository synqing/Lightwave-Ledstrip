// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPQuantumEffects.cpp
 * @brief LGP Quantum-inspired effects implementation
 *
 * Mind-bending optical effects based on quantum mechanics and exotic physics.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPQuantumEffects.h"
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

// ==================== QUANTUM TUNNELING ====================
void effectQuantumTunneling(RenderContext& ctx) {
    // MIGRATED TO LGPQuantumTunnelingEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== GRAVITATIONAL LENSING ====================
void effectGravitationalLensing(RenderContext& ctx) {
    // MIGRATED TO LGPGravitationalLensingEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== TIME CRYSTAL ====================
void effectTimeCrystal(RenderContext& ctx) {
    // MIGRATED TO LGPTimeCrystalEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== SOLITON WAVES ====================
void effectSolitonWaves(RenderContext& ctx) {
    // MIGRATED TO LGPSolitonWavesEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== METAMATERIAL CLOAKING ====================
void effectMetamaterialCloaking(RenderContext& ctx) {
    // MIGRATED TO LGPMetamaterialCloakEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== GRIN CLOAK ====================
void effectGrinCloak(RenderContext& ctx) {
    // MIGRATED TO LGPGrinCloakEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CAUSTIC FAN ====================
void effectCausticFan(RenderContext& ctx) {
    // MIGRATED TO LGPCausticFanEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== BIREFRINGENT SHEAR ====================
void effectBirefringentShear(RenderContext& ctx) {
    // MIGRATED TO LGPBirefringentShearEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== ANISOTROPIC CLOAK ====================
void effectAnisotropicCloak(RenderContext& ctx) {
    // MIGRATED TO LGPAnisotropicCloakEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EVANESCENT SKIN ====================
void effectEvanescentSkin(RenderContext& ctx) {
    // MIGRATED TO LGPEvanescentSkinEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPQuantumEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Quantum Tunneling (ID 40) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Gravitational Lensing (ID 41) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Time Crystal (ID 42) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Soliton Waves (ID 43) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Metamaterial Cloak (ID 44) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP GRIN Cloak (ID 45) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Caustic Fan (ID 46) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Birefringent Shear (ID 47) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Anisotropic Cloak (ID 48) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Evanescent Skin (ID 49) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
