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

// Turbulent Flow Dynamics - Fluid vortices with color mixing
// Encoder 3 (Speed): Flow velocity/vortex spawn rate
// Encoder 4 (Intensity): Vortex strength
// Encoder 5 (Saturation): Color mixing intensity
// Encoder 6 (Complexity): Vortex size/influence radius
// Encoder 7 (Variation): Turbulence visualization (>200 shows vortex cores)
void lgpTurbulentFlow();

#endif // LGP_PHYSICS_EFFECTS_H