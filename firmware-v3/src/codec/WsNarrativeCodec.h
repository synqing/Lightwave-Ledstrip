// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsNarrativeCodec.h
 * @brief JSON codec for WebSocket narrative commands parsing and validation
 *
 * Single canonical location for parsing WebSocket narrative command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from narrative WS commands.
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
 * @brief Decoded simple request (requestId only, for getStatus)
 */
struct NarrativeSimpleRequest {
    const char* requestId;   // Optional
    
    NarrativeSimpleRequest() : requestId("") {}
};

struct NarrativeSimpleDecodeResult {
    bool success;
    NarrativeSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    NarrativeSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded narrative.config request (handles both GET and SET)
 */
struct NarrativeConfigRequest {
    const char* requestId;   // Optional
    bool isSet;              // true if this is a SET operation (has durations/enabled/curves)
    
    // SET operation fields (all optional)
    bool hasDurations;
    float buildDuration;
    float holdDuration;
    float releaseDuration;
    float restDuration;
    
    bool hasCurves;
    uint8_t buildCurveId;
    uint8_t releaseCurveId;
    
    bool hasHoldBreathe;
    float holdBreathe;
    
    bool hasSnapAmount;
    float snapAmount;
    
    bool hasDurationVariance;
    float durationVariance;
    
    bool hasEnabled;
    bool enabled;
    
    NarrativeConfigRequest() : requestId(""), isSet(false),
        hasDurations(false), buildDuration(1.5f), holdDuration(0.5f), releaseDuration(1.5f), restDuration(0.5f),
        hasCurves(false), buildCurveId(1), releaseCurveId(6),
        hasHoldBreathe(false), holdBreathe(0.0f),
        hasSnapAmount(false), snapAmount(0.0f),
        hasDurationVariance(false), durationVariance(0.0f),
        hasEnabled(false), enabled(false) {}
};

struct NarrativeConfigDecodeResult {
    bool success;
    NarrativeConfigRequest request;
    char errorMsg[MAX_ERROR_MSG];

    NarrativeConfigDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Narrative Command JSON Codec
 *
 * Single canonical parser for narrative WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsNarrativeCodec {
public:
    // Decode functions (request parsing)
    static NarrativeSimpleDecodeResult decodeSimple(JsonObjectConst root);
    static NarrativeConfigDecodeResult decodeConfig(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    
    // Status encoder
    static void encodeStatus(bool enabled, float tensionPercent, float phaseT, float cycleT, const char* phaseName, uint8_t phaseId, float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, float tempoMultiplier, float complexityScaling, JsonObject& data);
    
    // Config GET encoder
    static void encodeConfigGet(float buildDuration, float holdDuration, float releaseDuration, float restDuration, float totalDuration, uint8_t buildCurveId, uint8_t releaseCurveId, float holdBreathe, float snapAmount, float durationVariance, bool enabled, JsonObject& data);
    
    // Config SET result encoder
    static void encodeConfigSetResult(bool updated, JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
