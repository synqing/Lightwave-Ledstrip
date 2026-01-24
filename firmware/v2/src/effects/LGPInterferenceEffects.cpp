/**
 * @file LGPInterferenceEffects.cpp
 * @brief LGP Interference pattern effects implementation
 *
 * These effects exploit optical waveguide properties to create interference patterns.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPInterferenceEffects.h"
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

// ==================== BOX WAVE ====================
void effectBoxWave(RenderContext& ctx) {
    // MIGRATED TO LGPBoxWaveEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== HOLOGRAPHIC ====================
void effectHolographic(RenderContext& ctx) {
    // MIGRATED TO LGPHolographicEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== MODAL RESONANCE ====================
void effectModalResonance(RenderContext& ctx) {
    // MIGRATED TO ModalResonanceEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== INTERFERENCE SCANNER ====================
void effectInterferenceScanner(RenderContext& ctx) {
    // MIGRATED TO LGPInterferenceScannerEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== WAVE COLLISION ====================
void effectWaveCollision(RenderContext& ctx) {
    // MIGRATED TO LGPWaveCollisionEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPInterferenceEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Box Wave (ID 13) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Holographic (ID 14) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Modal Resonance (ID 15) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Interference Scanner (ID 16) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Wave Collision (ID 17) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
