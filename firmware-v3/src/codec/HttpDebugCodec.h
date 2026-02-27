// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpDebugCodec.h
 * @brief JSON codec for HTTP debug endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP debug endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from debug HTTP endpoints.
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
#include "WsDebugCodec.h"  // Reuse POD structs

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsDebugCodec (same namespace)

// Reuse POD structs from WsDebugCodec where applicable:
// - DebugAudioSetRequest (HTTP just ignores requestId)
// - DebugAudioSetDecodeResult

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Audio debug config response data
 */
struct HttpDebugAudioConfigData {
    uint8_t verbosity;
    uint16_t baseInterval;
    uint16_t interval8Band;
    uint16_t interval64Bin;
    uint16_t intervalDMA;
    
    HttpDebugAudioConfigData() 
        : verbosity(0), baseInterval(1000), interval8Band(0), interval64Bin(0), intervalDMA(0) {}
};

/**
 * @brief HTTP Debug Command JSON Codec
 *
 * Single canonical parser for debug HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpDebugCodec {
public:
    // Decode functions (request parsing)
    // Reuse WsDebugCodec::decodeDebugAudioSet (HTTP just ignores requestId)
    static DebugAudioSetDecodeResult decodeAudioDebugSet(JsonObjectConst root);
    
    // Encode functions (response building)
    static void encodeAudioDebugGet(const HttpDebugAudioConfigData& data, JsonObject& obj);
    
    /**
     * @brief Encode simple audio debug disabled response
     * @param obj JSON object to encode into
     */
    static void encodeAudioDebugDisabled(JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
