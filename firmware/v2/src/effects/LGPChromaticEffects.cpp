/**
 * @file LGPChromaticEffects.cpp
 * @brief Physics-accurate chromatic dispersion effects using Cauchy equation
 *
 * Implements chromatic dispersion based on Cauchy equation: n(λ) = A + B/λ² + C/λ⁴
 * Reference: b1. LGP_OPTICAL_PHYSICS_REFERENCE.md lines 380-450
 *
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPChromaticEffects.h"
#include "CoreEffects.h"
#include "../core/actors/RendererActor.h"
#include "utils/FastLEDOptim.h"
#include <FastLED.h>
#include <math.h>

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// NOTE: Effects migrated to IEffect classes; legacy stubs kept for reference.

// ============================================================================
// Effect 1: Chromatic Lens
// ============================================================================

void effectChromaticLens(RenderContext& ctx) {
    // MIGRATED TO LGPChromaticLensEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ============================================================================
// Effect 2: Chromatic Pulse
// ============================================================================

void effectChromaticPulse(RenderContext& ctx) {
    // MIGRATED TO LGPChromaticPulseEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ============================================================================
// Effect 3: Chromatic Interference
// ============================================================================

void effectChromaticInterference(RenderContext& ctx) {
    // MIGRATED TO ChromaticInterferenceEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ============================================================================
// Effect Registration
// ============================================================================

uint8_t registerLGPChromaticEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Chromatic Lens (ID 65) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Chromatic Pulse (ID 66) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // Pilot: LGP Chromatic Interference (ID 67) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
