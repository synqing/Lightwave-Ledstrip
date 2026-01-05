/**
 * @file CommandSerializer.cpp
 * @brief Command serialization implementation
 */

#include "CommandSerializer.h"
#include "SyncProtocol.h"
#include <cstring>
#include <cstdio>

#ifdef NATIVE_BUILD
#define millis() 0
#else
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace sync {

// Implementation of codeToCommandType from CommandType.h
CommandType codeToCommandType(const char* code) {
    if (!code || strlen(code) != 3) return CommandType::UNKNOWN;

    if (strcmp(code, CommandCodes::SET_EFFECT) == 0)          return CommandType::SET_EFFECT;
    if (strcmp(code, CommandCodes::SET_BRIGHTNESS) == 0)      return CommandType::SET_BRIGHTNESS;
    if (strcmp(code, CommandCodes::SET_PALETTE) == 0)         return CommandType::SET_PALETTE;
    if (strcmp(code, CommandCodes::SET_SPEED) == 0)           return CommandType::SET_SPEED;
    if (strcmp(code, CommandCodes::ZONE_ENABLE) == 0)         return CommandType::ZONE_ENABLE;
    if (strcmp(code, CommandCodes::ZONE_SET_EFFECT) == 0)     return CommandType::ZONE_SET_EFFECT;
    if (strcmp(code, CommandCodes::ZONE_SET_PALETTE) == 0)    return CommandType::ZONE_SET_PALETTE;
    if (strcmp(code, CommandCodes::ZONE_SET_BRIGHTNESS) == 0) return CommandType::ZONE_SET_BRIGHTNESS;
    if (strcmp(code, CommandCodes::ZONE_SET_SPEED) == 0)      return CommandType::ZONE_SET_SPEED;
    if (strcmp(code, CommandCodes::SET_ZONE_MODE) == 0)       return CommandType::SET_ZONE_MODE;
    if (strcmp(code, CommandCodes::TRIGGER_TRANSITION) == 0)  return CommandType::TRIGGER_TRANSITION;
    if (strcmp(code, CommandCodes::UPDATE_TRANSITION) == 0)   return CommandType::UPDATE_TRANSITION;
    if (strcmp(code, CommandCodes::COMPLETE_TRANSITION) == 0) return CommandType::COMPLETE_TRANSITION;
    if (strcmp(code, CommandCodes::INCREMENT_HUE) == 0)       return CommandType::INCREMENT_HUE;
    if (strcmp(code, CommandCodes::SET_VISUAL_PARAMS) == 0)   return CommandType::SET_VISUAL_PARAMS;
    if (strcmp(code, CommandCodes::SET_INTENSITY) == 0)       return CommandType::SET_INTENSITY;
    if (strcmp(code, CommandCodes::SET_SATURATION) == 0)      return CommandType::SET_SATURATION;
    if (strcmp(code, CommandCodes::SET_COMPLEXITY) == 0)      return CommandType::SET_COMPLEXITY;
    if (strcmp(code, CommandCodes::SET_VARIATION) == 0)       return CommandType::SET_VARIATION;

    return CommandType::UNKNOWN;
}

size_t CommandSerializer::writeEnvelopeStart(
    char* buffer,
    size_t bufferSize,
    const char* code,
    uint32_t version,
    const char* uuid
) {
    return snprintf(buffer, bufferSize,
        "{\"t\":\"sync.cmd\",\"c\":\"%s\",\"v\":%lu,\"ts\":%lu,\"u\":\"%s\",\"p\":{",
        code,
        static_cast<unsigned long>(version),
        static_cast<unsigned long>(millis()),
        uuid ? uuid : ""
    );
}

size_t CommandSerializer::writeEnvelopeEnd(char* buffer, size_t remaining) {
    return snprintf(buffer, remaining, "}}");
}

size_t CommandSerializer::serializeSetEffect(
    uint8_t effectId,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_EFFECT, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"e\":%u", effectId);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeSetBrightness(
    uint8_t brightness,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_BRIGHTNESS, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"b\":%u", brightness);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeSetSpeed(
    uint8_t speed,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_SPEED, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"s\":%u", speed);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeSetPalette(
    uint8_t paletteId,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_PALETTE, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"p\":%u", paletteId);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeZoneSetEffect(
    uint8_t zoneId,
    uint8_t effectId,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::ZONE_SET_EFFECT, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"z\":%u,\"e\":%u", zoneId, effectId);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeSetZoneMode(
    bool enabled,
    uint8_t zoneCount,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_ZONE_MODE, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"n\":%d,\"c\":%u", enabled ? 1 : 0, zoneCount);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serializeSetVisualParams(
    uint8_t intensity,
    uint8_t saturation,
    uint8_t complexity,
    uint8_t variation,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize
) {
    size_t written = writeEnvelopeStart(outBuffer, bufferSize,
        CommandCodes::SET_VISUAL_PARAMS, version, senderUuid);
    if (written >= bufferSize) return 0;

    written += snprintf(outBuffer + written, bufferSize - written,
        "\"i\":%u,\"a\":%u,\"x\":%u,\"r\":%u",
        intensity, saturation, complexity, variation);
    if (written >= bufferSize) return 0;

    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    return written;
}

size_t CommandSerializer::serialize(
    CommandType type,
    uint32_t version,
    const char* senderUuid,
    char* outBuffer,
    size_t bufferSize,
    const void* params
) {
    // Dispatch to specific serializer based on type
    // This is a generic fallback - prefer the specific methods

    const char* code = commandTypeToCode(type);
    size_t written = writeEnvelopeStart(outBuffer, bufferSize, code, version, senderUuid);
    if (written >= bufferSize) return 0;

    // Add empty params for parameterless commands
    written += writeEnvelopeEnd(outBuffer + written, bufferSize - written);
    (void)params; // May be used in future for generic serialization

    return written;
}

// Simple JSON parser helpers
namespace {
    // Skip whitespace
    const char* skipWs(const char* p, const char* end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        return p;
    }

    // Find a string value after a key
    bool findString(const char* json, const char* key, char* out, size_t outSize) {
        if (!json || !key || !out || outSize == 0) return false;
        const char* keyPos = strstr(json, key);
        if (!keyPos) return false;

        keyPos += strlen(key);
        keyPos = strchr(keyPos, ':');
        if (!keyPos) return false;

        keyPos++;
        while (*keyPos == ' ' || *keyPos == '"') keyPos++;

        size_t i = 0;
        while (*keyPos && *keyPos != '"' && i < outSize - 1) {
            out[i++] = *keyPos++;
        }
        out[i] = '\0';
        return i > 0;
    }

    // Find an integer value after a key
    bool findInt(const char* json, const char* key, long* out) {
        if (!json || !key || !out) return false;
        const char* keyPos = strstr(json, key);
        if (!keyPos) return false;

        keyPos += strlen(key);
        keyPos = strchr(keyPos, ':');
        if (!keyPos) return false;

        keyPos++;
        while (*keyPos == ' ') keyPos++;

        char* endPtr;
        *out = strtol(keyPos, &endPtr, 10);
        return endPtr != keyPos;
    }
}

ParsedCommand CommandSerializer::parse(const char* json, size_t length) {
    ParsedCommand result;
    if (!json || length == 0) return result;

    // Incoming frames may not be NUL-terminated; parsing below expects C strings.
    if (length > MAX_MESSAGE_SIZE) return result;
    char buf[MAX_MESSAGE_SIZE + 1];
    memcpy(buf, json, length);
    buf[length] = '\0';
    json = buf;

    // Check message type
    char msgType[16] = {0};
    if (!findString(json, "\"t\"", msgType, sizeof(msgType))) return result;
    if (strcmp(msgType, "sync.cmd") != 0) return result;

    // Get command code
    char code[4] = {0};
    if (!findString(json, "\"c\"", code, sizeof(code))) return result;
    result.type = codeToCommandType(code);
    if (result.type == CommandType::UNKNOWN) return result;

    // Get version
    long version = 0;
    if (findInt(json, "\"v\"", &version)) {
        result.version = static_cast<uint32_t>(version);
    }

    // Get timestamp
    long timestamp = 0;
    if (findInt(json, "\"ts\"", &timestamp)) {
        result.timestamp = static_cast<uint32_t>(timestamp);
    }

    // Get sender UUID
    findString(json, "\"u\"", result.senderUuid, sizeof(result.senderUuid));

    // Parse parameters based on command type
    long val1 = 0, val2 = 0, val3 = 0, val4 = 0;

    switch (result.type) {
        case CommandType::SET_EFFECT:
            if (findInt(json, "\"e\"", &val1)) {
                result.params.effect.effectId = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_BRIGHTNESS:
            if (findInt(json, "\"b\"", &val1)) {
                result.params.brightness.brightness = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_PALETTE:
            if (findInt(json, "\"p\"", &val1)) {
                result.params.palette.paletteId = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_SPEED:
            if (findInt(json, "\"s\"", &val1)) {
                result.params.speed.speed = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::ZONE_ENABLE:
            if (findInt(json, "\"z\"", &val1) && findInt(json, "\"n\"", &val2)) {
                result.params.zoneEnable.zoneId = static_cast<uint8_t>(val1);
                result.params.zoneEnable.enabled = val2 != 0;
                result.valid = true;
            }
            break;

        case CommandType::ZONE_SET_EFFECT:
            if (findInt(json, "\"z\"", &val1) && findInt(json, "\"e\"", &val2)) {
                result.params.zoneEffect.zoneId = static_cast<uint8_t>(val1);
                result.params.zoneEffect.effectId = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::ZONE_SET_PALETTE:
            if (findInt(json, "\"z\"", &val1) && findInt(json, "\"p\"", &val2)) {
                result.params.zonePalette.zoneId = static_cast<uint8_t>(val1);
                result.params.zonePalette.paletteId = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::ZONE_SET_BRIGHTNESS:
            if (findInt(json, "\"z\"", &val1) && findInt(json, "\"b\"", &val2)) {
                result.params.zoneBrightness.zoneId = static_cast<uint8_t>(val1);
                result.params.zoneBrightness.brightness = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::ZONE_SET_SPEED:
            if (findInt(json, "\"z\"", &val1) && findInt(json, "\"s\"", &val2)) {
                result.params.zoneSpeed.zoneId = static_cast<uint8_t>(val1);
                result.params.zoneSpeed.speed = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::SET_ZONE_MODE:
            if (findInt(json, "\"n\"", &val1) && findInt(json, "\"c\"", &val2)) {
                result.params.zoneMode.enabled = val1 != 0;
                result.params.zoneMode.zoneCount = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::TRIGGER_TRANSITION:
            if (findInt(json, "\"t\"", &val1)) {
                result.params.triggerTransition.transitionType = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::UPDATE_TRANSITION:
            if (findInt(json, "\"t\"", &val1) && findInt(json, "\"g\"", &val2)) {
                result.params.updateTransition.transitionType = static_cast<uint8_t>(val1);
                result.params.updateTransition.progress = static_cast<uint8_t>(val2);
                result.valid = true;
            }
            break;

        case CommandType::COMPLETE_TRANSITION:
        case CommandType::INCREMENT_HUE:
            // Parameterless commands
            result.valid = true;
            break;

        case CommandType::SET_VISUAL_PARAMS:
            if (findInt(json, "\"i\"", &val1) && findInt(json, "\"a\"", &val2) &&
                findInt(json, "\"x\"", &val3) && findInt(json, "\"r\"", &val4)) {
                result.params.visualParams.intensity = static_cast<uint8_t>(val1);
                result.params.visualParams.saturation = static_cast<uint8_t>(val2);
                result.params.visualParams.complexity = static_cast<uint8_t>(val3);
                result.params.visualParams.variation = static_cast<uint8_t>(val4);
                result.valid = true;
            }
            break;

        case CommandType::SET_INTENSITY:
            if (findInt(json, "\"i\"", &val1)) {
                result.params.singleParam.value = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_SATURATION:
            if (findInt(json, "\"a\"", &val1)) {
                result.params.singleParam.value = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_COMPLEXITY:
            if (findInt(json, "\"x\"", &val1)) {
                result.params.singleParam.value = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        case CommandType::SET_VARIATION:
            if (findInt(json, "\"r\"", &val1)) {
                result.params.singleParam.value = static_cast<uint8_t>(val1);
                result.valid = true;
            }
            break;

        default:
            break;
    }

    return result;
}

state::ICommand* CommandSerializer::createCommand(const ParsedCommand& parsed) {
    if (!parsed.valid) return nullptr;

    using namespace state;

    switch (parsed.type) {
        case CommandType::SET_EFFECT:
            return new SetEffectCommand(parsed.params.effect.effectId);

        case CommandType::SET_BRIGHTNESS:
            return new SetBrightnessCommand(parsed.params.brightness.brightness);

        case CommandType::SET_PALETTE:
            return new SetPaletteCommand(parsed.params.palette.paletteId);

        case CommandType::SET_SPEED:
            return new SetSpeedCommand(parsed.params.speed.speed);

        case CommandType::ZONE_ENABLE:
            return new ZoneEnableCommand(
                parsed.params.zoneEnable.zoneId,
                parsed.params.zoneEnable.enabled
            );

        case CommandType::ZONE_SET_EFFECT:
            return new ZoneSetEffectCommand(
                parsed.params.zoneEffect.zoneId,
                parsed.params.zoneEffect.effectId
            );

        case CommandType::ZONE_SET_PALETTE:
            return new ZoneSetPaletteCommand(
                parsed.params.zonePalette.zoneId,
                parsed.params.zonePalette.paletteId
            );

        case CommandType::ZONE_SET_BRIGHTNESS:
            return new ZoneSetBrightnessCommand(
                parsed.params.zoneBrightness.zoneId,
                parsed.params.zoneBrightness.brightness
            );

        case CommandType::ZONE_SET_SPEED:
            return new ZoneSetSpeedCommand(
                parsed.params.zoneSpeed.zoneId,
                parsed.params.zoneSpeed.speed
            );

        case CommandType::SET_ZONE_MODE:
            return new SetZoneModeCommand(
                parsed.params.zoneMode.enabled,
                parsed.params.zoneMode.zoneCount
            );

        case CommandType::TRIGGER_TRANSITION:
            return new TriggerTransitionCommand(
                parsed.params.triggerTransition.transitionType
            );

        case CommandType::UPDATE_TRANSITION:
            return new UpdateTransitionCommand(
                parsed.params.updateTransition.transitionType,
                parsed.params.updateTransition.progress
            );

        case CommandType::COMPLETE_TRANSITION:
            return new CompleteTransitionCommand();

        case CommandType::INCREMENT_HUE:
            return new IncrementHueCommand();

        case CommandType::SET_VISUAL_PARAMS:
            return new SetVisualParamsCommand(
                parsed.params.visualParams.intensity,
                parsed.params.visualParams.saturation,
                parsed.params.visualParams.complexity,
                parsed.params.visualParams.variation
            );

        case CommandType::SET_INTENSITY:
            return new SetIntensityCommand(parsed.params.singleParam.value);

        case CommandType::SET_SATURATION:
            return new SetSaturationCommand(parsed.params.singleParam.value);

        case CommandType::SET_COMPLEXITY:
            return new SetComplexityCommand(parsed.params.singleParam.value);

        case CommandType::SET_VARIATION:
            return new SetVariationCommand(parsed.params.singleParam.value);

        default:
            return nullptr;
    }
}

} // namespace sync
} // namespace lightwaveos
