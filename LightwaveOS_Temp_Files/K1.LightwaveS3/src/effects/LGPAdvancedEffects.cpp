/**
 * @file LGPAdvancedEffects.cpp
 * @brief LGP Advanced optical effects implementation
 *
 * Implementation of advanced optical phenomena including Moire patterns,
 * Fresnel zones, and photonic crystal effects.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPAdvancedEffects.h"
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

// ==================== MOIRE CURTAINS ====================
void effectMoireCurtains(RenderContext& ctx) {
    // MIGRATED TO LGPMoireCurtainsEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== RADIAL RIPPLE ====================
void effectRadialRipple(RenderContext& ctx) {
    // MIGRATED TO LGPRadialRippleEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== HOLOGRAPHIC VORTEX ====================
void effectHolographicVortex(RenderContext& ctx) {
    // MIGRATED TO LGPHolographicVortexEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EVANESCENT DRIFT ====================
void effectEvanescentDrift(RenderContext& ctx) {
    // MIGRATED TO LGPEvanescentDriftEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== CHROMATIC SHEAR ====================
void effectChromaticShear(RenderContext& ctx) {
    // MIGRATED TO LGPChromaticShearEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== MODAL CAVITY ====================
void effectModalCavity(RenderContext& ctx) {
    // MIGRATED TO LGPModalCavityEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== FRESNEL ZONES ====================
void effectFresnelZones(RenderContext& ctx) {
    // MIGRATED TO LGPFresnelZonesEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== PHOTONIC CRYSTAL ====================
void effectPhotonicCrystal(RenderContext& ctx) {
    // MIGRATED TO LGPPhotonicCrystalEffect IEffect class
    // This function is no longer registered - kept for reference
    (void)ctx;
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPAdvancedEffects(RendererNode* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    // LGP Moire Curtains (ID 26) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Radial Ripple (ID 27) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Holographic Vortex (ID 28) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Evanescent Drift (ID 29) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Chromatic Shear (ID 30) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Modal Cavity (ID 31) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Fresnel Zones (ID 32) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence
    // LGP Photonic Crystal (ID 33) - registered as IEffect in registerAllEffects()
    count++;  // Increment count to maintain ID sequence

    return count;
}

} // namespace effects
} // namespace lightwaveos
