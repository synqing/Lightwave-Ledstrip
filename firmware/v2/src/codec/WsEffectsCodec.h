/**
 * @file WsEffectsCodec.h
 * @brief JSON codec for WebSocket effects commands parsing and validation
 *
 * Single canonical location for parsing WebSocket effects command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from effects WS commands.
 * All other code consumes typed request structs.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <cstddef>
#include "../config/effect_ids.h"

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

/**
 * @brief Decoded effects.setCurrent request
 */
struct EffectsSetCurrentRequest {
    EffectId effectId;               // Effect ID (stable namespaced)
    const char* requestId;           // Optional (for correlation)
    bool hasTransition;              // True if transition object present
    uint8_t transitionType;          // Transition type (if hasTransition)
    uint16_t transitionDuration;     // Duration in ms (if hasTransition)

    EffectsSetCurrentRequest()
        : effectId(INVALID_EFFECT_ID), requestId(""), hasTransition(false),
          transitionType(0), transitionDuration(1000) {}
};

/**
 * @brief Decode result with typed request and error
 */
struct EffectsSetCurrentDecodeResult {
    bool success;
    EffectsSetCurrentRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSetCurrentDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Group B: Single-Value Setters
// ============================================================================

/**
 * @brief Decoded setEffect request (legacy command)
 */
struct EffectsSetEffectRequest {
    EffectId effectId;       // Effect ID (stable namespaced)

    EffectsSetEffectRequest() : effectId(INVALID_EFFECT_ID) {}
};

struct EffectsSetEffectDecodeResult {
    bool success;
    EffectsSetEffectRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSetEffectDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded setBrightness request
 */
struct EffectsSetBrightnessRequest {
    uint8_t value;           // Required (0-255)
    
    EffectsSetBrightnessRequest() : value(128) {}
};

struct EffectsSetBrightnessDecodeResult {
    bool success;
    EffectsSetBrightnessRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSetBrightnessDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded setSpeed request
 */
struct EffectsSetSpeedRequest {
    uint8_t value;           // Required (1-50)
    
    EffectsSetSpeedRequest() : value(15) {}
};

struct EffectsSetSpeedDecodeResult {
    bool success;
    EffectsSetSpeedRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSetSpeedDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded setPalette request
 */
struct EffectsSetPaletteRequest {
    uint8_t paletteId;       // Required (0-N, validated externally)
    
    EffectsSetPaletteRequest() : paletteId(0) {}
};

struct EffectsSetPaletteDecodeResult {
    bool success;
    EffectsSetPaletteRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSetPaletteDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Group C: Complex Payloads
// ============================================================================

/**
 * @brief Decoded effects.getMetadata request
 */
struct EffectsGetMetadataRequest {
    EffectId effectId;       // Optional (INVALID_EFFECT_ID means invalid/missing)
    const char* requestId;   // Optional

    EffectsGetMetadataRequest() : effectId(INVALID_EFFECT_ID), requestId("") {}
};

struct EffectsGetMetadataDecodeResult {
    bool success;
    EffectsGetMetadataRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsGetMetadataDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded effects.list request
 */
struct EffectsListRequest {
    uint8_t page;            // Optional (1+, default: 1)
    uint8_t limit;           // Optional (1-50, default: 20)
    bool details;            // Optional (default: false)
    const char* requestId;   // Optional
    
    EffectsListRequest() : page(1), limit(20), details(false), requestId("") {}
};

struct EffectsListDecodeResult {
    bool success;
    EffectsListRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsListDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded effects.parameters.get request
 */
struct EffectsParametersGetRequest {
    EffectId effectId;       // Optional (INVALID_EFFECT_ID means use current)
    const char* requestId;   // Optional

    EffectsParametersGetRequest() : effectId(INVALID_EFFECT_ID), requestId("") {}
};

struct EffectsParametersGetDecodeResult {
    bool success;
    EffectsParametersGetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsParametersGetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded effects.parameters.set request
 */
struct EffectsParametersSetRequest {
    EffectId effectId;               // Optional (INVALID_EFFECT_ID means use current)
    const char* requestId;           // Optional
    bool hasParameters;              // True if parameters object present
    JsonObjectConst parameters;      // Dynamic parameters object (if hasParameters)

    EffectsParametersSetRequest() : effectId(INVALID_EFFECT_ID), requestId(""), hasParameters(false), parameters() {}
};

struct EffectsParametersSetDecodeResult {
    bool success;
    EffectsParametersSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsParametersSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded effects.getByFamily request
 */
struct EffectsGetByFamilyRequest {
    uint8_t familyId;        // Required (0-9)
    const char* requestId;   // Optional
    
    EffectsGetByFamilyRequest() : familyId(255), requestId("") {}
};

struct EffectsGetByFamilyDecodeResult {
    bool success;
    EffectsGetByFamilyRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsGetByFamilyDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded parameters.set request (global parameters)
 */
struct ParametersSetRequest {
    const char* requestId;   // Optional
    bool hasBrightness;      // True if field present
    bool hasSpeed;
    bool hasPaletteId;
    bool hasHue;
    bool hasIntensity;
    bool hasSaturation;
    bool hasComplexity;
    bool hasVariation;
    uint8_t brightness;      // If hasBrightness
    uint8_t speed;           // If hasSpeed (1-50)
    uint8_t paletteId;       // If hasPaletteId
    uint8_t hue;             // If hasHue
    uint8_t intensity;       // If hasIntensity
    uint8_t saturation;      // If hasSaturation
    uint8_t complexity;      // If hasComplexity
    uint8_t variation;       // If hasVariation
    
    ParametersSetRequest() 
        : requestId(""), hasBrightness(false), hasSpeed(false), hasPaletteId(false),
          hasHue(false), hasIntensity(false), hasSaturation(false),
          hasComplexity(false), hasVariation(false),
          brightness(128), speed(15), paletteId(0), hue(0),
          intensity(128), saturation(255), complexity(128), variation(0) {}
};

struct ParametersSetDecodeResult {
    bool success;
    ParametersSetRequest request;
    char errorMsg[MAX_ERROR_MSG];

    ParametersSetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Decoded simple request (requestId only, for getCurrent, getCategories, parameters.get)
 */
struct EffectsSimpleRequest {
    const char* requestId;   // Optional
    
    EffectsSimpleRequest() : requestId("") {}
};

struct EffectsSimpleDecodeResult {
    bool success;
    EffectsSimpleRequest request;
    char errorMsg[MAX_ERROR_MSG];

    EffectsSimpleDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief WebSocket Effects Command JSON Codec
 *
 * Single canonical parser for effects WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class WsEffectsCodec {
public:
    // Decode functions (request parsing)
    static EffectsSetCurrentDecodeResult decodeSetCurrent(JsonObjectConst root);
    
    // Group B: Single-value setters
    static EffectsSetEffectDecodeResult decodeSetEffect(JsonObjectConst root);
    static EffectsSetBrightnessDecodeResult decodeSetBrightness(JsonObjectConst root);
    static EffectsSetSpeedDecodeResult decodeSetSpeed(JsonObjectConst root);
    static EffectsSetPaletteDecodeResult decodeSetPalette(JsonObjectConst root);
    
    // Group C: Complex payloads
    static EffectsGetMetadataDecodeResult decodeGetMetadata(JsonObjectConst root);
    static EffectsListDecodeResult decodeList(JsonObjectConst root);
    static EffectsParametersGetDecodeResult decodeParametersGet(JsonObjectConst root);
    static EffectsParametersSetDecodeResult decodeEffectsParametersSet(JsonObjectConst root);
    static EffectsGetByFamilyDecodeResult decodeGetByFamily(JsonObjectConst root);
    static ParametersSetDecodeResult decodeParametersSet(JsonObjectConst root);
    static EffectsSimpleDecodeResult decodeSimple(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    
    // Effects encoders
    static void encodeGetCurrent(EffectId effectId, const char* name, uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, bool isIEffect, const char* description, uint8_t version, JsonObject& data);
    static void encodeChanged(EffectId effectId, const char* name, bool transitionActive, JsonObject& data);
    static void encodeMetadata(EffectId effectId, const char* name, const char* familyName, uint8_t familyId, const char* story, const char* opticalIntent, uint8_t tags, JsonObject& data);
    static void encodeList(uint16_t effectCount, uint16_t startIdx, uint16_t endIdx, uint8_t page, uint8_t limit, bool details, const char* const effectNames[], const EffectId effectIds[], const char* const categories[], JsonObject& data);
    static void encodeByFamily(uint8_t familyId, const char* familyName, const EffectId patternIndices[], uint16_t count, JsonObject& data);
    static void encodeCategories(const char* const familyNames[], const uint8_t familyCounts[], uint8_t total, JsonObject& data);
    
    // Parameters encoders
    static void encodeParametersGet(EffectId effectId,
                                    const char* name,
                                    bool hasParameters,
                                    const char* const paramNames[],
                                    const char* const paramDisplayNames[],
                                    const float paramMins[],
                                    const float paramMaxs[],
                                    const float paramDefaults[],
                                    const float paramValues[],
                                    const char* const paramTypes[],
                                    const float paramSteps[],
                                    const char* const paramGroups[],
                                    const char* const paramUnits[],
                                    const bool paramAdvanced[],
                                    uint8_t paramCount,
                                    const char* persistenceMode,
                                    bool persistenceDirty,
                                    const char* persistenceLastError,
                                    JsonObject& data);
    static void encodeParametersSetChanged(EffectId effectId, const char* name, const char* const queuedKeys[], uint8_t queuedCount, const char* const failedKeys[], uint8_t failedCount, JsonObject& data);
    static void encodeGlobalParametersGet(uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, JsonObject& data);
    static void encodeParametersChanged(const char* const updatedKeys[], uint8_t updatedCount, uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t hue, uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation, JsonObject& data);
};

} // namespace codec
} // namespace lightwaveos
