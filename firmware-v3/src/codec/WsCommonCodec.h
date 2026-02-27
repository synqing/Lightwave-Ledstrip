// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsCommonCodec.h
 * @brief Common JSON codec utilities shared across WebSocket command codecs
 *
 * Provides shared decode helpers to prevent duplication and drift across codecs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>

namespace lightwaveos {
namespace codec {

/**
 * @brief Decoded requestId (optional field common to all WS commands)
 */
struct RequestIdDecodeResult {
    bool success;
    const char* requestId;
    // Note: errorMsg not used for requestId (always succeeds), but kept for consistency
    char errorMsg[128];

    RequestIdDecodeResult() : success(false), requestId("") {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded type field (common to routing/gateway)
 */
struct TypeDecodeResult {
    bool success;
    const char* type;
    char errorMsg[128];

    TypeDecodeResult() : success(false), type("") {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Common WebSocket Codec Utilities
 *
 * Shared helpers to prevent duplication across codec modules.
 */
class WsCommonCodec {
public:
    /**
     * @brief Extract requestId from JSON root (optional field)
     * 
     * @param root JSON root object (const, prevents mutation)
     * @return RequestIdDecodeResult with requestId or empty string
     */
    static RequestIdDecodeResult decodeRequestId(JsonObjectConst root);
    
    /**
     * @brief Extract type from JSON root (required field for routing)
     * 
     * @param root JSON root object (const, prevents mutation)
     * @return TypeDecodeResult with type or empty string
     */
    static TypeDecodeResult decodeType(JsonObjectConst root);
    
    /**
     * @brief Decode apiKey field (for WebSocket auth)
     * @param root JSON root object
     * @return apiKey string (empty if not present)
     */
    static const char* decodeApiKey(JsonObjectConst root);
    
    /**
     * @brief Decode transition type from batch action params
     * @param params JSON variant (mutable) for batch action parameters
     * @return Transition type (defaults to 0 if not present)
     */
    static uint8_t decodeBatchTransitionType(JsonVariant params);
    
    /**
     * @brief Encode WebSocket event document
     * @param type Event type string
     * @param enabled Enabled state (for zone.enabledChanged events)
     * @param obj JSON object to encode into
     */
    static void encodeZoneEnabledEvent(const char* type, bool enabled, JsonObject& obj);

    /**
     * @brief Encode WebSocket response type
     * @param type Response type string
     * @param obj JSON object to encode into
     */
    static void encodeResponseType(const char* type, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
