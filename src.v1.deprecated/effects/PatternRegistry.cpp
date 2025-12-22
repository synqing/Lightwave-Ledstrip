/**
 * Pattern Registry Implementation
 * 
 * Provides metadata for all registered patterns, enabling runtime discovery
 * and filtering by family, tags, and relationships.
 */

#include "PatternRegistry.h"
#include "effects.h"  // For effects[] array

// ============================================================================
// Pattern Metadata Definitions (PROGMEM)
// ============================================================================

// Helper macros for PROGMEM strings
#define PM_STR(s) (const char*)(s)

// Pattern metadata entries - mapped to effects[] array indices
const PatternMetadata PATTERN_METADATA[] PROGMEM = {
    // Index 0-8: Classic/Shockwave effects (not LGP-specific, but included for completeness)
    {PM_STR("Fire"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN, 
     PM_STR("Realistic fire simulation radiating from centre"), 
     PM_STR("Thermal convection, intensity gradient"), PM_STR("")},
    
    {PM_STR("Ocean"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Deep ocean wave patterns from centre point"),
     PM_STR("Wave propagation, depth gradient"), PM_STR("")},
    
    {PM_STR("Wave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Smooth sine wave propagating from centre"),
     PM_STR("Wave propagation, phase control"), PM_STR("")},
    
    {PM_STR("Ripple"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Water ripple effect expanding outward"),
     PM_STR("Radial wave propagation, decay"), PM_STR("")},
    
    {PM_STR("Sinelon"), PatternFamily::GEOMETRIC, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Bouncing particle with palette trails"),
     PM_STR("Position-based motion, trail decay"), PM_STR("")},
    
    {PM_STR("Shockwave"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Energy pulse expanding from centre"),
     PM_STR("Radial expansion, intensity falloff"), PM_STR("Collision")},
    
    {PM_STR("Collision"), PatternFamily::FLUID_PLASMA, PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Dual waves colliding at centre"),
     PM_STR("Wave interference, collision dynamics"), PM_STR("Shockwave")},
    
    {PM_STR("Gravity Well"), PatternFamily::PHYSICS_BASED, PatternTags::CENTER_ORIGIN,
     PM_STR("Gravitational attraction to centre point"),
     PM_STR("Inverse square law, radial gradient"), PM_STR("")},
    
    // Index 9-12: LGP Interference Effects
    {PM_STR("LGP Holographic"), PatternFamily::INTERFERENCE, 
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE | PatternTags::DEPTH,
     PM_STR("Holographic interference patterns through multi-layer depth"),
     PM_STR("Multi-layer interference, phase relationships, depth illusion"), 
     PM_STR("LGP Modal Resonance, LGP Interference Scanner")},
    
    {PM_STR("LGP Modal Resonance"), PatternFamily::INTERFERENCE,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING,
     PM_STR("Explores different optical cavity resonance modes"),
     PM_STR("Cavity modes, standing wave patterns, mode selection"), 
     PM_STR("LGP Holographic, LGP Standing Wave")},
    
    {PM_STR("LGP Interference Scanner"), PatternFamily::INTERFERENCE,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Scanning interference beam creating moving patterns"),
     PM_STR("Traveling interference, phase scanning, beam dynamics"), 
     PM_STR("LGP Holographic, LGP Wave Collision")},
    
    {PM_STR("LGP Wave Collision"), PatternFamily::INTERFERENCE,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Dual wave collision interference at centre"),
     PM_STR("Wave interference, collision dynamics, constructive/destructive zones"), 
     PM_STR("LGP Interference Scanner, Collision")},
    
    // Index 13-15: LGP Geometric Effects
    {PM_STR("LGP Diamond Lattice"), PatternFamily::GEOMETRIC,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Diamond/rhombus patterns through angular interference"),
     PM_STR("Angular wave fronts, X-ray crystallography patterns, diamond nodes"), 
     PM_STR("LGP Hexagonal Grid, LGP Concentric Rings")},
    
    {PM_STR("LGP Concentric Rings"), PatternFamily::GEOMETRIC,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Expanding concentric ring geometry from centre"),
     PM_STR("Radial expansion, ring spacing, geometric progression"), 
     PM_STR("LGP Diamond Lattice, Ripple")},
    
    {PM_STR("LGP Star Burst"), PatternFamily::GEOMETRIC,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Radial star burst pattern from centre"),
     PM_STR("Radial rays, angular distribution, intensity falloff"), 
     PM_STR("LGP Concentric Rings")},
    
    // Index 16-21: LGP Advanced Effects
    {PM_STR("LGP Moiré Curtains"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Moiré pattern optical illusion through interference"),
     PM_STR("Spatial frequency beating, moiré interference, curtain effect"), 
     PM_STR("LGP Holographic, LGP Diamond Lattice")},
    
    {PM_STR("LGP Radial Ripple"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Radial ripple propagation with interference"),
     PM_STR("Radial wave propagation, interference zones, ripple dynamics"), 
     PM_STR("Ripple, LGP Concentric Rings")},
    
    {PM_STR("LGP Holographic Vortex"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Holographic vortex spiral pattern"),
     PM_STR("Spiral wave propagation, vortex dynamics, depth illusion"), 
     PM_STR("LGP Holographic, LGP Radial Ripple")},
    
    {PM_STR("LGP Chromatic Shear"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Chromatic shear displacement effect"),
     PM_STR("Wavelength-dependent refraction, chromatic dispersion, shear mapping"), 
     PM_STR("LGP Chromatic Aberration")},
    
    {PM_STR("LGP Fresnel Zones"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Fresnel zone plate diffraction pattern"),
     PM_STR("Fresnel zones, diffraction patterns, alternating zones"), 
     PM_STR("LGP Concentric Rings, LGP Moiré Curtains")},
    
    {PM_STR("LGP Photonic Crystal"), PatternFamily::ADVANCED_OPTICAL,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Photonic crystal band structure visualization"),
     PM_STR("Band structure, periodic modulation, photonic band gaps"), 
     PM_STR("LGP Diamond Lattice, LGP Moiré Curtains")},
    
    // Index 22-24: LGP Organic Effects
    {PM_STR("LGP Aurora Borealis"), PatternFamily::ORGANIC,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Aurora borealis curtain effect"),
     PM_STR("Curtain dynamics, colour gradients, wave motion"), 
     PM_STR("LGP Bioluminescent, LGP Plasma Membrane")},
    
    {PM_STR("LGP Bioluminescent"), PatternFamily::ORGANIC,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Bioluminescent wave patterns"),
     PM_STR("Wave propagation, organic motion, intensity pulsing"), 
     PM_STR("LGP Aurora Borealis, Ocean")},
    
    {PM_STR("LGP Plasma Membrane"), PatternFamily::ORGANIC,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Plasma membrane oscillation effect"),
     PM_STR("Membrane dynamics, oscillation patterns, fluid motion"), 
     PM_STR("LGP Aurora Borealis, LGP Liquid Crystal")},
    
    // Index 25-33: LGP Quantum Effects
    {PM_STR("LGP Quantum Tunneling"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Quantum tunneling probability waves"),
     PM_STR("Wave function probability, tunneling dynamics, quantum mechanics"), 
     PM_STR("LGP Quantum Entangle, LGP Wave Function")},
    
    {PM_STR("LGP Gravitational Lens"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gravitational lensing distortion effect"),
     PM_STR("Lens distortion, gravitational effects, light bending"), 
     PM_STR("LGP GRIN Cloak, Gravity Well")},
    
    {PM_STR("LGP Time Crystal"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Discrete time crystal oscillation"),
     PM_STR("Time crystal dynamics, periodic oscillation, symmetry breaking"), 
     PM_STR("LGP Quantum Tunneling")},
    
    {PM_STR("LGP Metamaterial Cloak"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Metamaterial cloaking gradient with negative refractive index"),
     PM_STR("Negative refractive index, cloaking dynamics, destructive interference"), 
     PM_STR("LGP GRIN Cloak, LGP Anisotropic Cloak")},
    
    {PM_STR("LGP GRIN Cloak"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gradient index lens cloaking effect"),
     PM_STR("Gradient index, lens dynamics, cloaking"), 
     PM_STR("LGP Metamaterial Cloak, LGP Gravitational Lens")},
    
    {PM_STR("LGP Caustic Fan"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Caustic light fan projection"),
     PM_STR("Caustic patterns, light focusing, fan projection"), 
     PM_STR("LGP Fresnel Zones")},
    
    {PM_STR("LGP Birefringent Shear"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Birefringent shear splitting effect"),
     PM_STR("Birefringence, shear splitting, polarization effects"), 
     PM_STR("LGP Chromatic Shear, LGP Chromatic Aberration")},
    
    {PM_STR("LGP Anisotropic Cloak"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Anisotropic cloaking field effect"),
     PM_STR("Anisotropic materials, directional cloaking, field effects"), 
     PM_STR("LGP Metamaterial Cloak, LGP GRIN Cloak")},
    
    {PM_STR("LGP Evanescent Skin"), PatternFamily::QUANTUM,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Evanescent wave skin effect"),
     PM_STR("Evanescent waves, skin depth, exponential decay"), 
     PM_STR("LGP Metamaterial Cloak")},
    
    // Index 34-35: LGP Color Mixing Effects
    {PM_STR("LGP Chromatic Aberration"), PatternFamily::COLOR_MIXING,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Chromatic aberration RGB split through wavelength-dependent refraction"),
     PM_STR("Chromatic dispersion, wavelength-dependent focus, RGB separation"), 
     PM_STR("LGP Chromatic Shear, LGP Birefringent Shear")},
    
    {PM_STR("LGP Color Accelerator"), PatternFamily::COLOR_MIXING,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Color momentum acceleration effect"),
     PM_STR("Momentum dynamics, acceleration patterns, colour motion"), 
     PM_STR("LGP Chromatic Aberration")},
    
    // Index 36-41: LGP Physics-Based Effects
    {PM_STR("LGP Liquid Crystal"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Liquid crystal birefringence effect"),
     PM_STR("Liquid crystal dynamics, birefringence, director field"), 
     PM_STR("LGP Birefringent Shear, LGP Plasma Membrane")},
    
    {PM_STR("LGP Prism Cascade"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::SPECTRAL,
     PM_STR("Prism cascade light splitting effect"),
     PM_STR("Prism refraction, cascade dynamics, spectral splitting"), 
     PM_STR("LGP Chromatic Aberration, LGP Caustic Fan")},
    
    {PM_STR("LGP Silk Waves"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Silk-like flowing wave patterns"),
     PM_STR("Fluid dynamics, silk-like motion, wave flow"), 
     PM_STR("LGP Bioluminescent, Ocean")},
    
    {PM_STR("LGP Beam Collision"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Beam collision interference pattern"),
     PM_STR("Beam dynamics, collision interference, wave interaction"), 
     PM_STR("LGP Wave Collision, Collision")},
    
    {PM_STR("LGP Laser Duel"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::TRAVELING,
     PM_STR("Dual laser beam interaction effect"),
     PM_STR("Laser beam dynamics, dual beam interaction, interference"), 
     PM_STR("LGP Beam Collision, LGP Wave Collision")},
    
    {PM_STR("LGP Tidal Forces"), PatternFamily::PHYSICS_BASED,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Tidal gravitational forces effect"),
     PM_STR("Tidal forces, gravitational effects, force dynamics"), 
     PM_STR("Gravity Well, LGP Gravitational Lens")},
    
    // Index 42-46: LGP Novel Physics Effects
    {PM_STR("LGP Chladni Harmonics"), PatternFamily::NOVEL_PHYSICS,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::STANDING,
     PM_STR("Chladni plate vibration harmonics"),
     PM_STR("Vibration harmonics, nodal patterns, Chladni figures"), 
     PM_STR("LGP Modal Resonance, LGP Standing Wave")},
    
    {PM_STR("LGP Gravitational Chirp"), PatternFamily::NOVEL_PHYSICS,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Gravitational wave chirp signal"),
     PM_STR("Gravitational waves, chirp signal, frequency evolution"), 
     PM_STR("LGP Gravitational Lens, LGP Tidal Forces")},
    
    {PM_STR("LGP Quantum Entangle"), PatternFamily::NOVEL_PHYSICS,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::PHYSICS,
     PM_STR("Quantum entanglement collapse visualization"),
     PM_STR("Quantum entanglement, wave function collapse, correlation"), 
     PM_STR("LGP Quantum Tunneling, LGP Time Crystal")},
    
    {PM_STR("LGP Mycelial Network"), PatternFamily::NOVEL_PHYSICS,
     PatternTags::CENTER_ORIGIN | PatternTags::TRAVELING,
     PM_STR("Mycelial network branching pattern"),
     PM_STR("Network branching, organic growth, fractal patterns"), 
     PM_STR("LGP Bioluminescent, LGP Aurora Borealis")},
    
    {PM_STR("LGP Riley Dissonance"), PatternFamily::NOVEL_PHYSICS,
     PatternTags::CENTER_ORIGIN | PatternTags::DUAL_STRIP | PatternTags::MOIRE,
     PM_STR("Bridget Riley-inspired optical dissonance"),
     PM_STR("Optical art, moiré patterns, visual dissonance"), 
     PM_STR("LGP Moiré Curtains, LGP Diamond Lattice")},
    
    // Add more entries as needed for remaining effects...
    // Note: This is a subset - full implementation would include all effects
};

const uint8_t PATTERN_METADATA_COUNT = sizeof(PATTERN_METADATA) / sizeof(PatternMetadata);

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
    // Map effect index to metadata index
    // Note: This assumes effects[] array order matches PATTERN_METADATA order
    if (index >= PATTERN_METADATA_COUNT) {
        return nullptr;
    }
    return &PATTERN_METADATA[index];
}

uint8_t getPatternsByFamily(PatternFamily family, uint8_t* output, uint8_t maxOutput) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < PATTERN_METADATA_COUNT && count < maxOutput; i++) {
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
    return PATTERN_METADATA_COUNT;
}

uint8_t getFamilyCount(PatternFamily family) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < PATTERN_METADATA_COUNT; i++) {
        PatternMetadata meta;
        memcpy_P(&meta, &PATTERN_METADATA[i], sizeof(PatternMetadata));
        if (meta.family == family) {
            count++;
        }
    }
    return count;
}

} // namespace PatternRegistry

