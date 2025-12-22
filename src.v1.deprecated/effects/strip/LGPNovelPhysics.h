#ifndef LGP_NOVEL_PHYSICS_H
#define LGP_NOVEL_PHYSICS_H

// ============================================================================
// LGP Novel Physics Effects
// Advanced effects exploiting dual-edge optical interference properties
// These effects are IMPOSSIBLE on single LED strips - they require two
// coherent light sources creating real interference patterns
// ============================================================================

// ============== CHLADNI PLATE HARMONICS ==============
// Visualizes acoustic resonance patterns on vibrating plates
// "Sand particles" migrate from antinodes to nodes, creating geometric patterns
// Dual strips show TOP and BOTTOM plate surface with 180° phase offset
//
// Encoder 3 (Speed): Vibration frequency - plate oscillation rate
// Encoder 4 (Intensity): Drive amplitude - antinode brightness
// Encoder 5 (Saturation): Particle glow - brightness at nodes
// Encoder 6 (Complexity): Mode number - harmonic (1=fundamental, 12=complex)
// Encoder 7 (Variation): Damping/chaos - pure modes vs mixed
void lgpChladniHarmonics();

// ============== GRAVITATIONAL WAVE CHIRP ==============
// Binary black hole inspiral with LIGO-accurate frequency evolution
// Two spiral waves accelerate exponentially (chirp), merge at center, ringdown
// Strip1 = h+ polarization, Strip2 = hx polarization (orthogonal)
//
// Encoder 3 (Speed): Inspiral duration - 2-10 seconds to merger
// Encoder 4 (Intensity): Strain amplitude - wave visibility
// Encoder 5 (Saturation): Color saturation
// Encoder 6 (Complexity): System mass - heavier = faster chirp
// Encoder 7 (Variation): Ringdown frequency - final BH oscillation pitch
void lgpGravitationalWaveChirp();

// ============== QUANTUM ENTANGLEMENT COLLAPSE ==============
// EPR paradox visualization with superposition and measurement
// Strips start in quantum foam (chaotic), collapse wavefront from center
// reveals perfect anti-correlation (complementary colors)
//
// Encoder 3 (Speed): Collapse speed - wavefront expansion rate
// Encoder 4 (Intensity): Superposition chaos - pre-collapse fluctuation
// Encoder 5 (Saturation): Color purity
// Encoder 6 (Complexity): Quantum mode n - wave function nodes (1-8)
// Encoder 7 (Variation): Decoherence rate - edge noise accumulation
void lgpQuantumEntanglementCollapse();

// ============== MYCELIAL NETWORK PROPAGATION ==============
// Fungal hyphal growth with fractal branching and nutrient flow
// Dual strips create depth layers (upper/lower mycelial mats)
// Interference zones form glowing "fruiting bodies"
//
// Encoder 3 (Speed): Growth rate - hyphal extension speed
// Encoder 4 (Intensity): Network density - number of growth tips
// Encoder 5 (Saturation): Nutrient visibility
// Encoder 6 (Complexity): Branching frequency - fractal depth (1-10)
// Encoder 7 (Variation): Flow direction bias - inward vs outward nutrients
void lgpMycelialNetwork();

// ============== RILEY DISSONANCE ==============
// Op Art perceptual instability inspired by Bridget Riley
// High-contrast patterns with frequency mismatch create binocular rivalry
// Static patterns appear to shimmer and breathe
//
// Encoder 3 (Speed): Pattern drift - slow rotation of conflict zones
// Encoder 4 (Intensity): Contrast/aggression - perceptual discomfort level
// Encoder 5 (Saturation): Color vs B&W
// Encoder 6 (Complexity): Pattern type - circles/stripes/checkerboard/spirals
// Encoder 7 (Variation): Frequency mismatch - beat envelope width
void lgpRileyDissonance();

#endif // LGP_NOVEL_PHYSICS_H
