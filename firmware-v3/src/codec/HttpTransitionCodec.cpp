// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpTransitionCodec.cpp
 * @brief HTTP transition codec implementation
 *
 * Single canonical JSON parser for transition HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpTransitionCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

HttpTransitionTriggerDecodeResult HttpTransitionCodec::decodeTrigger(JsonObjectConst root) {
    HttpTransitionTriggerDecodeResult result;
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
    
    // Extract transitionType (optional, HTTP uses "type" not "transitionType")
    if (root["type"].is<int>()) {
        int transType = root["type"].as<int>();
        if (transType < 0 || transType > 255) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "type out of range (0-255): %d", transType);
            return result;
        }
        result.request.transitionType = static_cast<uint8_t>(transType);
    } else {
        result.request.transitionType = 0;
    }
    
    // Extract random (optional bool, default: false)
    if (root["random"].is<bool>()) {
        result.request.random = root["random"].as<bool>();
    } else {
        result.request.random = false;
    }
    
    result.success = true;
    return result;
}

HttpTransitionConfigSetDecodeResult HttpTransitionCodec::decodeConfigSet(JsonObjectConst root) {
    // Reuse WS codec decode function (HTTP just ignores requestId)
    return WsTransitionCodec::decodeConfigSet(root);
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpTransitionCodec::encodeConfigGet(const HttpTransitionConfigGetData& data, JsonObject& obj) {
    obj["enabled"] = data.enabled;
    obj["defaultDuration"] = data.defaultDuration;
    obj["defaultType"] = data.defaultType;
    if (data.defaultTypeName) {
        obj["defaultTypeName"] = data.defaultTypeName;
    }
}

} // namespace codec
} // namespace lightwaveos
