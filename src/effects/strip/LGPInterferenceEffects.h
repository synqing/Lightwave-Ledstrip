#ifndef LGP_INTERFERENCE_EFFECTS_H
#define LGP_INTERFERENCE_EFFECTS_H

// Light Guide Plate Interference Effects
// These effects are specifically designed to exploit the optical properties
// of edge-lit light guide plates to create unique visual phenomena

// Box Wave Controller - Creates 3-12 controllable standing wave boxes
// Encoder 3 (Speed): Box oscillation speed
// Encoder 4 (Intensity): Box contrast/sharpness  
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of boxes (3-12)
// Encoder 7 (Variation): Motion type (standing/traveling/rotating)
void lgpBoxWave();

// Holographic Shimmer - Multi-layer interference for depth illusion
// Encoder 3 (Speed): Shimmer animation speed
// Encoder 4 (Intensity): Effect brightness/visibility
// Encoder 5 (Saturation): Color richness
// Encoder 6 (Complexity): Number of depth layers (2-5)
// Encoder 7 (Variation): Layer interaction mode
void lgpHolographic();

// Modal Resonance - Explores optical cavity modes (1-20)
// Encoder 3 (Speed): Mode sweep speed
// Encoder 4 (Intensity): Mode amplitude
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Mode number selection
// Encoder 7 (Variation): Mode blend type
void lgpModalResonance();

// Interference Scanner - Creates scanning interference patterns
// Encoder 3 (Speed): Scan speed
// Encoder 4 (Intensity): Pattern contrast
// Encoder 5 (Saturation): Color depth
// Encoder 6 (Complexity): Interference complexity
// Encoder 7 (Variation): Scan pattern type
void lgpInterferenceScanner();

// Wave Collision - Simulates colliding wave packets
// Encoder 3 (Speed): Wave velocity
// Encoder 4 (Intensity): Wave amplitude
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Wave packet width
// Encoder 7 (Variation): Collision behavior
void lgpWaveCollision();

// Soliton Explorer - Self-maintaining wave packets that preserve shape
// Encoder 3 (Speed): Soliton velocity
// Encoder 4 (Intensity): Soliton amplitude/nonlinearity
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of solitons (1-4)
// Encoder 7 (Variation): Soliton type (bright/dark/breather)
void lgpSolitonExplorer();

// Quantum Tunneling - Wave packets that tunnel through barrier regions
// Encoder 3 (Speed): Packet velocity
// Encoder 4 (Intensity): Barrier height/tunneling probability
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of barriers (1-3)
// Encoder 7 (Variation): Barrier type (rectangular/gaussian/periodic)
void lgpQuantumTunneling();

// Rogue Wave Generator - Extreme wave events from background noise
// Encoder 3 (Speed): Background wave frequency
// Encoder 4 (Intensity): Rogue wave amplitude multiplier
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of background modes (3-12)
// Encoder 7 (Variation): Rogue wave trigger probability
void lgpRogueWaveGenerator();

// Turing Pattern Engine - Biological pattern formation via reaction-diffusion
// Encoder 3 (Speed): Reaction rate
// Encoder 4 (Intensity): Pattern contrast
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Pattern scale (spots to stripes)
// Encoder 7 (Variation): Pattern type (spots/stripes/maze/spiral)
void lgpTuringPatternEngine();

// Kelvin-Helmholtz Instabilities - Vortex formation at shear boundaries
// Encoder 3 (Speed): Shear velocity/instability growth rate
// Encoder 4 (Intensity): Vortex strength/circulation
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of vortices (1-6)
// Encoder 7 (Variation): Instability type (single/multiple/turbulent)
void lgpKelvinHelmholtzInstabilities();

// Faraday Rotation - Polarization rotation in magnetized optical medium
// Encoder 3 (Speed): Rotation rate/magnetic field strength
// Encoder 4 (Intensity): Faraday rotation angle
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of magnetic domains (1-5)
// Encoder 7 (Variation): Field configuration (uniform/gradient/alternating)
void lgpFaradayRotation();

// Brillouin Zones - Electronic band structure in crystalline materials
// Encoder 3 (Speed): Electron velocity/band filling
// Encoder 4 (Intensity): Band gap energy
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of bands (1-8)
// Encoder 7 (Variation): Crystal structure (1D/2D/3D projection)
void lgpBrillouinZones();

// Shock Wave Formation - Nonlinear steepening of wave fronts
// Encoder 3 (Speed): Wave propagation velocity
// Encoder 4 (Intensity): Nonlinearity strength/shock steepness
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of shock fronts (1-4)
// Encoder 7 (Variation): Shock type (compression/rarefaction/N-wave)
void lgpShockWaveFormation();

// Chaos Visualization - Strange attractors and bifurcation cascades
// Encoder 3 (Speed): Evolution rate/time step
// Encoder 4 (Intensity): Chaos parameter/bifurcation control
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Attractor type (1-6)
// Encoder 7 (Variation): Visualization mode (trajectory/phase/poincare)
void lgpChaosVisualization();

// Neural Avalanche Cascades - Brain activity pattern explosions
// Encoder 3 (Speed): Neural firing rate/cascade velocity
// Encoder 4 (Intensity): Synaptic strength/avalanche magnitude
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Network connectivity (sparse to dense)
// Encoder 7 (Variation): Cascade type (critical/subcritical/supercritical)
void lgpNeuralAvalancheCascades();

// Cardiac Arrhythmia Spirals - Heart rhythm chaos with spiral wave breakup
// Encoder 3 (Speed): Heart rate/conduction velocity
// Encoder 4 (Intensity): Arrhythmia severity/spiral instability
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Number of spiral cores (1-4)
// Encoder 7 (Variation): Arrhythmia type (AF/VF/VT)
void lgpCardiacArrhythmiaSpirals();

// Supernova Shockfront - Stellar explosion dynamics with nucleosynthesis
// Encoder 3 (Speed): Explosion velocity/shockfront speed
// Encoder 4 (Intensity): Explosion energy/temperature
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Stellar mass (affects explosion dynamics)
// Encoder 7 (Variation): Supernova type (Ia/IIb/hypernova)
void lgpSupernovaShockfront();

// Plasma Instabilities - Fusion reactor turbulence with MHD modes
// Encoder 3 (Speed): Plasma flow velocity/rotation
// Encoder 4 (Intensity): Magnetic field strength/instability growth
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Mode number (m/n resonances)
// Encoder 7 (Variation): Instability type (tearing/ballooning/kink)
void lgpPlasmaInstabilities();

// Fractal Dimension Generator - Infinite recursive madness with self-similarity
// Encoder 3 (Speed): Fractal zoom/iteration speed
// Encoder 4 (Intensity): Fractal complexity/iteration depth
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Fractal type selection (1-8)
// Encoder 7 (Variation): Dimension parameter/scaling
void lgpFractalDimensionGenerator();

// Earthquake Seismic Waves - Tectonic fury with P-waves and S-waves
// Encoder 3 (Speed): Wave propagation velocity
// Encoder 4 (Intensity): Earthquake magnitude/energy
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): Fault complexity/rupture pattern
// Encoder 7 (Variation): Wave type (P/S/surface/tsunami)
void lgpEarthquakeSeismicWaves();

#endif // LGP_INTERFERENCE_EFFECTS_H