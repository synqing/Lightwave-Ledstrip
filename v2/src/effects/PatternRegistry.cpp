/**
 * Pattern Registry Implementation
 *
 * Provides metadata for all 118 registered patterns.
 * Pattern indices match effect IDs exactly (stable ID mapping).
 */

#include "PatternRegistry.h"

// ============================================================================
// Pattern Metadata Definitions (PROGMEM)
// ============================================================================

// Helper macros for PROGMEM strings
#define PM_STR(s) (const char*)(s)

// Pattern metadata entries - mapped to effect IDs (matches v2 registration order)
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // Index 0: Fire+
    {PM_STR("Fire+"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Enhanced realistic fire simulation radiating from centre"),
     PM_STR("Thermal convection, intensity gradient"), PM_STR("")},

    // Index 1: Ocean+
    {PM_STR("Ocean+"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Enhanced deep ocean wave patterns from centre point"),
     PM_STR("Wave propagation, depth gradient"), PM_STR("")},

    // Index 2: LGP Holographic+
    {PM_STR("LGP Holographic+"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH,
     PM_STR("Enhanced holographic interference patterns through multi-layer depth"),
     PM_STR("Multi-layer interference, phase relationships, depth illusion"), PM_STR("LGP Modal Resonance, LGP Interference Scanner")},

    // Index 3: Shockwave+
    {PM_STR("Shockwave+"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Enhanced energy pulse expanding from centre"),
     PM_STR("Radial expansion, intensity falloff"), PM_STR("Collision")},

    // Index 4: Collision+
    {PM_STR("Collision+"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Enhanced dual waves colliding at centre"),
     PM_STR("Wave interference, collision dynamics"), PM_STR("Shockwave")},

    // Index 5: LGP Wave Collision+
    {PM_STR("LGP Wave Collision+"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Enhanced dual wave collision interference at centre"),
     PM_STR("Wave interference, collision dynamics, constructive/destructive zones"), PM_STR("LGP Interference Scanner, Collision")},

    // Index 6: Fire
    {PM_STR("Fire"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Realistic fire simulation radiating from centre"),
     PM_STR("Thermal convection, intensity gradient"), PM_STR("")},

    // Index 7: Ocean
    {PM_STR("Ocean"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Deep ocean wave patterns from centre point"),
     PM_STR("Wave propagation, depth gradient"), PM_STR("")},

    // Index 8: Wave
    {PM_STR("Wave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Smooth sine wave propagating from centre"),
     PM_STR("Wave propagation, phase control"), PM_STR("")},

    // Index 9: Ripple
    {PM_STR("Ripple"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Water ripple effect expanding outward"),
     PM_STR("Radial wave propagation, decay"), PM_STR("")},

    // Index 10: Sinelon
    {PM_STR("Sinelon"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Bouncing particle with palette trails"),
     PM_STR("Position-based motion, trail decay"), PM_STR("")},

    // Index 11: Shockwave
    {PM_STR("Shockwave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Energy pulse expanding from centre"),
     PM_STR("Radial expansion, intensity falloff"), PM_STR("Collision")},

    // Index 12: Collision
    {PM_STR("Collision"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Dual waves colliding at centre"),
     PM_STR("Wave interference, collision dynamics"), PM_STR("Shockwave")},

    // Index 13: Gravity Well
    {PM_STR("Gravity Well"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN,
     PM_STR("Gravitational attraction to centre point"),
     PM_STR("Inverse square law, radial gradient"), PM_STR("")},

    // Index 14: LGP Holographic
    {PM_STR("LGP Holographic"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH,
     PM_STR("Holographic interference patterns through multi-layer depth"),
     PM_STR("Multi-layer interference, phase relationships, depth illusion"), PM_STR("LGP Modal Resonance, LGP Interference Scanner")},

    // Index 15: LGP Modal Resonance
    {PM_STR("LGP Modal Resonance"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING,
     PM_STR("Explores different optical cavity resonance modes"),
     PM_STR("Cavity modes, standing wave patterns, mode selection"), PM_STR("LGP Holographic, LGP Standing Wave")},

    // Index 16: LGP Interference Scanner
    {PM_STR("LGP Interference Scanner"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Scanning interference beam creating moving patterns"),
     PM_STR("Traveling interference, phase scanning, beam dynamics"), PM_STR("LGP Holographic, LGP Wave Collision")},

    // Index 17: LGP Wave Collision
    {PM_STR("LGP Wave Collision"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Dual wave collision interference at centre"),
     PM_STR("Wave interference, collision dynamics, constructive/destructive zones"), PM_STR("LGP Interference Scanner, Collision")},

    // Index 18: LGP Diamond Lattice
    {PM_STR("LGP Diamond Lattice"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Diamond/rhombus patterns through angular interference"),
     PM_STR("Angular wave fronts, X-ray crystallography patterns, diamond nodes"), PM_STR("LGP Hexagonal Grid, LGP Concentric Rings")},

    // Index 19: LGP Concentric Rings
    {PM_STR("LGP Concentric Rings"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Expanding concentric ring geometry from centre"),
     PM_STR("Radial expansion, ring spacing, geometric progression"), PM_STR("LGP Diamond Lattice, Ripple")},

    // Index 20: LGP Star Burst
    {PM_STR("LGP Star Burst"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Radial star burst pattern from centre"),
     PM_STR("Radial rays, angular distribution, intensity falloff"), PM_STR("LGP Concentric Rings")},

    // Index 21: LGP Moiré Curtains
    {PM_STR("LGP Moiré Curtains"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Moiré pattern optical illusion through interference"),
     PM_STR("Spatial frequency beating, moiré interference, curtain effect"), PM_STR("LGP Holographic, LGP Diamond Lattice")},

    // Index 22: LGP Radial Ripple
    {PM_STR("LGP Radial Ripple"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Radial ripple propagation with interference"),
     PM_STR("Radial wave propagation, interference zones, ripple dynamics"), PM_STR("Ripple, LGP Concentric Rings")},

    // Index 23: LGP Holographic Vortex
    {PM_STR("LGP Holographic Vortex"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Holographic vortex spiral pattern"),
     PM_STR("Spiral wave propagation, vortex dynamics, depth illusion"), PM_STR("LGP Holographic, LGP Radial Ripple")},

    // Index 24: LGP Chromatic Shear
    {PM_STR("LGP Chromatic Shear"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Chromatic shear displacement effect"),
     PM_STR("Wavelength-dependent refraction, chromatic dispersion, shear mapping"), PM_STR("LGP Chromatic Aberration")},

    // Index 25: LGP Fresnel Zones
    {PM_STR("LGP Fresnel Zones"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Fresnel zone plate diffraction pattern"),
     PM_STR("Fresnel zones, diffraction patterns, alternating zones"), PM_STR("LGP Concentric Rings, LGP Moiré Curtains")},

    // Index 26: LGP Photonic Crystal
    {PM_STR("LGP Photonic Crystal"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Photonic crystal band structure visualization"),
     PM_STR("Band structure, periodic modulation, photonic band gaps"), PM_STR("LGP Diamond Lattice, LGP Moiré Curtains")},

    // Index 27: LGP Aurora Borealis
    {PM_STR("LGP Aurora Borealis"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Aurora borealis curtain effect"),
     PM_STR("Curtain dynamics, colour gradients, wave motion"), PM_STR("LGP Bioluminescent, LGP Plasma Membrane")},

    // Index 28: LGP Bioluminescent
    {PM_STR("LGP Bioluminescent"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Bioluminescent wave patterns"),
     PM_STR("Wave propagation, organic motion, intensity pulsing"), PM_STR("LGP Aurora Borealis, Ocean")},

    // Index 29: LGP Plasma Membrane
    {PM_STR("LGP Plasma Membrane"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Plasma membrane oscillation effect"),
     PM_STR("Membrane dynamics, oscillation patterns, fluid motion"), PM_STR("LGP Aurora Borealis, LGP Liquid Crystal")},

    // Index 30: LGP Quantum Tunneling
    {PM_STR("LGP Quantum Tunneling"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Quantum tunneling probability waves"),
     PM_STR("Wave function probability, tunneling dynamics, quantum mechanics"), PM_STR("LGP Quantum Entangle, LGP Wave Function")},

    // Index 31: LGP Gravitational Lens
    {PM_STR("LGP Gravitational Lens"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gravitational lensing distortion effect"),
     PM_STR("Lens distortion, gravitational effects, light bending"), PM_STR("LGP GRIN Cloak, Gravity Well")},

    // Index 32: LGP Time Crystal
    {PM_STR("LGP Time Crystal"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Discrete time crystal oscillation"),
     PM_STR("Time crystal dynamics, periodic oscillation, symmetry breaking"), PM_STR("LGP Quantum Tunneling")},

    // Index 33: LGP Metamaterial Cloak
    {PM_STR("LGP Metamaterial Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Metamaterial cloaking gradient with negative refractive index"),
     PM_STR("Negative refractive index, cloaking dynamics, destructive interference"), PM_STR("LGP GRIN Cloak, LGP Anisotropic Cloak")},

    // Index 34: LGP GRIN Cloak
    {PM_STR("LGP GRIN Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gradient index lens cloaking effect"),
     PM_STR("Gradient index, lens dynamics, cloaking"), PM_STR("LGP Metamaterial Cloak, LGP Gravitational Lens")},

    // Index 35: LGP Caustic Fan
    {PM_STR("LGP Caustic Fan"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Caustic light fan projection"),
     PM_STR("Caustic patterns, light focusing, fan projection"), PM_STR("LGP Fresnel Zones")},

    // Index 36: LGP Birefringent Shear
    {PM_STR("LGP Birefringent Shear"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Birefringent shear splitting effect"),
     PM_STR("Birefringence, shear splitting, polarization effects"), PM_STR("LGP Chromatic Shear, LGP Chromatic Aberration")},

    // Index 37: LGP Anisotropic Cloak
    {PM_STR("LGP Anisotropic Cloak"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Anisotropic cloaking field effect"),
     PM_STR("Anisotropic materials, directional cloaking, field effects"), PM_STR("LGP Metamaterial Cloak, LGP GRIN Cloak")},

    // Index 38: LGP Evanescent Skin
    {PM_STR("LGP Evanescent Skin"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Evanescent wave skin effect"),
     PM_STR("Evanescent waves, skin depth, exponential decay"), PM_STR("LGP Metamaterial Cloak")},

    // Index 39: LGP Chromatic Aberration
    {PM_STR("LGP Chromatic Aberration"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Chromatic aberration RGB split through wavelength-dependent refraction"),
     PM_STR("Chromatic dispersion, wavelength-dependent focus, RGB separation"), PM_STR("LGP Chromatic Shear, LGP Birefringent Shear")},

    // Index 40: LGP Color Accelerator
    {PM_STR("LGP Color Accelerator"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Color momentum acceleration effect"),
     PM_STR("Momentum dynamics, acceleration patterns, colour motion"), PM_STR("LGP Chromatic Aberration")},

    // Index 41: LGP Chromatic Lens
    {PM_STR("LGP Chromatic Lens"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Chromatic Lens pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 42: LGP Chromatic Pulse
    {PM_STR("LGP Chromatic Pulse"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Chromatic Pulse pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 43: LGP Chromatic Interference
    {PM_STR("LGP Chromatic Interference"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Chromatic Interference pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 44: LGP Liquid Crystal
    {PM_STR("LGP Liquid Crystal"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Liquid crystal birefringence effect"),
     PM_STR("Liquid crystal dynamics, birefringence, director field"), PM_STR("LGP Birefringent Shear, LGP Plasma Membrane")},

    // Index 45: LGP Prism Cascade
    {PM_STR("LGP Prism Cascade"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Prism cascade light splitting effect"),
     PM_STR("Prism refraction, cascade dynamics, spectral splitting"), PM_STR("LGP Chromatic Aberration, LGP Caustic Fan")},

    // Index 46: LGP Silk Waves
    {PM_STR("LGP Silk Waves"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Silk-like flowing wave patterns"),
     PM_STR("Fluid dynamics, silk-like motion, wave flow"), PM_STR("LGP Bioluminescent, Ocean")},

    // Index 47: LGP Beam Collision
    {PM_STR("LGP Beam Collision"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Beam collision interference pattern"),
     PM_STR("Beam dynamics, collision interference, wave interaction"), PM_STR("LGP Wave Collision, Collision")},

    // Index 48: LGP Laser Duel
    {PM_STR("LGP Laser Duel"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Dual laser beam interaction effect"),
     PM_STR("Laser beam dynamics, dual beam interaction, interference"), PM_STR("LGP Beam Collision, LGP Wave Collision")},

    // Index 49: LGP Tidal Forces
    {PM_STR("LGP Tidal Forces"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Tidal gravitational forces effect"),
     PM_STR("Tidal forces, gravitational effects, force dynamics"), PM_STR("Gravity Well, LGP Gravitational Lens")},

    // Index 50: LGP Chladni Harmonics
    {PM_STR("LGP Chladni Harmonics"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING,
     PM_STR("Chladni plate vibration harmonics"),
     PM_STR("Vibration harmonics, nodal patterns, Chladni figures"), PM_STR("LGP Modal Resonance, LGP Standing Wave")},

    // Index 51: LGP Gravitational Chirp
    {PM_STR("LGP Gravitational Chirp"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gravitational wave chirp signal"),
     PM_STR("Gravitational waves, chirp signal, frequency evolution"), PM_STR("LGP Gravitational Lens, LGP Tidal Forces")},

    // Index 52: LGP Quantum Entangle
    {PM_STR("LGP Quantum Entangle"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Quantum entanglement collapse visualization"),
     PM_STR("Quantum entanglement, wave function collapse, correlation"), PM_STR("LGP Quantum Tunneling, LGP Time Crystal")},

    // Index 53: LGP Mycelial Network
    {PM_STR("LGP Mycelial Network"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Mycelial network branching pattern"),
     PM_STR("Network branching, organic growth, fractal patterns"), PM_STR("LGP Bioluminescent, LGP Aurora Borealis")},

    // Index 54: LGP Riley Dissonance
    {PM_STR("LGP Riley Dissonance"), PatternFamily::NOVEL_PHYSICS, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Bridget Riley-inspired optical dissonance"),
     PM_STR("Optical art, moiré patterns, visual dissonance"), PM_STR("LGP Moiré Curtains, LGP Diamond Lattice")},

    // Index 55: Solid Color
    {PM_STR("Solid Color"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Solid Color pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 56: Pulse
    {PM_STR("Pulse"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Pulse pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 57: Confetti
    {PM_STR("Confetti"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Confetti pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 58: Strip Confetti
    {PM_STR("Strip Confetti"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Strip Confetti pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 59: Juggle
    {PM_STR("Juggle"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Juggle pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 60: Strip Juggle
    {PM_STR("Strip Juggle"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Strip Juggle pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 61: BPM
    {PM_STR("BPM"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("BPM pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 62: Strip BPM
    {PM_STR("Strip BPM"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN,
     PM_STR("Strip BPM pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 63: Strip Plasma
    {PM_STR("Strip Plasma"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Strip Plasma pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 64: Plasma
    {PM_STR("Plasma"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN,
     PM_STR("Plasma pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 65: LGP Chromatic Lens (physics-accurate Cauchy dispersion)
    {PM_STR("LGP Chromatic Lens"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Physics-accurate chromatic lens with Cauchy equation dispersion"),
     PM_STR("Cauchy equation dispersion, wavelength-dependent refraction, lens-like color separation"), PM_STR("LGP Chromatic Aberration, LGP Chromatic Shear")},

    // Index 66: LGP Chromatic Pulse (physics-accurate Cauchy dispersion)
    {PM_STR("LGP Chromatic Pulse"), PatternFamily::ADVANCED_OPTICAL, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Pulsing chromatic dispersion with physics-accurate Cauchy equation"),
     PM_STR("Cauchy equation dispersion, pulsing aberration, expanding/contracting color separation"), PM_STR("LGP Chromatic Lens, LGP Chromatic Aberration")},

    // Index 67: LGP Chromatic Interference (physics-accurate Cauchy dispersion)
    {PM_STR("LGP Chromatic Interference"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING | PatternTags::SPECTRAL,
     PM_STR("Dual-edge interference with physics-accurate chromatic dispersion"),
     PM_STR("Cauchy equation dispersion, wave interference, dual-edge injection, constructive/destructive patterns"), PM_STR("LGP Chromatic Lens, LGP Wave Collision, LGP Interference Scanner")},

    // Index 68: LGP Hexagonal Grid
    {PM_STR("LGP Hexagonal Grid"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Hexagonal Grid pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 69: LGP Spiral Vortex
    {PM_STR("LGP Spiral Vortex"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Spiral Vortex pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 70: LGP Sierpinski
    {PM_STR("LGP Sierpinski"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Sierpinski pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 71: LGP Chevron Waves
    {PM_STR("LGP Chevron Waves"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Chevron Waves pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 72: LGP Mesh Network
    {PM_STR("LGP Mesh Network"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Mesh Network pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 73: LGP Color Temperature
    {PM_STR("LGP Color Temperature"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Color Temperature pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 74: LGP RGB Prism
    {PM_STR("LGP RGB Prism"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP RGB Prism pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 75: LGP Complementary
    {PM_STR("LGP Complementary"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Complementary pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 76: LGP Additive Mix
    {PM_STR("LGP Additive Mix"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Additive Mix pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 77: LGP Quantum Colors
    {PM_STR("LGP Quantum Colors"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Quantum Colors pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 78: LGP Doppler Shift
    {PM_STR("LGP Doppler Shift"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Doppler Shift pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 79: LGP HSV Cylinder
    {PM_STR("LGP HSV Cylinder"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP HSV Cylinder pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 80: LGP Perceptual Blend
    {PM_STR("LGP Perceptual Blend"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Perceptual Blend pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 81: LGP Metameric
    {PM_STR("LGP Metameric"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Metameric pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 82: LGP DNA Helix
    {PM_STR("LGP DNA Helix"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP DNA Helix pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 83: LGP Phase Transition
    {PM_STR("LGP Phase Transition"), PatternFamily::COLOR_MIXING, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("LGP Phase Transition pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 84: LGP Box Wave
    {PM_STR("LGP Box Wave"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Box Wave pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 85: LGP Soliton Explorer
    {PM_STR("LGP Soliton Explorer"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Soliton Explorer pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 86: LGP Rogue Wave
    {PM_STR("LGP Rogue Wave"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Rogue Wave pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 87: LGP Turing Pattern
    {PM_STR("LGP Turing Pattern"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Turing Pattern pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 88: LGP Kelvin-Helmholtz
    {PM_STR("LGP Kelvin-Helmholtz"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Kelvin-Helmholtz pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 89: LGP Faraday Rotation
    {PM_STR("LGP Faraday Rotation"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Faraday Rotation pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 90: LGP Brillouin Zones
    {PM_STR("LGP Brillouin Zones"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Brillouin Zones pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 91: LGP Shock Formation
    {PM_STR("LGP Shock Formation"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Shock Formation pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 92: LGP Chaos Visual
    {PM_STR("LGP Chaos Visual"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Chaos Visual pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 93: LGP Neural Avalanche
    {PM_STR("LGP Neural Avalanche"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Neural Avalanche pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 94: LGP Cardiac Spirals
    {PM_STR("LGP Cardiac Spirals"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP,
     PM_STR("LGP Cardiac Spirals pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 95: LGP Neural Network
    {PM_STR("LGP Neural Network"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Neural Network pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 96: LGP Crystalline
    {PM_STR("LGP Crystalline"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Crystalline pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 97: LGP Fluid Dynamics
    {PM_STR("LGP Fluid Dynamics"), PatternFamily::UNKNOWN, PatternTags::CENTER_ORIGIN,
     PM_STR("LGP Fluid Dynamics pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 98: LGP Plankton Waves
    {PM_STR("LGP Plankton Waves"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Plankton Waves pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 99: LGP Bacterial Growth
    {PM_STR("LGP Bacterial Growth"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Bacterial Growth pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 100: LGP DNA Replication
    {PM_STR("LGP DNA Replication"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP DNA Replication pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 101: LGP Protein Folding
    {PM_STR("LGP Protein Folding"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Protein Folding pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 102: LGP Mycelium Growth
    {PM_STR("LGP Mycelium Growth"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Mycelium Growth pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 103: LGP Slime Mold
    {PM_STR("LGP Slime Mold"), PatternFamily::ORGANIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("LGP Slime Mold pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 104: LGP Modal Cavity
    {PM_STR("LGP Modal Cavity"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Modal Cavity pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 105: LGP Evanescent Drift
    {PM_STR("LGP Evanescent Drift"), PatternFamily::QUANTUM, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("LGP Evanescent Drift pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 106: LGP Soliton Waves
    {PM_STR("LGP Soliton Waves"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Soliton Waves pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 107: LGP Sonic Boom
    {PM_STR("LGP Sonic Boom"), PatternFamily::INTERFERENCE, PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("LGP Sonic Boom pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 108: LGP Benard Convection
    {PM_STR("LGP Benard Convection"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Benard Convection pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 109: LGP Rayleigh-Taylor
    {PM_STR("LGP Rayleigh-Taylor"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Rayleigh-Taylor pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 110: LGP Plasma Pinch
    {PM_STR("LGP Plasma Pinch"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Plasma Pinch pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 111: LGP Magnetic Reconnect
    {PM_STR("LGP Magnetic Reconnect"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Magnetic Reconnect pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 112: LGP KH Enhanced
    {PM_STR("LGP KH Enhanced"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP KH Enhanced pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 113: LGP Cellular Automata
    {PM_STR("LGP Cellular Automata"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Cellular Automata pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 114: LGP Gray-Scott
    {PM_STR("LGP Gray-Scott"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Gray-Scott pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 115: LGP Mandelbrot Zoom
    {PM_STR("LGP Mandelbrot Zoom"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Mandelbrot Zoom pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 116: LGP Strange Attractor
    {PM_STR("LGP Strange Attractor"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Strange Attractor pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},

    // Index 117: LGP Kuramoto Sync
    {PM_STR("LGP Kuramoto Sync"), PatternFamily::MATHEMATICAL, PatternTags::CENTER_ORIGIN | PatternTags::PHYSICS,
     PM_STR("LGP Kuramoto Sync pattern"),
     PM_STR("Standard optical effects"), PM_STR("")},
};

const uint8_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

// ============================================================================
// Effect Count Parity Validation
// ============================================================================

// Expected number of implemented effects (must match registerAllEffects() return value)
constexpr uint8_t EXPECTED_EFFECT_COUNT = 68;

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

const PatternMetadata* getPatternMetadata(uint8_t index) {
    // Map effect ID to metadata index (stable ID mapping)
    // Effect IDs match v1/v2 registration order exactly
    // Phase 2 parity enforcement: only expose metadata for implemented effects
    if (index >= EXPECTED_EFFECT_COUNT) {
        return nullptr;
    }
    return &PATTERN_METADATA[index];
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

} // namespace PatternRegistry