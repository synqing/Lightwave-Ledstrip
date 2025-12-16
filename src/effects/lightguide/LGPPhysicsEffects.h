#ifndef LGP_PHYSICS_EFFECTS_H
#define LGP_PHYSICS_EFFECTS_H

// Light Guide Plate Physics-Based Effects
// Advanced simulations exploiting optical waveguide properties

// Plasma Field Visualization - Charged particle interactions
// Encoder 3 (Speed): Particle spawn rate
// Encoder 4 (Intensity): Electric field strength
// Encoder 5 (Saturation): Field line visibility
// Encoder 6 (Complexity): Oscillation frequency
// Encoder 7 (Variation): Charge distribution
void lgpPlasmaField();

// Magnetic Field Lines - Dipole field visualization
// Encoder 3 (Speed): Magnet rotation speed
// Encoder 4 (Intensity): Field strength
// Encoder 5 (Saturation): Pole glow intensity
// Encoder 6 (Complexity): Field line density
// Encoder 7 (Variation): Magnet configuration
void lgpMagneticField();

// Particle Collision Chamber - High-energy physics simulation
// Encoder 3 (Speed): Particle velocity
// Encoder 4 (Intensity): Collision energy
// Encoder 5 (Saturation): Track brightness
// Encoder 6 (Complexity): Particle count
// Encoder 7 (Variation): Collision types
void lgpParticleCollider();

// Wave Tank Simulation - Realistic water waves
// Encoder 3 (Speed): Wave frequency
// Encoder 4 (Intensity): Wave amplitude
// Encoder 5 (Saturation): Water color depth
// Encoder 6 (Complexity): Wave count
// Encoder 7 (Variation): Damping factor
void lgpWaveTank();

// Energy Transfer Visualization - Power flow between edges
// Encoder 3 (Speed): Transfer rate
// Encoder 4 (Intensity): Packet brightness
// Encoder 5 (Saturation): Color vibrancy
// Encoder 6 (Complexity): Packet size variation
// Encoder 7 (Variation): Loss/efficiency
void lgpEnergyTransfer();

// Quantum Field Fluctuations - Virtual particle pairs
// Encoder 3 (Speed): Fluctuation rate
// Encoder 4 (Intensity): Field energy density
// Encoder 5 (Saturation): Particle color depth
// Encoder 6 (Complexity): Vacuum energy level
// Encoder 7 (Variation): Correlation length
void lgpQuantumFluctuations();

// Liquid Crystal Flow - Smooth organic color transitions
// Encoder 3 (Speed): Flow animation speed
// Encoder 4 (Intensity): Overall brightness
// Encoder 5 (Saturation): Color saturation control
// Encoder 6 (Complexity): Wave complexity/layers
// Encoder 7 (Variation): Center pulse intensity (>100 enables)
void lgpLiquidCrystal();

// Prism Cascade - Beautiful spectral dispersion effects
// Encoder 3 (Speed): Wave spawn rate
// Encoder 4 (Intensity): Wave propagation speed
// Encoder 5 (Saturation): Center glow intensity
// Encoder 6 (Complexity): Spectral spread/wavelength
// Encoder 7 (Variation): Unused
void lgpPrismCascade();

// Silk Waves - Smooth flowing waves like silk fabric
// Encoder 3 (Speed): Wave flow speed
// Encoder 4 (Intensity): Overall brightness
// Encoder 5 (Saturation): Highlight intensity
// Encoder 6 (Complexity): Wave frequency/detail
// Encoder 7 (Variation): Unused
void lgpSilkWaves();

// Beam Collision Explosion - Laser beams shoot and EXPLODE when they meet!
// Encoder 3 (Speed): Beam spawn rate
// Encoder 4 (Intensity): Beam speed multiplier
// Encoder 5 (Saturation): Explosion flash intensity (>150 adds white flash)
// Encoder 6 (Complexity): Explosion particle count
// Encoder 7 (Variation): Unused
void lgpBeamCollision();

// Laser Duel - Opposing lasers fight with power struggles and sparks!
// Encoder 3 (Speed): Battle intensity/charge rate
// Encoder 4 (Intensity): Beam movement speed
// Encoder 5 (Saturation): Unused
// Encoder 6 (Complexity): Spark count multiplier
// Encoder 7 (Variation): Unused
void lgpLaserDuel();

// Tidal Forces - Massive waves crash from both sides!
// Encoder 3 (Speed): Wave spawn frequency
// Encoder 4 (Intensity): Wave speed multiplier
// Encoder 5 (Saturation): Unused
// Encoder 6 (Complexity): Splash particle multiplier
// Encoder 7 (Variation): Unused
void lgpTidalForces();

#endif // LGP_PHYSICS_EFFECTS_H