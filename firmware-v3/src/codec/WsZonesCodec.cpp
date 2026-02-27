/**
 * @file WsZonesCodec.cpp
 * @brief WebSocket zones codec implementation
 *
 * Single canonical JSON parser for zones WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsZonesCodec.h"
#ifdef NATIVE_BUILD
#include "ZoneComposerStub.h"
#include "RendererActorStub.h"
#else
#include "../effects/zones/ZoneComposer.h"
#include "../core/actors/RendererActor.h"  // RendererActor is in lightwaveos::actors namespace
#endif
#ifndef NATIVE_BUILD
#include "../effects/zones/BlendMode.h"
#endif
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Unknown Key Checking Helper
// ============================================================================

static bool isAllowedKey(const char* key, const char* const* allowedKeys, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(key, allowedKeys[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool WsZonesCodec::checkUnknownKeys(JsonObjectConst root, const char* allowedKeys[], size_t allowedCount, char* errorMsg) {
    for (JsonPairConst kv : root) {
        const char* key = kv.key().c_str();
        if (!isAllowedKey(key, allowedKeys, allowedCount)) {
            snprintf(errorMsg, MAX_ERROR_MSG, "Unknown key '%s'", key);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Zone Enable Decode
// ============================================================================

ZoneEnableDecodeResult WsZonesCodec::decodeZoneEnable(JsonObjectConst root) {
    ZoneEnableDecodeResult result;
    result.request = ZoneEnableRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"enable", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract enable (optional bool, default: false)
    result.request.enable = root["enable"] | false;
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Set Effect Decode
// ============================================================================

ZoneSetEffectDecodeResult WsZonesCodec::decodeZoneSetEffect(JsonObjectConst root) {
    ZoneSetEffectDecodeResult result;
    result.request = ZoneSetEffectRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "effectId", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required, 0-3)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract effectId (required, 0-127)
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int effectId = root["effectId"].as<int>();
    if (effectId < 0 || effectId > 127) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of range (0-127): %d", effectId);
        return result;
    }
    result.request.effectId = static_cast<uint8_t>(effectId);
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Set Brightness Decode
// ============================================================================

ZoneSetBrightnessDecodeResult WsZonesCodec::decodeZoneSetBrightness(JsonObjectConst root) {
    ZoneSetBrightnessDecodeResult result;
    result.request = ZoneSetBrightnessRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "brightness", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required, 0-3)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract brightness (optional, 0-255, default: 128)
    int brightness = root["brightness"] | 128;
    if (brightness < 0 || brightness > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "brightness out of range (0-255): %d", brightness);
        return result;
    }
    result.request.brightness = static_cast<uint8_t>(brightness);
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Set Speed Decode
// ============================================================================

ZoneSetSpeedDecodeResult WsZonesCodec::decodeZoneSetSpeed(JsonObjectConst root) {
    ZoneSetSpeedDecodeResult result;
    result.request = ZoneSetSpeedRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "speed", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required, 0-3)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract speed (optional, 1-100, default: 15)
    int speed = root["speed"] | 15;
    if (speed < 1 || speed > 100) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "speed out of range (1-100): %d", speed);
        return result;
    }
    result.request.speed = static_cast<uint8_t>(speed);
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Set Palette Decode
// ============================================================================

ZoneSetPaletteDecodeResult WsZonesCodec::decodeZoneSetPalette(JsonObjectConst root) {
    ZoneSetPaletteDecodeResult result;
    result.request = ZoneSetPaletteRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "paletteId", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required, 0-3)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract paletteId (required, 0-255)
    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0 || paletteId > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId out of range (0-255): %d", paletteId);
        return result;
    }
    result.request.paletteId = static_cast<uint8_t>(paletteId);
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Set Blend Decode
// ============================================================================

ZoneSetBlendDecodeResult WsZonesCodec::decodeZoneSetBlend(JsonObjectConst root) {
    ZoneSetBlendDecodeResult result;
    result.request = ZoneSetBlendRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "blendMode", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required, 0-3)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract blendMode (required, 0-7)
    if (!root["blendMode"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'blendMode'");
        return result;
    }
    int blendMode = root["blendMode"].as<int>();
    if (blendMode < 0 || blendMode > 7) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "blendMode out of range (0-7): %d", blendMode);
        return result;
    }
    result.request.blendMode = static_cast<uint8_t>(blendMode);
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zone Load Preset Decode
// ============================================================================

ZoneLoadPresetDecodeResult WsZonesCodec::decodeZoneLoadPreset(JsonObjectConst root) {
    ZoneLoadPresetDecodeResult result;
    result.request = ZoneLoadPresetRequest();
    
    // Allowed keys (presetId only, no requestId for this command)
    static const char* ALLOWED_KEYS[] = {"presetId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract presetId (optional, 0-4, default: 0)
    int presetId = root["presetId"] | 0;
    if (presetId < 0 || presetId > 4) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "presetId out of range (0-4): %d", presetId);
        return result;
    }
    result.request.presetId = static_cast<uint8_t>(presetId);
    
    result.success = true;
    return result;
}

// ============================================================================
// Zones Get Decode
// ============================================================================

ZonesGetDecodeResult WsZonesCodec::decodeZonesGet(JsonObjectConst root) {
    ZonesGetDecodeResult result;
    result.request = ZonesGetRequest();
    
    // Allowed keys (only requestId for get commands)
    static const char* ALLOWED_KEYS[] = {"requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zones Update Decode (optional fields)
// ============================================================================

ZonesUpdateDecodeResult WsZonesCodec::decodeZonesUpdate(JsonObjectConst root) {
    ZonesUpdateDecodeResult result;
    result.request = ZonesUpdateRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zoneId", "effectId", "brightness", "speed", "paletteId", "blendMode", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zoneId (required)
    if (!root["zoneId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zoneId'");
        return result;
    }
    int zoneId = root["zoneId"].as<int>();
    if (zoneId < 0 || zoneId > 3) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zoneId out of range (0-3): %d", zoneId);
        return result;
    }
    result.request.zoneId = static_cast<uint8_t>(zoneId);
    
    // Extract optional fields
    if (root.containsKey("effectId") && root["effectId"].is<int>()) {
        int effectId = root["effectId"].as<int>();
        if (effectId >= 0 && effectId <= 127) {
            result.request.effectId = static_cast<uint8_t>(effectId);
            result.request.hasEffectId = true;
        }
    }
    
    if (root.containsKey("brightness") && root["brightness"].is<int>()) {
        int brightness = root["brightness"].as<int>();
        if (brightness >= 0 && brightness <= 255) {
            result.request.brightness = static_cast<uint8_t>(brightness);
            result.request.hasBrightness = true;
        }
    }
    
    if (root.containsKey("speed") && root["speed"].is<int>()) {
        int speed = root["speed"].as<int>();
        if (speed >= 1 && speed <= 100) {
            result.request.speed = static_cast<uint8_t>(speed);
            result.request.hasSpeed = true;
        }
    }
    
    if (root.containsKey("paletteId") && root["paletteId"].is<int>()) {
        int paletteId = root["paletteId"].as<int>();
        if (paletteId >= 0 && paletteId <= 255) {
            result.request.paletteId = static_cast<uint8_t>(paletteId);
            result.request.hasPaletteId = true;
        }
    }
    
    if (root.containsKey("blendMode") && root["blendMode"].is<int>()) {
        int blendMode = root["blendMode"].as<int>();
        if (blendMode >= 0 && blendMode <= 7) {
            result.request.blendMode = static_cast<uint8_t>(blendMode);
            result.request.hasBlendMode = true;
        }
    }
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Zones Set Layout Decode (array parsing)
// ============================================================================

ZonesSetLayoutDecodeResult WsZonesCodec::decodeZonesSetLayout(JsonObjectConst root) {
    ZonesSetLayoutDecodeResult result;
    result.request = ZonesSetLayoutRequest();
    
    // Allowed keys
    static const char* ALLOWED_KEYS[] = {"zones", "requestId"};
    static const size_t ALLOWED_COUNT = sizeof(ALLOWED_KEYS) / sizeof(ALLOWED_KEYS[0]);
    
    if (!checkUnknownKeys(root, ALLOWED_KEYS, ALLOWED_COUNT, result.errorMsg)) {
        return result;
    }
    
    // Extract zones array (required)
    if (!root["zones"].is<JsonArrayConst>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'zones' array");
        return result;
    }
    JsonArrayConst zonesArray = root["zones"].as<JsonArrayConst>();
    
    if (zonesArray.size() == 0 || zonesArray.size() > ZonesSetLayoutRequest::MAX_ZONES) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "zones array size out of range (1-%u): %u", 
                 ZonesSetLayoutRequest::MAX_ZONES, static_cast<unsigned>(zonesArray.size()));
        return result;
    }
    
    // Allowed keys for each zone segment object
    static const char* SEGMENT_ALLOWED_KEYS[] = {"zoneId", "s1LeftStart", "s1LeftEnd", "s1RightStart", "s1RightEnd"};
    static const size_t SEGMENT_ALLOWED_COUNT = sizeof(SEGMENT_ALLOWED_KEYS) / sizeof(SEGMENT_ALLOWED_KEYS[0]);
    
    // Parse each zone segment
    result.request.zoneCount = 0;
    for (JsonVariantConst zoneVariant : zonesArray) {
        if (!zoneVariant.is<JsonObjectConst>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] must be an object", result.request.zoneCount);
            return result;
        }
        
        JsonObjectConst zoneObj = zoneVariant.as<JsonObjectConst>();
        
        // Check for unknown keys in zone object
        if (!checkUnknownKeys(zoneObj, SEGMENT_ALLOWED_KEYS, SEGMENT_ALLOWED_COUNT, result.errorMsg)) {
            return result;
        }
        
        // Extract required fields
        if (!zoneObj["zoneId"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] missing required field 'zoneId'", result.request.zoneCount);
            return result;
        }
        if (!zoneObj["s1LeftStart"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] missing required field 's1LeftStart'", result.request.zoneCount);
            return result;
        }
        if (!zoneObj["s1LeftEnd"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] missing required field 's1LeftEnd'", result.request.zoneCount);
            return result;
        }
        if (!zoneObj["s1RightStart"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] missing required field 's1RightStart'", result.request.zoneCount);
            return result;
        }
        if (!zoneObj["s1RightEnd"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "zones[%u] missing required field 's1RightEnd'", result.request.zoneCount);
            return result;
        }
        
        ZoneSegmentRequest& seg = result.request.zones[result.request.zoneCount];
        seg.zoneId = static_cast<uint8_t>(zoneObj["zoneId"].as<int>());
        seg.s1LeftStart = static_cast<uint8_t>(zoneObj["s1LeftStart"].as<int>());
        seg.s1LeftEnd = static_cast<uint8_t>(zoneObj["s1LeftEnd"].as<int>());
        seg.s1RightStart = static_cast<uint8_t>(zoneObj["s1RightStart"].as<int>());
        seg.s1RightEnd = static_cast<uint8_t>(zoneObj["s1RightEnd"].as<int>());
        
        result.request.zoneCount++;
    }
    
    // Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }
    
    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsZonesCodec::encodeZonesGet(const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    data["enabled"] = composer.isEnabled();
    data["zoneCount"] = composer.getZoneCount();
    
    // Include segment definitions
    JsonArray segmentsArray = data["segments"].to<JsonArray>();
    const zones::ZoneSegment* segments = composer.getZoneConfig();
    for (uint8_t i = 0; i < composer.getZoneCount(); i++) {
        JsonObject seg = segmentsArray.add<JsonObject>();
        seg["zoneId"] = segments[i].zoneId;
        seg["s1LeftStart"] = segments[i].s1LeftStart;
        seg["s1LeftEnd"] = segments[i].s1LeftEnd;
        seg["s1RightStart"] = segments[i].s1RightStart;
        seg["s1RightEnd"] = segments[i].s1RightEnd;
        seg["totalLeds"] = segments[i].totalLeds;
    }
    
    // Include zone state
    JsonArray zones = data["zones"].to<JsonArray>();
    for (uint8_t i = 0; i < composer.getZoneCount(); i++) {
        JsonObject zone = zones.add<JsonObject>();
        zone["id"] = i;
        zone["enabled"] = composer.isZoneEnabled(i);
        zone["effectId"] = composer.getZoneEffect(i);
        if (renderer) {
            zone["effectName"] = renderer->getEffectName(composer.getZoneEffect(i));
        }
        zone["brightness"] = composer.getZoneBrightness(i);
        zone["speed"] = composer.getZoneSpeed(i);
        zone["paletteId"] = composer.getZonePalette(i);
        zone["blendMode"] = static_cast<uint8_t>(composer.getZoneBlendMode(i));
        zone["blendModeName"] = ::lightwaveos::zones::getBlendModeName(composer.getZoneBlendMode(i));
    }
    
    // Include presets
    JsonArray presets = data["presets"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        JsonObject preset = presets.add<JsonObject>();
        preset["id"] = i;
        preset["name"] = ::lightwaveos::zones::ZoneComposer::getPresetName(i);
    }
}

void WsZonesCodec::encodeZonesList(const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    // Same structure as encodeZonesGet
    encodeZonesGet(composer, renderer, data);
}

void WsZonesCodec::encodeZoneEnabledChanged(bool enabled, JsonObject& data) {
    data["enabled"] = enabled;
}

void WsZonesCodec::encodeZonesChanged(uint8_t zoneId, const char* const updatedFields[], uint8_t updatedCount, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    (void)renderer;
    data["zoneId"] = zoneId;
    JsonArray updated = data["updated"].to<JsonArray>();
    for (uint8_t i = 0; i < updatedCount; i++) {
        updated.add(updatedFields[i]);
    }
    JsonObject current = data["current"].to<JsonObject>();
    current["effectId"] = composer.getZoneEffect(zoneId);
    current["brightness"] = composer.getZoneBrightness(zoneId);
    current["speed"] = composer.getZoneSpeed(zoneId);
    current["paletteId"] = composer.getZonePalette(zoneId);
    current["blendMode"] = static_cast<uint8_t>(composer.getZoneBlendMode(zoneId));
    current["blendModeName"] = ::lightwaveos::zones::getBlendModeName(composer.getZoneBlendMode(zoneId));
}

void WsZonesCodec::encodeZonesEffectChanged(uint8_t zoneId, uint8_t effectId, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    data["zoneId"] = zoneId;
    JsonObject current = data["current"].to<JsonObject>();
    current["effectId"] = effectId;
    current["effectName"] = renderer ? renderer->getEffectName(effectId) : "";
    current["brightness"] = composer.getZoneBrightness(zoneId);
    current["speed"] = composer.getZoneSpeed(zoneId);
    current["paletteId"] = composer.getZonePalette(zoneId);
    current["blendMode"] = static_cast<uint8_t>(composer.getZoneBlendMode(zoneId));
    current["blendModeName"] = ::lightwaveos::zones::getBlendModeName(composer.getZoneBlendMode(zoneId));
}

void WsZonesCodec::encodeZonePaletteChanged(uint8_t zoneId, uint8_t paletteId, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    data["zoneId"] = zoneId;
    JsonObject current = data["current"].to<JsonObject>();
    current["effectId"] = composer.getZoneEffect(zoneId);
    current["effectName"] = renderer ? renderer->getEffectName(composer.getZoneEffect(zoneId)) : "";
    current["brightness"] = composer.getZoneBrightness(zoneId);
    current["speed"] = composer.getZoneSpeed(zoneId);
    current["paletteId"] = paletteId;
    current["blendMode"] = static_cast<uint8_t>(composer.getZoneBlendMode(zoneId));
    current["blendModeName"] = ::lightwaveos::zones::getBlendModeName(composer.getZoneBlendMode(zoneId));
}

void WsZonesCodec::encodeZoneBlendChanged(uint8_t zoneId, uint8_t blendMode, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data) {
    data["zoneId"] = zoneId;
    JsonObject current = data["current"].to<JsonObject>();
    current["effectId"] = composer.getZoneEffect(zoneId);
    current["effectName"] = renderer ? renderer->getEffectName(composer.getZoneEffect(zoneId)) : "";
    current["brightness"] = composer.getZoneBrightness(zoneId);
    current["speed"] = composer.getZoneSpeed(zoneId);
    current["paletteId"] = composer.getZonePalette(zoneId);
    current["blendMode"] = blendMode;
    current["blendModeName"] = ::lightwaveos::zones::getBlendModeName(blendMode);
}

void WsZonesCodec::encodeZonesLayoutChanged(uint8_t zoneCount, JsonObject& data) {
    data["zoneCount"] = zoneCount;
}

} // namespace codec
} // namespace lightwaveos
