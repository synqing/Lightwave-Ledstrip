// ============================================================================
// ParameterMap - Parameter Definition Table for Tab5.encoder
// ============================================================================
// Extended for 16 parameters across dual M5ROTATE8 units.
// Ported from K1.8encoderS3.
// ============================================================================

#include "ParameterMap.h"
#include <Arduino.h>
#include <cstring>

// Runtime metadata storage (initialized from PARAMETER_TABLE, can be updated dynamically)
static ParameterMetadata s_parameterMetadata[PARAMETER_COUNT];
static bool s_metadataInitialized = false;

// Parameter definitions table - single source of truth
// Indices 0-7: Unit A (Global LightwaveOS parameters)
// Indices 8-15: Unit B (Zone parameters)
static const ParameterDef PARAMETER_TABLE[] = {
    // id, encoderIndex, statusField, wsCommandType, min, max, defaultValue

    // Unit A (0-7) - Global LightwaveOS parameters
    // Note: Effect max=95 (0-95 = 96 effects, safe default for future expansion)
    // Actual max is dynamically updated from v2 server (currently 92 effects, max=91)
    // v2 firmware: EXPECTED_EFFECT_COUNT = 92 (see CoreEffects.cpp)
    // Note: Palette max=74 (0-74 = 75 palettes, matches v2 MASTER_PALETTE_COUNT=75)
    {ParameterId::EffectId,   0, "effectId",   "effects.setCurrent", 0,   95,  0},
    {ParameterId::Brightness, 1, "brightness", "parameters.set",     0,   255, 128},
    {ParameterId::PaletteId,  2, "paletteId",  "parameters.set",     0,   74,  0},
    {ParameterId::Speed,      3, "speed",      "parameters.set",     1,   100, 25},
    {ParameterId::Mood,       4, "mood",       "parameters.set",     0,   255, 0},
    {ParameterId::FadeAmount, 5, "fadeAmount", "parameters.set",     0,   255, 0},
    {ParameterId::Complexity, 6, "complexity", "parameters.set",     0,   255, 128},
    {ParameterId::Variation,  7, "variation",  "parameters.set",     0,   255, 0},

    // Unit B (8-15) - No parameters assigned (encoders disabled/unused)
    // Zone parameters have been removed from Unit B
    // Unit B buttons are still used for preset management
};

const ParameterDef* getParameterByIndex(uint8_t index) {
    if (index >= getParameterCount()) {
        return nullptr;
    }
    // Unit B (8-15) no longer has parameters assigned
    if (index >= 8) {
        return nullptr;
    }
    return &PARAMETER_TABLE[index];
}

const ParameterDef* getParameterById(ParameterId id) {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= getParameterCount()) {
        return nullptr;
    }
    // Unit B (8-15) no longer has parameters assigned
    if (index >= 8) {
        return nullptr;
    }
    return &PARAMETER_TABLE[index];
}

const ParameterDef* getParameterByField(const char* fieldName) {
    if (!fieldName) {
        return nullptr;
    }

    for (uint8_t i = 0; i < getParameterCount(); i++) {
        if (strcmp(PARAMETER_TABLE[i].statusField, fieldName) == 0) {
            return &PARAMETER_TABLE[i];
        }
    }

    return nullptr;
}

// Initialize metadata from hardcoded defaults (called on first access)
static void initializeMetadata() {
    if (s_metadataInitialized) {
        return;
    }

    // Only initialize metadata for Unit A (0-7) - Unit B (8-15) has no parameters
    for (uint8_t i = 0; i < 8; i++) {
        const ParameterDef* param = &PARAMETER_TABLE[i];
        s_parameterMetadata[i].min = param->min;
        s_parameterMetadata[i].max = param->max;
        s_parameterMetadata[i].isDynamic = false;  // Start with hardcoded defaults
    }
    // Unit B (8-15): Initialize with safe defaults (no parameters assigned)
    for (uint8_t i = 8; i < PARAMETER_COUNT; i++) {
        s_parameterMetadata[i].min = 0;
        s_parameterMetadata[i].max = 255;
        s_parameterMetadata[i].isDynamic = false;
    }

    s_metadataInitialized = true;
}

void updateParameterMetadata(uint8_t index, uint8_t min, uint8_t max) {
    if (index >= PARAMETER_COUNT) {
        return;
    }

    // Ensure metadata is initialized
    if (!s_metadataInitialized) {
        initializeMetadata();
    }

    s_parameterMetadata[index].min = min;
    s_parameterMetadata[index].max = max;
    s_parameterMetadata[index].isDynamic = true;

    // Log the update for debugging
    const ParameterDef* param = getParameterByIndex(index);
    if (param) {
        Serial.printf("[ParamMap] Updated metadata for %s: min=%u, max=%u (was %u)\n",
                      param->statusField, min, max, param->max);
    }
}

uint8_t getParameterMax(uint8_t index) {
    if (index >= PARAMETER_COUNT) {
        return 255;  // Safe fallback
    }

    // Ensure metadata is initialized
    if (!s_metadataInitialized) {
        initializeMetadata();
    }

    // Return dynamic max if available, else fall back to hardcoded
    if (s_parameterMetadata[index].isDynamic) {
        return s_parameterMetadata[index].max;
    }

    // Fallback to hardcoded value from PARAMETER_TABLE
    const ParameterDef* param = getParameterByIndex(index);
    return param ? param->max : 255;
}

uint8_t getParameterMin(uint8_t index) {
    if (index >= PARAMETER_COUNT) {
        return 0;  // Safe fallback
    }

    // Ensure metadata is initialized
    if (!s_metadataInitialized) {
        initializeMetadata();
    }

    // Return dynamic min if available, else fall back to hardcoded
    if (s_parameterMetadata[index].isDynamic) {
        return s_parameterMetadata[index].min;
    }

    // Fallback to hardcoded value from PARAMETER_TABLE
    const ParameterDef* param = getParameterByIndex(index);
    return param ? param->min : 0;
}
