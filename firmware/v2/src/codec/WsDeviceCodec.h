/**
 * @file WsDeviceCodec.h
 * @brief JSON codec for WebSocket device commands parsing and validation
 *
 * Single canonical location for parsing WebSocket device command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from device WS commands.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

/**
 * @brief Decoded device command request (all device commands are no-param, just requestId)
 */
struct DeviceRequest {
    const char* requestId;   // Optional
    
    DeviceRequest() : requestId("") {}
};

struct DeviceDecodeResult {
    bool success;
    DeviceRequest request;
    char errorMsg[MAX_ERROR_MSG];

    DeviceDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Device Command JSON Codec
 *
 * Single canonical parser for device WebSocket commands. All device commands
 * are no-param queries (requestId only), so this is a minimal codec.
 */
class WsDeviceCodec {
public:
    /**
     * @brief Decode any device command JSON (extracts requestId only)
     *
     * @param root JSON root object (const, prevents mutation)
     * @return DeviceDecodeResult with request or error
     */
    static DeviceDecodeResult decode(JsonObjectConst root);
};

} // namespace codec
} // namespace lightwaveos
