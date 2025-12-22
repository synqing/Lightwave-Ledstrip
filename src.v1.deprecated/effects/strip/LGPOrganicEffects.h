#ifndef LGP_ORGANIC_EFFECTS_H
#define LGP_ORGANIC_EFFECTS_H

// Light Guide Plate Organic Pattern Effects
// Natural phenomena leveraging optical diffusion

// Aurora Borealis - Northern lights with curtain dynamics
// Encoder 3 (Speed): Curtain movement speed
// Encoder 4 (Intensity): Aurora brightness
// Encoder 6 (Complexity): Number of curtains (2-5)
void lgpAuroraBorealis();

// Bioluminescent Waves - Ocean with glowing plankton
// Encoder 3 (Speed): Wave movement
// Encoder 4 (Intensity): Glow brightness
// Encoder 6 (Complexity): Wave count (2-6)
void lgpBioluminescentWaves();

// Plasma Membrane - Cellular lipid bilayer
// Encoder 3 (Speed): Membrane undulation
// Encoder 4 (Intensity): Membrane brightness
// Encoder 6 (Complexity): Undulation frequency
void lgpPlasmaMembrane();

// Neural Network - Synaptic firing visualization
// Encoder 3 (Speed): Signal propagation speed
// Encoder 4 (Intensity): Neuron brightness
// Encoder 6 (Complexity): Firing rate
void lgpNeuralNetwork();

// Crystalline Growth - Crystal formation patterns
// Encoder 3 (Speed): Growth rate
// Encoder 4 (Intensity): Crystal brightness
// Encoder 6 (Complexity): Growth probability
void lgpCrystallineGrowth();

// Fluid Dynamics - Laminar/turbulent flow
// Encoder 3 (Speed): Flow speed
// Encoder 4 (Intensity): Visualization brightness
// Encoder 6 (Complexity): Reynolds number (turbulence)
void lgpFluidDynamics();

#endif // LGP_ORGANIC_EFFECTS_H 