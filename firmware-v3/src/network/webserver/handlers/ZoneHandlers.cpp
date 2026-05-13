#include "ZoneHandlers.h"
#include "../../RequestValidator.h"
#include "../../../config/effect_ids.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../palettes/Palettes_Master.h"
#include "../../../effects/zones/BlendMode.h"

using namespace lightwaveos::palettes;
using namespace lightwaveos::actors;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ZoneHandlers::handleList(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    sendSuccessResponseLarge(request, [composer, &cachedState](JsonObject& data) {
        data["enabled"] = composer->isEnabled();
        data["zoneCount"] = composer->getZoneCount();

        // Wire-format note (2026-05-02 migration): every zoneId emitted on the
        // wire is 1-indexed (1..3). Internal C++ storage stays 0-indexed; the
        // boundary translation is `wire = internal + 1`.
        JsonArray segmentsArray = data["segments"].to<JsonArray>();
        const lightwaveos::zones::ZoneSegment* segments = composer->getZoneConfig();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject seg = segmentsArray.add<JsonObject>();
            seg["zoneId"] = static_cast<uint8_t>(segments[i].zoneId + 1);
            seg["s1LeftStart"] = segments[i].s1LeftStart;
            seg["s1LeftEnd"] = segments[i].s1LeftEnd;
            seg["s1RightStart"] = segments[i].s1RightStart;
            seg["s1RightEnd"] = segments[i].s1RightEnd;
            seg["totalLeds"] = segments[i].totalLeds;
        }

        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            const uint8_t wireZoneId = static_cast<uint8_t>(i + 1);
            zone["id"] = wireZoneId;
            zone["zoneId"] = wireZoneId;
            zone["enabled"] = composer->isZoneEnabled(i);
            zone["effectId"] = composer->getZoneEffect(i);
            // SAFE: Uses cached state (no cross-core access)
            EffectId effectId = composer->getZoneEffect(i);
            const char* effName = cachedState.findEffectName(effectId);
            if (effName) {
                zone["effectName"] = effName;
            }
            zone["brightness"] = composer->getZoneBrightness(i);
            zone["speed"] = composer->getZoneSpeed(i);
            zone["paletteId"] = composer->getZonePalette(i);
            zone["blendMode"] = static_cast<uint8_t>(composer->getZoneBlendMode(i));
            zone["blendModeName"] = lightwaveos::zones::getBlendModeName(composer->getZoneBlendMode(i));
        }

        // Available presets
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < 5; i++) {
            JsonObject preset = presets.add<JsonObject>();
            preset["id"] = i;
            preset["name"] = lightwaveos::zones::ZoneComposer::getPresetName(i);
        }
    }, 1536);
}

void ZoneHandlers::handleLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneLayout, request);

    // Parse zones array
    JsonArray zonesArray = doc["zones"];
    if (!zonesArray || zonesArray.size() == 0 || zonesArray.size() > lightwaveos::zones::MAX_ZONES) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid zones array");
        return;
    }

    // Convert JSON array to ZoneSegment array
    lightwaveos::zones::ZoneSegment segments[lightwaveos::zones::MAX_ZONES];
    uint8_t zoneCount = zonesArray.size();
    
    // DEFENSIVE CHECK: Validate zoneCount doesn't exceed array bounds
    if (zoneCount > lightwaveos::zones::MAX_ZONES) {
        zoneCount = lightwaveos::zones::MAX_ZONES;  // Clamp to safe maximum
    }
    
    for (uint8_t i = 0; i < zoneCount; i++) {
        // DEFENSIVE CHECK: Validate array index before access
        if (i >= lightwaveos::zones::MAX_ZONES) {
            break;  // Safety: should never happen, but protects against corruption
        }
        
        JsonObject zoneObj = zonesArray[i];
        if (!zoneObj.containsKey("zoneId") || !zoneObj.containsKey("s1LeftStart") ||
            !zoneObj.containsKey("s1LeftEnd") || !zoneObj.containsKey("s1RightStart") ||
            !zoneObj.containsKey("s1RightEnd")) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::INVALID_VALUE, "Zone segment missing required fields");
            return;
        }
        
        // Wire-format migration (2026-05-02): segment zoneId on the wire is
        // 1-indexed (1..3). Translate to 0-indexed internal storage which is
        // what ZoneSegment.zoneId expects (matches the predefined ZONE_*_CONFIG
        // arrays in ZoneDefinition.h).
        uint8_t rawWireZoneId = zoneObj["zoneId"];
        bool segZoneIdValid = false;
        uint8_t internalSegZoneId = lightwaveos::network::wireZoneIdToInternal(
            rawWireZoneId, segZoneIdValid);
        if (!segZoneIdValid) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::INVALID_VALUE,
                              "Segment zoneId out of range (must be 1-3)",
                              "zoneId");
            return;
        }
        segments[i].zoneId = internalSegZoneId;
        
        // DEFENSIVE CHECK: Validate LED indices against STRIP_LENGTH
        uint8_t s1LeftStart = zoneObj["s1LeftStart"];
        uint8_t s1LeftEnd = zoneObj["s1LeftEnd"];
        uint8_t s1RightStart = zoneObj["s1RightStart"];
        uint8_t s1RightEnd = zoneObj["s1RightEnd"];
        
        // Clamp to valid range [0, STRIP_LENGTH-1]
        constexpr uint16_t STRIP_LENGTH = 160;
        if (s1LeftStart >= STRIP_LENGTH) s1LeftStart = 0;
        if (s1LeftEnd >= STRIP_LENGTH) s1LeftEnd = STRIP_LENGTH - 1;
        if (s1RightStart >= STRIP_LENGTH) s1RightStart = 0;
        if (s1RightEnd >= STRIP_LENGTH) s1RightEnd = STRIP_LENGTH - 1;
        
        // Ensure start <= end
        if (s1LeftStart > s1LeftEnd) s1LeftEnd = s1LeftStart;
        if (s1RightStart > s1RightEnd) s1RightEnd = s1RightStart;
        
        segments[i].s1LeftStart = s1LeftStart;
        segments[i].s1LeftEnd = s1LeftEnd;
        segments[i].s1RightStart = s1RightStart;
        segments[i].s1RightEnd = s1RightEnd;
        
        // Calculate totalLeds (with overflow protection)
        uint8_t leftSize = (s1LeftEnd >= s1LeftStart) ? (s1LeftEnd - s1LeftStart + 1) : 1;
        uint8_t rightSize = (s1RightEnd >= s1RightStart) ? (s1RightEnd - s1RightStart + 1) : 1;
        segments[i].totalLeds = leftSize + rightSize; // Per-strip count (strip 2 mirrors strip 1)
    }

    // Set layout with validation
    if (!composer->setLayout(segments, zoneCount)) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Layout validation failed");
        return;
    }

    sendSuccessResponse(request, [zoneCount](JsonObject& respData) {
        respData["zoneCount"] = zoneCount;
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleGet(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    // Wire-format migration (2026-05-02): path :zoneId is 1-indexed.
    // extractZoneIdFromPath() now performs the wire→internal translation and
    // returns 255 (sentinel) when the wire value is outside [1..3].
    uint8_t zoneId = extractZoneIdFromPath(request);

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    sendSuccessResponse(request, [composer, zoneId, &cachedState](JsonObject& data) {
        const uint8_t wireZoneId = static_cast<uint8_t>(zoneId + 1);
        data["id"] = wireZoneId;
        data["zoneId"] = wireZoneId;
        data["enabled"] = composer->isZoneEnabled(zoneId);
        EffectId effectId = composer->getZoneEffect(zoneId);
        data["effectId"] = effectId;
        // SAFE: Uses cached state (no cross-core access)
        if (const char* name = cachedState.findEffectName(effectId)) {
            data["effectName"] = name;
        }
        data["brightness"] = composer->getZoneBrightness(zoneId);
        data["speed"] = composer->getZoneSpeed(zoneId);
        data["paletteId"] = composer->getZonePalette(zoneId);
        data["blendMode"] = static_cast<uint8_t>(composer->getZoneBlendMode(zoneId));
        data["blendModeName"] = lightwaveos::zones::getBlendModeName(composer->getZoneBlendMode(zoneId));
    });
}

void ZoneHandlers::handleSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::ActorSystem& actors, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    // extractZoneIdFromPath() returns the internal 0-indexed zone index after
    // wire→internal translation, or 255 if the wire path digit is out of range.
    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEffect, request);

    EffectId effectId = doc["effectId"];

    // SAFE: Uses cached state (no cross-core access)
    // Accept stable namespaced EffectIds. Also tolerate legacy numeric indices (0..effectCount-1)
    // by translating them to the cached registry EffectId.
    const char* effectName = cachedState.findEffectName(effectId);
    if (!effectName && effectId < cachedState.effectCount && effectId < cachedState.MAX_CACHED_EFFECTS) {
        effectId = cachedState.effectIds[static_cast<uint16_t>(effectId)];
        effectName = cachedState.findEffectName(effectId);
    }
    if (!effectName) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID not registered", "effectId");
        return;
    }

    composer->setZoneEffect(zoneId, effectId);

    sendSuccessResponse(request, [zoneId, effectId, &cachedState](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["effectId"] = effectId;
        // SAFE: Uses cached state (no cross-core access)
        if (const char* name = cachedState.findEffectName(effectId)) {
            respData["effectName"] = name;
        }
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBrightness, request);

    uint8_t brightness = doc["brightness"];
    composer->setZoneBrightness(zoneId, brightness);

    sendSuccessResponse(request, [zoneId, brightness](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["brightness"] = brightness;
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneSpeed, request);

    // Schema validates speed is 1-100
    uint8_t speed = doc["speed"];
    composer->setZoneSpeed(zoneId, speed);

    sendSuccessResponse(request, [zoneId, speed](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["speed"] = speed;
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZonePalette, request);

    uint8_t paletteId = doc["paletteId"];
    if (paletteId >= MASTER_PALETTE_COUNT) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Palette ID out of range (0-74)", "paletteId");
        return;
    }

    // Validate palette ID before access (defensive check)
    uint8_t safe_palette = lightwaveos::palettes::validatePaletteId(paletteId);
    composer->setZonePalette(zoneId, safe_palette);

    sendSuccessResponse(request, [zoneId, safe_palette](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["paletteId"] = safe_palette;
        respData["paletteName"] = MasterPaletteNames[safe_palette];
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBlend, request);

    // Schema validates blendMode is 0-7
    uint8_t blendModeVal = doc["blendMode"];
    lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
    composer->setZoneBlendMode(zoneId, blendMode);

    sendSuccessResponse(request, [zoneId, blendModeVal, blendMode](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["blendMode"] = blendModeVal;
        respData["blendModeName"] = lightwaveos::zones::getBlendModeName(blendMode);
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range (must be 1-3)");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEnabled, request);

    bool enabled = doc["enabled"];
    composer->setZoneEnabled(zoneId, enabled);

    sendSuccessResponse(request, [zoneId, enabled](JsonObject& respData) {
        respData["zoneId"] = static_cast<uint8_t>(zoneId + 1);
        respData["enabled"] = enabled;
    });

    if (broadcastZoneState) broadcastZoneState();
}

uint8_t ZoneHandlers::extractZoneIdFromPath(AsyncWebServerRequest* request) {
    // Extract zone ID from path like /api/v1/zones/2/effect.
    // Wire-format migration (2026-05-02): the path digit is a 1-indexed wire
    // zoneId. Translate to internal 0-indexed and reject out-of-range with the
    // sentinel value 255 (callers report HTTP 404 NOT_FOUND).
    String path = request->url();
    int zonesIdx = path.indexOf("/zones/");
    if (zonesIdx >= 0 && zonesIdx + 7 < path.length()) {
        char digit = path.charAt(zonesIdx + 7);
        if (digit >= '1' && digit <= '9') {  // explicit reject of '0'
            uint8_t wireZoneId = static_cast<uint8_t>(digit - '0');
            bool valid = false;
            uint8_t internalZoneId = lightwaveos::network::wireZoneIdToInternal(wireZoneId, valid);
            if (valid) {
                return internalZoneId;
            }
        }
    }
    return 255;  // Invalid (out of range or unparsable)
}

// ============================================================================
// Zone Configuration Persistence (stubs)
// ============================================================================

void ZoneHandlers::handleConfigGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr) {
    (void)zoneConfigMgr;
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone configuration export not yet implemented (planned: A6 snapshot system, Phase 3)");
}

void ZoneHandlers::handleConfigSave(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr) {
    (void)zoneConfigMgr;
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone configuration save not yet implemented (planned: A6 snapshot system, Phase 3)");
}

void ZoneHandlers::handleConfigLoad(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, void* zoneConfigMgr, std::function<void()> broadcastZoneState) {
    (void)zoneConfigMgr;
    (void)broadcastZoneState;
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone configuration load not yet implemented (planned: A6 snapshot system, Phase 3)");
}

// ============================================================================
// Zone Timing
// ============================================================================

void ZoneHandlers::handleTimingGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone timing metrics not yet implemented (planned: A12, Phase 8)");
}

void ZoneHandlers::handleTimingReset(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone timing metrics not yet implemented (planned: A12, Phase 8)");
}

// ============================================================================
// Zone Audio Configuration
// ============================================================================

void ZoneHandlers::handleAudioConfigGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone audio routing not yet implemented (planned: D-4/A3, Phase 1)");
}

void ZoneHandlers::handleAudioConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    (void)data;
    (void)len;
    (void)broadcastZoneState;

    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone audio routing not yet implemented (planned: D-4/A3, Phase 1)");
}

// ============================================================================
// Zone Beat Trigger
// ============================================================================

void ZoneHandlers::handleBeatTriggerGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone beat-trigger not yet implemented (planned: Phase 4 live performance controls)");
}

void ZoneHandlers::handleBeatTriggerSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    (void)data;
    (void)len;
    (void)broadcastZoneState;

    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone beat-trigger not yet implemented (planned: Phase 4 live performance controls)");
}

// ============================================================================
// Zone Reorder
// ============================================================================

void ZoneHandlers::handleReorder(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    (void)data;
    (void)len;
    (void)broadcastZoneState;

    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    sendErrorResponse(request, HttpStatus::NOT_IMPLEMENTED,
                      "NOT_IMPLEMENTED",
                      "Zone reorder not yet implemented (planned: A9, Phase 6 advanced layering)");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
