/**
 * @file HttpZoneCodec.h
 * @brief JSON codec for HTTP zone endpoints parsing and validation
 *
 * Single canonical location for parsing HTTP zone endpoint JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from zone HTTP endpoints.
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
#include "WsZonesCodec.h"  // Reuse MAX_ERROR_MSG
#include "../effects/zones/ZoneDefinition.h"

namespace lightwaveos {
namespace codec {

// Reuse MAX_ERROR_MSG from WsZonesCodec (same namespace)

// ============================================================================
// Encoder Input Structs (POD, stack-friendly)
// ============================================================================

/**
 * @brief Zone list response data (summary)
 */
struct HttpZoneListSummaryData {
    bool enabled;
    uint8_t zoneCount;
    
    HttpZoneListSummaryData() : enabled(false), zoneCount(0) {}
};

/**
 * @brief Zone GET response data
 */
struct HttpZoneGetData {
    uint8_t id;
    bool enabled;
    
    HttpZoneGetData() : id(255), enabled(false) {}
};

/**
 * @brief Zone segment data
 */
struct HttpZoneSegmentData {
    uint8_t zoneId;
    uint8_t s1LeftStart;
    uint8_t s1LeftEnd;
    uint8_t s1RightStart;
    uint8_t s1RightEnd;
    uint8_t totalLeds;
    
    HttpZoneSegmentData()
        : zoneId(0), s1LeftStart(0), s1LeftEnd(0), s1RightStart(0), s1RightEnd(0), totalLeds(0) {}
};

/**
 * @brief Zone list item data
 */
struct HttpZoneListItemData {
    uint8_t id;
    bool enabled;
    uint8_t effectId;
    const char* effectName;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t blendMode;
    const char* blendModeName;
    
    HttpZoneListItemData()
        : id(0), enabled(false), effectId(0), effectName(nullptr), brightness(0),
          speed(0), paletteId(0), blendMode(0), blendModeName(nullptr) {}
};

/**
 * @brief Zone preset data
 */
struct HttpZonePresetData {
    uint8_t id;
    const char* name;
    
    HttpZonePresetData() : id(0), name("") {}
};

/**
 * @brief Zone list response data (full)
 */
struct HttpZoneListFullData {
    bool enabled;
    uint8_t zoneCount;
    const HttpZoneSegmentData* segments;
    size_t segmentCount;
    const HttpZoneListItemData* zones;
    size_t zoneItemCount;
    const HttpZonePresetData* presets;
    size_t presetCount;
    
    HttpZoneListFullData()
        : enabled(false), zoneCount(0), segments(nullptr), segmentCount(0),
          zones(nullptr), zoneItemCount(0), presets(nullptr), presetCount(0) {}
};

/**
 * @brief Zone GET response data (full)
 */
struct HttpZoneGetFullData {
    uint8_t id;
    bool enabled;
    uint8_t effectId;
    const char* effectName;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t blendMode;
    const char* blendModeName;
    
    HttpZoneGetFullData()
        : id(255), enabled(false), effectId(0), effectName(nullptr), brightness(0),
          speed(0), paletteId(0), blendMode(0), blendModeName(nullptr) {}
};

/**
 * @brief Zone set response data
 */
struct HttpZoneSetResultData {
    uint8_t zoneId;
    bool hasEffectId;
    uint8_t effectId;
    const char* effectName;
    bool hasBrightness;
    uint8_t brightness;
    bool hasSpeed;
    uint8_t speed;
    bool hasPaletteId;
    uint8_t paletteId;
    const char* paletteName;
    bool hasBlendMode;
    uint8_t blendMode;
    const char* blendModeName;
    bool hasEnabled;
    bool enabled;
    
    HttpZoneSetResultData()
        : zoneId(0), hasEffectId(false), effectId(0), effectName(nullptr),
          hasBrightness(false), brightness(0), hasSpeed(false), speed(0),
          hasPaletteId(false), paletteId(0), paletteName(nullptr),
          hasBlendMode(false), blendMode(0), blendModeName(nullptr),
          hasEnabled(false), enabled(false) {}
};

/**
 * @brief Zone layout decode result
 */
struct HttpZoneLayoutDecodeResult {
    bool success;
    uint8_t zoneCount;
    lightwaveos::zones::ZoneSegment segments[lightwaveos::zones::MAX_ZONES];
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneLayoutDecodeResult() : success(false), zoneCount(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetEffectDecodeResult {
    bool success;
    uint8_t effectId;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetEffectDecodeResult() : success(false), effectId(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetBrightnessDecodeResult {
    bool success;
    uint8_t brightness;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetBrightnessDecodeResult() : success(false), brightness(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetSpeedDecodeResult {
    bool success;
    uint8_t speed;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetSpeedDecodeResult() : success(false), speed(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetPaletteDecodeResult {
    bool success;
    uint8_t paletteId;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetPaletteDecodeResult() : success(false), paletteId(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetBlendDecodeResult {
    bool success;
    uint8_t blendMode;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetBlendDecodeResult() : success(false), blendMode(0) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

struct HttpZoneSetEnabledDecodeResult {
    bool success;
    bool enabled;
    char errorMsg[MAX_ERROR_MSG];
    
    HttpZoneSetEnabledDecodeResult() : success(false), enabled(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

/**
 * @brief Zone layout response data
 */
struct HttpZoneLayoutResultData {
    uint8_t zoneCount;
    
    HttpZoneLayoutResultData() : zoneCount(0) {}
};

/**
 * @brief HTTP Zone Command JSON Codec
 *
 * Single canonical parser for zone HTTP endpoints. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 */
class HttpZoneCodec {
public:
    // Decode functions (request parsing)
    static HttpZoneLayoutDecodeResult decodeLayout(JsonObjectConst root);
    static HttpZoneSetEffectDecodeResult decodeSetEffect(JsonObjectConst root);
    static HttpZoneSetBrightnessDecodeResult decodeSetBrightness(JsonObjectConst root);
    static HttpZoneSetSpeedDecodeResult decodeSetSpeed(JsonObjectConst root);
    static HttpZoneSetPaletteDecodeResult decodeSetPalette(JsonObjectConst root);
    static HttpZoneSetBlendDecodeResult decodeSetBlend(JsonObjectConst root);
    static HttpZoneSetEnabledDecodeResult decodeSetEnabled(JsonObjectConst root);

    // Encode functions (response building)
    static void encodeListSummary(const HttpZoneListSummaryData& data, JsonObject& obj);
    static void encodeGet(const HttpZoneGetData& data, JsonObject& obj);
    static void encodeListFull(const HttpZoneListFullData& data, JsonObject& obj);
    static void encodeGetFull(const HttpZoneGetFullData& data, JsonObject& obj);
    static void encodeSetResult(const HttpZoneSetResultData& data, JsonObject& obj);
    static void encodeLayoutResult(const HttpZoneLayoutResultData& data, JsonObject& obj);
};

} // namespace codec
} // namespace lightwaveos
