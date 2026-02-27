// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
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
    // Unit A (0-7) - Global LightwaveOS parameters
    // Note: Enum values match encoder indices for direct mapping
    EffectId = 0,     // Encoder 0
    PaletteId = 1,    // Encoder 1
    Speed = 2,        // Encoder 2
    Mood = 3,         // Encoder 3
    FadeAmount = 4,   // Encoder 4
    Complexity = 5,   // Encoder 5
    Variation = 6,    // Encoder 6
    Brightness = 7,   // Encoder 7
    // Unit B (8-15) - Zone parameters
    // Pattern: [Zone N Effect, Zone N Speed/Palette] pairs
    // Note: Encoders 9, 11, 13, 15 toggle between Speed and Palette via button
    Zone0Effect = 8,
    Zone0Speed = 9,      // Also Zone0Palette when button toggled
    Zone1Effect = 10,
    Zone1Speed = 11,     // Also Zone1Palette when button toggled
    Zone2Effect = 12,
    Zone2Speed = 13,     // Also Zone2Palette when button toggled
    Zone3Effect = 14,
    Zone3Speed = 15     // Also Zone3Palette when button toggled
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
