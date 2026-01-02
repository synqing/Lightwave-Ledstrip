#pragma once
// ============================================================================
// ParameterMap - Parameter Definition Table for Tab5.encoder
// ============================================================================
// Single source of truth for parameter mapping.
// Maps encoder indices to parameter IDs, field names, and validation ranges.
// Eliminates duplicated mapping logic across encoder and WebSocket handlers.
//
// Supports 16 parameters across dual M5ROTATE8 units:
// - Unit A (indices 0-7): Core LightwaveOS parameters
// - Unit B (indices 8-15): Placeholder parameters (TBD)
//
// Ported from K1.8encoderS3, extended for dual-unit support.
// ============================================================================

#include <cstdint>

// Total number of parameters (dual M5ROTATE8 = 16 encoders)
constexpr uint8_t PARAMETER_COUNT = 16;

/**
 * Parameter identifiers matching encoder indices
 */
enum class ParameterId : uint8_t {
    // Unit A (0-7) - Core LightwaveOS parameters
    EffectId = 0,
    Brightness = 1,
    PaletteId = 2,
    Speed = 3,
    Intensity = 4,
    Saturation = 5,
    Complexity = 6,
    Variation = 7,
    // Unit B (8-15) - Placeholder parameters
    Param8 = 8,
    Param9 = 9,
    Param10 = 10,
    Param11 = 11,
    Param12 = 12,
    Param13 = 13,
    Param14 = 14,
    Param15 = 15
};

/**
 * Parameter definition structure
 */
struct ParameterDef {
    ParameterId id;            // Parameter identifier
    uint8_t encoderIndex;      // Encoder/parameter index (0-15)
    const char* statusField;   // Field name in LightwaveOS "status" message
    const char* wsCommandType; // WebSocket command type for sending changes
    uint8_t min;               // Minimum valid value
    uint8_t max;               // Maximum valid value
    uint8_t defaultValue;      // Default/reset value
};

/**
 * Get parameter definition by encoder index
 * @param index Encoder index (0-15)
 * @return Parameter definition, or nullptr if invalid
 */
const ParameterDef* getParameterByIndex(uint8_t index);

/**
 * Get parameter definition by parameter ID
 * @param id Parameter ID
 * @return Parameter definition, or nullptr if invalid
 */
const ParameterDef* getParameterById(ParameterId id);

/**
 * Get parameter definition by status field name
 * @param fieldName Field name from "status" message (e.g., "brightness", "effectId")
 * @return Parameter definition, or nullptr if not found
 */
const ParameterDef* getParameterByField(const char* fieldName);

/**
 * Get total number of parameters
 * @return Number of parameters (16)
 */
constexpr uint8_t getParameterCount() { return PARAMETER_COUNT; }
