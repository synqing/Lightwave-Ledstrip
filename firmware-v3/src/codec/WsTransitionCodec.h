// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsTransitionCodec.h
 * @brief JSON codec for WebSocket transition commands parsing and validation
 *
 * Single canonical location for parsing WebSocket transition command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from transition WS commands.
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
 * @brief Decoded transition.trigger request
 */
struct TransitionTriggerRequest {
    uint8_t toEffect;         // Required (0-127)
    uint8_t transitionType;   // Optional (default: 0)
    bool random;              // Optional (default: false)
    
    TransitionTriggerRequest() : toEffect(255), transitionType(0), random(false) {}
};

struct TransitionTriggerDecodeResult {
    bool success;
    TransitionTriggerRequest request;
    char errorMsg[MAX_ERROR_MSG];

    TransitionTriggerDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded transition.config SET request
 */
struct TransitionConfigSetRequest {
    const char* requestId;    // Optional
    uint16_t defaultDuration; // Optional (default: 1000)
    uint8_t defaultType;      // Optional (default: 0)
    
    TransitionConfigSetRequest() : requestId(""), defaultDuration(1000), defaultType(0) {}
};

struct TransitionConfigSetDecodeResult {
    bool success;
    TransitionConfigSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    TransitionConfigSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded transitions.trigger request
 */
struct TransitionsTriggerRequest {
    const char* requestId;    // Optional
    uint8_t toEffect;         // Required (0-127)
    uint8_t type;             // Optional (default: 0)
    uint16_t duration;        // Optional (default: 1000)
    
    TransitionsTriggerRequest() : requestId(""), toEffect(255), type(0), duration(1000) {}
};

struct TransitionsTriggerDecodeResult {
    bool success;
    TransitionsTriggerRequest request;
    char errorMsg[MAX_ERROR_MSG];

    TransitionsTriggerDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded simple request (requestId only, for get/list commands)
 */
struct TransitionSimpleRequest {
    const char* requestId;   // Optional
    
    TransitionSimpleRequest() : requestId("") {}
};

struct TransitionSimpleDecodeResult {
    bool success;
    TransitionSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    TransitionSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Transition Command JSON Codec
 *
 * Single canonical parser for transition WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsTransitionCodec {
public:
    // Decode functions (request parsing)
    static TransitionTriggerDecodeResult decodeTrigger(JsonObjectConst root);
    static TransitionConfigSetDecodeResult decodeConfigSet(JsonObjectConst root);
    static TransitionsTriggerDecodeResult decodeTransitionsTrigger(JsonObjectConst root);
    static TransitionSimpleDecodeResult decodeSimple(JsonObjectConst root);  // For getTypes, configGet, list
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    static void encodeGetTypes(JsonObject& data);
    static void encodeConfigGet(JsonObject& data);
    static void encodeConfigSet(uint16_t defaultDuration, uint8_t defaultType, JsonObject& data);
    static void encodeList(JsonObject& data);
    static void encodeTriggerStarted(uint8_t fromEffect, uint8_t toEffect, const char* toEffectName, uint8_t transitionType, const char* transitionName, uint16_t duration, JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
