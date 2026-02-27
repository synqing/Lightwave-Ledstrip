/**
 * @file WsDebugCodec.h
 * @brief JSON codec for WebSocket debug commands parsing and validation
 *
 * Single canonical location for parsing WebSocket debug command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from debug WS commands.
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

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Simple Request (requestId only)
// ============================================================================

/**
 * @brief Decoded simple request (requestId only)
 */
struct DebugSimpleRequest {
    const char* requestId;   // Optional
    
    DebugSimpleRequest() : requestId("") {}
};

struct DebugSimpleDecodeResult {
    bool success;
    DebugSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    DebugSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
        request = DebugSimpleRequest();
    }
};

// ============================================================================
// Debug Audio Set Command
// ============================================================================

/**
 * @brief Decoded debug.audio.set request
 */
struct DebugAudioSetRequest {
    bool hasVerbosity;       // Whether verbosity was provided
    uint8_t verbosity;       // 0-5 (only valid if hasVerbosity is true)
    bool hasBaseInterval;    // Whether baseInterval was provided
    uint16_t baseInterval;   // 1-1000 (only valid if hasBaseInterval is true)
    const char* requestId;   // Optional
    
    DebugAudioSetRequest() : hasVerbosity(false), verbosity(0), hasBaseInterval(false), baseInterval(1000), requestId("") {}
};

struct DebugAudioSetDecodeResult {
    bool success;
    DebugAudioSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    DebugAudioSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
        request = DebugAudioSetRequest();
    }
};

/**
 * @brief WebSocket Debug Command JSON Codec
 *
 * Single canonical parser for debug WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsDebugCodec {
public:
    // Decode functions (request parsing)
    static DebugSimpleDecodeResult decodeSimple(JsonObjectConst root);
    static DebugAudioSetDecodeResult decodeDebugAudioSet(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    
    // Debug audio encoders
    static void encodeDebugAudioState(
        uint8_t verbosity,
        uint16_t baseInterval,
        uint16_t interval8band,
        uint16_t interval64bin,
        uint16_t intervalDma,
        const char* const* levels,
        size_t levelsCount,
        JsonObject& data
    );
    
    static void encodeDebugAudioUpdated(
        uint8_t verbosity,
        uint16_t baseInterval,
        uint16_t interval8band,
        uint16_t interval64bin,
        uint16_t intervalDma,
        JsonObject& data
    );
};

} // namespace codec
} // namespace lightwaveos
