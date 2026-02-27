/**
 * @file HttpDebugCodec.cpp
 * @brief HTTP debug codec implementation
 *
 * Single canonical JSON parser for debug HTTP endpoints.
 * Enforces strict validation and prevents JSON key access outside this module.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "HttpDebugCodec.h"

namespace lightwaveos {
namespace codec {

// ============================================================================
// Decode Functions
// ============================================================================

DebugAudioSetDecodeResult HttpDebugCodec::decodeAudioDebugSet(JsonObjectConst root) {
    // Reuse WS codec decode function (HTTP just ignores requestId)
    return WsDebugCodec::decodeDebugAudioSet(root);
}

// ============================================================================
// Encode Functions
// ============================================================================

void HttpDebugCodec::encodeAudioDebugGet(const HttpDebugAudioConfigData& data, JsonObject& obj) {
    obj["verbosity"] = data.verbosity;
    obj["baseInterval"] = data.baseInterval;
    
    JsonObject intervals = obj["intervals"].to<JsonObject>();
    intervals["8band"] = data.interval8Band;
    intervals["64bin"] = data.interval64Bin;
    intervals["dma"] = data.intervalDMA;
}

void HttpDebugCodec::encodeAudioDebugDisabled(JsonObject& obj) {
    obj["verbosity"] = 0;
    obj["message"] = "Audio sync not enabled";
}

} // namespace codec
} // namespace lightwaveos
