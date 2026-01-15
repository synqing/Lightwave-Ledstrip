/**
 * @file LGPNovelPhysicsEffects.cpp
 * @brief LGP Novel Physics effects implementation
 *
 * Advanced effects exploiting dual-edge optical interference properties.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPNovelPhysicsEffects.h"
#include "CoreEffects.h"
#include <FastLED.h>
#include <math.h>

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::nodes;

// NOTE: Effects migrated to IEffect classes; legacy stubs kept for reference.

// ==================== CHLADNI HARMONICS ====================
void effectChladniHarmonics(RenderContext& ctx) {
    // MIGRATED TO LGPChladniHarmonicsEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== GRAVITATIONAL WAVE CHIRP ====================
void effectGravitationalWaveChirp(RenderContext& ctx) {
    // MIGRATED TO LGPGravitationalWaveChirpEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== QUANTUM ENTANGLEMENT COLLAPSE ====================
void effectQuantumEntanglementCollapse(RenderContext& ctx) {
    // MIGRATED TO LGPQuantumEntanglementEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== MYCELIAL NETWORK ====================
void effectMycelialNetwork(RenderContext& ctx) {
    // MIGRATED TO LGPMycelialNetworkEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== RILEY DISSONANCE ====================
void effectRileyDissonance(RenderContext& ctx) {
    // MIGRATED TO LGPRileyDissonanceEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPNovelPhysicsEffects(RendererNode* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Chladni Harmonics (ID 60) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Gravitational Wave Chirp (ID 61) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Quantum Entanglement (ID 62) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Mycelial Network (ID 63) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Riley Dissonance (ID 64) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
