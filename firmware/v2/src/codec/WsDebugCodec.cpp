/**
 * @file WsDebugCodec.cpp
 * @brief WebSocket debug codec implementation
 *
 * Single canonical JSON parser for debug WebSocket commands.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsDebugCodec.h"
#include "WsCommonCodec.h"
#include <cstring>

namespace lightwaveos {
namespace codec {

// ============================================================================
// Simple Request
// ============================================================================

DebugSimpleDecodeResult WsDebugCodec::decodeSimple(JsonObjectConst root) {
    DebugSimpleDecodeResult result;
    result.request = DebugSimpleRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    result.success = true;
    return result;
}

// ============================================================================
// Debug Audio Set Command
// ============================================================================

DebugAudioSetDecodeResult WsDebugCodec::decodeDebugAudioSet(JsonObjectConst root) {
    DebugAudioSetDecodeResult result;
    result.request = DebugAudioSetRequest();

    // Extract requestId using shared helper
    RequestIdDecodeResult requestIdResult = WsCommonCodec::decodeRequestId(root);
    result.request.requestId = requestIdResult.requestId;

    // Extract verbosity (optional, 0-5)
    if (root.containsKey("verbosity")) {
        if (!root["verbosity"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'verbosity' must be an integer");
            return result;
        }
        int verbosity = root["verbosity"].as<int>();
        if (verbosity < 0 || verbosity > 5) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "verbosity must be 0-5: %d", verbosity);
            return result;
        }
        result.request.hasVerbosity = true;
        result.request.verbosity = static_cast<uint8_t>(verbosity);
    }

    // Extract baseInterval (optional, 1-1000)
    if (root.containsKey("baseInterval")) {
        if (!root["baseInterval"].is<int>()) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "Field 'baseInterval' must be an integer");
            return result;
        }
        int interval = root["baseInterval"].as<int>();
        if (interval < 1 || interval > 1000) {
            snprintf(result.errorMsg, MAX_ERROR_MSG, "baseInterval must be 1-1000: %d", interval);
            return result;
        }
        result.request.hasBaseInterval = true;
        result.request.baseInterval = static_cast<uint16_t>(interval);
    }

    // Require at least one field
    if (!result.request.hasVerbosity && !result.request.hasBaseInterval) {
        snprintf(result.errorMsg, MAX_ERROR_MSG, "At least one of 'verbosity' or 'baseInterval' required");
        return result;
    }

    result.success = true;
    return result;
}

// ============================================================================
// Encoder Functions (Response Encoding)
// ============================================================================

void WsDebugCodec::encodeDebugAudioState(
    uint8_t verbosity,
    uint16_t baseInterval,
    uint16_t interval8band,
    uint16_t interval64bin,
    uint16_t intervalDma,
    const char* const* levels,
    size_t levelsCount,
    JsonObject& data
) {
    data["verbosity"] = verbosity;
    data["baseInterval"] = baseInterval;

    // Encode intervals object
    JsonObject intervals = data["intervals"].to<JsonObject>();
    intervals["8band"] = interval8band;
    intervals["64bin"] = interval64bin;
    intervals["dma"] = intervalDma;

    // Encode levels array
    JsonArray levelsArray = data["levels"].to<JsonArray>();
    for (size_t i = 0; i < levelsCount; i++) {
        levelsArray.add(levels[i]);
    }
}

void WsDebugCodec::encodeDebugAudioUpdated(
    uint8_t verbosity,
    uint16_t baseInterval,
    uint16_t interval8band,
    uint16_t interval64bin,
    uint16_t intervalDma,
    JsonObject& data
) {
    data["verbosity"] = verbosity;
    data["baseInterval"] = baseInterval;

    // Encode intervals object
    JsonObject intervals = data["intervals"].to<JsonObject>();
    intervals["8band"] = interval8band;
    intervals["64bin"] = interval64bin;
    intervals["dma"] = intervalDma;
}

} // namespace codec
} // namespace lightwaveos
