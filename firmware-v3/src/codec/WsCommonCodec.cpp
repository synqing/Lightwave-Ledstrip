// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsCommonCodec.cpp
 * @brief Common WebSocket codec utilities implementation
 *
 * Shared decode helpers to prevent duplication and drift across codecs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsCommonCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

RequestIdDecodeResult WsCommonCodec::decodeRequestId(JsonObjectConst root) {
    RequestIdDecodeResult result;
    result.success = true;
    result.requestId = "";
    memset(result.errorMsg, 0, sizeof(result.errorMsg));

    // Extract requestId (optional)
    if (root["requestId"].is<const char*>()) {
        result.requestId = root["requestId"].as<const char*>();
    }

    return result;
}

TypeDecodeResult WsCommonCodec::decodeType(JsonObjectConst root) {
    TypeDecodeResult result;
    result.success = true;
    result.type = "";
    memset(result.errorMsg, 0, sizeof(result.errorMsg));

    // Extract type (optional, defaults to empty string)
    if (root["type"].is<const char*>()) {
        result.type = root["type"].as<const char*>();
    }

    return result;
}

const char* WsCommonCodec::decodeApiKey(JsonObjectConst root) {
    return root["apiKey"] | "";
}

uint8_t WsCommonCodec::decodeBatchTransitionType(JsonVariant params) {
    return params["type"] | 0;
}

void WsCommonCodec::encodeZoneEnabledEvent(const char* type, bool enabled, JsonObject& obj) {
    obj["type"] = type;
    obj["enabled"] = enabled;
}

void WsCommonCodec::encodeResponseType(const char* type, JsonObject& obj) {
    obj["type"] = type;
}

} // namespace codec
} // namespace lightwaveos
