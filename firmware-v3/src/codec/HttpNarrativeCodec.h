// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HttpNarrativeCodec.h
 * @brief JSON codec for HTTP narrative endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP narrative endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from narrative HTTP endpoints.
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
#include "WsNarrativeCodec.h"  // Reuse POD structs and encode functions

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsNarrativeCodec (same namespace)

// Reuse POD structs from WsNarrativeCodec where applicable:
// - NarrativeConfigRequest (HTTP just ignores requestId)
// - NarrativeConfigDecodeResult

/**
 * @brief HTTP Narrative Command JSON Codec
 *
 * Single canonical parser for narrative HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpNarrativeCodec {
public:
    // Decode functions (request parsing)
    // Reuse WsNarrativeCodec::decodeConfig (HTTP just ignores requestId)
    static NarrativeConfigDecodeResult decodeConfigSet(JsonObjectConst root);
    
    // Encode functions (response building)
    // Wrapper functions that call WsNarrativeCodec encode functions
    static void encodeStatus(bool enabled, float tensionPercent, float phaseT, float cycleT, const char* phaseName, uint8_t phaseId, float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, float tempoMultiplier, float complexityScaling, JsonObject& data);
    static void encodeConfigGet(float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, uint8_t buildCurveId, uint8_t releaseCurveId, float holdBreathe, float snapAmount, float durationVariance, bool enabled, JsonObject& data);
    static void encodeConfigSetResult(bool updated, JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
