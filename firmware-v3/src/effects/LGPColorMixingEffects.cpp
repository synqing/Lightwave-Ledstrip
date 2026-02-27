// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPColorMixingEffects.cpp
 * @brief LGP Color Mixing effects implementation
 *
 * Exploiting opposing light channels for unprecedented color phenomena.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPColorMixingEffects.h"
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

// ==================== COLOR TEMPERATURE ====================
void effectColorTemperature(RenderContext& ctx) {
    // MIGRATED TO LGPColorTemperatureEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== RGB PRISM ====================
void effectRGBPrism(RenderContext& ctx) {
    // MIGRATED TO LGPRGBPrismEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== COMPLEMENTARY MIXING ====================
void effectComplementaryMixing(RenderContext& ctx) {
    // MIGRATED TO LGPComplementaryMixingEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== QUANTUM COLORS ====================
void effectQuantumColors(RenderContext& ctx) {
    // MIGRATED TO LGPQuantumColorsEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== DOPPLER SHIFT ====================
void effectDopplerShift(RenderContext& ctx) {
    // MIGRATED TO LGPDopplerShiftEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== COLOR ACCELERATOR ====================
void effectColorAccelerator(RenderContext& ctx) {
    // MIGRATED TO LGPColorAcceleratorEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== DNA HELIX ====================
void effectDNAHelix(RenderContext& ctx) {
    // MIGRATED TO LGPDNAHelixEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PHASE TRANSITION ====================
void effectPhaseTransition(RenderContext& ctx) {
    // MIGRATED TO LGPPhaseTransitionEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CHROMATIC ABERRATION ====================
void effectChromaticAberration(RenderContext& ctx) {
    // MIGRATED TO LGPChromaticAberrationEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PERCEPTUAL BLEND ====================
void effectPerceptualBlend(RenderContext& ctx) {
    // MIGRATED TO LGPPerceptualBlendEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPColorMixingEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Color Temperature (ID 50) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP RGB Prism (ID 51) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Complementary Mixing (ID 52) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Quantum Colors (ID 53) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Doppler Shift (ID 54) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Color Accelerator (ID 55) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP DNA Helix (ID 56) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Phase Transition (ID 57) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Chromatic Aberration (ID 58) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Perceptual Blend (ID 59) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
