// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpZoneCodec.cpp
 * @brief HTTP zone codec implementation
 *
 * Single canonical JSON parser for zone HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpZoneCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

HttpZoneLayoutDecodeResult HttpZoneCodec::decodeLayout(JsonObjectConst root) {
    HttpZoneLayoutDecodeResult result;
    if (!root.containsKey("zones") || !root["zones"].is<JsonArrayConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zones' (must be array)");
        return result;
    }

    JsonArrayConst zonesArray = root["zones"].as<JsonArrayConst>();
    if (zonesArray.size() == 0 || zonesArray.size() > lightwaveos::zones::MAX_ZONES) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Invalid zones array size");
        return result;
    }

    uint8_t zoneCount = zonesArray.size();
    if (zoneCount > lightwaveos::zones::MAX_ZONES) {
        zoneCount = lightwaveos::zones::MAX_ZONES;
    }

    constexpr uint16_t STRIP_LENGTH = 160;
    for (uint8_t i = 0; i < zoneCount; i++) {
        JsonObjectConst zoneObj = zonesArray[i].as<JsonObjectConst>();
        if (!zoneObj.containsKey("zoneId") || !zoneObj.containsKey("s1LeftStart") ||
            !zoneObj.containsKey("s1LeftEnd") || !zoneObj.containsKey("s1RightStart") ||
            !zoneObj.containsKey("s1RightEnd")) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Zone segment missing required fields");
            return result;
        }

        int zoneId = zoneObj["zoneId"].as<int>();
        int s1LeftStart = zoneObj["s1LeftStart"].as<int>();
        int s1LeftEnd = zoneObj["s1LeftEnd"].as<int>();
        int s1RightStart = zoneObj["s1RightStart"].as<int>();
        int s1RightEnd = zoneObj["s1RightEnd"].as<int>();

        if (s1LeftStart < 0) s1LeftStart = 0;
        if (s1LeftEnd < 0) s1LeftEnd = 0;
        if (s1RightStart < 0) s1RightStart = 0;
        if (s1RightEnd < 0) s1RightEnd = 0;

        if (s1LeftStart >= STRIP_LENGTH) s1LeftStart = 0;
        if (s1LeftEnd >= STRIP_LENGTH) s1LeftEnd = STRIP_LENGTH - 1;
        if (s1RightStart >= STRIP_LENGTH) s1RightStart = 0;
        if (s1RightEnd >= STRIP_LENGTH) s1RightEnd = STRIP_LENGTH - 1;

        if (s1LeftStart > s1LeftEnd) s1LeftEnd = s1LeftStart;
        if (s1RightStart > s1RightEnd) s1RightEnd = s1RightStart;

        result.segments[i].zoneId = static_cast<uint8_t>(zoneId);
        result.segments[i].s1LeftStart = static_cast<uint8_t>(s1LeftStart);
        result.segments[i].s1LeftEnd = static_cast<uint8_t>(s1LeftEnd);
        result.segments[i].s1RightStart = static_cast<uint8_t>(s1RightStart);
        result.segments[i].s1RightEnd = static_cast<uint8_t>(s1RightEnd);

        uint8_t leftSize = (s1LeftEnd >= s1LeftStart) ? (s1LeftEnd - s1LeftStart + 1) : 1;
        uint8_t rightSize = (s1RightEnd >= s1RightStart) ? (s1RightEnd - s1RightStart + 1) : 1;
        result.segments[i].totalLeds = leftSize + rightSize;
    }

    result.zoneCount = zoneCount;
    result.success = true;
    return result;
}

HttpZoneSetEffectDecodeResult HttpZoneCodec::decodeSetEffect(JsonObjectConst root) {
    HttpZoneSetEffectDecodeResult result;
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int effectId = root["effectId"].as<int>();
    if (effectId < 0 || effectId > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of range (0-255)");
        return result;
    }
    result.effectId = static_cast<uint8_t>(effectId);
    result.success = true;
    return result;
}

HttpZoneSetBrightnessDecodeResult HttpZoneCodec::decodeSetBrightness(JsonObjectConst root) {
    HttpZoneSetBrightnessDecodeResult result;
    if (!root["brightness"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'brightness'");
        return result;
    }
    int brightness = root["brightness"].as<int>();
    if (brightness < 0 || brightness > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "brightness out of range (0-255)");
        return result;
    }
    result.brightness = static_cast<uint8_t>(brightness);
    result.success = true;
    return result;
}

HttpZoneSetSpeedDecodeResult HttpZoneCodec::decodeSetSpeed(JsonObjectConst root) {
    HttpZoneSetSpeedDecodeResult result;
    if (!root["speed"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'speed'");
        return result;
    }
    int speed = root["speed"].as<int>();
    if (speed < 0 || speed > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "speed out of range (0-255)");
        return result;
    }
    result.speed = static_cast<uint8_t>(speed);
    result.success = true;
    return result;
}

HttpZoneSetPaletteDecodeResult HttpZoneCodec::decodeSetPalette(JsonObjectConst root) {
    HttpZoneSetPaletteDecodeResult result;
    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0 || paletteId > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId out of range (0-255)");
        return result;
    }
    result.paletteId = static_cast<uint8_t>(paletteId);
    result.success = true;
    return result;
}

HttpZoneSetBlendDecodeResult HttpZoneCodec::decodeSetBlend(JsonObjectConst root) {
    HttpZoneSetBlendDecodeResult result;
    if (!root["blendMode"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'blendMode'");
        return result;
    }
    int blendMode = root["blendMode"].as<int>();
    if (blendMode < 0 || blendMode > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "blendMode out of range (0-255)");
        return result;
    }
    result.blendMode = static_cast<uint8_t>(blendMode);
    result.success = true;
    return result;
}

HttpZoneSetEnabledDecodeResult HttpZoneCodec::decodeSetEnabled(JsonObjectConst root) {
    HttpZoneSetEnabledDecodeResult result;
    if (!root["enabled"].is<bool>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'enabled'");
        return result;
    }
    result.enabled = root["enabled"].as<bool>();
    result.success = true;
    return result;
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpZoneCodec::encodeListSummary(const HttpZoneListSummaryData& data, JsonObject& obj) {
    obj["enabled"] = data.enabled;
    obj["zoneCount"] = data.zoneCount;
}

void HttpZoneCodec::encodeGet(const HttpZoneGetData& data, JsonObject& obj) {
    obj["id"] = data.id;
    obj["enabled"] = data.enabled;
}

void HttpZoneCodec::encodeListFull(const HttpZoneListFullData& data, JsonObject& obj) {
    obj["enabled"] = data.enabled;
    obj["zoneCount"] = data.zoneCount;

    JsonArray segments = obj["segments"].to<JsonArray>();
    for (size_t i = 0; i < data.segmentCount; ++i) {
        JsonObject seg = segments.add<JsonObject>();
        seg["zoneId"] = data.segments[i].zoneId;
        seg["s1LeftStart"] = data.segments[i].s1LeftStart;
        seg["s1LeftEnd"] = data.segments[i].s1LeftEnd;
        seg["s1RightStart"] = data.segments[i].s1RightStart;
        seg["s1RightEnd"] = data.segments[i].s1RightEnd;
        seg["totalLeds"] = data.segments[i].totalLeds;
    }

    JsonArray zones = obj["zones"].to<JsonArray>();
    for (size_t i = 0; i < data.zoneItemCount; ++i) {
        const HttpZoneListItemData& zoneItem = data.zones[i];
        JsonObject zone = zones.add<JsonObject>();
        zone["id"] = zoneItem.id;
        zone["enabled"] = zoneItem.enabled;
        zone["effectId"] = zoneItem.effectId;
        if (zoneItem.effectName) {
            zone["effectName"] = zoneItem.effectName;
        }
        zone["brightness"] = zoneItem.brightness;
        zone["speed"] = zoneItem.speed;
        zone["paletteId"] = zoneItem.paletteId;
        zone["blendMode"] = zoneItem.blendMode;
        if (zoneItem.blendModeName) {
            zone["blendModeName"] = zoneItem.blendModeName;
        }
    }

    JsonArray presets = obj["presets"].to<JsonArray>();
    for (size_t i = 0; i < data.presetCount; ++i) {
        JsonObject preset = presets.add<JsonObject>();
        preset["id"] = data.presets[i].id;
        preset["name"] = data.presets[i].name;
    }
}

void HttpZoneCodec::encodeGetFull(const HttpZoneGetFullData& data, JsonObject& obj) {
    obj["id"] = data.id;
    obj["enabled"] = data.enabled;
    obj["effectId"] = data.effectId;
    if (data.effectName) {
        obj["effectName"] = data.effectName;
    }
    obj["brightness"] = data.brightness;
    obj["speed"] = data.speed;
    obj["paletteId"] = data.paletteId;
    obj["blendMode"] = data.blendMode;
    if (data.blendModeName) {
        obj["blendModeName"] = data.blendModeName;
    }
}

void HttpZoneCodec::encodeSetResult(const HttpZoneSetResultData& data, JsonObject& obj) {
    obj["zoneId"] = data.zoneId;
    if (data.hasEffectId) {
        obj["effectId"] = data.effectId;
        if (data.effectName) {
            obj["effectName"] = data.effectName;
        }
    }
    if (data.hasBrightness) {
        obj["brightness"] = data.brightness;
    }
    if (data.hasSpeed) {
        obj["speed"] = data.speed;
    }
    if (data.hasPaletteId) {
        obj["paletteId"] = data.paletteId;
        if (data.paletteName) {
            obj["paletteName"] = data.paletteName;
        }
    }
    if (data.hasBlendMode) {
        obj["blendMode"] = data.blendMode;
        if (data.blendModeName) {
            obj["blendModeName"] = data.blendModeName;
        }
    }
    if (data.hasEnabled) {
        obj["enabled"] = data.enabled;
    }
}

void HttpZoneCodec::encodeLayoutResult(const HttpZoneLayoutResultData& data, JsonObject& obj) {
    obj["zoneCount"] = data.zoneCount;
}

} // namespace codec
} // namespace lightwaveos
