/**
 * @file HttpTransitionCodec.h
 * @brief JSON codec for HTTP transition endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP transition endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from transition HTTP endpoints.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include "WsTransitionCodec.h"  // Reuse POD structs

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsTransitionCodec (same namespace)

// Reuse POD structs from WsTransitionCodec where applicable:
// - TransitionTriggerRequest (HTTP version is simpler, no requestId)
// - TransitionConfigSetRequest (HTTP version is simpler, no requestId)

// ============================================================================
// Decode Request Structs (for POST/PUT endpoints)
// ============================================================================

/**
 * @brief Decoded transition.trigger request (HTTP version)
 * Reuses TransitionTriggerRequest from WsTransitionCodec (just ignore requestId)
 */
using HttpTransitionTriggerRequest = TransitionTriggerRequest;
using HttpTransitionTriggerDecodeResult = TransitionTriggerDecodeResult;

/**
 * @brief Decoded transition.config SET request (HTTP version)
 * Reuses TransitionConfigSetRequest from WsTransitionCodec (just ignore requestId)
 */
using HttpTransitionConfigSetRequest = TransitionConfigSetRequest;
using HttpTransitionConfigSetDecodeResult = TransitionConfigSetDecodeResult;

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Config GET response data
 */
struct HttpTransitionConfigGetData {
    bool enabled;
    uint16_t defaultDuration;
    uint8_t defaultType;
    const char* defaultTypeName;  // Optional, can be nullptr
    
    HttpTransitionConfigGetData() : enabled(true), defaultDuration(1000), defaultType(0), defaultTypeName(nullptr) {}
};

/**
 * @brief HTTP Transition Command JSON Codec
 *
 * Single canonical parser for transition HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpTransitionCodec {
public:
    // Decode functions (request parsing)
    static HttpTransitionTriggerDecodeResult decodeTrigger(JsonObjectConst root);
    static HttpTransitionConfigSetDecodeResult decodeConfigSet(JsonObjectConst root);
    
    // Encode functions (response building)
    static void encodeConfigGet(const HttpTransitionConfigGetData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
