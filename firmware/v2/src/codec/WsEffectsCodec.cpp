/**
 * @file WsEffectsCodec.cpp
 * @brief WebSocket effects codec implementation
 *
 * Single canonical JSON parser for effects.setCurrent WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsEffectsCodec.h"
#include "../config/limits.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Main Decode Function
// ============================================================================

EffectsSetCurrentDecodeResult WsEffectsCodec::decodeSetCurrent(JsonObjectConst root) {
    EffectsSetCurrentDecodeResult result;
    result.request = EffectsSetCurrentRequest();

    // Step 1: Extract effectId (required, stable namespaced EffectId)
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int32_t effectId = root["effectId"].as<int32_t>();
    if (effectId < 0 || effectId > 0xFFFF) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of uint16 range (0-65535): %ld", (long)effectId);
        return result;
    }
    result.request.effectId = static_cast<EffectId>(effectId);

    // Step 2: Extract requestId (optional string)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    } else {
        result.request.requestId = "";
    }

    // Step 3: Extract transition object (optional)
    if (root.containsKey("transition") && root["transition"].is<JsonObjectConst>()) {
        JsonObjectConst trans = root["transition"].as<JsonObjectConst>();
        result.request.hasTransition = true;

        // Transition type (optional, default: 0)
        if (trans["type"].is<int>()) {
            int transType = trans["type"].as<int>();
            if (transType < 0 || transType > 255) {
                snprintf(result.errorMsg, MAX_ERROR_MSG, "transition.type out of range (0-255): %d", transType);
                return result;
            }
            result.request.transitionType = static_cast<uint8_t>(transType);
        } else {
            result.request.transitionType = 0;
        }

        // Transition duration (optional, default: 1000ms)
        if (trans["duration"].is<int>()) {
            int duration = trans["duration"].as<int>();
            if (duration < 0 || duration > 65535) {
                snprintf(result.errorMsg, MAX_ERROR_MSG, "transition.duration out of range (0-65535): %d", duration);
                return result;
            }
            result.request.transitionDuration = static_cast<uint16_t>(duration);
        } else {
            result.request.transitionDuration = 1000;
        }
    } else {
        result.request.hasTransition = false;
        result.request.transitionType = 0;
        result.request.transitionDuration = 1000;
    }

    result.success = true;
    return result;
}

// ============================================================================
// Group B: Single-Value Setters
// ============================================================================

EffectsSetEffectDecodeResult WsEffectsCodec::decodeSetEffect(JsonObjectConst root) {
    EffectsSetEffectDecodeResult result;
    result.request = EffectsSetEffectRequest();

    // Extract effectId (required, stable namespaced EffectId)
    if (!root["effectId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'effectId'");
        return result;
    }
    int32_t effectId = root["effectId"].as<int32_t>();
    if (effectId < 0 || effectId > 0xFFFF) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "effectId out of uint16 range (0-65535): %ld", (long)effectId);
        return result;
    }
    result.request.effectId = static_cast<EffectId>(effectId);
    result.success = true;
    return result;
}

EffectsSetBrightnessDecodeResult WsEffectsCodec::decodeSetBrightness(JsonObjectConst root) {
    EffectsSetBrightnessDecodeResult result;
    result.request = EffectsSetBrightnessRequest();

    // Extract value (required, 0-255)
    if (!root["value"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'value'");
        return result;
    }
    int value = root["value"].as<int>();
    if (value < 0 || value > 255) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "value out of range (0-255): %d", value);
        return result;
    }
    result.request.value = static_cast<uint8_t>(value);
    result.success = true;
    return result;
}

EffectsSetSpeedDecodeResult WsEffectsCodec::decodeSetSpeed(JsonObjectConst root) {
    EffectsSetSpeedDecodeResult result;
    result.request = EffectsSetSpeedRequest();

    // Extract value (required, 1-50)
    if (!root["value"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'value'");
        return result;
    }
    int value = root["value"].as<int>();
    if (value < 1 || value > 50) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "value out of range (1-50): %d", value);
        return result;
    }
    result.request.value = static_cast<uint8_t>(value);
    result.success = true;
    return result;
}

EffectsSetPaletteDecodeResult WsEffectsCodec::decodeSetPalette(JsonObjectConst root) {
    EffectsSetPaletteDecodeResult result;
    result.request = EffectsSetPaletteRequest();

    // Extract paletteId (required, range validated externally)
    if (!root["paletteId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'paletteId'");
        return result;
    }
    int paletteId = root["paletteId"].as<int>();
    if (paletteId < 0) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId must be >= 0: %d", paletteId);
        return result;
    }
    result.request.paletteId = static_cast<uint8_t>(paletteId);
    result.success = true;
    return result;
}

// ============================================================================
// Group C: Complex Payloads
// ============================================================================

EffectsGetMetadataDecodeResult WsEffectsCodec::decodeGetMetadata(JsonObjectConst root) {
    EffectsGetMetadataDecodeResult result;
    result.request = EffectsGetMetadataRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract effectId (optional, INVALID_EFFECT_ID means missing/invalid)
    if (root["effectId"].is<int>()) {
        int32_t effectId = root["effectId"].as<int32_t>();
        if (effectId >= 0 && effectId <= 0xFFFF) {
            result.request.effectId = static_cast<EffectId>(effectId);
        }
        // If out of range, leave as INVALID_EFFECT_ID
    }

    result.success = true;
    return result;
}

EffectsListDecodeResult WsEffectsCodec::decodeList(JsonObjectConst root) {
    EffectsListDecodeResult result;
    result.request = EffectsListRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract page (optional, default: 1, min: 1)
    if (root["page"].is<int>()) {
        int page = root["page"].as<int>();
        if (page < 1) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "page must be >= 1: %d", page);
            return result;
        }
        result.request.page = static_cast<uint8_t>(page);
    }

    // Extract limit (optional, default: 20, range: 1-50)
    if (root["limit"].is<int>()) {
        int limit = root["limit"].as<int>();
        if (limit < 1 || limit > 50) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "limit out of range (1-50): %d", limit);
            return result;
        }
        result.request.limit = static_cast<uint8_t>(limit);
    }

    // Extract details (optional bool, default: false)
    if (root["details"].is<bool>()) {
        result.request.details = root["details"].as<bool>();
    }

    result.success = true;
    return result;
}

EffectsParametersGetDecodeResult WsEffectsCodec::decodeParametersGet(JsonObjectConst root) {
    EffectsParametersGetDecodeResult result;
    result.request = EffectsParametersGetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract effectId (optional, INVALID_EFFECT_ID means use current)
    if (root["effectId"].is<int>()) {
        int32_t effectId = root["effectId"].as<int32_t>();
        if (effectId >= 0 && effectId <= 0xFFFF) {
            result.request.effectId = static_cast<EffectId>(effectId);
        }
    }

    result.success = true;
    return result;
}

EffectsParametersSetDecodeResult WsEffectsCodec::decodeEffectsParametersSet(JsonObjectConst root) {
    EffectsParametersSetDecodeResult result;
    result.request = EffectsParametersSetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract effectId (optional, INVALID_EFFECT_ID means use current)
    if (root["effectId"].is<int>()) {
        int32_t effectId = root["effectId"].as<int32_t>();
        if (effectId >= 0 && effectId <= 0xFFFF) {
            result.request.effectId = static_cast<EffectId>(effectId);
        }
    }

    // Extract parameters object (required if command is used)
    if (root.containsKey("parameters") && root["parameters"].is<JsonObjectConst>()) {
        result.request.parameters = root["parameters"].as<JsonObjectConst>();
        result.request.hasParameters = true;
    } else {
        // parameters object is required for this command
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'parameters'");
        return result;
    }

    result.success = true;
    return result;
}

EffectsGetByFamilyDecodeResult WsEffectsCodec::decodeGetByFamily(JsonObjectConst root) {
    EffectsGetByFamilyDecodeResult result;
    result.request = EffectsGetByFamilyRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract familyId (required, 0-9)
    if (!root["familyId"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'familyId'");
        return result;
    }
    int familyId = root["familyId"].as<int>();
    if (familyId < 0 || familyId >= 10) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "familyId out of range (0-9): %d", familyId);
        return result;
    }
    result.request.familyId = static_cast<uint8_t>(familyId);

    result.success = true;
    return result;
}

ParametersSetDecodeResult WsEffectsCodec::decodeParametersSet(JsonObjectConst root) {
    ParametersSetDecodeResult result;
    result.request = ParametersSetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract optional fields (all have defaults if not present)
    if (root.containsKey("brightness") && root["brightness"].is<int>()) {
        int value = root["brightness"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "brightness out of range (0-255): %d", value);
            return result;
        }
        result.request.brightness = static_cast<uint8_t>(value);
        result.request.hasBrightness = true;
    }

    if (root.containsKey("speed") && root["speed"].is<int>()) {
        int value = root["speed"].as<int>();
        if (value < 1 || value > 50) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "speed out of range (1-50): %d", value);
            return result;
        }
        result.request.speed = static_cast<uint8_t>(value);
        result.request.hasSpeed = true;
    }

    if (root.containsKey("paletteId") && root["paletteId"].is<int>()) {
        int value = root["paletteId"].as<int>();
        if (value < 0) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "paletteId must be >= 0: %d", value);
            return result;
        }
        result.request.paletteId = static_cast<uint8_t>(value);
        result.request.hasPaletteId = true;
    }

    if (root.containsKey("hue") && root["hue"].is<int>()) {
        int value = root["hue"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "hue out of range (0-255): %d", value);
            return result;
        }
        result.request.hue = static_cast<uint8_t>(value);
        result.request.hasHue = true;
    }

    if (root.containsKey("intensity") && root["intensity"].is<int>()) {
        int value = root["intensity"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "intensity out of range (0-255): %d", value);
            return result;
        }
        result.request.intensity = static_cast<uint8_t>(value);
        result.request.hasIntensity = true;
    }

    if (root.containsKey("saturation") && root["saturation"].is<int>()) {
        int value = root["saturation"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "saturation out of range (0-255): %d", value);
            return result;
        }
        result.request.saturation = static_cast<uint8_t>(value);
        result.request.hasSaturation = true;
    }

    if (root.containsKey("complexity") && root["complexity"].is<int>()) {
        int value = root["complexity"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "complexity out of range (0-255): %d", value);
            return result;
        }
        result.request.complexity = static_cast<uint8_t>(value);
        result.request.hasComplexity = true;
    }

    if (root.containsKey("variation") && root["variation"].is<int>()) {
        int value = root["variation"].as<int>();
        if (value < 0 || value > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "variation out of range (0-255): %d", value);
            return result;
        }
        result.request.variation = static_cast<uint8_t>(value);
        result.request.hasVariation = true;
    }

    result.success = true;
    return result;
}

EffectsSimpleDecodeResult WsEffectsCodec::decodeSimple(JsonObjectConst root) {
    EffectsSimpleDecodeResult result;
    result.request = EffectsSimpleRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsEffectsCodec::encodeGetCurrent(EffectId effectId, const char* name, uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, bool isIEffect, const char* description, uint8_t version, JsonObject& data) {
    data["effectId"] = effectId;
    data["name"] = name ? name : "";
    data["brightness"] = brightness;
    data["speed"] = speed;
    data["paletteId"] = paletteId;
    data["hue"] = hue;
    data["intensity"] = intensity;
    data["saturation"] = saturation;
    data["complexity"] = complexity;
    data["variation"] = variation;
    data["isIEffect"] = isIEffect;
    if (description) data["description"] = description;
    data["version"] = version;
}

void WsEffectsCodec::encodeChanged(EffectId effectId, const char* name, bool transitionActive, JsonObject& data) {
    data["effectId"] = effectId;
    data["name"] = name ? name : "";
    data["transitionActive"] = transitionActive;
}

void WsEffectsCodec::encodeMetadata(EffectId effectId, const char* name, const char* familyName, uint8_t familyId, const char* story, const char* opticalIntent, uint8_t tags, JsonObject& data) {
    data["id"] = effectId;
    data["name"] = name ? name : "";
    data["family"] = familyName ? familyName : "Unknown";
    data["familyId"] = familyId;
    
    if (story) {
        data["story"] = story;
    }
    if (opticalIntent) {
        data["opticalIntent"] = opticalIntent;
    }
    
    JsonArray tagsArr = data["tags"].to<JsonArray>();
    if (tags & 0x01) tagsArr.add("STANDING");
    if (tags & 0x02) tagsArr.add("TRAVELING");
    if (tags & 0x04) tagsArr.add("MOIRE");
    if (tags & 0x08) tagsArr.add("DEPTH");
    if (tags & 0x10) tagsArr.add("SPECTRAL");
    if (tags & 0x20) tagsArr.add("CENTER_ORIGIN");
    if (tags & 0x40) tagsArr.add("DUAL_STRIP");
    if (tags & 0x80) tagsArr.add("PHYSICS");
    
    JsonObject properties = data["properties"].to<JsonObject>();
    properties["centerOrigin"] = true;
    properties["symmetricStrips"] = true;
    properties["paletteAware"] = true;
    properties["speedResponsive"] = true;
}

void WsEffectsCodec::encodeList(uint16_t effectCount, uint16_t startIdx, uint16_t endIdx, uint8_t page, uint8_t limit, bool details, const char* const effectNames[], const EffectId effectIds[], const char* const categories[], JsonObject& data) {
    JsonArray effects = data["effects"].to<JsonArray>();
    for (uint16_t i = startIdx; i < endIdx; i++) {
        JsonObject effect = effects.add<JsonObject>();
        effect["id"] = effectIds ? effectIds[i] : static_cast<EffectId>(i);
        effect["name"] = effectNames[i] ? effectNames[i] : "";
        if (details && categories && categories[i]) {
            effect["category"] = categories[i];
        }
    }

    JsonObject pagination = data["pagination"].to<JsonObject>();
    pagination["page"] = page;
    pagination["limit"] = limit;
    pagination["total"] = effectCount;
    pagination["pages"] = (effectCount + limit - 1) / limit;
}

void WsEffectsCodec::encodeByFamily(uint8_t familyId, const char* familyName, const EffectId patternIndices[], uint16_t count, JsonObject& data) {
    data["familyId"] = familyId;
    data["familyName"] = familyName ? familyName : "";

    JsonArray effects = data["effects"].to<JsonArray>();
    for (uint16_t i = 0; i < count; i++) {
        effects.add(patternIndices[i]);
    }

    data["count"] = count;
}

void WsEffectsCodec::encodeCategories(const char* const familyNames[], const uint8_t familyCounts[], uint8_t total, JsonObject& data) {
    JsonArray families = data["categories"].to<JsonArray>();
    for (uint8_t i = 0; i < total; i++) {
        JsonObject familyObj = families.add<JsonObject>();
        familyObj["id"] = i;
        familyObj["name"] = familyNames[i] ? familyNames[i] : "";
        familyObj["count"] = familyCounts[i];
    }
    data["total"] = total;
}

void WsEffectsCodec::encodeParametersGet(EffectId effectId,
                                         const char* name,
                                         bool hasParameters,
                                         const char* const paramNames[],
                                         const char* const paramDisplayNames[],
                                         const float paramMins[],
                                         const float paramMaxs[],
                                         const float paramDefaults[],
                                         const float paramValues[],
                                         const char* const paramTypes[],
                                         const float paramSteps[],
                                         const char* const paramGroups[],
                                         const char* const paramUnits[],
                                         const bool paramAdvanced[],
                                         uint8_t paramCount,
                                         const char* persistenceMode,
                                         bool persistenceDirty,
                                         const char* persistenceLastError,
                                         JsonObject& data) {
    data["effectId"] = effectId;
    data["name"] = name ? name : "";
    data["hasParameters"] = hasParameters;
    JsonObject persistence = data["persistence"].to<JsonObject>();
    persistence["mode"] = persistenceMode ? persistenceMode : "volatile";
    persistence["dirty"] = persistenceDirty;
    if (persistenceLastError && persistenceLastError[0] != '\0') {
        persistence["lastError"] = persistenceLastError;
    }
    
    JsonArray params = data["parameters"].to<JsonArray>();
    for (uint8_t i = 0; i < paramCount; i++) {
        JsonObject p = params.add<JsonObject>();
        p["name"] = paramNames[i] ? paramNames[i] : "";
        p["displayName"] = paramDisplayNames[i] ? paramDisplayNames[i] : "";
        p["min"] = paramMins[i];
        p["max"] = paramMaxs[i];
        p["default"] = paramDefaults[i];
        p["value"] = paramValues[i];
        p["type"] = (paramTypes && paramTypes[i]) ? paramTypes[i] : "float";
        p["step"] = paramSteps ? paramSteps[i] : 0.01f;
        p["group"] = (paramGroups && paramGroups[i]) ? paramGroups[i] : "";
        p["unit"] = (paramUnits && paramUnits[i]) ? paramUnits[i] : "";
        p["advanced"] = paramAdvanced ? paramAdvanced[i] : false;
    }
}

void WsEffectsCodec::encodeParametersSetChanged(EffectId effectId, const char* name, const char* const queuedKeys[], uint8_t queuedCount, const char* const failedKeys[], uint8_t failedCount, JsonObject& data) {
    data["effectId"] = effectId;
    data["name"] = name ? name : "";
    
    JsonArray queuedArr = data["queued"].to<JsonArray>();
    for (uint8_t i = 0; i < queuedCount; i++) {
        queuedArr.add(queuedKeys[i] ? queuedKeys[i] : "");
    }
    
    JsonArray failedArr = data["failed"].to<JsonArray>();
    for (uint8_t i = 0; i < failedCount; i++) {
        failedArr.add(failedKeys[i] ? failedKeys[i] : "");
    }
}

void WsEffectsCodec::encodeGlobalParametersGet(uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, JsonObject& data) {
    data["brightness"] = brightness;
    data["speed"] = speed;
    data["paletteId"] = paletteId;
    data["hue"] = hue;
    data["intensity"] = intensity;
    data["saturation"] = saturation;
    data["complexity"] = complexity;
    data["variation"] = variation;
}

void WsEffectsCodec::encodeParametersChanged(const char* const updatedKeys[], uint8_t updatedCount, uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, JsonObject& data) {
    JsonArray updated = data["updated"].to<JsonArray>();
    for (uint8_t i = 0; i < updatedCount; i++) {
        updated.add(updatedKeys[i] ? updatedKeys[i] : "");
    }
    
    JsonObject current = data["current"].to<JsonObject>();
    current["brightness"] = brightness;
    current["speed"] = speed;
    current["paletteId"] = paletteId;
    current["hue"] = hue;
    current["intensity"] = intensity;
    current["saturation"] = saturation;
    current["complexity"] = complexity;
    current["variation"] = variation;
}

} // namespace codec
} // namespace lightwaveos
