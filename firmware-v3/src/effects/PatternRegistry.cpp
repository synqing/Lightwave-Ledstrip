/**
 * Pattern Registry Implementation
 *
 * Provides metadata for all registered patterns.
 * Pattern metadata is keyed by stable EffectId (uint16_t) from effect_ids.h.
 * Lookup is by linear scan of the metadata array.
 */

#include "PatternRegistry.h"
#include "../config/limits.h"
#include "../config/effect_ids.h"
#include "../core/actors/ActorSystem.h"
#include "../core/actors/RendererActor.h"

using namespace lightwaveos;

// ============================================================================
// Pattern Metadata Definitions (PROGMEM)
// ============================================================================

// Helper macros for PROGMEM strings
#define PM_STR(s) (const char*)(s)

// Pattern metadata entries - keyed by stable EffectId (matches OLD_TO_NEW order)
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // --- Core Effects (old 0-12) ---
    {EID_FIRE, PM_STR("Fire"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Realistic fire simulation radiating from centre"), PM_STR("Thermal convection, intensity gradient"), PM_STR("")},
    {EID_OCEAN, PM_STR("Ocean"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Deep ocean wave patterns from centre point"), PM_STR("Wave propagation, depth gradient"), PM_STR("")},
    {EID_PLASMA, PM_STR("Plasma"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Smoothly shifting color plasma"), PM_STR("Perlin noise, continuous flow"), PM_STR("")},
    {EID_CONFETTI, PM_STR("Confetti"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Random colored speckles fading"), PM_STR("Stochastic particles"), PM_STR("")},
    {EID_SINELON, PM_STR("Sinelon"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Bouncing particle with palette trails"), PM_STR("Position-based motion, trail decay"), PM_STR("")},
    {EID_JUGGLE, PM_STR("Juggle"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Multiple colored balls juggling"), PM_STR("Physics-based gravity simulation"), PM_STR("")},
    {EID_BPM, PM_STR("BPM"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-synced pulsing sawtooth waves"), PM_STR("Rhythmic timing"), PM_STR("")},
    {EID_WAVE_AMBIENT, PM_STR("Wave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Simple sine wave propagation"), PM_STR("Oscillation"), PM_STR("")},
    {EID_RIPPLE, PM_STR("Ripple"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Expanding water ripples"), PM_STR("Radial decay"), PM_STR("")},
    {EID_HEARTBEAT, PM_STR("Heartbeat"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Rhythmic cardiac pulsing"), PM_STR("Biological timing"), PM_STR("")},
    {EID_INTERFERENCE, PM_STR("Interference"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN, PM_STR("Basic wave interference"), PM_STR("Wave superposition"), PM_STR("")},
    {EID_BREATHING, PM_STR("Breathing"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow rhythmic brightness pulsing"), PM_STR("Ambient modulation"), PM_STR("")},
    {EID_PULSE, PM_STR("Pulse"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Sharp energy pulses"), PM_STR("Transient signal"), PM_STR("")},

    // --- LGP Interference Effects (old 13-17) ---
    {EID_LGP_BOX_WAVE, PM_STR("LGP Box Wave"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Square wave standing patterns"), PM_STR("Harmonic resonance"), PM_STR("")},
    {EID_LGP_HOLOGRAPHIC, PM_STR("LGP Holographic"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Holographic interference patterns through multi-layer depth"), PM_STR("Multi-layer interference, phase relationships, depth illusion"), PM_STR("LGP Modal Resonance, LGP Interference Scanner")},
    {EID_MODAL_RESONANCE, PM_STR("LGP Modal Resonance"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING, PM_STR("Explores different optical cavity resonance modes"), PM_STR("Cavity modes, standing wave patterns, mode selection"), PM_STR("LGP Holographic")},
    {EID_LGP_INTERFERENCE_SCANNER, PM_STR("LGP Interference Scanner"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Scanning beam with interference fringes"), PM_STR("Phase scanning, fringe projection"), PM_STR("LGP Wave Collision")},
    {EID_LGP_WAVE_COLLISION, PM_STR("LGP Wave Collision"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Colliding wave fronts creating standing nodes"), PM_STR("Wave superposition, constructive/destructive interference"), PM_STR("LGP Interference Scanner")},

    // --- LGP Geometric Effects (old 18-25) ---
    {EID_LGP_DIAMOND_LATTICE, PM_STR("LGP Diamond Lattice"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Interwoven diamond patterns"), PM_STR("Geometric tiling"), PM_STR("")},
    {EID_LGP_HEXAGONAL_GRID, PM_STR("LGP Hexagonal Grid"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Hexagonal cell structure"), PM_STR("Geometric tiling"), PM_STR("")},
    {EID_LGP_SPIRAL_VORTEX, PM_STR("LGP Spiral Vortex"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Rotating spiral arms"), PM_STR("Radial geometry"), PM_STR("")},
    {EID_LGP_SIERPINSKI, PM_STR("LGP Sierpinski"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Fractal triangle generation"), PM_STR("Recursive geometry"), PM_STR("")},
    {EID_CHEVRON_WAVES, PM_STR("LGP Chevron Waves"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("V-shaped wave propagation"), PM_STR("Directional wave"), PM_STR("")},
    {EID_LGP_CONCENTRIC_RINGS, PM_STR("LGP Concentric Rings"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Expanding circular rings"), PM_STR("Radial symmetry"), PM_STR("")},
    {EID_LGP_STAR_BURST, PM_STR("LGP Star Burst"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Explosive radial lines"), PM_STR("Radial projection"), PM_STR("")},
    {EID_LGP_MESH_NETWORK, PM_STR("LGP Mesh Network"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Interconnected node graph"), PM_STR("Network topology"), PM_STR("")},

    // --- LGP Advanced Effects (old 26-33) ---
    {EID_LGP_MOIRE_CURTAINS, PM_STR("LGP Moire Curtains"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::MOIRE, PM_STR("Shifting moire interference layers"), PM_STR("Optical beating"), PM_STR("")},
    {EID_LGP_RADIAL_RIPPLE, PM_STR("LGP Radial Ripple"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Complex radial wave interference"), PM_STR("Wave propagation"), PM_STR("")},
    {EID_LGP_HOLOGRAPHIC_VORTEX, PM_STR("LGP Holographic Vortex"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DEPTH, PM_STR("Deep 3D vortex illusion"), PM_STR("Perspective projection"), PM_STR("")},
    {EID_LGP_EVANESCENT_DRIFT, PM_STR("LGP Evanescent Drift"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Ghostly drifting particles"), PM_STR("Sub-threshold illumination"), PM_STR("")},
    {EID_LGP_CHROMATIC_SHEAR, PM_STR("LGP Chromatic Shear"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Color-splitting shear effect"), PM_STR("Prismatic dispersion"), PM_STR("")},
    {EID_LGP_MODAL_CAVITY, PM_STR("LGP Modal Cavity"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Resonant optical cavity modes"), PM_STR("Harmonics"), PM_STR("")},
    {EID_LGP_FRESNEL_ZONES, PM_STR("LGP Fresnel Zones"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Fresnel lens zone plate pattern"), PM_STR("Diffraction optics"), PM_STR("")},
    {EID_LGP_PHOTONIC_CRYSTAL, PM_STR("LGP Photonic Crystal"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Bandgap structure simulation"), PM_STR("Periodic nanostructure"), PM_STR("")},

    // --- LGP Organic Effects (old 34-39) ---
    {EID_LGP_AURORA_BOREALIS, PM_STR("LGP Aurora Borealis"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Shimmering curtain lights"), PM_STR("Atmospheric ionization"), PM_STR("")},
    {EID_LGP_BIOLUMINESCENT_WAVES, PM_STR("LGP Bioluminescent Waves"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Glowing plankton in waves"), PM_STR("Biological light"), PM_STR("")},
    {EID_LGP_PLASMA_MEMBRANE, PM_STR("LGP Plasma Membrane"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Cellular membrane fluctuations"), PM_STR("Fluid mosaic model"), PM_STR("")},
    {EID_LGP_NEURAL_NETWORK, PM_STR("LGP Neural Network"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Firing synaptic pathways"), PM_STR("Action potentials"), PM_STR("")},
    {EID_LGP_CRYSTALLINE_GROWTH, PM_STR("LGP Crystalline Growth"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN, PM_STR("Growing crystal facets"), PM_STR("Nucleation and growth"), PM_STR("")},
    {EID_LGP_FLUID_DYNAMICS, PM_STR("LGP Fluid Dynamics"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Fluid flow simulation"), PM_STR("Navier-Stokes approximation"), PM_STR("")},

    // --- LGP Quantum Effects (old 40-49) ---
    {EID_LGP_QUANTUM_TUNNELING, PM_STR("LGP Quantum Tunneling"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Particles passing through barriers"), PM_STR("Wave function probability"), PM_STR("")},
    {EID_LGP_GRAVITATIONAL_LENSING, PM_STR("LGP Gravitational Lensing"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Light bending around mass"), PM_STR("General relativity"), PM_STR("")},
    {EID_LGP_TIME_CRYSTAL, PM_STR("LGP Time Crystal"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Periodic structure in time"), PM_STR("Non-equilibrium matter"), PM_STR("")},
    {EID_LGP_SOLITON_WAVES, PM_STR("LGP Soliton Waves"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS | PatternTags::TRAVELING, PM_STR("Self-reinforcing wave packets"), PM_STR("Non-linear dispersion"), PM_STR("")},
    {EID_LGP_METAMATERIAL_CLOAK, PM_STR("LGP Metamaterial Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Invisibility cloak simulation"), PM_STR("Negative refractive index"), PM_STR("")},
    {EID_LGP_GRIN_CLOAK, PM_STR("LGP GRIN Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Gradient index optics"), PM_STR("Variable refractive index"), PM_STR("")},
    {EID_LGP_CAUSTIC_FAN, PM_STR("LGP Caustic Fan"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Focused light caustics"), PM_STR("Ray optics"), PM_STR("")},
    {EID_LGP_BIREFRINGENT_SHEAR, PM_STR("LGP Birefringent Shear"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS | PatternTags::SPECTRAL, PM_STR("Polarization splitting"), PM_STR("Double refraction"), PM_STR("")},
    {EID_LGP_ANISOTROPIC_CLOAK, PM_STR("LGP Anisotropic Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Direction-dependent visibility"), PM_STR("Tensor optics"), PM_STR("")},
    {EID_LGP_EVANESCENT_SKIN, PM_STR("LGP Evanescent Skin"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Surface wave propagation"), PM_STR("Total internal reflection"), PM_STR("")},

    // --- LGP Color Mixing Effects (old 50-59) ---
    {EID_LGP_COLOR_TEMPERATURE, PM_STR("LGP Color Temperature"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Blackbody radiation gradients"), PM_STR("Planckian locus"), PM_STR("")},
    {EID_LGP_RGB_PRISM, PM_STR("LGP RGB Prism"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("RGB component splitting"), PM_STR("Additive mixing separation"), PM_STR("")},
    {EID_LGP_COMPLEMENTARY_MIXING, PM_STR("LGP Complementary Mixing"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Complementary color gradients"), PM_STR("Color theory"), PM_STR("")},
    {EID_LGP_QUANTUM_COLORS, PM_STR("LGP Quantum Colors"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Quantized energy levels"), PM_STR("Discrete spectrum"), PM_STR("")},
    {EID_LGP_DOPPLER_SHIFT, PM_STR("LGP Doppler Shift"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Red/Blue shift based on velocity"), PM_STR("Relativistic shift"), PM_STR("")},
    {EID_LGP_COLOR_ACCELERATOR, PM_STR("LGP Color Accelerator"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Color cycling with momentum"), PM_STR("Chromatic acceleration"), PM_STR("")},
    {EID_LGP_DNA_HELIX, PM_STR("LGP DNA Helix"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Double helix structure"), PM_STR("Biomolecular geometry"), PM_STR("")},
    {EID_LGP_PHASE_TRANSITION, PM_STR("LGP Phase Transition"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("State change simulation"), PM_STR("Thermodynamics"), PM_STR("")},
    {EID_LGP_CHROMATIC_ABERRATION, PM_STR("LGP Chromatic Aberration"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Lens dispersion edge effects"), PM_STR("Optical error"), PM_STR("")},
    {EID_LGP_PERCEPTUAL_BLEND, PM_STR("LGP Perceptual Blend"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN, PM_STR("Lab color space mixing"), PM_STR("Human perception"), PM_STR("")},

    // --- LGP Novel Physics Effects (old 60-64) ---
    {EID_LGP_CHLADNI_HARMONICS, PM_STR("LGP Chladni Harmonics"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::STANDING, PM_STR("Resonant nodal patterns"), PM_STR("Acoustic physics"), PM_STR("")},
    {EID_LGP_GRAVITATIONAL_WAVE_CHIRP, PM_STR("LGP Gravitational Wave Chirp"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Inspiral merger signal"), PM_STR("Astrophysics"), PM_STR("")},
    {EID_LGP_QUANTUM_ENTANGLEMENT, PM_STR("LGP Quantum Entanglement"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS, PM_STR("Correlated state collapse"), PM_STR("Quantum mechanics"), PM_STR("")},
    {EID_LGP_MYCELIAL_NETWORK, PM_STR("LGP Mycelial Network"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN, PM_STR("Fungal network expansion"), PM_STR("Biological growth"), PM_STR("")},
    {EID_LGP_RILEY_DISSONANCE, PM_STR("LGP Riley Dissonance"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN, PM_STR("Op-art visual vibration"), PM_STR("Perceptual psychology"), PM_STR("")},

    // --- LGP Chromatic Effects (old 65-67) ---
    {EID_LGP_CHROMATIC_LENS, PM_STR("LGP Chromatic Lens"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Simulated lens dispersion"), PM_STR("Cauchy equation"), PM_STR("")},
    {EID_LGP_CHROMATIC_PULSE, PM_STR("LGP Chromatic Pulse"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Pulsing dispersion wave"), PM_STR("Cauchy equation"), PM_STR("")},
    {EID_CHROMATIC_INTERFERENCE, PM_STR("LGP Chromatic Interference"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Interfering dispersion patterns"), PM_STR("Cauchy equation + Interference"), PM_STR("")},

    // --- Audio-Reactive Effects (old 68-76) ---
    {EID_LGP_AUDIO_TEST, PM_STR("Audio Test"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Audio-reactive spectrum visualization"), PM_STR("RMS energy, 8-band spectrum, beat detection"), PM_STR("")},
    {EID_LGP_BEAT_PULSE, PM_STR("Beat Pulse"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Beat-synchronized radial pulse from centre"), PM_STR("Tempo tracking, beat detection"), PM_STR("")},
    {EID_LGP_SPECTRUM_BARS, PM_STR("Spectrum Bars"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("8-band spectrum analyzer from centre to edge"), PM_STR("Frequency analysis, band energy"), PM_STR("")},
    {EID_LGP_BASS_BREATH, PM_STR("Bass Breath"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Organic breathing driven by bass energy"), PM_STR("Bass band tracking, breathing simulation"), PM_STR("")},
    {EID_AUDIO_WAVEFORM, PM_STR("Audio Waveform"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("True time-domain waveform visualization mirrored from centre"), PM_STR("Waveform samples, time-domain display"), PM_STR("")},
    {EID_AUDIO_BLOOM, PM_STR("Audio Bloom"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Centre bloom pulses triggered by audio transients"), PM_STR("Flux detection, pulse generation, radial decay"), PM_STR("")},
    {EID_LGP_STAR_BURST_NARRATIVE, PM_STR("LGP Star Burst (Narrative)"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Legacy starburst core with phrase-gated harmonic colour commits"), PM_STR("Narrative phrase state, snare bursts, chord-root gating"), PM_STR("LGP Star Burst")},
    {EID_LGP_CHORD_GLOW, PM_STR("Chord Glow"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Full chord detection showcase with harmonic color mapping"), PM_STR("Chord detection, root note color, quality modulation"), PM_STR("")},
    {EID_WAVE_REACTIVE, PM_STR("Wave Reactive"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Energy-accumulating wave with audio-driven motion"), PM_STR("Beat accumulation, frequency-driven velocity, kaleidoscope integration"), PM_STR("Wave")},

    // --- Perlin-based LGP Effects (old 77-80) - Audio-reactive noise field patterns ---
    {EID_LGP_PERLIN_VEIL, PM_STR("LGP Perlin Veil"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow drifting noise curtains from centre, audio-driven advection"), PM_STR("Perlin noise fields, flux/beat advection, RMS contrast"), PM_STR("")},
    {EID_LGP_PERLIN_SHOCKLINES, PM_STR("LGP Perlin Shocklines"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-driven travelling ridges propagating from centre"), PM_STR("Perlin noise, shockwave propagation, treble sharpness"), PM_STR("")},
    {EID_LGP_PERLIN_CAUSTICS, PM_STR("LGP Perlin Caustics"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sparkling caustic lobes, treble->sparkle, bass->scale"), PM_STR("Multi-octave Perlin noise, caustic focus, spectral modulation"), PM_STR("")},
    {EID_LGP_PERLIN_INTERFERENCE_WEAVE, PM_STR("LGP Perlin Interference Weave"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Dual-strip moire interference, beat->phase, chroma->colour"), PM_STR("Phase-offset Perlin noise, interference patterns, chroma mapping"), PM_STR("")},

    // --- Perlin-based LGP Effects Ambient (old 81-84) - Time-driven variants ---
    {EID_LGP_PERLIN_VEIL_AMBIENT, PM_STR("LGP Perlin Veil Ambient"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Slow drifting noise curtains from centre, time-driven"), PM_STR("Perlin noise fields, slow drift, breathing contrast"), PM_STR("LGP Perlin Veil")},
    {EID_LGP_PERLIN_SHOCKLINES_AMBIENT, PM_STR("LGP Perlin Shocklines Ambient"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Time-driven travelling ridges propagating from centre"), PM_STR("Perlin noise, periodic shockwaves, time-based propagation"), PM_STR("LGP Perlin Shocklines")},
    {EID_LGP_PERLIN_CAUSTICS_AMBIENT, PM_STR("LGP Perlin Caustics Ambient"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sparkling caustic lobes, time-driven modulation"), PM_STR("Multi-octave Perlin noise, caustic focus, slow parameter drift"), PM_STR("LGP Perlin Caustics")},
    {EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT, PM_STR("LGP Perlin Interference Weave Ambient"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Dual-strip moire interference, time-driven phase"), PM_STR("Phase-offset Perlin noise, interference patterns, slow phase modulation"), PM_STR("LGP Perlin Interference Weave")},

    // --- Perlin Backend Test Effects (old 85-87) - A/B/C comparison harness ---
    {EID_LGP_PERLIN_BACKEND_FAST_LED, PM_STR("Perlin Test: FastLED"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("FastLED inoise8 baseline test (TEST)"), PM_STR("FastLED noise, centre-origin mapping, seed + advection"), PM_STR("")},
    {EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL, PM_STR("Perlin Test: Emotiscope2 Full"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Emotiscope 2.0 Perlin full-res per-frame test (TEST)"), PM_STR("Seedable Perlin noise, hash-based gradients, octaves, full resolution"), PM_STR("")},
    {EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER, PM_STR("Perlin Test: Emotiscope2 Quarter"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Emotiscope 2.0 Perlin quarter-res + interpolation test (TEST)"), PM_STR("Seedable Perlin noise, quarter resolution, periodic refresh, linear interpolation"), PM_STR("")},

    // --- Enhanced Audio-Reactive Effects (old 88-97) - Beat-sync, musical intelligence ---
    {EID_BPM_ENHANCED, PM_STR("BPM Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Tempo-locked pulse rings with beat-phase sync"), PM_STR("Beat phase, sub-bass reinforcement, spring speed"), PM_STR("BPM")},
    {EID_BREATHING_ENHANCED, PM_STR("Breathing Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Style-adaptive breathing with harmonic saliency"), PM_STR("RMS breathing, saliency-aware pacing"), PM_STR("Breathing")},
    {EID_CHEVRON_WAVES_ENHANCED, PM_STR("LGP Chevron Waves Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-synced chevron propagation with chroma detail"), PM_STR("Chroma mapping, beat phase, wave sharpening"), PM_STR("LGP Chevron Waves")},
    {EID_LGP_INTERFERENCE_SCANNER_ENHANCED, PM_STR("LGP Interference Scanner Enhanced"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Audio-reactive scan speed with interference fringes"), PM_STR("Phase scanning, snare emphasis"), PM_STR("LGP Interference Scanner")},
    {EID_LGP_PHOTONIC_CRYSTAL_ENHANCED, PM_STR("LGP Photonic Crystal Enhanced"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Harmonic lattice modulation with saliency weighting"), PM_STR("Harmonic saliency, lattice shimmer"), PM_STR("LGP Photonic Crystal")},
    {EID_LGP_SPECTRUM_DETAIL, PM_STR("LGP Spectrum Detail"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("64-bin spectrum detail, bass at centre and treble at edges"), PM_STR("Logarithmic bin mapping, palette bands"), PM_STR("Spectrum Bars")},
    {EID_LGP_SPECTRUM_DETAIL_ENHANCED, PM_STR("LGP Spectrum Detail Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Saliency-weighted spectrum detail with adaptive emphasis"), PM_STR("Musical saliency, adaptive spectrum weighting"), PM_STR("LGP Spectrum Detail")},
    {EID_LGP_STAR_BURST_ENHANCED, PM_STR("LGP Star Burst Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Beat-triggered star bursts with sub-bass boost"), PM_STR("Beat phase, sub-bass burst, chroma colour"), PM_STR("LGP Star Burst")},
    {EID_LGP_WAVE_COLLISION_ENHANCED, PM_STR("LGP Wave Collision Enhanced"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Audio-driven wave collision with sub-bass boost"), PM_STR("Snare collisions, beat phase, chroma weighting"), PM_STR("LGP Wave Collision")},
    {EID_RIPPLE_ENHANCED, PM_STR("Ripple Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-sync ripple propagation with musical intelligence"), PM_STR("Beat sync, style-adaptive, harmonic saliency, centre-origin"), PM_STR("Ripple")},

    // --- old 98: Snapwave Linear (was Audio Bloom Parity) ---
    {EID_SNAPWAVE_LINEAR, PM_STR("Audio Bloom Parity"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("SB 4.1.1 bloom parity with centre smear"), PM_STR("Chroma-weighted palette hue, centre smear, edge fade"), PM_STR("Audio Bloom")},

    // --- old 99: Trinity Test (was Audio Waveform Parity) ---
    {EID_TRINITY_TEST, PM_STR("Audio Waveform Parity"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("SB 3.1.0 waveform parity with MOOD smoothing"), PM_STR("Waveform history, palette chroma hue, centre-origin"), PM_STR("Audio Waveform")},

    // --- Palette Auto-Cycle (old 100) ---
    {EID_LGP_HOLOGRAPHIC_AUTO_CYCLE, PM_STR("LGP Holographic Auto-Cycle"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Holographic effect with internal palette auto-cycling"), PM_STR("Multi-layer interference, palette cycling, depth illusion"), PM_STR("LGP Holographic")},

    // --- ES v1.1 Reference Shows (old 101-105) ---
    {EID_ES_ANALOG, PM_STR("ES Analog (Ref)"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: VU dot display"), PM_STR("VU level, centre-origin dot"), PM_STR("")},
    {EID_ES_SPECTRUM, PM_STR("ES Spectrum (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: 64-bin spectrum strip"), PM_STR("Spectrogram magnitude, HSV gradient"), PM_STR("")},
    {EID_ES_OCTAVE, PM_STR("ES Octave (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: chromagram strip"), PM_STR("Chromagram magnitude, HSV gradient"), PM_STR("")},
    {EID_ES_BLOOM, PM_STR("ES Bloom (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: chroma bloom"), PM_STR("Chromagram sampling, squared response"), PM_STR("")},
    {EID_ES_WAVEFORM, PM_STR("ES Waveform (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: waveform strip"), PM_STR("Waveform history, low-pass smoothing, HSV gradient"), PM_STR("")},

    // --- ES tuned ports (old 106-108) ---
    {EID_RIPPLE_ES_TUNED, PM_STR("Ripple (ES tuned)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-locked ripples tuned for ES v1.1 audio backend"), PM_STR("Beat strength + flux accents, FFT sub-bass/treble shaping"), PM_STR("Ripple")},
    {EID_HEARTBEAT_ES_TUNED, PM_STR("Heartbeat (ES tuned)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Beat-locked lub-dub heartbeat tuned for ES v1.1 audio backend"), PM_STR("Beat tick + flux accents, chroma-anchored palette hue"), PM_STR("Heartbeat")},
    {EID_LGP_HOLOGRAPHIC_ES_TUNED, PM_STR("LGP Holographic (ES tuned)"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Musically driven holographic depth layers tuned for ES v1.1 audio backend"), PM_STR("Band energy voicing, beat phase ratios, flux refraction accents, chroma-anchored colour"), PM_STR("LGP Holographic")},

    // --- Sensory Bridge Reference Shows (old 109) ---
    {EID_SB_WAVEFORM310, PM_STR("SB Waveform (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sensory Bridge 3.1.0 reference: waveform mode"), PM_STR("Waveform history, MOOD smoothing, note chromagram -> colour summation"), PM_STR("Audio Waveform, ES Waveform (Ref)")},

    // --- Beat Pulse Family (old 110-112) ---
    {EID_BEAT_PULSE_STACK, PM_STR("Beat Pulse (Stack)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("UI preview parity: static palette gradient with beat-driven white push"), PM_STR("Beat envelope, white push, static centre-origin gradient"), PM_STR("Beat Pulse")},
    {EID_BEAT_PULSE_SHOCKWAVE, PM_STR("Beat Pulse (Shockwave)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Canonical shockwave: ring travels outward from centre on beat"), PM_STR("Amplitude-driven ring position, HTML parity envelope, white push"), PM_STR("Beat Pulse (Stack), Beat Pulse")},
    {EID_RETIRED_112, PM_STR("Beat Pulse (Shockwave In)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Inward shockwave: ring travels edge->centre on beat"), PM_STR("Amplitude-driven inward ring position, HTML parity envelope, white push"), PM_STR("Beat Pulse (Shockwave)")},

    // --- Beat Pulse Family continued (old 113-121) ---
    {EID_BEAT_PULSE_VOID, PM_STR("Beat Pulse (Void)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Hard detonation in darkness: palette ring against black"), PM_STR("Parity ring maths, zero base brightness, palette-coloured detonation"), PM_STR("Beat Pulse (Stack)")},
    {EID_BEAT_PULSE_RESONANT, PM_STR("Beat Pulse (Resonant)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Double ring contracting inward: sharp attack + warm resonant body"), PM_STR("Dual ring inward contraction, attack/body separation, differential decay"), PM_STR("Beat Pulse (Shockwave In)")},
    {EID_BEAT_PULSE_RIPPLE, PM_STR("Beat Pulse (Ripple)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Cascading implosion ripples: up to 3 rings converge on centre"), PM_STR("3-slot ring buffer, independent ring decay, inward contraction"), PM_STR("Beat Pulse (Resonant)")},
    {EID_BEAT_PULSE_SHOCKWAVE_CASCADE, PM_STR("Beat Pulse (Shockwave Cascade)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Outward pressure wave with trailing echo rings"), PM_STR("Primary + 2 echo rings, fixed offset trailing, outward expansion"), PM_STR("Beat Pulse (Shockwave)")},
    {EID_BEAT_PULSE_SPECTRAL, PM_STR("Beat Pulse (Spectral)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Three frequency-driven rings: bass outer, mid middle, treble centre"), PM_STR("Continuous frequency response, smoothed band tracking, spatial spectrum mapping"), PM_STR("Spectrum Bars")},
    {EID_BEAT_PULSE_SPECTRAL_PULSE, PM_STR("Beat Pulse (Spectral Pulse)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Stationary zones pulsing by frequency band"), PM_STR("Fixed zone mapping, soft crossfade boundaries, continuous spectral response"), PM_STR("Beat Pulse (Spectral)")},
    {EID_BEAT_PULSE_BREATHE, PM_STR("Beat Pulse (Breathe)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Warm whole-strip amplitude pump with centre-weighted glow"), PM_STR("No ring shape, centre-biased amplitude, slower decay for sustain"), PM_STR("Beat Pulse (Stack), Breathing")},
    {EID_BEAT_PULSE_LGP_INTERFERENCE, PM_STR("Beat Pulse (LGP Interference)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING, PM_STR("Dual-strip interference: standing waves exploit LGP optics"), PM_STR("Phase-offset strip driving, spatial frequency control, anti-phase/quadrature modes"), PM_STR("LGP Chladni Harmonics, Modal Resonance")},
    {EID_BEAT_PULSE_BLOOM, PM_STR("Beat Pulse (Bloom)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Beat Pulse transport bloom with beat-envelope driven injection"), PM_STR("Beat-gated centre injection, raw-time transport/advection, bloom-style trail persistence and post shaping."), PM_STR("Beat Pulse (Stack), Beat Pulse (Ripple), Transport/Advection/Trails")},

    // --- Transport / Parity (old 122-123) ---
    {EID_BLOOM_PARITY, PM_STR("Bloom (Parity)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Bloom-style subpixel transport (liquid trails)"), PM_STR("Stateful HDR-ish history buffer advected outward by fractional offsets (subpixel), with beat+flux centre injection."), PM_STR("Transport/Advection/Trails")},
    {EID_KURAMOTO_TRANSPORT, PM_STR("Kuramoto Transport"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Invisible oscillator field -> event-driven light transport"), PM_STR("80-oscillator Kuramoto with nonlocal coupling, RK2 integration, phase-slip/coherence-edge injection into Bloom-style transport buffer. Audio steers regime, not pixels."), PM_STR("Dynamical System/Emergence/Transport")},

    // --- Holographic Variants Pack (old 124-133) ---
    {EID_LGP_OPAL_FILM, PM_STR("LGP Opal Film"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::DEPTH | PatternTags::PHYSICS, PM_STR("Iridescent thin-film bands drifting from centre"), PM_STR("Thin-film interference, thickness waves, pearlescent lift"), PM_STR("LGP Holographic, LGP Chromatic Interference")},
    {EID_LGP_GRATING_SCAN, PM_STR("LGP Grating Scan"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Spectral scan highlight sweeping from centre"), PM_STR("Diffraction grating dispersion, moving incidence angle"), PM_STR("LGP Chromatic Lens, LGP Chromatic Pulse")},
    {EID_LGP_STRESS_GLASS, PM_STR("LGP Stress Glass"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Polarised stress fringes with slow analyser rotation"), PM_STR("Birefringence, phase retardation fringes"), PM_STR("LGP Birefringent Shear")},
    {EID_LGP_MOIRE_SILK, PM_STR("LGP Moire Silk"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Large moire beat envelopes with fine ribbing"), PM_STR("Two-lattice interference, beat envelope modulation"), PM_STR("LGP Moire Curtains")},
    {EID_LGP_CAUSTIC_SHARDS, PM_STR("LGP Caustic Shards"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Interference field punctuated by prismatic glints"), PM_STR("Caustic concentration, sharp specular ridges"), PM_STR("LGP Caustic Fan, LGP Perlin Caustics")},
    {EID_LGP_PARALLAX_DEPTH, PM_STR("LGP Parallax Depth"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::DEPTH, PM_STR("Two-layer field with subtle parallax between strips"), PM_STR("Phase-offset layers, depth separation"), PM_STR("LGP Holographic, LGP Holographic Vortex")},
    {EID_LGP_STRESS_GLASS_MELT, PM_STR("LGP Stress Glass (Melt)"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Photoelastic fringes with phase-locked wings"), PM_STR("Birefringence, phase retardation, wing melt"), PM_STR("LGP Stress Glass")},
    {EID_LGP_GRATING_SCAN_BREAKUP, PM_STR("LGP Grating Scan (Breakup)"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Diffraction scan with controlled halo breakup"), PM_STR("Diffraction grating dispersion, halo spatter decay"), PM_STR("LGP Grating Scan")},
    {EID_LGP_WATER_CAUSTICS, PM_STR("LGP Water Caustics"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Ray-envelope caustic filaments with cusp spikes"), PM_STR("Ray mapping, density blow-ups, caustic sheet"), PM_STR("LGP Caustic Fan, LGP Caustic Shards")},
    {EID_LGP_SCHLIEREN_FLOW, PM_STR("LGP Schlieren Flow"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Knife-edge gradient flow with heat-haze shimmer"), PM_STR("Gradient response, refractive index flow"), PM_STR("LGP Stress Glass")},

    // --- Reaction Diffusion (old 134-135) ---
    {EID_LGP_REACTION_DIFFUSION, PM_STR("LGP Reaction Diffusion"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Gray-Scott slime with self-replicating dynamics"), PM_STR("Reaction-diffusion, feed/kill balance"), PM_STR("LGP Mycelial Network")},
    {EID_LGP_REACTION_DIFFUSION_TRIANGLE, PM_STR("LGP RD Triangle"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Front wedge isolation from reaction-diffusion field"), PM_STR("Thresholded fronts, triangle envelope, edge pins"), PM_STR("LGP Reaction Diffusion")},

    // --- Shape Bangers Pack (old 136-146) ---
    {EID_LGP_TALBOT_CARPET, PM_STR("LGP Talbot Carpet"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Self-imaging lattice rug (near-field diffraction vibe)"), PM_STR("Fresnel harmonic sum, Talbot self-imaging, grating pitch"), PM_STR("LGP Holographic")},
    {EID_LGP_AIRY_COMET, PM_STR("LGP Airy Comet"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Self-accelerating comet with trailing lobes"), PM_STR("Airy beam parabolic motion, oscillatory tail lobes"), PM_STR("LGP Evanescent Drift")},
    {EID_LGP_MOIRE_CATHEDRAL, PM_STR("LGP Moire Cathedral"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::STANDING, PM_STR("Interference arches from close gratings (giant beats)"), PM_STR("Two-grating moire, beat envelopes, cathedral ribs"), PM_STR("LGP Moire Silk, LGP Moire Curtains")},
    {EID_LGP_SUPERFORMULA_GLYPH, PM_STR("LGP Living Glyph"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Superformula sigils (morphing supershapes)"), PM_STR("Superformula r(phi,m,n1,n2,n3), slow morph, distance-to-curve band"), PM_STR("LGP Diamond Lattice")},
    {EID_LGP_SPIROGRAPH_CROWN, PM_STR("LGP Spirograph Crown"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Hypotrochoid crown loops (gear-flower royal seal)"), PM_STR("Hypotrochoid radius projection, facet sparkle"), PM_STR("LGP Concentric Rings")},
    {EID_LGP_ROSE_BLOOM, PM_STR("LGP Rose Bloom"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Rhodonea petals (geometric bloom)"), PM_STR("Rose curve cos(k*theta), petal count drift, opening bloom modulation"), PM_STR("LGP Spirograph Crown")},
    {EID_LGP_HARMONOGRAPH_HALO, PM_STR("LGP Harmonograph Halo"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Lissajous orbitals (aura loops, premium calm)"), PM_STR("Integer-ratio Lissajous, orbit radius projection, gentle energy pulse"), PM_STR("LGP Spiral Vortex")},
    {EID_LGP_RULE30_CATHEDRAL, PM_STR("LGP Rule 30 Cathedral"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Elementary CA textile (triangles + chaos + ribs)"), PM_STR("Rule 30 cellular automaton, binary-to-textile blur, neighbourhood tinting"), PM_STR("LGP Mycelial Network")},
    {EID_LGP_LANGTON_HIGHWAY, PM_STR("LGP Langton Highway"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Emergent order reveal (ant to chaos to highway) projected to 1D"), PM_STR("Langton ant 64x64 grid, diagonal slice projection, ant spark"), PM_STR("LGP Rule 30 Cathedral")},
    {EID_LGP_CYMATIC_LADDER, PM_STR("LGP Cymatic Ladder"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Standing-wave nodes/antinodes sculpted into LGP glass"), PM_STR("Standing wave harmonics 2-8, node/antinode sculpture"), PM_STR("LGP Chladni Harmonics")},
    {EID_LGP_MACH_DIAMONDS, PM_STR("LGP Mach Diamonds"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Shock-diamond jewellery (standing shock-cell pulses)"), PM_STR("Triangle-wave shock cells, diamond breathing, jewel tones"), PM_STR("LGP Cymatic Ladder")},

    // --- LGP Holy Shit Bangers Pack (old 147-151) ---
    {EID_LGP_CHIMERA_CROWN, PM_STR("Chimera Crown"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Coherent and incoherent domains (sync fracture line)"), PM_STR("Kuramoto-Sakaguchi nonlocal coupling, local order parameter"), PM_STR("")},
    {EID_LGP_CATASTROPHE_CAUSTICS, PM_STR("Catastrophe Caustics"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Ray-envelope filaments (focus pull + cusp spark)"), PM_STR("1D ray histogram, lens thickness field, caustic concentration"), PM_STR("LGP Caustic Fan, LGP Water Caustics")},
    {EID_LGP_HYPERBOLIC_PORTAL, PM_STR("Hyperbolic Portal"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Edge densification via atanh(r) (Poincare vibe)"), PM_STR("Hyperbolic stretch, multi-band ribs, centre calm"), PM_STR("")},
    {EID_LGP_LORENZ_RIBBON, PM_STR("Lorenz Ribbon"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Chaotic attractor ribbon (never repeats the same way twice)"), PM_STR("Lorenz ODE trail, radial projection, age fade"), PM_STR("")},
    {EID_LGP_IFS_BIO_RELIC, PM_STR("IFS Botanical Relic"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Mirrored IFS growth (fractal botany in glass)"), PM_STR("Barnsley fern IFS, radial histogram, vein specular"), PM_STR("LGP Crystalline Growth")},

    // --- Experimental Audio Pack (old 152-161) ---
    {EID_LGP_FLUX_RIFT, PM_STR("LGP Flux Rift"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Transient flux opens a travelling centre-out rift"), PM_STR("Fast flux envelope, beat pulse coupling, radial ring pressure"), PM_STR("Beat Pulse (Shockwave), Audio Bloom")},
    {EID_LGP_BEAT_PRISM, PM_STR("LGP Beat Prism"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Beat-front prism rays with edgeward pressure travel"), PM_STR("Beat-strength front, treble prism modulation, radial ray field"), PM_STR("Beat Pulse (Stack), LGP Star Burst")},
    {EID_LGP_HARMONIC_TIDE, PM_STR("LGP Harmonic Tide"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Chord-anchored tidal bands with centre-held continuity"), PM_STR("Chord/root anchoring, harmonic saliency weighting, dual-wave tide"), PM_STR("Chord Glow, Breathing")},
    {EID_LGP_BASS_QUAKE, PM_STR("LGP Bass Quake"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Sub-bass compression waves with outward shock release"), PM_STR("Heavy-bass envelope, beat-coupled shock ring, centre pressure core"), PM_STR("Bass Breath, Beat Pulse (Shockwave Cascade)")},
    {EID_LGP_TREBLE_NET, PM_STR("LGP Treble Net"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::STANDING | PatternTags::SPECTRAL, PM_STR("Timbral shimmer lattice with edge-biased spectral filaments"), PM_STR("Treble/timbral envelope, interference net, shimmer burst accents"), PM_STR("LGP Interference Scanner, LGP Chladni Harmonics")},
    {EID_LGP_RHYTHMIC_GATE, PM_STR("LGP Rhythmic Gate"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-gated lattice shutters with travelling pulse seams"), PM_STR("Rhythmic saliency gate, moving bar field, beat seam projection"), PM_STR("BPM Enhanced, Chevron Waves Enhanced")},
    {EID_LGP_SPECTRAL_KNOT, PM_STR("LGP Spectral Knot"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Frequency-balance knot fields crossing in mirrored rings"), PM_STR("Low/mid/high balance knotting, crossing radial rings, weave envelope"), PM_STR("Spectrum Bars, Beat Pulse (Spectral)")},
    {EID_LGP_SALIENCY_BLOOM, PM_STR("LGP Saliency Bloom"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Overall novelty drives expanding bloom radius and density"), PM_STR("Overall saliency envelope, beat-assisted bloom radius, centre glow bed"), PM_STR("Audio Bloom, Wave Reactive")},
    {EID_LGP_TRANSIENT_LATTICE, PM_STR("LGP Transient Lattice"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Snare and flux impacts drive a decaying interference scaffold"), PM_STR("Transient memory field, impact ring launch, dual-frequency lattice"), PM_STR("Ripple Enhanced, LGP Wave Collision")},
    {EID_LGP_WAVELET_MIRROR, PM_STR("LGP Wavelet Mirror"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Waveform crest mirroring with beat-travel ridge reinforcement"), PM_STR("Waveform crest sampling, mirrored radial mapping, beat-ridge overlay"), PM_STR("Audio Waveform, Snapwave")},

    // --- Showpiece Pack 3 ---
    {EID_LGP_TIME_REVERSAL_MIRROR, PM_STR("Time-Reversal Mirror"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("1D damped wave recorded then replayed backwards"), PM_STR("Damped wave equation, record/playback buffer, time-reversal symmetry"), PM_STR("")},
    {EID_LGP_KDV_SOLITON_PAIR, PM_STR("KdV Soliton Pair"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Two soliton pulses that pass through each other"), PM_STR("Korteweg-de Vries equation, sech^2 soliton profiles, elastic collision"), PM_STR("")},
    {EID_LGP_GOLD_CODE_SPECKLE, PM_STR("Gold Code Speckle"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("CDMA Gold-code pseudo-random speckle field"), PM_STR("Gold code LFSR, correlation peaks, spread-spectrum shimmer"), PM_STR("")},
    {EID_LGP_QUASICRYSTAL_LATTICE, PM_STR("Quasicrystal Lattice"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Penrose-tiling aperiodic lattice shimmer"), PM_STR("5-fold rotational symmetry, Fibonacci spacing, diffraction peaks"), PM_STR("")},
    {EID_LGP_FRESNEL_CAUSTIC_SWEEP, PM_STR("Fresnel Caustic Sweep"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Fresnel integral caustic envelope sweeping outward"), PM_STR("Fresnel integrals C(x)/S(x), Cornu spiral projection, caustic focus"), PM_STR("")},
    {EID_LGP_TIME_REVERSAL_MIRROR_AR, PM_STR("Time-Reversal Mirror AR"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS | PatternTags::SPECTRAL, PM_STR("Audio-reactive time-reversal with beat-triggered playback"), PM_STR("Beat-gated record/play, audio envelope modulation, spectral colouring"), PM_STR("Time-Reversal Mirror")},
    {EID_LGP_TIME_REVERSAL_MIRROR_MOD1, PM_STR("Time-Reversal Mirror Mod1"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Time-reversal with dispersive medium (frequency-dependent speed)"), PM_STR("Dispersive wave equation, chromatic spreading on replay, focus inversion"), PM_STR("Time-Reversal Mirror")},
    {EID_LGP_TIME_REVERSAL_MIRROR_MOD2, PM_STR("Time-Reversal Mirror Mod2"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Time-reversal with nonlinear medium (amplitude-dependent speed)"), PM_STR("Nonlinear wave, shock formation on forward, shock healing on reversal"), PM_STR("Time-Reversal Mirror")},
    {EID_LGP_TIME_REVERSAL_MIRROR_MOD3, PM_STR("Time-Reversal Mirror Mod3"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Time-reversal with lossy medium (absorption on forward, gain on replay)"), PM_STR("Lossy wave equation, exponential decay forward, exponential gain reverse"), PM_STR("Time-Reversal Mirror")}
};

const uint16_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

// ============================================================================
// Effect Count Validation
// ============================================================================

constexpr uint16_t EXPECTED_EFFECT_COUNT = lightwaveos::limits::MAX_EFFECTS;

// ============================================================================
// Pattern Registry Implementation
// ============================================================================

namespace PatternRegistry {

const PatternMetadata* getPatternMetadata(const char* name) {
    for (uint16_t i = 0; i < PATTERN_METADATA_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (strcmp_P(name, meta.name) == 0) {
            return &PATTERN_METADATA[i];
        }
    }
    return nullptr;
}

const PatternMetadata* getPatternMetadata(EffectId id) {
    for (uint16_t i = 0; i < PATTERN_METADATA_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.id == id) {
            return &PATTERN_METADATA[i];
        }
    }
    return nullptr;
}

EffectId validateEffectId(EffectId effectId) {
    if (getPatternMetadata(effectId) != nullptr) return effectId;
    return lightwaveos::INVALID_EFFECT_ID;
}

uint16_t getPatternsByFamily(PatternFamily family, EffectId* output, uint16_t maxOutput) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < PATTERN_METADATA_COUNT && count < maxOutput; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.family == family) {
            output[count++] = meta.id;
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

uint16_t getPatternCount() {
    return PATTERN_METADATA_COUNT;
}

uint16_t getFamilyCount(PatternFamily family) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < PATTERN_METADATA_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.family == family) {
            count++;
        }
    }
    return count;
}

bool isLGPSensitive(EffectId effectId) {
    const PatternMetadata* metadata = getPatternMetadata(effectId);
    if (!metadata) return false;

    // Known LGP-sensitive effects by stable ID
    if (effectId == EID_INTERFERENCE || effectId == EID_LGP_BOX_WAVE ||
        effectId == EID_LGP_INTERFERENCE_SCANNER || effectId == EID_LGP_MOIRE_CURTAINS ||
        effectId == EID_LGP_FRESNEL_ZONES || effectId == EID_LGP_CHROMATIC_LENS ||
        effectId == EID_LGP_CHROMATIC_PULSE || effectId == EID_CHROMATIC_INTERFERENCE) {
        return true;
    }

    // Check if effect is in INTERFERENCE family
    if (metadata->family == PatternFamily::INTERFERENCE) return true;

    // Check if effect is in ADVANCED_OPTICAL family with CENTER_ORIGIN tag
    if (metadata->family == PatternFamily::ADVANCED_OPTICAL &&
        metadata->hasTag(PatternTags::CENTER_ORIGIN)) {
        return true;
    }

    // Check if effect is in QUANTUM or ORGANIC family with CENTER_ORIGIN and PHYSICS tags
    if ((metadata->family == PatternFamily::QUANTUM || metadata->family == PatternFamily::ORGANIC) &&
        metadata->hasTag(PatternTags::CENTER_ORIGIN) &&
        metadata->hasTag(PatternTags::PHYSICS)) {
        return true;
    }

    return false;
}

bool isStatefulEffect(EffectId effectId) {
    // Stateful effects read from ctx.leds in previous frame
    return (effectId == EID_CONFETTI || effectId == EID_RIPPLE ||
            effectId == EID_LGP_STAR_BURST || effectId == EID_LGP_STAR_BURST_NARRATIVE);
}

const lightwaveos::plugins::EffectMetadata* getIEffectMetadata(EffectId effectId) {
    using namespace lightwaveos::actors;
    RendererActor* renderer = ActorSystem::instance().getRenderer();
    if (!renderer) return nullptr;

    lightwaveos::plugins::IEffect* effect = renderer->getEffectInstance(effectId);
    if (!effect) return nullptr;

    return &effect->getMetadata();
}

bool hasIEffectMetadata(EffectId effectId) {
    return getIEffectMetadata(effectId) != nullptr;
}

bool shouldSkipColorCorrection(EffectId effectId) {
    // Fast path: known sensitive effects
    if (isLGPSensitive(effectId) || isStatefulEffect(effectId)) {
        return true;
    }

    // Check family-based skip logic
    const PatternMetadata* meta = getPatternMetadata(effectId);
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

// Audio-reactive effect IDs using stable EffectId constants
static const EffectId REACTIVE_EFFECT_IDS[] PROGMEM = {
    EID_JUGGLE,                              // old   5: Juggle
    EID_BPM,                                 // old   6: BPM
    EID_WAVE_AMBIENT,                        // old   7: Wave
    EID_RIPPLE,                              // old   8: Ripple
    EID_RIPPLE_ES_TUNED,                     // old 106: Ripple (ES tuned)
    EID_HEARTBEAT,                           // old   9: Heartbeat
    EID_HEARTBEAT_ES_TUNED,                  // old 107: Heartbeat (ES tuned)
    EID_BREATHING,                           // old  11: Breathing
    EID_LGP_INTERFERENCE_SCANNER,            // old  16: Interference Scanner
    EID_LGP_WAVE_COLLISION,                  // old  17: Wave Collision
    EID_LGP_HOLOGRAPHIC_ES_TUNED,            // old 108: LGP Holographic (ES tuned)
    EID_CHEVRON_WAVES,                       // old  22: Chevron Waves
    EID_LGP_STAR_BURST,                      // old  24: Star Burst
    EID_LGP_PHOTONIC_CRYSTAL,                // old  33: Photonic Crystal
    EID_LGP_AUDIO_TEST,                      // old  68: Audio Test
    EID_LGP_BEAT_PULSE,                      // old  69: Beat Pulse
    EID_LGP_SPECTRUM_BARS,                   // old  70: Spectrum Bars
    EID_LGP_BASS_BREATH,                     // old  71: Bass Breath
    EID_AUDIO_WAVEFORM,                      // old  72: Audio Waveform
    EID_AUDIO_BLOOM,                         // old  73: Audio Bloom
    EID_LGP_STAR_BURST_NARRATIVE,            // old  74: Star Burst Narrative
    EID_LGP_CHORD_GLOW,                      // old  75: Chord Glow
    EID_WAVE_REACTIVE,                       // old  76: Wave Reactive
    EID_LGP_PERLIN_VEIL,                     // old  77: Perlin Veil
    EID_LGP_PERLIN_SHOCKLINES,              // old  78: Perlin Shocklines
    EID_LGP_PERLIN_CAUSTICS,                 // old  79: Perlin Caustics
    EID_LGP_PERLIN_INTERFERENCE_WEAVE,       // old  80: Perlin Interference Weave
    EID_BPM_ENHANCED,                        // old  88: BPM Enhanced
    EID_BREATHING_ENHANCED,                  // old  89: Breathing Enhanced
    EID_CHEVRON_WAVES_ENHANCED,              // old  90: Chevron Waves Enhanced
    EID_LGP_INTERFERENCE_SCANNER_ENHANCED,   // old  91: Interference Scanner Enhanced
    EID_LGP_PHOTONIC_CRYSTAL_ENHANCED,       // old  92: Photonic Crystal Enhanced
    EID_LGP_SPECTRUM_DETAIL,                 // old  93: Spectrum Detail
    EID_LGP_SPECTRUM_DETAIL_ENHANCED,        // old  94: Spectrum Detail Enhanced
    EID_LGP_STAR_BURST_ENHANCED,             // old  95: Star Burst Enhanced
    EID_LGP_WAVE_COLLISION_ENHANCED,         // old  96: Wave Collision Enhanced
    EID_RIPPLE_ENHANCED,                     // old  97: Ripple Enhanced
    EID_SNAPWAVE_LINEAR,                     // old  98: Audio Bloom Parity
    EID_TRINITY_TEST,                        // old  99: Audio Waveform Parity
    EID_LGP_HOLOGRAPHIC_AUTO_CYCLE,          // old 100: LGP Holographic Auto-Cycle
    EID_ES_ANALOG,                           // old 101: ES Analog (Ref)
    EID_ES_SPECTRUM,                         // old 102: ES Spectrum (Ref)
    EID_ES_OCTAVE,                           // old 103: ES Octave (Ref)
    EID_ES_BLOOM,                            // old 104: ES Bloom (Ref)
    EID_ES_WAVEFORM,                         // old 105: ES Waveform (Ref)
    EID_SB_WAVEFORM310,                      // old 109: SB Waveform (Ref)
    EID_BEAT_PULSE_STACK,                    // old 110: Beat Pulse (Stack)
    EID_BEAT_PULSE_SHOCKWAVE,                // old 111: Beat Pulse (Shockwave)
    EID_RETIRED_112,                         // old 112: Beat Pulse (Shockwave In) RETIRED
    EID_BEAT_PULSE_VOID,                     // old 113: Beat Pulse (Void)
    EID_BEAT_PULSE_RESONANT,                 // old 114: Beat Pulse (Resonant)
    EID_BEAT_PULSE_RIPPLE,                   // old 115: Beat Pulse (Ripple)
    EID_BEAT_PULSE_SHOCKWAVE_CASCADE,        // old 116: Beat Pulse (Shockwave Cascade)
    EID_BEAT_PULSE_SPECTRAL,                 // old 117: Beat Pulse (Spectral)
    EID_BEAT_PULSE_SPECTRAL_PULSE,           // old 118: Beat Pulse (Spectral Pulse)
    EID_BEAT_PULSE_BREATHE,                  // old 119: Beat Pulse (Breathe)
    EID_BEAT_PULSE_LGP_INTERFERENCE,         // old 120: Beat Pulse (LGP Interference)
    EID_BEAT_PULSE_BLOOM,                    // old 121: Beat Pulse (Bloom)
    EID_BLOOM_PARITY,                        // old 122: Bloom (Parity)
    EID_KURAMOTO_TRANSPORT,                  // old 123: Kuramoto Transport
    EID_LGP_FLUX_RIFT,                       // old 152: LGP Flux Rift
    EID_LGP_BEAT_PRISM,                      // old 153: LGP Beat Prism
    EID_LGP_HARMONIC_TIDE,                   // old 154: LGP Harmonic Tide
    EID_LGP_BASS_QUAKE,                      // old 155: LGP Bass Quake
    EID_LGP_TREBLE_NET,                      // old 156: LGP Treble Net
    EID_LGP_RHYTHMIC_GATE,                   // old 157: LGP Rhythmic Gate
    EID_LGP_SPECTRAL_KNOT,                   // old 158: LGP Spectral Knot
    EID_LGP_SALIENCY_BLOOM,                  // old 159: LGP Saliency Bloom
    EID_LGP_TRANSIENT_LATTICE,               // old 160: LGP Transient Lattice
    EID_LGP_WAVELET_MIRROR                   // old 161: LGP Wavelet Mirror
};
static constexpr uint8_t REACTIVE_EFFECT_COUNT = sizeof(REACTIVE_EFFECT_IDS) / sizeof(EffectId);

bool isAudioReactive(EffectId effectId) {
    for (uint8_t i = 0; i < REACTIVE_EFFECT_COUNT; i++) {
        if (pgm_read_word(&REACTIVE_EFFECT_IDS[i]) == effectId) {
            return true;
        }
    }
    return false;
}

uint8_t getReactiveEffectCount() {
    return REACTIVE_EFFECT_COUNT;
}

EffectId getReactiveEffectId(uint8_t index) {
    if (index >= REACTIVE_EFFECT_COUNT) return lightwaveos::INVALID_EFFECT_ID;
    return pgm_read_word(&REACTIVE_EFFECT_IDS[index]);
}

uint16_t buildAmbientEffectArray(EffectId* outputArray, uint16_t maxOutput, const EffectId* allIds, uint16_t effectCount) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < effectCount && count < maxOutput; i++) {
        if (!isAudioReactive(allIds[i])) {
            outputArray[count++] = allIds[i];
        }
    }
    return count;
}

} // namespace PatternRegistry
