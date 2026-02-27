// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsTransitionCodec.cpp
 * @brief WebSocket transition codec implementation
 *
 * Single canonical JSON parser for transition WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsTransitionCodec.h"
#ifdef NATIVE_BUILD
#include "../../test/test_native/mocks/TransitionTypes.h"
#else
#include "../effects/transitions/TransitionTypes.h"
#endif
#include <cstring>

namespace lightwaveos {
namespace codec {

TransitionTriggerDecodeResult WsTransitionCodec::decodeTrigger(JsonObjectConst root) {
    TransitionTriggerDecodeResult result;
    result.request = TransitionTriggerRequest();

    // Extract toEffect (required, 0-127)
    if (!root["toEffect"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'toEffect'");
        return result;
    }
    int toEffect = root["toEffect"].as<int>();
    if (toEffect < 0 || toEffect > 127) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "toEffect out of range (0-127): %d", toEffect);
        return result;
    }
    result.request.toEffect = static_cast<uint8_t>(toEffect);

    // Extract transitionType (optional, default: 0)
    if (root["transitionType"].is<int>()) {
        int transType = root["transitionType"].as<int>();
        if (transType < 0 || transType > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "transitionType out of range (0-255): %d", transType);
            return result;
        }
        result.request.transitionType = static_cast<uint8_t>(transType);
    }

    // Extract random (optional bool, default: false)
    if (root["random"].is<bool>()) {
        result.request.random = root["random"].as<bool>();
    }

    result.success = true;
    return result;
}

TransitionConfigSetDecodeResult WsTransitionCodec::decodeConfigSet(JsonObjectConst root) {
    TransitionConfigSetDecodeResult result;
    result.request = TransitionConfigSetRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract defaultDuration (optional, default: 1000)
    if (root["defaultDuration"].is<int>()) {
        int duration = root["defaultDuration"].as<int>();
        if (duration < 0 || duration > 65535) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "defaultDuration out of range (0-65535): %d", duration);
            return result;
        }
        result.request.defaultDuration = static_cast<uint16_t>(duration);
    }

    // Extract defaultType (optional, default: 0)
    if (root["defaultType"].is<int>()) {
        int type = root["defaultType"].as<int>();
        if (type < 0 || type > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "defaultType out of range (0-255): %d", type);
            return result;
        }
        result.request.defaultType = static_cast<uint8_t>(type);
    }

    result.success = true;
    return result;
}

TransitionsTriggerDecodeResult WsTransitionCodec::decodeTransitionsTrigger(JsonObjectConst root) {
    TransitionsTriggerDecodeResult result;
    result.request = TransitionsTriggerRequest();

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.request.requestId = root["requestId"].as<const char*>();
    }

    // Extract toEffect (required, 0-127)
    if (!root["toEffect"].is<int>()) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "Missing required field 'toEffect'");
        return result;
    }
    int toEffect = root["toEffect"].as<int>();
    if (toEffect < 0 || toEffect > 127) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "toEffect out of range (0-127): %d", toEffect);
        return result;
    }
    result.request.toEffect = static_cast<uint8_t>(toEffect);

    // Extract type (optional, default: 0)
    if (root["type"].is<int>()) {
        int type = root["type"].as<int>();
        if (type < 0 || type > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "type out of range (0-255): %d", type);
            return result;
        }
        result.request.type = static_cast<uint8_t>(type);
    }

    // Extract duration (optional, default: 1000)
    if (root["duration"].is<int>()) {
        int duration = root["duration"].as<int>();
        if (duration < 0 || duration > 65535) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "duration out of range (0-65535): %d", duration);
            return result;
        }
        result.request.duration = static_cast<uint16_t>(duration);
    }

    result.success = true;
    return result;
}

TransitionSimpleDecodeResult WsTransitionCodec::decodeSimple(JsonObjectConst root) {
    TransitionSimpleDecodeResult result;
    result.request = TransitionSimpleRequest();

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

void WsTransitionCodec::encodeGetTypes(JsonObject& data) {
    JsonArray types = data["types"].to<JsonArray>();
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(::lightwaveos::transitions::TransitionType::TYPE_COUNT); i++) {
        JsonObject type = types.add<JsonObject>();
        type["id"] = i;
        type["name"] = ::lightwaveos::transitions::getTransitionName(static_cast<::lightwaveos::transitions::TransitionType>(i));
    }
    
    data["total"] = static_cast<uint8_t>(::lightwaveos::transitions::TransitionType::TYPE_COUNT);
}

void WsTransitionCodec::encodeConfigGet(JsonObject& data) {
    data["enabled"] = true;
    data["defaultDuration"] = 1000;
    data["defaultType"] = 0;
    data["defaultTypeName"] = ::lightwaveos::transitions::getTransitionName(::lightwaveos::transitions::TransitionType::FADE);
    
    JsonArray easings = data["easings"].to<JsonArray>();
    const char* easingNames[] = {
        "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
        "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
        "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
    };
    for (uint8_t i = 0; i < 10; i++) {
        JsonObject easing = easings.add<JsonObject>();
        easing["id"] = i;
        easing["name"] = easingNames[i];
    }
}

void WsTransitionCodec::encodeConfigSet(uint16_t defaultDuration, uint8_t defaultType, JsonObject& data) {
    data["defaultDuration"] = defaultDuration;
    data["defaultType"] = defaultType;
    data["defaultTypeName"] = ::lightwaveos::transitions::getTransitionName(static_cast<::lightwaveos::transitions::TransitionType>(defaultType));
    data["message"] = "Transition config updated";
}

void WsTransitionCodec::encodeList(JsonObject& data) {
    JsonArray types = data["types"].to<JsonArray>();
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(::lightwaveos::transitions::TransitionType::TYPE_COUNT); i++) {
        JsonObject type = types.add<JsonObject>();
        type["id"] = i;
        type["name"] = ::lightwaveos::transitions::getTransitionName(static_cast<::lightwaveos::transitions::TransitionType>(i));
    }
    
    JsonArray easings = data["easingCurves"].to<JsonArray>();
    const char* easingNames[] = {
        "LINEAR", "IN_QUAD", "OUT_QUAD", "IN_OUT_QUAD",
        "IN_CUBIC", "OUT_CUBIC", "IN_OUT_CUBIC",
        "IN_ELASTIC", "OUT_ELASTIC", "IN_OUT_ELASTIC"
    };
    for (uint8_t i = 0; i < 10; i++) {
        JsonObject easing = easings.add<JsonObject>();
        easing["id"] = i;
        easing["name"] = easingNames[i];
    }
    
    data["total"] = static_cast<uint8_t>(::lightwaveos::transitions::TransitionType::TYPE_COUNT);
}

void WsTransitionCodec::encodeTriggerStarted(uint8_t fromEffect, uint8_t toEffect, const char* toEffectName, uint8_t transitionType, const char* transitionName, uint16_t duration, JsonObject& data) {
    data["fromEffect"] = fromEffect;
    data["toEffect"] = toEffect;
    data["toEffectName"] = toEffectName ? toEffectName : "";
    data["transitionType"] = transitionType;
    data["transitionName"] = transitionName ? transitionName : "";
    data["duration"] = duration;
}

} // namespace codec
} // namespace lightwaveos
