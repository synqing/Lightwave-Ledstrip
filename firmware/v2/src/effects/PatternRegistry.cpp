/**
 * Pattern Registry Implementation
 *
 * Provides metadata for all registered patterns.
 * Pattern indices match effect IDs exactly (stable ID mapping).
 */

#include "PatternRegistry.h"
#include "../config/limits.h"
#include "../core/actors/ActorSystem.h"
#include "../core/actors/RendererActor.h"

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
    {PM_STR("LGP Star Burst (Narrative)"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Legacy starburst core with phrase-gated harmonic colour commits"), PM_STR("Narrative phrase state, snare bursts, chord-root gating"), PM_STR("LGP Star Burst")},
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
    {PM_STR("Perlin Test: Emotiscope2 Quarter"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Emotiscope 2.0 Perlin quarter-res + interpolation test (TEST)"), PM_STR("Seedable Perlin noise, quarter resolution, periodic refresh, linear interpolation"), PM_STR("")},

    // --- Enhanced Audio-Reactive Effects (88-97) - Beat-sync, musical intelligence ---
    {PM_STR("BPM Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Tempo-locked pulse rings with beat-phase sync"), PM_STR("Beat phase, sub-bass reinforcement, spring speed"), PM_STR("BPM")},
    {PM_STR("Breathing Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Style-adaptive breathing with harmonic saliency"), PM_STR("RMS breathing, saliency-aware pacing"), PM_STR("Breathing")},
    {PM_STR("LGP Chevron Waves Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-synced chevron propagation with chroma detail"), PM_STR("Chroma mapping, beat phase, wave sharpening"), PM_STR("LGP Chevron Waves")},
    {PM_STR("LGP Interference Scanner Enhanced"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Audio-reactive scan speed with interference fringes"), PM_STR("Phase scanning, snare emphasis"), PM_STR("LGP Interference Scanner")},
    {PM_STR("LGP Photonic Crystal Enhanced"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN, PM_STR("Harmonic lattice modulation with saliency weighting"), PM_STR("Harmonic saliency, lattice shimmer"), PM_STR("LGP Photonic Crystal")},
    {PM_STR("LGP Spectrum Detail"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("64-bin spectrum detail, bass at centre and treble at edges"), PM_STR("Logarithmic bin mapping, palette bands"), PM_STR("Spectrum Bars")},
    {PM_STR("LGP Spectrum Detail Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Saliency-weighted spectrum detail with adaptive emphasis"), PM_STR("Musical saliency, adaptive spectrum weighting"), PM_STR("LGP Spectrum Detail")},
    {PM_STR("LGP Star Burst Enhanced"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN, PM_STR("Beat-triggered star bursts with sub-bass boost"), PM_STR("Beat phase, sub-bass burst, chroma colour"), PM_STR("LGP Star Burst")},
    {PM_STR("LGP Wave Collision Enhanced"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Audio-driven wave collision with sub-bass boost"), PM_STR("Snare collisions, beat phase, chroma weighting"), PM_STR("LGP Wave Collision")},
    {PM_STR("Ripple Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-sync ripple propagation with musical intelligence"), PM_STR("Beat sync, style-adaptive, harmonic saliency, centre-origin"), PM_STR("Ripple")},
    {PM_STR("Audio Bloom Parity"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("SB 4.1.1 bloom parity with centre smear"), PM_STR("Chroma-weighted palette hue, centre smear, edge fade"), PM_STR("Audio Bloom")},
    {PM_STR("Audio Waveform Parity"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("SB 3.1.0 waveform parity with MOOD smoothing"), PM_STR("Waveform history, palette chroma hue, centre-origin"), PM_STR("Audio Waveform")},

    // --- Palette Auto-Cycle (100) ---
    {PM_STR("LGP Holographic Auto-Cycle"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Holographic effect with internal palette auto-cycling"), PM_STR("Multi-layer interference, palette cycling, depth illusion"), PM_STR("LGP Holographic")},

    // --- ES v1.1 Reference Shows (101-105) ---
    {PM_STR("ES Analog (Ref)"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: VU dot display"), PM_STR("VU level, centre-origin dot"), PM_STR("")},
    {PM_STR("ES Spectrum (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: 64-bin spectrum strip"), PM_STR("Spectrogram magnitude, HSV gradient"), PM_STR("")},
    {PM_STR("ES Octave (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: chromagram strip"), PM_STR("Chromagram magnitude, HSV gradient"), PM_STR("")},
    {PM_STR("ES Bloom (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: chroma bloom"), PM_STR("Chromagram sampling, squared response"), PM_STR("")},
    {PM_STR("ES Waveform (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Emotiscope v1.1 reference: waveform strip"), PM_STR("Waveform history, low-pass smoothing, HSV gradient"), PM_STR("")},

    // --- ES tuned ports (106+) ---
    {PM_STR("Ripple (ES tuned)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-locked ripples tuned for ES v1.1 audio backend"), PM_STR("Beat strength + flux accents, FFT sub-bass/treble shaping"), PM_STR("Ripple")},
    {PM_STR("Heartbeat (ES tuned)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Beat-locked lub-dub heartbeat tuned for ES v1.1 audio backend"), PM_STR("Beat tick + flux accents, chroma-anchored palette hue"), PM_STR("Heartbeat")},
    {PM_STR("LGP Holographic (ES tuned)"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH, PM_STR("Musically driven holographic depth layers tuned for ES v1.1 audio backend"), PM_STR("Band energy voicing, beat phase ratios, flux refraction accents, chroma-anchored colour"), PM_STR("LGP Holographic")},

    // --- Sensory Bridge Reference Shows (109+) ---
    {PM_STR("SB Waveform (Ref)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Sensory Bridge 3.1.0 reference: waveform mode"), PM_STR("Waveform history, MOOD smoothing, note chromagram → colour summation"), PM_STR("Audio Waveform, ES Waveform (Ref)")},

    // --- UI Preview Parity (110+) ---
    {PM_STR("Beat Pulse (Stack)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("UI preview parity: static palette gradient with beat-driven white push"), PM_STR("Beat envelope, white push, static centre-origin gradient"), PM_STR("Beat Pulse")},
    {PM_STR("Beat Pulse (Shockwave)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Canonical shockwave: ring travels outward from centre on beat"), PM_STR("Amplitude-driven ring position, HTML parity envelope, white push"), PM_STR("Beat Pulse (Stack), Beat Pulse")},
    {PM_STR("Beat Pulse (Shockwave In)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Inward shockwave: ring travels edge→centre on beat"), PM_STR("Amplitude-driven inward ring position, HTML parity envelope, white push"), PM_STR("Beat Pulse (Shockwave)")},

    // --- Beat Pulse Family (113-119) ---
    {PM_STR("Beat Pulse (Void)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Hard detonation in darkness: palette ring against black"), PM_STR("Parity ring maths, zero base brightness, palette-coloured detonation"), PM_STR("Beat Pulse (Stack)")},
    {PM_STR("Beat Pulse (Resonant)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Double ring contracting inward: sharp attack + warm resonant body"), PM_STR("Dual ring inward contraction, attack/body separation, differential decay"), PM_STR("Beat Pulse (Shockwave In)")},
    {PM_STR("Beat Pulse (Ripple)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Cascading implosion ripples: up to 3 rings converge on centre"), PM_STR("3-slot ring buffer, independent ring decay, inward contraction"), PM_STR("Beat Pulse (Resonant)")},
    {PM_STR("Beat Pulse (Shockwave Cascade)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Outward pressure wave with trailing echo rings"), PM_STR("Primary + 2 echo rings, fixed offset trailing, outward expansion"), PM_STR("Beat Pulse (Shockwave)")},
    {PM_STR("Beat Pulse (Spectral)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Three frequency-driven rings: bass outer, mid middle, treble centre"), PM_STR("Continuous frequency response, smoothed band tracking, spatial spectrum mapping"), PM_STR("Spectrum Bars")},
    {PM_STR("Beat Pulse (Spectral Pulse)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Stationary zones pulsing by frequency band"), PM_STR("Fixed zone mapping, soft crossfade boundaries, continuous spectral response"), PM_STR("Beat Pulse (Spectral)")},
    {PM_STR("Beat Pulse (Breathe)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Warm whole-strip amplitude pump with centre-weighted glow"), PM_STR("No ring shape, centre-biased amplitude, slower decay for sustain"), PM_STR("Beat Pulse (Stack), Breathing")},
    {PM_STR("Beat Pulse (LGP Interference)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING, PM_STR("Dual-strip interference: standing waves exploit LGP optics"), PM_STR("Phase-offset strip driving, spatial frequency control, anti-phase/quadrature modes"), PM_STR("LGP Chladni Harmonics, Modal Resonance")},

    // --- Beat Pulse (Bloom) - ID 120 ---
    {PM_STR("Beat Pulse (Bloom)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING, PM_STR("Beat Pulse transport bloom with beat-envelope driven injection"), PM_STR("Beat-gated centre injection, raw-time transport/advection, bloom-style trail persistence and post shaping."), PM_STR("Beat Pulse (Stack), Beat Pulse (Ripple), Transport/Advection/Trails")},

    // --- Bloom (Parity) - ID 121 ---
    {PM_STR("Bloom (Parity)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Bloom-style subpixel transport (liquid trails)"), PM_STR("Stateful HDR-ish history buffer advected outward by fractional offsets (subpixel), with beat+flux centre injection."), PM_STR("Transport/Advection/Trails")},

    // --- Kuramoto Transport - ID 122 ---
    {PM_STR("Kuramoto Transport"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Invisible oscillator field → event-driven light transport"), PM_STR("80-oscillator Kuramoto with nonlocal coupling, RK2 integration, phase-slip/coherence-edge injection into Bloom-style transport buffer. Audio steers regime, not pixels."), PM_STR("Dynamical System/Emergence/Transport")},

    // --- SB Waveform Parity - ID 123 ---
    {PM_STR("SB Waveform (Parity)"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Sensory Bridge 3.1.0 waveform: audio oscilloscope with palette"), PM_STR("4-frame history averaging, dynamic max-follower normalisation, MOOD-based temporal smoothing, dt-corrected. Intensity-only transport with palette at output."), PM_STR("Waveform/Oscilloscope/Audio")},

    // --- Holographic Variants Pack (124-129) ---
    {PM_STR("LGP Opal Film"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::DEPTH | PatternTags::PHYSICS, PM_STR("Iridescent thin-film bands drifting from centre"), PM_STR("Thin-film interference, thickness waves, pearlescent lift"), PM_STR("LGP Holographic, LGP Chromatic Interference")},
    {PM_STR("LGP Grating Scan"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Spectral scan highlight sweeping from centre"), PM_STR("Diffraction grating dispersion, moving incidence angle"), PM_STR("LGP Chromatic Lens, LGP Chromatic Pulse")},
    {PM_STR("LGP Stress Glass"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Polarised stress fringes with slow analyser rotation"), PM_STR("Birefringence, phase retardation fringes"), PM_STR("LGP Birefringent Shear")},
    {PM_STR("LGP Moire Silk"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE, PM_STR("Large moire beat envelopes with fine ribbing"), PM_STR("Two-lattice interference, beat envelope modulation"), PM_STR("LGP Moire Curtains")},
    {PM_STR("LGP Caustic Shards"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Interference field punctuated by prismatic glints"), PM_STR("Caustic concentration, sharp specular ridges"), PM_STR("LGP Caustic Fan, LGP Perlin Caustics")},
    {PM_STR("LGP Parallax Depth"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::DEPTH, PM_STR("Two-layer field with subtle parallax between strips"), PM_STR("Phase-offset layers, depth separation"), PM_STR("LGP Holographic, LGP Holographic Vortex")},

    // --- Extensions (130-135) ---
    {PM_STR("LGP Stress Glass (Melt)"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Photoelastic fringes with phase-locked wings"), PM_STR("Birefringence, phase retardation, wing melt"), PM_STR("LGP Stress Glass")},
    {PM_STR("LGP Grating Scan (Breakup)"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Diffraction scan with controlled halo breakup"), PM_STR("Diffraction grating dispersion, halo spatter decay"), PM_STR("LGP Grating Scan")},
    {PM_STR("LGP Water Caustics"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Ray-envelope caustic filaments with cusp spikes"), PM_STR("Ray mapping, density blow-ups, caustic sheet"), PM_STR("LGP Caustic Fan, LGP Caustic Shards")},
    {PM_STR("LGP Schlieren Flow"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL | PatternTags::PHYSICS, PM_STR("Knife-edge gradient flow with heat-haze shimmer"), PM_STR("Gradient response, refractive index flow"), PM_STR("LGP Stress Glass")},
    {PM_STR("LGP Reaction Diffusion"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Gray-Scott slime with self-replicating dynamics"), PM_STR("Reaction-diffusion, feed/kill balance"), PM_STR("LGP Mycelial Network")},
    {PM_STR("LGP RD Triangle"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Front wedge isolation from reaction-diffusion field"), PM_STR("Thresholded fronts, triangle envelope, edge pins"), PM_STR("LGP Reaction Diffusion")},

    // --- Shape Bangers Pack (136-146) ---
    {PM_STR("LGP Talbot Carpet"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Self-imaging lattice rug (near-field diffraction vibe)"), PM_STR("Fresnel harmonic sum, Talbot self-imaging, grating pitch"), PM_STR("LGP Holographic")},
    {PM_STR("LGP Airy Comet"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING | PatternTags::PHYSICS, PM_STR("Self-accelerating comet with trailing lobes"), PM_STR("Airy beam parabolic motion, oscillatory tail lobes"), PM_STR("LGP Evanescent Drift")},
    {PM_STR("LGP Moire Cathedral"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::STANDING, PM_STR("Interference arches from close gratings (giant beats)"), PM_STR("Two-grating moire, beat envelopes, cathedral ribs"), PM_STR("LGP Moire Silk, LGP Moire Curtains")},
    {PM_STR("LGP Living Glyph"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Superformula sigils (morphing supershapes)"), PM_STR("Superformula r(phi,m,n1,n2,n3), slow morph, distance-to-curve band"), PM_STR("LGP Diamond Lattice")},
    {PM_STR("LGP Spirograph Crown"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Hypotrochoid crown loops (gear-flower royal seal)"), PM_STR("Hypotrochoid radius projection, facet sparkle"), PM_STR("LGP Concentric Rings")},
    {PM_STR("LGP Rose Bloom"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Rhodonea petals (geometric bloom)"), PM_STR("Rose curve cos(k*theta), petal count drift, opening bloom modulation"), PM_STR("LGP Spirograph Crown")},
    {PM_STR("LGP Harmonograph Halo"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Lissajous orbitals (aura loops, premium calm)"), PM_STR("Integer-ratio Lissajous, orbit radius projection, gentle energy pulse"), PM_STR("LGP Spiral Vortex")},
    {PM_STR("LGP Rule 30 Cathedral"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Elementary CA textile (triangles + chaos + ribs)"), PM_STR("Rule 30 cellular automaton, binary-to-textile blur, neighbourhood tinting"), PM_STR("LGP Mycelial Network")},
    {PM_STR("LGP Langton Highway"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Emergent order reveal (ant to chaos to highway) projected to 1D"), PM_STR("Langton ant 64x64 grid, diagonal slice projection, ant spark"), PM_STR("LGP Rule 30 Cathedral")},
    {PM_STR("LGP Cymatic Ladder"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Standing-wave nodes/antinodes sculpted into LGP glass"), PM_STR("Standing wave harmonics 2-8, node/antinode sculpture"), PM_STR("LGP Chladni Harmonics")},
    {PM_STR("LGP Mach Diamonds"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING | PatternTags::PHYSICS, PM_STR("Shock-diamond jewellery (standing shock-cell pulses)"), PM_STR("Triangle-wave shock cells, diamond breathing, jewel tones"), PM_STR("LGP Cymatic Ladder")},

    // --- LGP Holy Shit Bangers Pack (147-151) ---
    {PM_STR("Chimera Crown"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Coherent and incoherent domains (sync fracture line)"), PM_STR("Kuramoto-Sakaguchi nonlocal coupling, local order parameter"), PM_STR("")},
    {PM_STR("Catastrophe Caustics"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Ray-envelope filaments (focus pull + cusp spark)"), PM_STR("1D ray histogram, lens thickness field, caustic concentration"), PM_STR("LGP Caustic Fan, LGP Water Caustics")},
    {PM_STR("Hyperbolic Portal"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Edge densification via atanh(r) (Poincaré vibe)"), PM_STR("Hyperbolic stretch, multi-band ribs, centre calm"), PM_STR("")},
    {PM_STR("Lorenz Ribbon"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS, PM_STR("Chaotic attractor ribbon (never repeats the same way twice)"), PM_STR("Lorenz ODE trail, radial projection, age fade"), PM_STR("")},
    {PM_STR("IFS Botanical Relic"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP, PM_STR("Mirrored IFS growth (fractal botany in glass)"), PM_STR("Barnsley fern IFS, radial histogram, vein specular"), PM_STR("LGP Crystalline Growth")},

    // --- Experimental Audio Pack (152-161) ---
    {PM_STR("LGP Flux Rift"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Transient flux opens a travelling centre-out rift"), PM_STR("Fast flux envelope, beat pulse coupling, radial ring pressure"), PM_STR("Beat Pulse (Shockwave), Audio Bloom")},
    {PM_STR("LGP Beat Prism"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Beat-front prism rays with edgeward pressure travel"), PM_STR("Beat-strength front, treble prism modulation, radial ray field"), PM_STR("Beat Pulse (Stack), LGP Star Burst")},
    {PM_STR("LGP Harmonic Tide"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Chord-anchored tidal bands with centre-held continuity"), PM_STR("Chord/root anchoring, harmonic saliency weighting, dual-wave tide"), PM_STR("Chord Glow, Breathing")},
    {PM_STR("LGP Bass Quake"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Sub-bass compression waves with outward shock release"), PM_STR("Heavy-bass envelope, beat-coupled shock ring, centre pressure core"), PM_STR("Bass Breath, Beat Pulse (Shockwave Cascade)")},
    {PM_STR("LGP Treble Net"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::STANDING | PatternTags::SPECTRAL, PM_STR("Timbral shimmer lattice with edge-biased spectral filaments"), PM_STR("Treble/timbral envelope, interference net, shimmer burst accents"), PM_STR("LGP Interference Scanner, LGP Chladni Harmonics")},
    {PM_STR("LGP Rhythmic Gate"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Beat-gated lattice shutters with travelling pulse seams"), PM_STR("Rhythmic saliency gate, moving bar field, beat seam projection"), PM_STR("BPM Enhanced, Chevron Waves Enhanced")},
    {PM_STR("LGP Spectral Knot"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING | PatternTags::SPECTRAL, PM_STR("Frequency-balance knot fields crossing in mirrored rings"), PM_STR("Low/mid/high balance knotting, crossing radial rings, weave envelope"), PM_STR("Spectrum Bars, Beat Pulse (Spectral)")},
    {PM_STR("LGP Saliency Bloom"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, PM_STR("Overall novelty drives expanding bloom radius and density"), PM_STR("Overall saliency envelope, beat-assisted bloom radius, centre glow bed"), PM_STR("Audio Bloom, Wave Reactive")},
    {PM_STR("LGP Transient Lattice"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING, PM_STR("Snare and flux impacts drive a decaying interference scaffold"), PM_STR("Transient memory field, impact ring launch, dual-frequency lattice"), PM_STR("Ripple Enhanced, LGP Wave Collision")},
    {PM_STR("LGP Wavelet Mirror"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::SPECTRAL, PM_STR("Waveform crest mirroring with beat-travel ridge reinforcement"), PM_STR("Waveform crest sampling, mirrored radial mapping, beat-ridge overlay"), PM_STR("Audio Waveform, Snapwave")}
};

const uint8_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

// ============================================================================
// Effect Count Parity Validation
// ============================================================================

// Expected number of implemented effects (must match registerAllEffects() return value)
// Uses limits::MAX_EFFECTS (single source of truth)
constexpr uint8_t EXPECTED_EFFECT_COUNT = lightwaveos::limits::MAX_EFFECTS;

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
    
    // Access RendererActor via ActorSystem
    using namespace lightwaveos::actors;
    RendererActor* renderer = ActorSystem::instance().getRenderer();
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
    5,   // Juggle - audio-reactive (spectrum/beat driven)
    6,   // BPM - beat-synced pulsing
    7,   // Wave Ambient - RMS amplitude modulation
    8,   // Ripple - kick/treble triggered, chroma-driven
    106, // Ripple (ES tuned) - ES backend tuned, beat-locked
    9,   // Heartbeat - (now reactive register): beat/flux can drive ES tuned companion
    107, // Heartbeat (ES tuned) - beat-locked lub-dub, chroma anchored
    11,  // Breathing - RMS breathing, beat-gated pulses
    16,  // Interference Scanner - heavyMid energy, snare boost
    17,  // Wave Collision - heavyBass, snare/hihat triggers
    108, // LGP Holographic (ES tuned) - beat/flux/chroma driven holographic depth
    22,  // Chevron Waves - chroma, snare sharpness
    24,  // Star Burst - full audio pipeline, snare bursts
    33,  // Photonic Crystal - tempo breathing, saliency
    68,  // Audio Test - demo effect
    69,  // Beat Pulse - beat-synced radial pulse
    70,  // Spectrum Bars - 8-band analyzer
    71,  // Bass Breath - bass-driven breathing
    72,  // Audio Waveform - time-domain visualization
    73,  // Audio Bloom - transient-triggered blooms
    74,  // Star Burst Narrative - legacy core + phrase-gated harmonic commits
    75,  // Chord Glow - chord detection showcase
    76,  // Wave Reactive - energy accumulation
    77,  // Perlin Veil - audio-driven advection
    78,  // Perlin Shocklines - beat-driven ridges
    79,  // Perlin Caustics - treble/bass modulation
    80,  // Perlin Interference Weave - dual-strip moiré
    // Enhanced Audio-Reactive Effects (88-99)
    88,  // BPM Enhanced - tempo-locked pulse rings
    89,  // Breathing Enhanced - style-adaptive breathing
    90,  // Chevron Waves Enhanced - beat-synced chevrons
    91,  // Interference Scanner Enhanced - audio-reactive scanning
    92,  // Photonic Crystal Enhanced - harmonic lattice modulation
    93,  // Spectrum Detail - 64-bin spectrum detail
    94,  // Spectrum Detail Enhanced - saliency-weighted spectrum
    95,  // LGP Star Burst Enhanced - beat-triggered bursts
    96,  // LGP Wave Collision Enhanced - audio-driven collisions
    97,  // Ripple Enhanced - beat-sync propagation
    98,  // Audio Bloom Parity - SB 4.1.1 bloom parity
    99,  // Audio Waveform Parity - SB 3.1.0 waveform parity
    100, // LGP Holographic Auto-Cycle - audio reactive (palette-cycled)
    101, // ES Analog (Ref) - reference show
    102, // ES Spectrum (Ref) - reference show
    103, // ES Octave (Ref) - reference show
    104, // ES Bloom (Ref) - reference show
    105, // ES Waveform (Ref) - reference show
    109, // SB Waveform (Ref) - reference show
    110, // Beat Pulse (Stack) - UI preview parity
    111, // Beat Pulse (Shockwave) - outward travelling ring
    112, // Beat Pulse (Shockwave In) - inward travelling ring
    113, // Beat Pulse (Void) - parity ring in darkness
    114, // Beat Pulse (Resonant) - double ring contracting inward
    115, // Beat Pulse (Ripple) - 3-slot cascading implosion rings
    116, // Beat Pulse (Shockwave Cascade) - outward with echo rings
    117, // Beat Pulse (Spectral) - three frequency-driven rings
    118, // Beat Pulse (Spectral Pulse) - stationary frequency zones
    119, // Beat Pulse (Breathe) - whole-strip amplitude pump
    120, // Beat Pulse (LGP Interference) - dual-strip standing wave interference
    121, // Beat Pulse (Bloom) - beat-gated transport bloom
    122, // Bloom (Parity) - SB parity transport bloom
    123, // Kuramoto Transport - event-driven coherence transport
    152, // LGP Flux Rift - flux-driven travelling rift
    153, // LGP Beat Prism - beat-front prism rays
    154, // LGP Harmonic Tide - chord-anchored tidal bands
    155, // LGP Bass Quake - bass compression with shock release
    156, // LGP Treble Net - timbral shimmer lattice
    157, // LGP Rhythmic Gate - saliency-gated shutter field
    158, // LGP Spectral Knot - low/mid/high knot crossings
    159, // LGP Saliency Bloom - novelty-weighted bloom radius
    160, // LGP Transient Lattice - snare/flux impact scaffold
    161  // LGP Wavelet Mirror - waveform crest mirror field
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
