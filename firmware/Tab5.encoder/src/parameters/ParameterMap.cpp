// ============================================================================
// ParameterMap - Parameter Definition Table for Tab5.encoder
// ============================================================================
// Extended for 16 parameters across dual M5ROTATE8 units.
// Ported from K1.8encoderS3.
// ============================================================================

#include "ParameterMap.h"
#include <cstring>

// Parameter definitions table - single source of truth
// Indices 0-7: Unit A (Core LightwaveOS parameters)
// Indices 8-15: Unit B (Placeholder parameters)
static const ParameterDef PARAMETER_TABLE[] = {
    // id, encoderIndex, statusField, wsCommandType, min, max, defaultValue

    // Unit A (0-7) - Core LightwaveOS parameters
    {ParameterId::EffectId,   0, "effectId",   "effects.setCurrent", 0,   95,  0},
    {ParameterId::Brightness, 1, "brightness", "parameters.set",     0,   255, 128},
    {ParameterId::PaletteId,  2, "paletteId",  "parameters.set",     0,   63,  0},
    {ParameterId::Speed,      3, "speed",      "parameters.set",     1,   100, 25},
    {ParameterId::Intensity,  4, "intensity",  "parameters.set",     0,   255, 128},
    {ParameterId::Saturation, 5, "saturation", "parameters.set",     0,   255, 255},
    {ParameterId::Complexity, 6, "complexity", "parameters.set",     0,   255, 128},
    {ParameterId::Variation,  7, "variation",  "parameters.set",     0,   255, 0},

    // Unit B (8-15) - Placeholder parameters (TBD functionality)
    {ParameterId::Param8,     8,  "param8",    "parameters.set",     0,   255, 128},
    {ParameterId::Param9,     9,  "param9",    "parameters.set",     0,   255, 128},
    {ParameterId::Param10,    10, "param10",   "parameters.set",     0,   255, 128},
    {ParameterId::Param11,    11, "param11",   "parameters.set",     0,   255, 128},
    {ParameterId::Param12,    12, "param12",   "parameters.set",     0,   255, 128},
    {ParameterId::Param13,    13, "param13",   "parameters.set",     0,   255, 128},
    {ParameterId::Param14,    14, "param14",   "parameters.set",     0,   255, 128},
    {ParameterId::Param15,    15, "param15",   "parameters.set",     0,   255, 128}
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
