/**
 * Pattern Registry Implementation
 *
 * Provides metadata for all 68 registered patterns.
 * Pattern indices match effect IDs exactly (stable ID mapping).
 */

#include "PatternRegistry.h"
#include "../core/actors/NodeOrchestrator.h"
#include "../core/actors/RendererNode.h"

// ============================================================================
// Pattern Metadata Definitions (PROGMEM)
// ============================================================================

// Helper macros for PROGMEM strings
#define PM_STR(s) (const char*)(s)

// Pattern metadata entries - mapped to effect IDs (matches v2 registration order)
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // --- Core Effects (0-12) ---
    {PM_STR("Fire"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Realistic fire simulation radiating from centre"), PM_STR("Thermal convection, intensity gradient"), PM_STR("")},
    {PM_STR("Ocean"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Deep ocean wave patterns from centre point"), PM_STR("Wave propagation, depth gradient"), PM_STR("")},
    {PM_STR("Plasma"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Smoothly shifting color plasma"), PM_STR("Perlin noise, continuous flow"), PM_STR("")},
    {PM_STR("Confetti"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Random colored speckles fading"), PM_STR("Stochastic particles"), PM_STR("")},
    {PM_STR("Sinelon"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Bouncing particle with palette trails"), PM_STR("Position-based motion, trail decay"), PM_STR("")},
    {PM_STR("Juggle"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Multiple colored balls juggling"), PM_STR("Physics-based gravity simulation"), PM_STR("")},
    {PM_STR("BPM"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-synced pulsing sawtooth waves"), PM_STR("Rhythmic timing"), PM_STR("")},
    {PM_STR("Wave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Simple sine wave propagation"), PM_STR("Oscillation"), PM_STR("")},
    {PM_STR("Ripple"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Expanding water ripples"), PM_STR("Radial decay"), PM_STR("")},
    {PM_STR("Heartbeat"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Rhythmic cardiac pulsing"), PM_STR("Biological timing"), PM_STR("")},
    {PM_STR("Interference"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN, PM_STR("Basic wave interference"), PM_STR("Wave superposition"), PM_STR("")},
    {PM_STR("Breathing"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow rhythmic brightness pulsing"), PM_STR("Ambient modulation"), PM_STR("")},
    {PM_STR("Pulse"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Sharp energy pulses"), PM_STR("Transient signal"), PM_STR("")},

    // --- LGP Interference Effects (13-17) ---
    {PM_STR("LGP Box Wave"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Square wave standing patterns"), PM_STR("Harmonic resonance"), PM_STR("")},
    {PM_STR("LGP Holographic"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Holographic interference patterns through multi-layer depth"), PM_STR("Multi-layer interference, phase relationships, depth illusion"), PM_STR("LGP Modal Resonance, LGP Interference Scanner")},
    {PM_STR("LGP Modal Resonance"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING, PM_STR("Explores different optical cavity resonance modes"), PM_STR("Cavity modes, standing wave patterns, mode selection"), PM_STR("LGP Holographic")},
    {PM_STR("LGP Interference Scanner"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Scanning beam with interference fringes"), PM_STR("Phase scanning, fringe projection"), PM_STR("LGP Wave Collision")},
    {PM_STR("LGP Wave Collision"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Colliding wave fronts creating standing nodes"), PM_STR("Wave superposition, constructive/destructive interference"), PM_STR("LGP Interference Scanner")},

    // --- LGP Geometric Effects (18-25) ---
    {PM_STR("LGP Diamond Lattice"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Interwoven diamond patterns"), PM_STR("Geometric tiling"), PM_STR("")},
    {PM_STR("LGP Hexagonal Grid"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Hexagonal cell structure"), PM_STR("Geometric tiling"), PM_STR("")},
    {PM_STR("LGP Spiral Vortex"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Rotating spiral arms"), PM_STR("Radial geometry"), PM_STR("")},
    {PM_STR("LGP Sierpinski"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Fractal triangle generation"), PM_STR("Recursive geometry"), PM_STR("")},
    {PM_STR("LGP Chevron Waves"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("V-shaped wave propagation"), PM_STR("Directional wave"), PM_STR("")},
    {PM_STR("LGP Concentric Rings"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Expanding circular rings"), PM_STR("Radial symmetry"), PM_STR("")},
    {PM_STR("LGP Star Burst"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Explosive radial lines"), PM_STR("Radial projection"), PM_STR("")},
    {PM_STR("LGP Mesh Network"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Interconnected node graph"), PM_STR("Network topology"), PM_STR("")},

    // --- LGP Advanced Effects (26-33) ---
    {PM_STR("LGP Moire Curtains"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::MOIRE, PM_STR("Shifting moire interference layers"), PM_STR("Optical beating"), PM_STR("")},
    {PM_STR("LGP Radial Ripple"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Complex radial wave interference"), PM_STR("Wave propagation"), PM_STR("")},
    {PM_STR("LGP Holographic Vortex"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DEPTH, PM_STR("Deep 3D vortex illusion"), PM_STR("Perspective projection"), PM_STR("")},
    {PM_STR("LGP Evanescent Drift"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Ghostly drifting particles"), PM_STR("Sub-threshold illumination"), PM_STR("")},
    {PM_STR("LGP Chromatic Shear"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Color-splitting shear effect"), PM_STR("Prismatic dispersion"), PM_STR("")},
    {PM_STR("LGP Modal Cavity"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Resonant optical cavity modes"), PM_STR("Harmonics"), PM_STR("")},
    {PM_STR("LGP Fresnel Zones"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Fresnel lens zone plate pattern"), PM_STR("Diffraction optics"), PM_STR("")},
    {PM_STR("LGP Photonic Crystal"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Bandgap structure simulation"), PM_STR("Periodic nanostructure"), PM_STR("")},

    // --- LGP Organic Effects (34-39) ---
    {PM_STR("LGP Aurora Borealis"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Shimmering curtain lights"), PM_STR("Atmospheric ionization"), PM_STR("")},
    {PM_STR("LGP Bioluminescent Waves"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Glowing plankton in waves"), PM_STR("Biological light"), PM_STR("")},
    {PM_STR("LGP Plasma Membrane"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Cellular membrane fluctuations"), PM_STR("Fluid mosaic model"), PM_STR("")},
    {PM_STR("LGP Neural Network"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Firing synaptic pathways"), PM_STR("Action potentials"), PM_STR("")},
    {PM_STR("LGP Crystalline Growth"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Growing crystal facets"), PM_STR("Nucleation and growth"), PM_STR("")},
    {PM_STR("LGP Fluid Dynamics"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Fluid flow simulation"), PM_STR("Navier-Stokes approximation"), PM_STR("")},

    // --- LGP Quantum Effects (40-49) ---
    {PM_STR("LGP Quantum Tunneling"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Particles passing through barriers"), PM_STR("Wave function probability"), PM_STR("")},
    {PM_STR("LGP Gravitational Lensing"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Light bending around mass"), PM_STR("General relativity"), PM_STR("")},
    {PM_STR("LGP Time Crystal"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Periodic structure in time"), PM_STR("Non-equilibrium matter"), PM_STR("")},
    {PM_STR("LGP Soliton Waves"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS | PatternTags::TRAVELING, PM_STR("Self-reinforcing wave packets"), PM_STR("Non-linear dispersion"), PM_STR("")},
    {PM_STR("LGP Metamaterial Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Invisibility cloak simulation"), PM_STR("Negative refractive index"), PM_STR("")},
    {PM_STR("LGP GRIN Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Gradient index optics"), PM_STR("Variable refractive index"), PM_STR("")},
    {PM_STR("LGP Caustic Fan"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Focused light caustics"), PM_STR("Ray optics"), PM_STR("")},
    {PM_STR("LGP Birefringent Shear"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS | PatternTags::SPECTRAL, PM_STR("Polarization splitting"), PM_STR("Double refraction"), PM_STR("")},
    {PM_STR("LGP Anisotropic Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Direction-dependent visibility"), PM_STR("Tensor optics"), PM_STR("")},
    {PM_STR("LGP Evanescent Skin"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Surface wave propagation"), PM_STR("Total internal reflection"), PM_STR("")},

    // --- LGP Color Mixing Effects (50-59) ---
    {PM_STR("LGP Color Temperature"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Blackbody radiation gradients"), PM_STR("Planckian locus"), PM_STR("")},
    {PM_STR("LGP RGB Prism"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("RGB component splitting"), PM_STR("Additive mixing separation"), PM_STR("")},
    {PM_STR("LGP Complementary Mixing"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Complementary color gradients"), PM_STR("Color theory"), PM_STR("")},
    {PM_STR("LGP Quantum Colors"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Quantized energy levels"), PM_STR("Discrete spectrum"), PM_STR("")},
    {PM_STR("LGP Doppler Shift"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Red/Blue shift based on velocity"), PM_STR("Relativistic shift"), PM_STR("")},
    {PM_STR("LGP Color Accelerator"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Color cycling with momentum"), PM_STR("Chromatic acceleration"), PM_STR("")},
    {PM_STR("LGP DNA Helix"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Double helix structure"), PM_STR("Biomolecular geometry"), PM_STR("")},
    {PM_STR("LGP Phase Transition"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("State change simulation"), PM_STR("Thermodynamics"), PM_STR("")},
    {PM_STR("LGP Chromatic Aberration"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Lens dispersion edge effects"), PM_STR("Optical error"), PM_STR("")},
    {PM_STR("LGP Perceptual Blend"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Lab color space mixing"), PM_STR("Human perception"), PM_STR("")},

    // --- LGP Novel Physics Effects (60-64) ---
    {PM_STR("LGP Chladni Harmonics"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Resonant nodal patterns"), PM_STR("Acoustic physics"), PM_STR("")},
    {PM_STR("LGP Gravitational Wave Chirp"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Inspiral merger signal"), PM_STR("Astrophysics"), PM_STR("")},
    {PM_STR("LGP Quantum Entanglement"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Correlated state collapse"), PM_STR("Quantum mechanics"), PM_STR("")},
    {PM_STR("LGP Mycelial Network"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN, PM_STR("Fungal network expansion"), PM_STR("Biological growth"), PM_STR("")},
    {PM_STR("LGP Riley Dissonance"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN, PM_STR("Op-art visual vibration"), PM_STR("Perceptual psychology"), PM_STR("")},

    // --- LGP Chromatic Effects (65-67) ---
    {PM_STR("LGP Chromatic Lens"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Simulated lens dispersion"), PM_STR("Cauchy equation"), PM_STR("")},
    {PM_STR("LGP Chromatic Pulse"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Pulsing dispersion wave"), PM_STR("Cauchy equation"), PM_STR("")},
    {PM_STR("LGP Chromatic Interference"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Interfering dispersion patterns"), PM_STR("Cauchy equation + Interference"), PM_STR("")},

    // --- Audio-Reactive Effects (68+) - Phase 2 ---
    {PM_STR("Audio Test"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Audio-reactive spectrum visualization"), PM_STR("RMS energy, 8-band spectrum, beat detection"), PM_STR("")},
    {PM_STR("Beat Pulse"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Beat-synchronized radial pulse from centre"), PM_STR("Tempo tracking, beat detection"), PM_STR("")},
    {PM_STR("Spectrum Bars"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("8-band spectrum analyzer from centre to edge"), PM_STR("Frequency analysis, band energy"), PM_STR("")},
    {PM_STR("Bass Breath"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Organic breathing driven by bass energy"), PM_STR("Bass band tracking, breathing simulation"), PM_STR("")},
    {PM_STR("Audio Waveform"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("True time-domain waveform visualization mirrored from centre"), PM_STR("Waveform samples, time-domain display"), PM_STR("")},
    {PM_STR("Audio Bloom"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Centre bloom pulses triggered by audio transients"), PM_STR("Flux detection, pulse generation, radial decay"), PM_STR("")},
    {PM_STR("LGP Star Burst (Narrative)"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Story-driven starburst with phrase-gated chord coloring"), PM_STR("Narrative conductor, chord harmonics, phrase timing"), PM_STR("LGP Star Burst")},
    {PM_STR("Chord Glow"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Full chord detection showcase with harmonic color mapping"), PM_STR("Chord detection, root note color, quality modulation"), PM_STR("")},
    {PM_STR("Wave Reactive"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Energy-accumulating wave with audio-driven motion"), PM_STR("Beat accumulation, frequency-driven velocity, kaleidoscope integration"), PM_STR("Wave")},

    // --- Perlin-based LGP Effects (77-80) - Audio-reactive noise field patterns ---
    {PM_STR("LGP Perlin Veil"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow drifting noise curtains from centre, audio-driven advection"), PM_STR("Perlin noise fields, flux/beat advection, RMS contrast"), PM_STR("")},
    {PM_STR("LGP Perlin Shocklines"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-driven travelling ridges propagating from centre"), PM_STR("Perlin noise, shockwave propagation, treble sharpness"), PM_STR("")},
    {PM_STR("LGP Perlin Caustics"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sparkling caustic lobes, treble→sparkle, bass→scale"), PM_STR("Multi-octave Perlin noise, caustic focus, spectral modulation"), PM_STR("")},
    {PM_STR("LGP Perlin Interference Weave"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Dual-strip moiré interference, beat→phase, chroma→colour"), PM_STR("Phase-offset Perlin noise, interference patterns, chroma mapping"), PM_STR("")},

    // --- Perlin-based LGP Effects Ambient (81-84) - Time-driven variants ---
    {PM_STR("LGP Perlin Veil Ambient"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow drifting noise curtains from centre, time-driven"), PM_STR("Perlin noise fields, slow drift, breathing contrast"), PM_STR("LGP Perlin Veil")},
    {PM_STR("LGP Perlin Shocklines Ambient"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Time-driven travelling ridges propagating from centre"), PM_STR("Perlin noise, periodic shockwaves, time-based propagation"), PM_STR("LGP Perlin Shocklines")},
    {PM_STR("LGP Perlin Caustics Ambient"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sparkling caustic lobes, time-driven modulation"), PM_STR("Multi-octave Perlin noise, caustic focus, slow parameter drift"), PM_STR("LGP Perlin Caustics")},
    {PM_STR("LGP Perlin Interference Weave Ambient"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Dual-strip moiré interference, time-driven phase"), PM_STR("Phase-offset Perlin noise, interference patterns, slow phase modulation"), PM_STR("LGP Perlin Interference Weave")},

    // --- Perlin Backend Test Effects (85-87) - A/B/C comparison harness ---
    {PM_STR("Perlin Test: FastLED"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("FastLED inoise8 baseline test (TEST)"), PM_STR("FastLED noise, centre-origin mapping, seed + advection"), PM_STR("")},
    {PM_STR("Perlin Test: Emotiscope2 Full"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Emotiscope 2.0 Perlin full-res per-frame test (TEST)"), PM_STR("Seedable Perlin noise, hash-based gradients, octaves, full resolution"), PM_STR("")},
    {PM_STR("Perlin Test: Emotiscope2 Quarter"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Emotiscope 2.0 Perlin quarter-res + interpolation test (TEST)"), PM_STR("Seedable Perlin noise, quarter resolution, periodic refresh, linear interpolation"), PM_STR("")}
};

const uint8_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

// ============================================================================
// Effect Count Parity Validation
// ============================================================================

// Expected number of implemented effects (must match registerAllEffects() return value)
constexpr uint8_t EXPECTED_EFFECT_COUNT = 88;  // 85 base + 3 Perlin backend tests

// Compile-time assertion: metadata must have at least as many entries as implemented effects
// This ensures we can always map effect IDs to metadata (allows for future effects in metadata)
static_assert(PATTERN_METADATA_COUNT >= EXPECTED_EFFECT_COUNT,
              "PATTERN_METADATA_COUNT must be >= EXPECTED_EFFECT_COUNT (metadata must cover all implemented effects)");

// ============================================================================
// Pattern Registry Implementation
// ============================================================================

namespace PatternRegistry {

const PatternMetadata* getPatternMetadata(const char* name) {
    // Search through metadata array
    for (uint8_t i = 0; i < PATTERN_METADATA_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (strcmp_P(name, meta.name) == 0) {
            return &PATTERN_METADATA[i];
        }
    }
    return nullptr;
}

/**
 * @brief Validate and clamp effectId to safe range [0, PATTERN_METADATA_COUNT-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted effect ID.
 * 
 * PatternRegistry uses PATTERN_METADATA array for metadata lookup. If effectId
 * is corrupted (e.g., by memory corruption, invalid input, or uninitialized state),
 * accessing PATTERN_METADATA[effectId] would cause out-of-bounds access and crash.
 * 
 * This validation ensures we always access valid array indices, returning safe
 * default (effect 0) if corruption is detected.
 * 
 * @param effectId Effect ID to validate
 * @return Valid effect ID, defaults to 0 if out of bounds
 */
uint8_t validateEffectId(uint8_t effectId) {
    // Validate effectId is within bounds [0, PATTERN_METADATA_COUNT-1]
    // Use actual array size for safety (static_assert ensures PATTERN_METADATA_COUNT >= EXPECTED_EFFECT_COUNT)
    if (effectId >= PATTERN_METADATA_COUNT) {
        return 0;  // Return safe default (effect 0)
    }
    return effectId;
}

const PatternMetadata* getPatternMetadata(uint8_t index) {
    // Map effect ID to metadata index (stable ID mapping)
    // Effect IDs match v1/v2 registration order exactly
    // Phase 2 parity enforcement: only expose metadata for implemented effects
    // Validate index before access
    uint8_t safe_index = validateEffectId(index);
    if (safe_index >= EXPECTED_EFFECT_COUNT) {
        return nullptr;
    }
    return &PATTERN_METADATA[safe_index];
}

uint8_t getPatternsByFamily(PatternFamily family, uint8_t* output, uint8_t maxOutput) {
    uint8_t count = 0;
    // Only return implemented effect IDs
    for (uint8_t i = 0; i < EXPECTED_EFFECT_COUNT && count < maxOutput; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.family == family) {
            output[count++] = i;
        }
    }
    return count;
}

uint8_t getRelatedPatterns(const char* name, const char** output, uint8_t maxOutput) {
    const PatternMetadata* meta = getPatternMetadata(name);
    if (!meta || !meta->relatedPatterns) {
        return 0;
    }
    // Parse comma-separated list (simplified - assumes small lists)
    // Full implementation would parse the string properly
    uint8_t count = 0;
    // For now, return 0 - full parsing would be implemented here
    return count;
}

void getFamilyName(PatternFamily family, char* buffer, size_t bufferSize) {
    if ((uint8_t)family < 10) {
        strncpy_P(buffer, (char*)pgm_read_ptr(&PATTERN_FAMILY_NAMES[(uint8_t)family]), bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        strncpy(buffer, "Unknown", bufferSize);
    }
}

bool patternInFamily(const char* name, PatternFamily family) {
    const PatternMetadata* meta = getPatternMetadata(name);
    return meta && meta->family == family;
}

uint8_t getPatternCount() {
    // Only count implemented effects (metadata may contain future patterns)
    return EXPECTED_EFFECT_COUNT;
}

uint8_t getFamilyCount(PatternFamily family) {
    uint8_t count = 0;
    // Only count implemented effects (metadata may contain future patterns)
    for (uint8_t i = 0; i < EXPECTED_EFFECT_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.family == family) {
            count++;
        }
    }
    return count;
}

bool isLGPSensitive(uint8_t effectId) {
    // Validate effectId before access
    uint8_t safe_id = validateEffectId(effectId);
    if (safe_id >= EXPECTED_EFFECT_COUNT) {
        return false;
    }
    
    const PatternMetadata* metadata = getPatternMetadata(effectId);
    if (!metadata) {
        return false;
    }
    
    // Check specific effect IDs that are known to be LGP-sensitive
    // These are the effects showing severe regression at Tap B
    if (effectId == 10 || effectId == 13 || effectId == 16 || 
        effectId == 26 || effectId == 32 || effectId == 65 || 
        effectId == 66 || effectId == 67) {
        return true;
    }
    
    // Check if effect is in INTERFERENCE family
    if (metadata->family == PatternFamily::INTERFERENCE) {
        return true;
    }
    
    // Check if effect is in ADVANCED_OPTICAL family with CENTER_ORIGIN tag
    if (metadata->family == PatternFamily::ADVANCED_OPTICAL && 
        metadata->hasTag(PatternTags::CENTER_ORIGIN)) {
        return true;
    }
    
    // Check if effect is in QUANTUM or ORGANIC family with CENTER_ORIGIN and PHYSICS tags
    // These are LGP physics-based effects that need precise amplitude relationships
    if ((metadata->family == PatternFamily::QUANTUM || metadata->family == PatternFamily::ORGANIC) &&
        metadata->hasTag(PatternTags::CENTER_ORIGIN) && 
        metadata->hasTag(PatternTags::PHYSICS)) {
        return true;
    }
    
    return false;
}

bool isStatefulEffect(uint8_t effectId) {
    // Stateful effects read from ctx.leds in previous frame
    // Currently: Confetti (3) and Ripple (8)
    return (effectId == 3 || effectId == 8 || effectId == 24 || effectId == 74);
}

const lightwaveos::plugins::EffectMetadata* getIEffectMetadata(uint8_t effectId) {
    // Validate effectId before access
    uint8_t safe_id = validateEffectId(effectId);
    if (safe_id >= EXPECTED_EFFECT_COUNT) {
        return nullptr;
    }
    
    // Access RendererNode via NodeOrchestrator
    using namespace lightwaveos::nodes;
    RendererNode* renderer = NodeOrchestrator::instance().getRenderer();
    if (!renderer) {
        return nullptr;
    }
    
    lightwaveos::plugins::IEffect* effect = renderer->getEffectInstance(effectId);
    if (!effect) {
        return nullptr;
    }
    
    return &effect->getMetadata();
}

bool hasIEffectMetadata(uint8_t effectId) {
    return getIEffectMetadata(effectId) != nullptr;
}

bool shouldSkipColorCorrection(uint8_t effectId) {
    // Fast path: known sensitive effects
    if (isLGPSensitive(effectId) || isStatefulEffect(effectId)) {
        return true;
    }

    // Check family-based skip logic (getPatternMetadata already validates)
    uint8_t safe_id = validateEffectId(effectId);
    const PatternMetadata* meta = getPatternMetadata(safe_id);
    if (meta) {
        // PHYSICS_BASED effects need precise amplitude for physics simulations
        // MATHEMATICAL effects need exact RGB values for mathematical mappings
        if (meta->family == PatternFamily::PHYSICS_BASED ||
            meta->family == PatternFamily::MATHEMATICAL) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Effect Register Functions (for filtered effect cycling)
// ============================================================================

// Audio-reactive effect IDs (effects that actively use ctx.audio)
// Determined by grep analysis of ctx.audio usage patterns in ieffect/*.cpp
static const uint8_t REACTIVE_EFFECT_IDS[] PROGMEM = {
    6,   // BPM - beat-synced pulsing
    7,   // Wave Ambient - RMS amplitude modulation
    8,   // Ripple - kick/treble triggered, chroma-driven
    11,  // Breathing - RMS breathing, beat-gated pulses
    16,  // Interference Scanner - heavyMid energy, snare boost
    17,  // Wave Collision - heavyBass, snare/hihat triggers
    22,  // Chevron Waves - chroma, snare sharpness
    24,  // Star Burst - full audio pipeline, snare bursts
    33,  // Photonic Crystal - tempo breathing, saliency
    68,  // Audio Test - demo effect
    69,  // Beat Pulse - beat-synced radial pulse
    70,  // Spectrum Bars - 8-band analyzer
    71,  // Bass Breath - bass-driven breathing
    72,  // Audio Waveform - time-domain visualization
    73,  // Audio Bloom - transient-triggered blooms
    74,  // Star Burst Narrative - full musical intelligence
    75,  // Chord Glow - chord detection showcase
    76,  // Wave Reactive - energy accumulation
    77,  // Perlin Veil - audio-driven advection
    78,  // Perlin Shocklines - beat-driven ridges
    79,  // Perlin Caustics - treble/bass modulation
    80   // Perlin Interference Weave - dual-strip moiré
};
static constexpr uint8_t REACTIVE_EFFECT_COUNT = sizeof(REACTIVE_EFFECT_IDS);

bool isAudioReactive(uint8_t effectId) {
    for (uint8_t i = 0; i < REACTIVE_EFFECT_COUNT; i++) {
        if (pgm_read_byte(&REACTIVE_EFFECT_IDS[i]) == effectId) {
            return true;
        }
    }
    return false;
}

uint8_t getReactiveEffectCount() {
    return REACTIVE_EFFECT_COUNT;
}

uint8_t getReactiveEffectId(uint8_t index) {
    if (index >= REACTIVE_EFFECT_COUNT) return 0xFF;
    return pgm_read_byte(&REACTIVE_EFFECT_IDS[index]);
}

uint8_t buildAmbientEffectArray(uint8_t* outputArray, uint8_t maxOutput, uint8_t effectCount) {
    uint8_t count = 0;
    for (uint8_t id = 0; id < effectCount && count < maxOutput; id++) {
        if (!isAudioReactive(id)) {
            outputArray[count++] = id;
        }
    }
    return count;
}

} // namespace PatternRegistry
