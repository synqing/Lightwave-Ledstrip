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
    // New order: Effect, Palette, Speed, Mood, Fade, Complexity, Variation, Brightness
    {ParameterId::EffectId,   0, "effectId",   "effects.setCurrent", 0,   87,  0},  // Encoder 0: Effect (88 effects 0-87)
    {ParameterId::PaletteId,  1, "paletteId",  "parameters.set",     0,   74,  0},  // Encoder 1: Palette (75 palettes 0-74)
    {ParameterId::Speed,      2, "speed",      "parameters.set",     1,   100, 25}, // Encoder 2: Speed
    {ParameterId::Mood,       3, "mood",       "parameters.set",     0,   255, 0},  // Encoder 3: Mood
    {ParameterId::FadeAmount, 4, "fadeAmount", "parameters.set",     0,   255, 0},  // Encoder 4: Fade
    {ParameterId::Complexity, 5, "complexity", "parameters.set",     0,   255, 128}, // Encoder 5: Complexity
    {ParameterId::Variation,  6, "variation",  "parameters.set",     0,   255, 0},  // Encoder 6: Variation
    {ParameterId::Brightness, 7, "brightness", "parameters.set",     0,   255, 128}, // Encoder 7: Brightness

    // Unit B (8-15) - Zone parameters
    // Pattern: [Zone N Effect, Zone N Speed/Palette] pairs
    // Note: Encoders 9, 11, 13, 15 can toggle between Speed and Palette via button
    // Default mode is Speed; when toggled, encoder controls Palette instead
    {ParameterId::Zone0Effect, 8,  "zone0Effect", "zone.setEffect",   0,   87,  0},  // 88 effects (0-87)
    {ParameterId::Zone0Speed,  9,  "zone0Speed",  "zone.setSpeed",    1,   100, 25},  // Also zone0Palette when toggled
    {ParameterId::Zone1Effect, 10, "zone1Effect", "zone.setEffect",   0,   87,  0},  // 88 effects (0-87)
    {ParameterId::Zone1Speed,  11, "zone1Speed",  "zone.setSpeed",    1,   100, 25},  // Also zone1Palette when toggled
    {ParameterId::Zone2Effect, 12, "zone2Effect", "zone.setEffect",   0,   87,  0},  // 88 effects (0-87)
    {ParameterId::Zone2Speed,  13, "zone2Speed",  "zone.setSpeed",    1,   100, 25},  // Also zone2Palette when toggled
    {ParameterId::Zone3Effect, 14, "zone3Effect", "zone.setEffect",   0,   87,  0},  // 88 effects (0-87)
    {ParameterId::Zone3Speed,  15, "zone3Speed",  "zone.setSpeed",    1,   100, 25}   // Also zone3Palette when toggled
};

const ParameterDef* getParameterByIndex(uint8_t index) {
    if (index >= getParameterCount()) {
        return nullptr;
    }
    return &PARAMETER_TABLE[index];
}

const ParameterDef* getParameterById(ParameterId id) {
    uint8_t index = static_cast<uint8_t>(id);
    if (index >= getParameterCount()) {
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

    for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
        const ParameterDef* param = &PARAMETER_TABLE[i];
        s_parameterMetadata[i].min = param->min;
        s_parameterMetadata[i].max = param->max;
        s_parameterMetadata[i].isDynamic = false;  // Start with hardcoded defaults
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
