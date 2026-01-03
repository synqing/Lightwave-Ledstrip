#include "ZoneHandlers.h"
#include "../../RequestValidator.h"
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

        // Include segment definitions
        JsonArray segmentsArray = data["segments"].to<JsonArray>();
        const lightwaveos::zones::ZoneSegment* segments = composer->getZoneConfig();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject seg = segmentsArray.add<JsonObject>();
            seg["zoneId"] = segments[i].zoneId;
            seg["s1LeftStart"] = segments[i].s1LeftStart;
            seg["s1LeftEnd"] = segments[i].s1LeftEnd;
            seg["s1RightStart"] = segments[i].s1RightStart;
            seg["s1RightEnd"] = segments[i].s1RightEnd;
            seg["totalLeds"] = segments[i].totalLeds;
        }

        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = i;
            zone["enabled"] = composer->isZoneEnabled(i);
            zone["effectId"] = composer->getZoneEffect(i);
            // SAFE: Uses cached state (no cross-core access)
            uint8_t effectId = composer->getZoneEffect(i);
            if (effectId < cachedState.effectCount && cachedState.effectNames[effectId]) {
                zone["effectName"] = cachedState.effectNames[effectId];
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
        
        // DEFENSIVE CHECK: Validate zoneId before using as array index
        uint8_t rawZoneId = zoneObj["zoneId"];
        segments[i].zoneId = lightwaveos::network::validateZoneIdInRequest(rawZoneId);
        
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

    uint8_t zoneId = extractZoneIdFromPath(request);

    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    sendSuccessResponse(request, [composer, zoneId, &cachedState](JsonObject& data) {
        data["id"] = zoneId;
        data["enabled"] = composer->isZoneEnabled(zoneId);
        uint8_t effectId = composer->getZoneEffect(zoneId);
        data["effectId"] = effectId;
        // SAFE: Uses cached state (no cross-core access)
        if (effectId < cachedState.effectCount && cachedState.effectNames[effectId]) {
            data["effectName"] = cachedState.effectNames[effectId];
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

    uint8_t zoneId = extractZoneIdFromPath(request);
    if (zoneId >= composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEffect, request);

    uint8_t effectId = doc["effectId"];
    // SAFE: Uses cached state (no cross-core access)
    if (effectId >= cachedState.effectCount) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    composer->setZoneEffect(zoneId, effectId);

    sendSuccessResponse(request, [zoneId, effectId, &cachedState](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["effectId"] = effectId;
        // SAFE: Uses cached state (no cross-core access)
        if (effectId < cachedState.effectCount && cachedState.effectNames[effectId]) {
            respData["effectName"] = cachedState.effectNames[effectId];
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
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBrightness, request);

    uint8_t brightness = doc["brightness"];
    composer->setZoneBrightness(zoneId, brightness);

    sendSuccessResponse(request, [zoneId, brightness](JsonObject& respData) {
        respData["zoneId"] = zoneId;
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
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneSpeed, request);

    // Schema validates speed is 1-50
    uint8_t speed = doc["speed"];
    composer->setZoneSpeed(zoneId, speed);

    sendSuccessResponse(request, [zoneId, speed](JsonObject& respData) {
        respData["zoneId"] = zoneId;
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
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
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
        respData["zoneId"] = zoneId;
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
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneBlend, request);

    // Schema validates blendMode is 0-7
    uint8_t blendModeVal = doc["blendMode"];
    lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
    composer->setZoneBlendMode(zoneId, blendMode);

    sendSuccessResponse(request, [zoneId, blendModeVal, blendMode](JsonObject& respData) {
        respData["zoneId"] = zoneId;
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
                          ErrorCodes::OUT_OF_RANGE, "Zone ID out of range");
        return;
    }

    JsonDocument doc;
    VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::ZoneEnabled, request);

    bool enabled = doc["enabled"];
    composer->setZoneEnabled(zoneId, enabled);

    sendSuccessResponse(request, [zoneId, enabled](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["enabled"] = enabled;
    });

    if (broadcastZoneState) broadcastZoneState();
}

uint8_t ZoneHandlers::extractZoneIdFromPath(AsyncWebServerRequest* request) {
    // Extract zone ID from path like /api/v1/zones/2/effect
    String path = request->url();
    // Find the digit after "/zones/"
    int zonesIdx = path.indexOf("/zones/");
    if (zonesIdx >= 0 && zonesIdx + 7 < path.length()) {
        char digit = path.charAt(zonesIdx + 7);
        if (digit >= '0' && digit <= '9') {
            return digit - '0';
        }
    }
    return 255;  // Invalid
}

// ============================================================================
// Persistence API Handlers (Phase 1.5)
// ============================================================================

void ZoneHandlers::handleConfigGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!configManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone config manager not available");
        return;
    }

    // Export current configuration to check persistence status
    lightwaveos::persistence::ZoneConfigData currentConfig;
    configManager->exportConfig(currentConfig);

    sendSuccessResponse(request, [composer, &currentConfig](JsonObject& data) {
        data["enabled"] = composer->isEnabled();
        data["zoneCount"] = composer->getZoneCount();
        data["configVersion"] = currentConfig.version;
        data["checksumValid"] = currentConfig.isValid();

        // Per-zone summary
        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = i;
            zone["effectId"] = composer->getZoneEffect(i);
            zone["brightness"] = composer->getZoneBrightness(i);
            zone["speed"] = composer->getZoneSpeed(i);
            zone["paletteId"] = composer->getZonePalette(i);
        }
    });
}

void ZoneHandlers::handleConfigSave(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!configManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone config manager not available");
        return;
    }

    bool success = configManager->saveToNVS();

    if (success) {
        sendSuccessResponse(request, [](JsonObject& data) {
            data["saved"] = true;
            data["message"] = "Zone configuration saved to NVS";
        });
    } else {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to save zone configuration to NVS");
    }
}

void ZoneHandlers::handleConfigLoad(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!configManager) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone config manager not available");
        return;
    }

    bool success = configManager->loadFromNVS();

    if (success) {
        // Broadcast updated zone state after loading
        if (broadcastZoneState) broadcastZoneState();

        sendSuccessResponse(request, [composer](JsonObject& data) {
            data["loaded"] = true;
            data["message"] = "Zone configuration loaded from NVS";
            data["enabled"] = composer->isEnabled();
            data["zoneCount"] = composer->getZoneCount();
        });
    } else {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "No saved zone configuration found in NVS or configuration invalid");
    }
}

// ============================================================================
// Timing Metrics API Handler (Phase 2a.1)
// ============================================================================

void ZoneHandlers::handleTimingGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    const lightwaveos::zones::ZoneTimingMetrics& timing = composer->getTimingMetrics();

    sendSuccessResponse(request, [composer, &timing](JsonObject& data) {
        // Per-zone render times array
        JsonArray zoneRenderUs = data["zoneRenderUs"].to<JsonArray>();
        for (uint8_t i = 0; i < lightwaveos::zones::MAX_ZONES; i++) {
            zoneRenderUs.add(timing.zoneRenderUs[i]);
        }

        // Blend/composite time
        data["zoneBlendUs"] = timing.zoneBlendUs;

        // Total zone system overhead
        data["zoneTotalUs"] = timing.zoneTotalUs;

        // Frame skip count (exceeded 2000us threshold)
        data["frameSkipCount"] = timing.frameSkipCount;

        // Average frame time in milliseconds
        data["averageFrameMs"] = timing.getAverageFrameMs();

        // Frame count for statistics
        data["frameCount"] = timing.frameCount;

        // Last update timestamp
        data["lastUpdateMs"] = timing.lastUpdateMs;

        // Zone system status
        data["enabled"] = composer->isEnabled();
        data["zoneCount"] = composer->getZoneCount();
    });
}

void ZoneHandlers::handleTimingReset(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    composer->resetTimingMetrics();

    sendSuccessResponse(request, [](JsonObject& data) {
        data["reset"] = true;
        data["message"] = "Zone timing metrics reset";
    });
}

// ============================================================================
// Zone Audio Config API Handlers (Phase 2b.1)
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

    lightwaveos::zones::ZoneAudioConfig audio = composer->getZoneAudioConfig(zoneId);

    sendSuccessResponse(request, [zoneId, &audio](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["tempoSync"] = audio.tempoSync;
        data["beatModulation"] = audio.beatModulation;
        data["tempoSpeedScale"] = audio.tempoSpeedScale;
        data["beatDecay"] = audio.beatDecay;
        data["audioBand"] = audio.audioBand;
    });
}

void ZoneHandlers::handleAudioConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
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

    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, error.c_str());
        return;
    }

    // Get current config and apply updates (partial update pattern)
    lightwaveos::zones::ZoneAudioConfig audio = composer->getZoneAudioConfig(zoneId);
    bool hasUpdates = false;

    if (doc.containsKey("tempoSync")) {
        audio.tempoSync = doc["tempoSync"].as<bool>();
        hasUpdates = true;
    }

    if (doc.containsKey("beatModulation")) {
        audio.beatModulation = doc["beatModulation"].as<uint8_t>();
        hasUpdates = true;
    }

    if (doc.containsKey("tempoSpeedScale")) {
        audio.tempoSpeedScale = doc["tempoSpeedScale"].as<uint8_t>();
        hasUpdates = true;
    }

    if (doc.containsKey("beatDecay")) {
        audio.beatDecay = doc["beatDecay"].as<uint8_t>();
        hasUpdates = true;
    }

    if (doc.containsKey("audioBand")) {
        uint8_t band = doc["audioBand"].as<uint8_t>();
        audio.audioBand = (band > 3) ? 0 : band;  // Clamp to 0-3
        hasUpdates = true;
    }

    if (!hasUpdates) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "No valid audio config fields provided");
        return;
    }

    // Apply the updated config
    composer->setZoneAudioConfig(zoneId, audio);

    sendSuccessResponse(request, [zoneId, &audio](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["tempoSync"] = audio.tempoSync;
        respData["beatModulation"] = audio.beatModulation;
        respData["tempoSpeedScale"] = audio.tempoSpeedScale;
        respData["beatDecay"] = audio.beatDecay;
        respData["audioBand"] = audio.audioBand;
    });

    if (broadcastZoneState) broadcastZoneState();
}

// ============================================================================
// Zone Beat Trigger API Handlers (Phase 2b.2)
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

    bool enabled = false;
    uint8_t interval = 4;
    uint8_t effectIds[8] = {0};
    uint8_t effectCount = 0;
    uint8_t currentIndex = 0;

    composer->getZoneBeatTriggerConfig(zoneId, enabled, interval, effectIds, effectCount, currentIndex);

    sendSuccessResponse(request, [zoneId, enabled, interval, &effectIds, effectCount, currentIndex](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["enabled"] = enabled;
        data["interval"] = interval;
        data["currentIndex"] = currentIndex;
        JsonArray effects = data["effectList"].to<JsonArray>();
        for (uint8_t i = 0; i < effectCount; i++) {
            effects.add(effectIds[i]);
        }
    });
}

void ZoneHandlers::handleBeatTriggerSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
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

    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, error.c_str());
        return;
    }

    bool hasUpdates = false;

    // Handle enabled field
    if (doc.containsKey("enabled")) {
        composer->setZoneBeatTriggerEnabled(zoneId, doc["enabled"].as<bool>());
        hasUpdates = true;
    }

    // Handle interval field
    if (doc.containsKey("interval")) {
        uint8_t interval = doc["interval"].as<uint8_t>();
        // Clamp to valid range (1-32)
        if (interval < 1) interval = 1;
        if (interval > 32) interval = 32;
        composer->setZoneBeatTriggerInterval(zoneId, interval);
        hasUpdates = true;
    }

    // Handle effectList array
    if (doc.containsKey("effectList")) {
        JsonArray effects = doc["effectList"].as<JsonArray>();
        if (effects) {
            uint8_t effectIds[8] = {0};
            uint8_t count = 0;
            for (JsonVariant v : effects) {
                if (count >= 8) break;
                effectIds[count++] = v.as<uint8_t>();
            }
            composer->setZoneBeatTriggerEffectList(zoneId, effectIds, count);
            hasUpdates = true;
        }
    }

    if (!hasUpdates) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "No valid beat trigger config fields provided");
        return;
    }

    // Retrieve updated config for response
    bool enabled = false;
    uint8_t interval = 4;
    uint8_t effectIds[8] = {0};
    uint8_t effectCount = 0;
    uint8_t currentIndex = 0;

    composer->getZoneBeatTriggerConfig(zoneId, enabled, interval, effectIds, effectCount, currentIndex);

    sendSuccessResponse(request, [zoneId, enabled, interval, &effectIds, effectCount, currentIndex](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["enabled"] = enabled;
        respData["interval"] = interval;
        respData["currentIndex"] = currentIndex;
        JsonArray effects = respData["effectList"].to<JsonArray>();
        for (uint8_t i = 0; i < effectCount; i++) {
            effects.add(effectIds[i]);
        }
    });

    if (broadcastZoneState) broadcastZoneState();
}

// ============================================================================
// Zone Reordering API Handler (Phase 2c.1)
// ============================================================================

void ZoneHandlers::handleReorder(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, error.c_str());
        return;
    }

    // Validate "order" array exists
    if (!doc.containsKey("order") || !doc["order"].is<JsonArray>()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "Required field 'order' must be an array", "order");
        return;
    }

    JsonArray orderArray = doc["order"];
    uint8_t orderCount = orderArray.size();

    // Validate order array size matches current zone count
    if (orderCount != composer->getZoneCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Order array size must match current zone count", "order");
        return;
    }

    // Validate order array size is within bounds
    if (orderCount == 0 || orderCount > lightwaveos::zones::MAX_ZONES) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Order array size out of range (1-4)", "order");
        return;
    }

    // Extract order values into array
    uint8_t newOrder[lightwaveos::zones::MAX_ZONES];
    uint8_t idx = 0;
    for (JsonVariant v : orderArray) {
        if (idx >= lightwaveos::zones::MAX_ZONES) break;
        if (!v.is<int>()) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::INVALID_TYPE, "Order array must contain integers", "order");
            return;
        }
        int val = v.as<int>();
        if (val < 0 || val >= orderCount) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE, "Zone ID in order array out of range", "order");
            return;
        }
        newOrder[idx++] = static_cast<uint8_t>(val);
    }

    // Attempt reorder - ZoneComposer will validate CENTER ORIGIN constraint
    bool success = composer->reorderZones(newOrder, orderCount);

    if (!success) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Reorder failed: CENTER ORIGIN constraint violated - Zone 0 must contain LEDs 79/80", "order");
        return;
    }

    // Build response with new order
    sendSuccessResponse(request, [&newOrder, orderCount](JsonObject& respData) {
        JsonArray orderResp = respData["order"].to<JsonArray>();
        for (uint8_t i = 0; i < orderCount; i++) {
            orderResp.add(newOrder[i]);
        }
        respData["zoneCount"] = orderCount;
    });

    // Broadcast zone state update to all WebSocket clients
    if (broadcastZoneState) broadcastZoneState();
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
