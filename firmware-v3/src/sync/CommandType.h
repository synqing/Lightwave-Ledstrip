// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file CommandType.h
 * @brief Command type codes for sync serialization
 *
 * Defines 3-character codes for all 19 command types.
 * These codes are used in JSON serialization for compact transmission.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Command type enumeration for serialization
 *
 * Each value corresponds to a CQRS command class.
 */
enum class CommandType : uint8_t {
    // Effect commands
    SET_EFFECT          = 0,    // SetEffectCommand
    SET_BRIGHTNESS      = 1,    // SetBrightnessCommand
    SET_PALETTE         = 2,    // SetPaletteCommand
    SET_SPEED           = 3,    // SetSpeedCommand

    // Zone commands
    ZONE_ENABLE         = 4,    // ZoneEnableCommand
    ZONE_SET_EFFECT     = 5,    // ZoneSetEffectCommand
    ZONE_SET_PALETTE    = 6,    // ZoneSetPaletteCommand
    ZONE_SET_BRIGHTNESS = 7,    // ZoneSetBrightnessCommand
    ZONE_SET_SPEED      = 8,    // ZoneSetSpeedCommand
    SET_ZONE_MODE       = 9,    // SetZoneModeCommand

    // Transition commands
    TRIGGER_TRANSITION  = 10,   // TriggerTransitionCommand
    UPDATE_TRANSITION   = 11,   // UpdateTransitionCommand
    COMPLETE_TRANSITION = 12,   // CompleteTransitionCommand

    // Hue command
    INCREMENT_HUE       = 13,   // IncrementHueCommand

    // Visual parameter commands
    SET_VISUAL_PARAMS   = 14,   // SetVisualParamsCommand
    SET_INTENSITY       = 15,   // SetIntensityCommand
    SET_SATURATION      = 16,   // SetSaturationCommand
    SET_COMPLEXITY      = 17,   // SetComplexityCommand
    SET_VARIATION       = 18,   // SetVariationCommand

    UNKNOWN             = 255
};

constexpr uint8_t COMMAND_TYPE_COUNT = 19;

/**
 * @brief 3-character codes for JSON serialization
 *
 * Compact codes reduce message size over the wire.
 */
namespace CommandCodes {
    constexpr const char* SET_EFFECT          = "eff";
    constexpr const char* SET_BRIGHTNESS      = "bri";
    constexpr const char* SET_PALETTE         = "pal";
    constexpr const char* SET_SPEED           = "spd";

    constexpr const char* ZONE_ENABLE         = "zen";
    constexpr const char* ZONE_SET_EFFECT     = "zef";
    constexpr const char* ZONE_SET_PALETTE    = "zpa";
    constexpr const char* ZONE_SET_BRIGHTNESS = "zbr";
    constexpr const char* ZONE_SET_SPEED      = "zsp";
    constexpr const char* SET_ZONE_MODE       = "zmm";

    constexpr const char* TRIGGER_TRANSITION  = "ttr";
    constexpr const char* UPDATE_TRANSITION   = "utr";
    constexpr const char* COMPLETE_TRANSITION = "ctr";

    constexpr const char* INCREMENT_HUE       = "hue";

    constexpr const char* SET_VISUAL_PARAMS   = "vps";
    constexpr const char* SET_INTENSITY       = "int";
    constexpr const char* SET_SATURATION      = "sat";
    constexpr const char* SET_COMPLEXITY      = "cpx";
    constexpr const char* SET_VARIATION       = "var";
}

/**
 * @brief Get command code string from CommandType
 */
inline const char* commandTypeToCode(CommandType type) {
    switch (type) {
        case CommandType::SET_EFFECT:          return CommandCodes::SET_EFFECT;
        case CommandType::SET_BRIGHTNESS:      return CommandCodes::SET_BRIGHTNESS;
        case CommandType::SET_PALETTE:         return CommandCodes::SET_PALETTE;
        case CommandType::SET_SPEED:           return CommandCodes::SET_SPEED;
        case CommandType::ZONE_ENABLE:         return CommandCodes::ZONE_ENABLE;
        case CommandType::ZONE_SET_EFFECT:     return CommandCodes::ZONE_SET_EFFECT;
        case CommandType::ZONE_SET_PALETTE:    return CommandCodes::ZONE_SET_PALETTE;
        case CommandType::ZONE_SET_BRIGHTNESS: return CommandCodes::ZONE_SET_BRIGHTNESS;
        case CommandType::ZONE_SET_SPEED:      return CommandCodes::ZONE_SET_SPEED;
        case CommandType::SET_ZONE_MODE:       return CommandCodes::SET_ZONE_MODE;
        case CommandType::TRIGGER_TRANSITION:  return CommandCodes::TRIGGER_TRANSITION;
        case CommandType::UPDATE_TRANSITION:   return CommandCodes::UPDATE_TRANSITION;
        case CommandType::COMPLETE_TRANSITION: return CommandCodes::COMPLETE_TRANSITION;
        case CommandType::INCREMENT_HUE:       return CommandCodes::INCREMENT_HUE;
        case CommandType::SET_VISUAL_PARAMS:   return CommandCodes::SET_VISUAL_PARAMS;
        case CommandType::SET_INTENSITY:       return CommandCodes::SET_INTENSITY;
        case CommandType::SET_SATURATION:      return CommandCodes::SET_SATURATION;
        case CommandType::SET_COMPLEXITY:      return CommandCodes::SET_COMPLEXITY;
        case CommandType::SET_VARIATION:       return CommandCodes::SET_VARIATION;
        default: return "unk";
    }
}

/**
 * @brief Parse command code string to CommandType
 */
CommandType codeToCommandType(const char* code);

} // namespace sync
} // namespace lightwaveos
