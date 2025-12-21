#ifndef EFFECT_METADATA_H
#define EFFECT_METADATA_H

/**
 * @file EffectMetadata.h
 * @brief PROGMEM-stored metadata for LightwaveOS effects
 *
 * Provides rich metadata for each effect including:
 * - Category classification
 * - Feature flags (center origin, palette usage, etc.)
 * - Human-readable descriptions
 *
 * All data stored in PROGMEM to minimize RAM usage (~0 bytes RAM).
 * Flash cost: ~4KB for 47 effects
 */

#include <Arduino.h>

// ============================================================================
// Feature Flags (bitfield)
// ============================================================================
namespace EffectFeatures {
    constexpr uint8_t CENTER_ORIGIN  = 0x01;  // Originates from LED 79/80
    constexpr uint8_t USES_SPEED     = 0x02;  // Responds to speed parameter
    constexpr uint8_t USES_PALETTE   = 0x04;  // Uses current palette
    constexpr uint8_t ZONE_AWARE     = 0x08;  // Works with zone system
    constexpr uint8_t DUAL_STRIP     = 0x10;  // Designed for dual strips
    constexpr uint8_t PHYSICS_BASED  = 0x20;  // Uses physics simulation
    constexpr uint8_t AUDIO_REACTIVE = 0x40;  // Responds to audio input
}

// ============================================================================
// Category IDs
// ============================================================================
enum EffectCategory : uint8_t {
    CAT_CLASSIC = 0,
    CAT_SHOCKWAVE,
    CAT_LGP_INTERFERENCE,
    CAT_LGP_GEOMETRIC,
    CAT_LGP_ADVANCED,
    CAT_LGP_ORGANIC,
    CAT_LGP_QUANTUM,
    CAT_LGP_COLOR_MIXING,
    CAT_LGP_PHYSICS,
    CAT_LGP_NOVEL_PHYSICS,
    CAT_AUDIO_REACTIVE,
    CAT_COUNT  // Number of categories
};

// Category names in PROGMEM
const char CAT_NAME_CLASSIC[] PROGMEM = "Classic";
const char CAT_NAME_SHOCKWAVE[] PROGMEM = "Shockwave";
const char CAT_NAME_LGP_INTERFERENCE[] PROGMEM = "LGP Interference";
const char CAT_NAME_LGP_GEOMETRIC[] PROGMEM = "LGP Geometric";
const char CAT_NAME_LGP_ADVANCED[] PROGMEM = "LGP Advanced";
const char CAT_NAME_LGP_ORGANIC[] PROGMEM = "LGP Organic";
const char CAT_NAME_LGP_QUANTUM[] PROGMEM = "LGP Quantum";
const char CAT_NAME_LGP_COLOR_MIXING[] PROGMEM = "LGP Color Mixing";
const char CAT_NAME_LGP_PHYSICS[] PROGMEM = "LGP Physics";
const char CAT_NAME_LGP_NOVEL_PHYSICS[] PROGMEM = "LGP Novel Physics";
const char CAT_NAME_AUDIO_REACTIVE[] PROGMEM = "Audio Reactive";

const char* const CATEGORY_NAMES[] PROGMEM = {
    CAT_NAME_CLASSIC,
    CAT_NAME_SHOCKWAVE,
    CAT_NAME_LGP_INTERFERENCE,
    CAT_NAME_LGP_GEOMETRIC,
    CAT_NAME_LGP_ADVANCED,
    CAT_NAME_LGP_ORGANIC,
    CAT_NAME_LGP_QUANTUM,
    CAT_NAME_LGP_COLOR_MIXING,
    CAT_NAME_LGP_PHYSICS,
    CAT_NAME_LGP_NOVEL_PHYSICS,
    CAT_NAME_AUDIO_REACTIVE
};

// ============================================================================
// Effect Descriptions (PROGMEM)
// ============================================================================

// Classic (0-4)
const char DESC_0[] PROGMEM = "Realistic fire simulation radiating from center";
const char DESC_1[] PROGMEM = "Deep ocean wave patterns from center point";
const char DESC_2[] PROGMEM = "Smooth sine wave propagating from center";
const char DESC_3[] PROGMEM = "Water ripple effect expanding outward";
const char DESC_4[] PROGMEM = "Bouncing particle with palette trails";

// Shockwave (5-8)
const char DESC_5[] PROGMEM = "Energy pulse expanding from center";
const char DESC_6[] PROGMEM = "Dual waves colliding at center";
const char DESC_7[] PROGMEM = "Gravitational attraction to center point";
const char DESC_8[] PROGMEM = "Reserved for future shockwave effect";

// LGP Interference (9-12)
const char DESC_9[] PROGMEM = "Holographic interference patterns";
const char DESC_10[] PROGMEM = "Standing wave resonance modes";
const char DESC_11[] PROGMEM = "Scanning interference beam";
const char DESC_12[] PROGMEM = "Dual wave collision interference";

// LGP Geometric (13-15)
const char DESC_13[] PROGMEM = "Diamond lattice crystal pattern";
const char DESC_14[] PROGMEM = "Expanding concentric ring geometry";
const char DESC_15[] PROGMEM = "Radial star burst from center";

// LGP Advanced (16-21)
const char DESC_16[] PROGMEM = "Moire pattern optical illusion";
const char DESC_17[] PROGMEM = "Radial ripple propagation";
const char DESC_18[] PROGMEM = "Holographic vortex spiral";
const char DESC_19[] PROGMEM = "Chromatic shear displacement";
const char DESC_20[] PROGMEM = "Fresnel zone plate diffraction";
const char DESC_21[] PROGMEM = "Photonic crystal band structure";

// LGP Organic (22-24)
const char DESC_22[] PROGMEM = "Aurora borealis curtain effect";
const char DESC_23[] PROGMEM = "Bioluminescent wave patterns";
const char DESC_24[] PROGMEM = "Plasma membrane oscillation";

// LGP Quantum (25-33)
const char DESC_25[] PROGMEM = "Quantum tunneling probability waves";
const char DESC_26[] PROGMEM = "Gravitational lensing distortion";
const char DESC_27[] PROGMEM = "Discrete time crystal oscillation";
const char DESC_28[] PROGMEM = "Metamaterial cloaking gradient";
const char DESC_29[] PROGMEM = "GRIN lens cloaking effect";
const char DESC_30[] PROGMEM = "Caustic light fan projection";
const char DESC_31[] PROGMEM = "Birefringent shear splitting";
const char DESC_32[] PROGMEM = "Anisotropic cloaking field";
const char DESC_33[] PROGMEM = "Evanescent wave skin effect";

// LGP Color Mixing (34-35)
const char DESC_34[] PROGMEM = "Chromatic aberration RGB split";
const char DESC_35[] PROGMEM = "Color momentum acceleration";

// LGP Physics (36-41)
const char DESC_36[] PROGMEM = "Liquid crystal birefringence";
const char DESC_37[] PROGMEM = "Prism cascade light splitting";
const char DESC_38[] PROGMEM = "Silk-like flowing waves";
const char DESC_39[] PROGMEM = "Beam collision interference";
const char DESC_40[] PROGMEM = "Dual laser beam interaction";
const char DESC_41[] PROGMEM = "Tidal gravitational forces";

// LGP Novel Physics (42-46)
const char DESC_42[] PROGMEM = "Chladni plate vibration harmonics";
const char DESC_43[] PROGMEM = "Gravitational wave chirp signal";
const char DESC_44[] PROGMEM = "Quantum entanglement collapse";
const char DESC_45[] PROGMEM = "Mycelial network branching";
const char DESC_46[] PROGMEM = "Bridget Riley-inspired dissonance";

// Audio (47+)
const char DESC_47[] PROGMEM = "Audio-reactive spectrum analyzer";

// Description pointer array
const char* const EFFECT_DESCRIPTIONS[] PROGMEM = {
    DESC_0, DESC_1, DESC_2, DESC_3, DESC_4,
    DESC_5, DESC_6, DESC_7, DESC_8,
    DESC_9, DESC_10, DESC_11, DESC_12,
    DESC_13, DESC_14, DESC_15,
    DESC_16, DESC_17, DESC_18, DESC_19, DESC_20, DESC_21,
    DESC_22, DESC_23, DESC_24,
    DESC_25, DESC_26, DESC_27, DESC_28, DESC_29, DESC_30, DESC_31, DESC_32, DESC_33,
    DESC_34, DESC_35,
    DESC_36, DESC_37, DESC_38, DESC_39, DESC_40, DESC_41,
    DESC_42, DESC_43, DESC_44, DESC_45, DESC_46,
    DESC_47
};

// ============================================================================
// Per-Effect Parameter Definitions (Phase C.4)
// ============================================================================

/**
 * @brief Target parameter for effect-specific controls
 * Maps semantic names (e.g., "Flame Height") to underlying VisualParams fields
 */
enum class ParamTarget : uint8_t {
    INTENSITY = 0,   // Maps to visualParams.intensity
    SATURATION = 1,  // Maps to visualParams.saturation
    COMPLEXITY = 2,  // Maps to visualParams.complexity
    VARIATION = 3    // Maps to visualParams.variation
};

/**
 * @brief Definition for a single effect parameter
 * Stored inline in EffectMeta for PROGMEM efficiency
 */
struct EffectParamDef {
    char name[16];         // Human-readable name ("Flame Height")
    uint8_t minVal;        // Minimum value (typically 0)
    uint8_t maxVal;        // Maximum value (typically 255)
    uint8_t defaultVal;    // Default value
    ParamTarget target;    // Which VisualParam this maps to
};

// Helper to create empty param (for unused slots)
#define EMPTY_PARAM {"", 0, 0, 0, ParamTarget::INTENSITY}

// ============================================================================
// Effect Metadata Structure
// ============================================================================

/**
 * @brief Complete metadata for an effect
 * Includes category, features, and up to 4 custom parameter definitions
 */
struct EffectMeta {
    uint8_t category;
    uint8_t features;
    uint8_t paramCount;              // Number of custom params (0-4)
    EffectParamDef params[4];        // Parameter definitions (PROGMEM-safe)
};

// Shorthand for standard features
#define STD_FEATURES (EffectFeatures::CENTER_ORIGIN | EffectFeatures::USES_SPEED | EffectFeatures::USES_PALETTE | EffectFeatures::ZONE_AWARE)
#define LGP_FEATURES (EffectFeatures::CENTER_ORIGIN | EffectFeatures::USES_SPEED | EffectFeatures::USES_PALETTE | EffectFeatures::DUAL_STRIP | EffectFeatures::PHYSICS_BASED)
#define LGP_GEO_FEATURES (EffectFeatures::CENTER_ORIGIN | EffectFeatures::USES_SPEED | EffectFeatures::USES_PALETTE | EffectFeatures::DUAL_STRIP)

// Full metadata array with parameter definitions
// Effects with paramCount > 0 have custom parameter labels; others use defaults
const EffectMeta PROGMEM EFFECT_METADATA[] = {
    // =========================================================================
    // Classic Effects (0-4) - WITH CUSTOM PARAMS
    // =========================================================================

    // 0: Fire - 2 custom params
    {CAT_CLASSIC, STD_FEATURES, 2, {
        {"Flame Height", 0, 255, 180, ParamTarget::INTENSITY},
        {"Spark Rate", 0, 255, 100, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 1: Ocean - 3 custom params
    {CAT_CLASSIC, STD_FEATURES, 3, {
        {"Wave Height", 0, 255, 150, ParamTarget::INTENSITY},
        {"Turbulence", 0, 255, 128, ParamTarget::COMPLEXITY},
        {"Foam", 0, 255, 80, ParamTarget::VARIATION},
        EMPTY_PARAM
    }},

    // 2: Wave - 2 custom params
    {CAT_CLASSIC, STD_FEATURES, 2, {
        {"Amplitude", 0, 255, 180, ParamTarget::INTENSITY},
        {"Wavelength", 0, 255, 128, ParamTarget::COMPLEXITY},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 3: Ripple - 3 custom params
    {CAT_CLASSIC, STD_FEATURES, 3, {
        {"Ring Size", 0, 255, 150, ParamTarget::INTENSITY},
        {"Frequency", 0, 255, 100, ParamTarget::COMPLEXITY},
        {"Decay", 0, 255, 180, ParamTarget::VARIATION},
        EMPTY_PARAM
    }},

    // 4: Sinelon - 2 custom params
    {CAT_CLASSIC, STD_FEATURES, 2, {
        {"Trail Length", 0, 255, 200, ParamTarget::INTENSITY},
        {"Bounce Rate", 0, 255, 128, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // =========================================================================
    // Shockwave Effects (5-8) - WITH CUSTOM PARAMS
    // =========================================================================

    // 5: Shockwave - 2 custom params
    {CAT_SHOCKWAVE, STD_FEATURES, 2, {
        {"Pulse Width", 0, 255, 150, ParamTarget::INTENSITY},
        {"Expansion", 0, 255, 180, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 6: Collision - 2 custom params
    {CAT_SHOCKWAVE, STD_FEATURES, 2, {
        {"Impact Force", 0, 255, 200, ParamTarget::INTENSITY},
        {"Splash", 0, 255, 150, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 7: Gravity Well - 2 custom params
    {CAT_SHOCKWAVE, STD_FEATURES, 2, {
        {"Pull Strength", 0, 255, 180, ParamTarget::INTENSITY},
        {"Distortion", 0, 255, 100, ParamTarget::COMPLEXITY},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 8: Reserved
    {CAT_SHOCKWAVE, STD_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},

    // =========================================================================
    // LGP Interference (9-12) - WITH CUSTOM PARAMS
    // =========================================================================

    // 9: Holographic
    {CAT_LGP_INTERFERENCE, LGP_FEATURES, 2, {
        {"Fringe Width", 0, 255, 128, ParamTarget::INTENSITY},
        {"Phase Shift", 0, 255, 100, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 10: Standing Wave
    {CAT_LGP_INTERFERENCE, LGP_FEATURES, 2, {
        {"Node Count", 0, 255, 150, ParamTarget::COMPLEXITY},
        {"Resonance", 0, 255, 180, ParamTarget::INTENSITY},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 11: Scanning
    {CAT_LGP_INTERFERENCE, LGP_FEATURES, 2, {
        {"Beam Width", 0, 255, 100, ParamTarget::INTENSITY},
        {"Scan Rate", 0, 255, 150, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 12: Wave Collision
    {CAT_LGP_INTERFERENCE, LGP_FEATURES, 2, {
        {"Wave Count", 0, 255, 128, ParamTarget::COMPLEXITY},
        {"Interference", 0, 255, 180, ParamTarget::INTENSITY},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // =========================================================================
    // LGP Geometric (13-15) - WITH CUSTOM PARAMS
    // =========================================================================

    // 13: Diamond Lattice
    {CAT_LGP_GEOMETRIC, LGP_GEO_FEATURES, 2, {
        {"Facet Size", 0, 255, 150, ParamTarget::INTENSITY},
        {"Sparkle", 0, 255, 100, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 14: Concentric Rings
    {CAT_LGP_GEOMETRIC, LGP_GEO_FEATURES, 2, {
        {"Ring Count", 0, 255, 128, ParamTarget::COMPLEXITY},
        {"Expansion", 0, 255, 180, ParamTarget::VARIATION},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // 15: Star Burst
    {CAT_LGP_GEOMETRIC, LGP_GEO_FEATURES, 2, {
        {"Ray Count", 0, 255, 150, ParamTarget::COMPLEXITY},
        {"Brightness", 0, 255, 200, ParamTarget::INTENSITY},
        EMPTY_PARAM, EMPTY_PARAM
    }},

    // =========================================================================
    // LGP Advanced (16-21) - No custom params (use defaults)
    // =========================================================================
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 16: Moire
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 17: Radial Ripple
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 18: Vortex
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 19: Chromatic Shear
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 20: Fresnel
    {CAT_LGP_ADVANCED, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 21: Photonic Crystal

    // =========================================================================
    // LGP Organic (22-24) - No custom params
    // =========================================================================
    {CAT_LGP_ORGANIC, LGP_GEO_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 22: Aurora
    {CAT_LGP_ORGANIC, LGP_GEO_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 23: Bioluminescent
    {CAT_LGP_ORGANIC, LGP_GEO_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 24: Plasma Membrane

    // =========================================================================
    // LGP Quantum (25-33) - No custom params
    // =========================================================================
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 25: Quantum Tunneling
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 26: Gravitational Lens
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 27: Time Crystal
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 28: Metamaterial
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 29: GRIN Cloak
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 30: Caustic Fan
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 31: Birefringent
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 32: Anisotropic
    {CAT_LGP_QUANTUM, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 33: Evanescent

    // =========================================================================
    // LGP Color Mixing (34-35) - No custom params
    // =========================================================================
    {CAT_LGP_COLOR_MIXING, LGP_GEO_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 34: Chromatic Aberration
    {CAT_LGP_COLOR_MIXING, LGP_GEO_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 35: Color Momentum

    // =========================================================================
    // LGP Physics (36-41) - No custom params
    // =========================================================================
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 36: Liquid Crystal
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 37: Prism Cascade
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 38: Silk Flow
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 39: Beam Collision
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 40: Dual Laser
    {CAT_LGP_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 41: Tidal

    // =========================================================================
    // LGP Novel Physics (42-46) - No custom params
    // =========================================================================
    {CAT_LGP_NOVEL_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 42: Chladni
    {CAT_LGP_NOVEL_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 43: Gravitational Chirp
    {CAT_LGP_NOVEL_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 44: Entanglement
    {CAT_LGP_NOVEL_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 45: Mycelial
    {CAT_LGP_NOVEL_PHYSICS, LGP_FEATURES, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}},  // 46: Riley Dissonance

    // =========================================================================
    // Audio Reactive (47+) - No custom params
    // =========================================================================
    {CAT_AUDIO_REACTIVE, EffectFeatures::CENTER_ORIGIN | EffectFeatures::USES_PALETTE | EffectFeatures::AUDIO_REACTIVE, 0, {EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM, EMPTY_PARAM}}
};

// Number of metadata entries
constexpr uint8_t EFFECT_METADATA_COUNT = sizeof(EFFECT_METADATA) / sizeof(EffectMeta);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get metadata for an effect by ID
 * @param effectId Effect index
 * @return EffectMeta structure (read from PROGMEM)
 */
inline EffectMeta getEffectMeta(uint8_t effectId) {
    if (effectId >= EFFECT_METADATA_COUNT) {
        return {CAT_CLASSIC, 0};  // Default fallback
    }
    EffectMeta meta;
    memcpy_P(&meta, &EFFECT_METADATA[effectId], sizeof(EffectMeta));
    return meta;
}

/**
 * @brief Get category name for an effect
 * @param effectId Effect index
 * @param buffer Output buffer (should be at least 20 chars)
 */
inline void getEffectCategoryName(uint8_t effectId, char* buffer, size_t bufferSize) {
    EffectMeta meta = getEffectMeta(effectId);
    if (meta.category < CAT_COUNT) {
        strncpy_P(buffer, (char*)pgm_read_ptr(&CATEGORY_NAMES[meta.category]), bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        strncpy(buffer, "Unknown", bufferSize);
    }
}

/**
 * @brief Get description for an effect
 * @param effectId Effect index
 * @param buffer Output buffer (should be at least 60 chars)
 */
inline void getEffectDescription(uint8_t effectId, char* buffer, size_t bufferSize) {
    if (effectId < sizeof(EFFECT_DESCRIPTIONS) / sizeof(EFFECT_DESCRIPTIONS[0])) {
        strncpy_P(buffer, (char*)pgm_read_ptr(&EFFECT_DESCRIPTIONS[effectId]), bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        strncpy(buffer, "No description available", bufferSize);
    }
}

/**
 * @brief Check if effect has a specific feature
 * @param effectId Effect index
 * @param feature Feature flag to check
 * @return true if effect has the feature
 */
inline bool effectHasFeature(uint8_t effectId, uint8_t feature) {
    EffectMeta meta = getEffectMeta(effectId);
    return (meta.features & feature) != 0;
}

#endif // EFFECT_METADATA_H
