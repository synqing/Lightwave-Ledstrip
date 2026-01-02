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

    // Unit B (8-15) - Zone parameters
    // Zone Effect: 0-95 (wraps for continuous scrolling), Zone Brightness: 0-255 (clamped)
    // WebSocket: zones.setEffect for effect, zones.setBrightness for brightness
    {ParameterId::Zone0Effect,     8,  "zone0Effect",     "zones.setEffect",     0,   95,  0},
    {ParameterId::Zone0Brightness, 9,  "zone0Brightness", "zones.setBrightness", 0,   255, 128},
    {ParameterId::Zone1Effect,     10, "zone1Effect",     "zones.setEffect",     0,   95,  0},
    {ParameterId::Zone1Brightness, 11, "zone1Brightness", "zones.setBrightness", 0,   255, 128},
    {ParameterId::Zone2Effect,     12, "zone2Effect",     "zones.setEffect",     0,   95,  0},
    {ParameterId::Zone2Brightness, 13, "zone2Brightness", "zones.setBrightness", 0,   255, 128},
    {ParameterId::Zone3Effect,     14, "zone3Effect",     "zones.setEffect",     0,   95,  0},
    {ParameterId::Zone3Brightness, 15, "zone3Brightness", "zones.setBrightness", 0,   255, 128}
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
