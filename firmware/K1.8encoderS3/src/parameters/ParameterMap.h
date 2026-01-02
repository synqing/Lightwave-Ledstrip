#pragma once

#include <cstdint>

/**
 * @file ParameterMap.h
 * @brief Single source of truth for parameter mapping
 * 
 * Maps encoder indices to parameter IDs, field names, and validation ranges.
 * Eliminates duplicated mapping logic across onEncoderChange() and onWebSocketMessage().
 */

enum class ParameterId : uint8_t {
    EffectId = 0,
    Brightness = 1,
    PaletteId = 2,
    Speed = 3,
    Intensity = 4,
    Saturation = 5,
    Complexity = 6,
    Variation = 7
};

struct ParameterDef {
    ParameterId id;
    uint8_t encoderIndex;      // EncoderController::Parameter index (0-7)
    const char* statusField;   // Field name in LightwaveOS "status" message
    const char* wsCommandType; // WebSocket command type for sending changes
    uint8_t min;
    uint8_t max;
    uint8_t defaultValue;
};

/**
 * @brief Get parameter definition by encoder index
 * @param index Encoder index (0-7)
 * @return Parameter definition, or nullptr if invalid
 */
const ParameterDef* getParameterByIndex(uint8_t index);

/**
 * @brief Get parameter definition by parameter ID
 * @param id Parameter ID
 * @return Parameter definition, or nullptr if invalid
 */
const ParameterDef* getParameterById(ParameterId id);

/**
 * @brief Get parameter definition by status field name
 * @param fieldName Field name from "status" message (e.g., "brightness", "effectId")
 * @return Parameter definition, or nullptr if not found
 */
const ParameterDef* getParameterByField(const char* fieldName);

/**
 * @brief Get total number of parameters
 * @return Number of parameters (8)
 */
constexpr uint8_t getParameterCount() { return 8; }

