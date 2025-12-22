#ifndef LGP_ADVANCED_EFFECTS_H
#define LGP_ADVANCED_EFFECTS_H

// Light Guide Plate Advanced Pattern Effects
// Implementation of optical phenomena from wave physics

// Moiré Curtains - Beat patterns from frequency mismatch
// Encoder 3 (Speed): Curtain movement speed
// Encoder 4 (Intensity): Pattern brightness
// Encoder 6 (Complexity): Base frequency (4-12 cycles)
// Encoder 7 (Variation): Frequency delta (beat rate)
void lgpMoireCurtains();

// Radial Ripple - Concentric expanding rings
// Encoder 3 (Speed): Ring expansion speed
// Encoder 4 (Intensity): Ring brightness/sharpness
// Encoder 6 (Complexity): Number of rings (2-10)
void lgpRadialRipple();

// Holographic Vortex - Spiral interference with depth
// Encoder 3 (Speed): Rotation speed
// Encoder 4 (Intensity): Radial tightness
// Encoder 6 (Complexity): Spiral count (1-4)
void lgpHolographicVortex();

// Evanescent Drift - Edge-fading waves
// Encoder 3 (Speed): Wave propagation speed
// Encoder 4 (Intensity): Decay rate (inverse)
void lgpEvanescentDrift();

// Chromatic Shear - Color plane sliding
// Encoder 3 (Speed): Shear velocity
// Encoder 6 (Complexity): Shear amount
// Encoder 7 (Variation): Palette rotation speed
void lgpChromaticShear();

// Modal Cavity - Waveguide mode excitation
// Encoder 3 (Speed): Mode evolution
// Encoder 4 (Intensity): Mode amplitude
// Encoder 6 (Complexity): Mode number (1-20)
// Encoder 7 (Variation): Beat mode offset
void lgpModalCavity();

// Fresnel Zones - Optical focusing effects
// Encoder 3 (Speed): Focal point movement
// Encoder 4 (Intensity): Zone edge smoothness
// Encoder 6 (Complexity): Zone count (3-11)
void lgpFresnelZones();

// Photonic Crystal - Bandgap structures
// Encoder 3 (Speed): Phase velocity
// Encoder 4 (Intensity): Mode amplitude
// Encoder 6 (Complexity): Lattice constant
// Encoder 7 (Variation): Defect density
void lgpPhotonicCrystal();

#endif // LGP_ADVANCED_EFFECTS_H 