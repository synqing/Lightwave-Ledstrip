#pragma once
// ============================================================================
// ParameterMap - Parameter Definition Table for Tab5.encoder
// ============================================================================
// Single source of truth for parameter mapping.
// Maps encoder indices to parameter IDs, field names, and validation ranges.
// Eliminates duplicated mapping logic across encoder and WebSocket handlers.
//
// Supports 8 parameters from Unit A only:
// - Unit A (indices 0-7): Core LightwaveOS parameters
// - Unit B (indices 8-15): Encoders disabled, buttons used for preset management
//
// Ported from K1.8encoderS3, extended for dual-unit support.
// ============================================================================

#include <cstdint>

// Total number of encoder slots (16 total, but only 8 have parameters)
// Unit A (0-7): Has parameters
// Unit B (8-15): No parameters, but buttons/LEDs still work
constexpr uint8_t PARAMETER_COUNT = 16;

/**
 * Parameter identifiers matching encoder indices
 */
enum class ParameterId : uint8_t {
    // Unit A (0-7) - Global LightwaveOS parameters
    EffectId = 0,
    Brightness = 1,
    PaletteId = 2,
    Speed = 3,
    Mood = 4,
    FadeAmount = 5,
    Complexity = 6,
    Variation = 7,
    // Unit B (8-15) - No parameters assigned (encoders disabled)
    // Zone parameters have been removed from Unit B
    // Unit B buttons are still used for preset management
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
 * Get total number of encoder slots
 * @return Number of encoder slots (16 total, but only 8 have parameters)
 */
constexpr uint8_t getParameterCount() { return PARAMETER_COUNT; }

/**
 * Parameter metadata structure for runtime min/max values
 */
struct ParameterMetadata {
    uint8_t min;               // Minimum valid value
    uint8_t max;               // Maximum valid value
    bool isDynamic;            // true if max was updated from server, false if using hardcoded default
};

/**
 * Update parameter metadata with dynamic values from server
 * @param index Parameter index (0-15)
 * @param min Minimum value
 * @param max Maximum value
 */
void updateParameterMetadata(uint8_t index, uint8_t min, uint8_t max);

/**
 * Get parameter max value (dynamic if available, else falls back to hardcoded)
 * @param index Parameter index (0-15)
 * @return Maximum value for the parameter
 */
uint8_t getParameterMax(uint8_t index);

/**
 * Get parameter min value (dynamic if available, else falls back to hardcoded)
 * @param index Parameter index (0-15)
 * @return Minimum value for the parameter
 */
uint8_t getParameterMin(uint8_t index);
