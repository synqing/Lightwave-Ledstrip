#ifndef LGP_FLUID_PLASMA_EFFECTS_H
#define LGP_FLUID_PLASMA_EFFECTS_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../config/features.h"

// ============================================================================
// LGP FLUID & PLASMA EFFECTS
// ============================================================================
// Physics-based effects simulating fluid dynamics and plasma phenomena
// All effects follow CENTER ORIGIN principle (originate from LED 79/80)
//
// Implements:
// - Rayleigh-Taylor Instability (heavy fluid plumes)
// - Benard Convection Cells (thermal hexagonal patterns)
// - Plasma Pinch (Z-Pinch magnetic compression)
// - Magnetic Reconnection (field line snap/release)
// - Kelvin-Helmholtz Enhanced (cat's eye vortices)
// ============================================================================

// Rayleigh-Taylor Instability
// Heavy fluid "mushroom" plumes growing outward from center interface
// Physics: ρ_heavy > ρ_light creates instability at interface
void lgpRayleighTaylorInstability();

// Benard Convection Cells
// Hexagonal thermal convection patterns radiating from center
// Physics: ΔT across fluid layer creates organized circulation cells
void lgpBenardConvection();

// Plasma Pinch (Z-Pinch)
// Magnetic compression toward center point
// Physics: Lorentz force J×B compresses plasma column
void lgpPlasmaPinch();

// Magnetic Reconnection
// Field lines snap and release energy jets from center X-point
// Physics: Oppositely directed B-fields annihilate, releasing energy
void lgpMagneticReconnection();

// Kelvin-Helmholtz Enhanced
// Cat's eye vortices at shear layer interface
// Physics: Velocity shear creates rolling vortex instabilities
void lgpKelvinHelmholtzEnhanced();

#endif // LGP_FLUID_PLASMA_EFFECTS_H
