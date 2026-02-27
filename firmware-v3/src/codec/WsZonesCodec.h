// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsZonesCodec.h
 * @brief JSON codec for WebSocket zones commands parsing and validation
 *
 * Single canonical location for parsing WebSocket zones command JSON into typed C++ structs.
 * Enforces type checking, range validation, and unknown-key rejection.
 *
 * Rule: Only this module is allowed to read JSON keys from zones WS commands.
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

// Forward declarations for encoder dependencies
namespace lightwaveos {
namespace zones {
class ZoneComposer;
struct ZoneSegment;
// getBlendModeName is inline in BlendMode.h
} // namespace zones

namespace actors {
class RendererActor;
} // namespace actors
} // namespace lightwaveos

namespace lightwaveos {
namespace codec {

/**
 * @brief Maximum length for error messages
 */
static constexpr size_t MAX_ERROR_MSG = 128;

// ============================================================================
// Zone Enable Request
// ============================================================================

struct ZoneEnableRequest {
    bool enable;
    const char* requestId;
    
    ZoneEnableRequest() : enable(false), requestId("") {}
};

struct ZoneEnableDecodeResult {
    bool success;
    ZoneEnableRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneEnableDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Set Effect Request
// ============================================================================

struct ZoneSetEffectRequest {
    uint8_t zoneId;      // Required (0-3)
    uint8_t effectId;    // Required (0-127)
    const char* requestId;
    
    ZoneSetEffectRequest() : zoneId(255), effectId(255), requestId("") {}
};

struct ZoneSetEffectDecodeResult {
    bool success;
    ZoneSetEffectRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneSetEffectDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Set Brightness Request
// ============================================================================

struct ZoneSetBrightnessRequest {
    uint8_t zoneId;
    uint8_t brightness;
    const char* requestId;
    
    ZoneSetBrightnessRequest() : zoneId(255), brightness(128), requestId("") {}
};

struct ZoneSetBrightnessDecodeResult {
    bool success;
    ZoneSetBrightnessRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneSetBrightnessDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Set Speed Request
// ============================================================================

struct ZoneSetSpeedRequest {
    uint8_t zoneId;
    uint8_t speed;      // 1-100
    const char* requestId;
    
    ZoneSetSpeedRequest() : zoneId(255), speed(15), requestId("") {}
};

struct ZoneSetSpeedDecodeResult {
    bool success;
    ZoneSetSpeedRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneSetSpeedDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Set Palette Request
// ============================================================================

struct ZoneSetPaletteRequest {
    uint8_t zoneId;
    uint8_t paletteId;
    const char* requestId;
    
    ZoneSetPaletteRequest() : zoneId(255), paletteId(0), requestId("") {}
};

struct ZoneSetPaletteDecodeResult {
    bool success;
    ZoneSetPaletteRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneSetPaletteDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Set Blend Request
// ============================================================================

struct ZoneSetBlendRequest {
    uint8_t zoneId;
    uint8_t blendMode;  // 0-7
    const char* requestId;
    
    ZoneSetBlendRequest() : zoneId(255), blendMode(0), requestId("") {}
};

struct ZoneSetBlendDecodeResult {
    bool success;
    ZoneSetBlendRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneSetBlendDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zone Load Preset Request
// ============================================================================

struct ZoneLoadPresetRequest {
    uint8_t presetId;
    
    ZoneLoadPresetRequest() : presetId(0) {}
};

struct ZoneLoadPresetDecodeResult {
    bool success;
    ZoneLoadPresetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZoneLoadPresetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zones Get Request (no fields, just requestId)
// ============================================================================

struct ZonesGetRequest {
    const char* requestId;
    
    ZonesGetRequest() : requestId("") {}
};

struct ZonesGetDecodeResult {
    bool success;
    ZonesGetRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZonesGetDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zones Update Request (optional fields)
// ============================================================================

struct ZonesUpdateRequest {
    uint8_t zoneId;     // Required
    bool hasEffectId;
    bool hasBrightness;
    bool hasSpeed;
    bool hasPaletteId;
    bool hasBlendMode;
    uint8_t effectId;
    uint8_t brightness;
    uint8_t speed;
    uint8_t paletteId;
    uint8_t blendMode;
    const char* requestId;
    
    ZonesUpdateRequest() : zoneId(255), hasEffectId(false), hasBrightness(false),
                          hasSpeed(false), hasPaletteId(false), hasBlendMode(false),
                          effectId(0), brightness(128), speed(15), paletteId(0),
                          blendMode(0), requestId("") {}
};

struct ZonesUpdateDecodeResult {
    bool success;
    ZonesUpdateRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZonesUpdateDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// Zones Set Layout Request
// ============================================================================

struct ZoneSegmentRequest {
    uint8_t zoneId;
    uint8_t s1LeftStart;
    uint8_t s1LeftEnd;
    uint8_t s1RightStart;
    uint8_t s1RightEnd;
};

struct ZonesSetLayoutRequest {
    static constexpr uint8_t MAX_ZONES = 4;
    ZoneSegmentRequest zones[MAX_ZONES];
    uint8_t zoneCount;
    const char* requestId;
    
    ZonesSetLayoutRequest() : zoneCount(0), requestId("") {
        memset(zones, 0, sizeof(zones));
    }
};

struct ZonesSetLayoutDecodeResult {
    bool success;
    ZonesSetLayoutRequest request;
    char errorMsg[MAX_ERROR_MSG];
    
    ZonesSetLayoutDecodeResult() : success(false) {
        memset(errorMsg, 0, sizeof(errorMsg));
    }
};

// ============================================================================
// WebSocket Zones Command JSON Codec
// ============================================================================

/**
 * @brief WebSocket Zones Command JSON Codec
 *
 * Single canonical parser for zones WebSocket commands. Enforces:
 * - Required field validation
 * - Type checking using is<T>()
 * - Range validation
 * - Optional field defaults
 * - Unknown-key rejection (always enforced, no schema versioning)
 */
class WsZonesCodec {
public:
    // Decode functions (request parsing)
    static ZoneEnableDecodeResult decodeZoneEnable(JsonObjectConst root);
    static ZoneSetEffectDecodeResult decodeZoneSetEffect(JsonObjectConst root);
    static ZoneSetBrightnessDecodeResult decodeZoneSetBrightness(JsonObjectConst root);
    static ZoneSetSpeedDecodeResult decodeZoneSetSpeed(JsonObjectConst root);
    static ZoneSetPaletteDecodeResult decodeZoneSetPalette(JsonObjectConst root);
    static ZoneSetBlendDecodeResult decodeZoneSetBlend(JsonObjectConst root);
    static ZoneLoadPresetDecodeResult decodeZoneLoadPreset(JsonObjectConst root);
    static ZonesGetDecodeResult decodeZonesGet(JsonObjectConst root);
    static ZonesUpdateDecodeResult decodeZonesUpdate(JsonObjectConst root);
    static ZonesSetLayoutDecodeResult decodeZonesSetLayout(JsonObjectConst root);
    
    // Encoder functions (response encoding)
    // Populate JsonObject data from domain objects
    static void encodeZonesGet(const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZonesList(const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZoneEnabledChanged(bool enabled, JsonObject& data);
    static void encodeZonesChanged(uint8_t zoneId, const char* const updatedFields[], uint8_t updatedCount, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZonesEffectChanged(uint8_t zoneId, uint8_t effectId, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZonePaletteChanged(uint8_t zoneId, uint8_t paletteId, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZoneBlendChanged(uint8_t zoneId, uint8_t blendMode, const ::lightwaveos::zones::ZoneComposer& composer, const ::lightwaveos::actors::RendererActor* renderer, JsonObject& data);
    static void encodeZonesLayoutChanged(uint8_t zoneCount, JsonObject& data);
    
private:
    /**
     * @brief Check for unknown keys in root object
     *
     * @param root JSON root object
     * @param allowedKeys Array of allowed key names
     * @param allowedCount Number of allowed keys
     * @param errorMsg Output buffer for error message
     * @return true if valid, false if unknown key found
     */
    static bool checkUnknownKeys(JsonObjectConst root, const char* allowedKeys[], size_t allowedCount, char* errorMsg);
};

} // namespace codec
} // namespace lightwaveos
