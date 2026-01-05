/**
 * @file WsZonesCommands.cpp
 * @brief WebSocket zones command handlers implementation
 */

#include "WsZonesCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../effects/zones/BlendMode.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::zones;

static void handleZoneEnable(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        return; // Silently ignore if zones not available
    }
    
    bool enable = doc["enable"] | false;
    ctx.zoneComposer->setEnabled(enable);
    
    // Send immediate zone.enabledChanged event
    if (ctx.ws) {
        StaticJsonDocument<128> eventDoc;
        eventDoc["type"] = "zones.enabledChanged";
        eventDoc["enabled"] = enable;
        String eventOutput;
        serializeJson(eventDoc, eventOutput);
        ctx.ws->textAll(eventOutput);
    }
    
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

static void handleZoneSetEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 0;
    uint8_t effectId = doc["effectId"] | 0;
    
    // DEFENSIVE CHECK: Validate zoneId and effectId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneEffect(zoneId, effectId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.effectChanged", requestId, [&ctx, zoneId, effectId](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = effectId;
        current["effectName"] = ctx.renderer->getEffectName(effectId);
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
    });
    client->text(response);
}

static void handleZoneSetBrightness(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 0;
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    uint8_t brightness = doc["brightness"] | 128;
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneBrightness(zoneId, brightness);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, brightness](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonArray updated = data["updated"].to<JsonArray>();
        updated.add("brightness");
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["brightness"] = brightness;
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
    });
    client->text(response);
}

static void handleZoneSetSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 0;
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    uint8_t speed = doc["speed"] | 15;
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    if (speed < 1 || speed > 100) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Speed must be 1-100", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneSpeed(zoneId, speed);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, speed](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonArray updated = data["updated"].to<JsonArray>();
        updated.add("speed");
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = speed;
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
    });
    client->text(response);
}

static void handleZoneSetPalette(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 0;
    uint8_t paletteId = doc["paletteId"] | 0;
    
    // DEFENSIVE CHECK: Validate zoneId and paletteId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    paletteId = lightwaveos::network::validatePaletteIdInRequest(paletteId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZonePalette(zoneId, paletteId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.paletteChanged", requestId, [&ctx, zoneId, paletteId](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["effectName"] = ctx.renderer->getEffectName(ctx.zoneComposer->getZoneEffect(zoneId));
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = paletteId;
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
    });
    client->text(response);
}

static void handleZoneSetBlend(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 0;
    uint8_t blendModeVal = doc["blendMode"] | 0;
    
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    // Validate blendMode is 0-7
    if (blendModeVal > 7) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "BlendMode must be 0-7", requestId));
        return;
    }
    
    lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
    ctx.zoneComposer->setZoneBlendMode(zoneId, blendMode);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.blendChanged", requestId, [&ctx, zoneId, blendModeVal, blendMode](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["effectName"] = ctx.renderer->getEffectName(ctx.zoneComposer->getZoneEffect(zoneId));
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = blendModeVal;
        current["blendModeName"] = getBlendModeName(blendMode);
    });
    client->text(response);
}

static void handleZoneLoadPreset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        return; // Silently ignore if zones not available
    }
    
    uint8_t presetId = doc["presetId"] | 0;
    ctx.zoneComposer->loadPreset(presetId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

static void handleZonesGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    String response = buildWsResponse("zones", requestId, [&ctx](JsonObject& data) {
        data["enabled"] = ctx.zoneComposer->isEnabled();
        data["zoneCount"] = ctx.zoneComposer->getZoneCount();
        
        // Include segment definitions
        JsonArray segmentsArray = data["segments"].to<JsonArray>();
        const ZoneSegment* segments = ctx.zoneComposer->getZoneConfig();
        for (uint8_t i = 0; i < ctx.zoneComposer->getZoneCount(); i++) {
            JsonObject seg = segmentsArray.add<JsonObject>();
            seg["zoneId"] = segments[i].zoneId;
            seg["s1LeftStart"] = segments[i].s1LeftStart;
            seg["s1LeftEnd"] = segments[i].s1LeftEnd;
            seg["s1RightStart"] = segments[i].s1RightStart;
            seg["s1RightEnd"] = segments[i].s1RightEnd;
            seg["totalLeds"] = segments[i].totalLeds;
        }
        
        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < ctx.zoneComposer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = i;
            zone["enabled"] = ctx.zoneComposer->isZoneEnabled(i);
            zone["effectId"] = ctx.zoneComposer->getZoneEffect(i);
            zone["effectName"] = ctx.renderer->getEffectName(ctx.zoneComposer->getZoneEffect(i));
            zone["brightness"] = ctx.zoneComposer->getZoneBrightness(i);
            zone["speed"] = ctx.zoneComposer->getZoneSpeed(i);
            zone["paletteId"] = ctx.zoneComposer->getZonePalette(i);
            zone["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(i));
            zone["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(i));
        }
        
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < 5; i++) {
            JsonObject preset = presets.add<JsonObject>();
            preset["id"] = i;
            preset["name"] = ZoneComposer::getPresetName(i);
        }
    });
    client->text(response);
}

static void handleZonesList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    String response = buildWsResponse("zones.list", requestId, [&ctx](JsonObject& data) {
        data["enabled"] = ctx.zoneComposer->isEnabled();
        data["zoneCount"] = ctx.zoneComposer->getZoneCount();
        
        // Include segment definitions
        JsonArray segmentsArray = data["segments"].to<JsonArray>();
        const ZoneSegment* segments = ctx.zoneComposer->getZoneConfig();
        for (uint8_t i = 0; i < ctx.zoneComposer->getZoneCount(); i++) {
            JsonObject seg = segmentsArray.add<JsonObject>();
            seg["zoneId"] = segments[i].zoneId;
            seg["s1LeftStart"] = segments[i].s1LeftStart;
            seg["s1LeftEnd"] = segments[i].s1LeftEnd;
            seg["s1RightStart"] = segments[i].s1RightStart;
            seg["s1RightEnd"] = segments[i].s1RightEnd;
            seg["totalLeds"] = segments[i].totalLeds;
        }
        
        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < ctx.zoneComposer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = i;
            zone["enabled"] = ctx.zoneComposer->isZoneEnabled(i);
            zone["effectId"] = ctx.zoneComposer->getZoneEffect(i);
            zone["effectName"] = ctx.renderer->getEffectName(ctx.zoneComposer->getZoneEffect(i));
            zone["brightness"] = ctx.zoneComposer->getZoneBrightness(i);
            zone["speed"] = ctx.zoneComposer->getZoneSpeed(i);
            zone["paletteId"] = ctx.zoneComposer->getZonePalette(i);
            zone["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(i));
            zone["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(i));
        }
        
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < 5; i++) {
            JsonObject preset = presets.add<JsonObject>();
            preset["id"] = i;
            preset["name"] = ZoneComposer::getPresetName(i);
        }
    });
    client->text(response);
}

static void handleZonesUpdate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;
    
    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }
    
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    bool updatedEffect = false;
    bool updatedBrightness = false;
    bool updatedSpeed = false;
    bool updatedPalette = false;
    bool updatedBlend = false;
    bool updatedAudio = false;  // Phase 2b.1

    if (doc.containsKey("effectId")) {
        uint8_t effectId = doc["effectId"] | 0;
        // DEFENSIVE CHECK: Validate effectId before array access
        effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
        if (effectId < ctx.renderer->getEffectCount()) {
            ctx.zoneComposer->setZoneEffect(zoneId, effectId);
            updatedEffect = true;
        }
    }

    if (doc.containsKey("brightness")) {
        uint8_t brightness = doc["brightness"] | 128;
        ctx.zoneComposer->setZoneBrightness(zoneId, brightness);
        updatedBrightness = true;
    }

    if (doc.containsKey("speed")) {
        uint8_t speed = doc["speed"] | 15;
        ctx.zoneComposer->setZoneSpeed(zoneId, speed);
        updatedSpeed = true;
    }

    if (doc.containsKey("paletteId")) {
        uint8_t paletteId = doc["paletteId"] | 0;
        // DEFENSIVE CHECK: Validate paletteId before array access
        paletteId = lightwaveos::network::validatePaletteIdInRequest(paletteId);
        ctx.zoneComposer->setZonePalette(zoneId, paletteId);
        updatedPalette = true;
    }

    if (doc.containsKey("blendMode")) {
        uint8_t blendModeVal = doc["blendMode"] | 0;
        if (blendModeVal <= 7) {
            lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
            ctx.zoneComposer->setZoneBlendMode(zoneId, blendMode);
            updatedBlend = true;
        }
    }

    // Phase 2b.1: Zone Audio Config fields
    if (doc.containsKey("tempoSync")) {
        ctx.zoneComposer->setZoneTempoSync(zoneId, doc["tempoSync"].as<bool>());
        updatedAudio = true;
    }
    if (doc.containsKey("beatModulation")) {
        ctx.zoneComposer->setZoneBeatModulation(zoneId, doc["beatModulation"].as<uint8_t>());
        updatedAudio = true;
    }
    if (doc.containsKey("tempoSpeedScale")) {
        ctx.zoneComposer->setZoneTempoSpeedScale(zoneId, doc["tempoSpeedScale"].as<uint8_t>());
        updatedAudio = true;
    }
    if (doc.containsKey("beatDecay")) {
        ctx.zoneComposer->setZoneBeatDecay(zoneId, doc["beatDecay"].as<uint8_t>());
        updatedAudio = true;
    }
    if (doc.containsKey("audioBand")) {
        ctx.zoneComposer->setZoneAudioBand(zoneId, doc["audioBand"].as<uint8_t>());
        updatedAudio = true;
    }

    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, updatedEffect, updatedBrightness, updatedSpeed, updatedPalette, updatedBlend, updatedAudio](JsonObject& data) {
        data["zoneId"] = zoneId;

        JsonArray updated = data["updated"].to<JsonArray>();
        if (updatedEffect) updated.add("effectId");
        if (updatedBrightness) updated.add("brightness");
        if (updatedSpeed) updated.add("speed");
        if (updatedPalette) updated.add("paletteId");
        if (updatedBlend) updated.add("blendMode");
        if (updatedAudio) updated.add("audioConfig");

        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));

        // Include audio config in response (Phase 2b.1)
        ZoneAudioConfig audio = ctx.zoneComposer->getZoneAudioConfig(zoneId);
        JsonObject audioObj = current["audio"].to<JsonObject>();
        audioObj["tempoSync"] = audio.tempoSync;
        audioObj["beatModulation"] = audio.beatModulation;
        audioObj["tempoSpeedScale"] = audio.tempoSpeedScale;
        audioObj["beatDecay"] = audio.beatDecay;
        audioObj["audioBand"] = audio.audioBand;
    });
    client->text(response);
}

static void handleZonesSetEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;
    uint8_t effectId = doc["effectId"] | 255;
    
    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }
    
    if (effectId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "effectId required", requestId));
        return;
    }

    // DEFENSIVE CHECK: Validate zoneId and effectId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    if (effectId >= ctx.renderer->getEffectCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneEffect(zoneId, effectId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.effectChanged", requestId, [&ctx, zoneId, effectId](JsonObject& data) {
        data["zoneId"] = zoneId;
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = effectId;
        current["effectName"] = ctx.renderer->getEffectName(effectId);
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
    });
    client->text(response);
}

static void handleGetZoneState(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

// Phase 2b.1: Get zone audio config
static void handleZonesGetAudio(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;

    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }

    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    ZoneAudioConfig audio = ctx.zoneComposer->getZoneAudioConfig(zoneId);

    String response = buildWsResponse("zones.audioConfig", requestId, [zoneId, &audio](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["tempoSync"] = audio.tempoSync;
        data["beatModulation"] = audio.beatModulation;
        data["tempoSpeedScale"] = audio.tempoSpeedScale;
        data["beatDecay"] = audio.beatDecay;
        data["audioBand"] = audio.audioBand;
    });
    client->text(response);
}

// Phase 2b.1: Set zone audio config
static void handleZonesSetAudio(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;

    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }

    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    // Get current audio config and apply updates
    ZoneAudioConfig audio = ctx.zoneComposer->getZoneAudioConfig(zoneId);
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
        audio.audioBand = (band > 3) ? 0 : band;
        hasUpdates = true;
    }

    if (!hasUpdates) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "No audio config fields provided", requestId));
        return;
    }

    ctx.zoneComposer->setZoneAudioConfig(zoneId, audio);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    String response = buildWsResponse("zones.audioChanged", requestId, [zoneId, &audio](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["tempoSync"] = audio.tempoSync;
        data["beatModulation"] = audio.beatModulation;
        data["tempoSpeedScale"] = audio.tempoSpeedScale;
        data["beatDecay"] = audio.beatDecay;
        data["audioBand"] = audio.audioBand;
    });
    client->text(response);
}

// Phase 2b.2: Get zone beat trigger config
static void handleZonesGetBeatTrigger(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;

    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }

    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    bool enabled = false;
    uint8_t interval = 4;
    uint8_t effectIds[8] = {0};
    uint8_t effectCount = 0;
    uint8_t currentIndex = 0;

    ctx.zoneComposer->getZoneBeatTriggerConfig(zoneId, enabled, interval, effectIds, effectCount, currentIndex);

    String response = buildWsResponse("zones.beatTrigger", requestId, [zoneId, enabled, interval, &effectIds, effectCount, currentIndex](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["enabled"] = enabled;
        data["interval"] = interval;
        data["currentIndex"] = currentIndex;
        JsonArray effects = data["effectList"].to<JsonArray>();
        for (uint8_t i = 0; i < effectCount; i++) {
            effects.add(effectIds[i]);
        }
    });
    client->text(response);
}

// Phase 2b.2: Set zone beat trigger config
static void handleZonesSetBeatTrigger(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";
    uint8_t zoneId = doc["zoneId"] | 255;

    if (zoneId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "zoneId required", requestId));
        return;
    }

    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    bool hasUpdates = false;

    // Handle enabled field
    if (doc.containsKey("enabled")) {
        ctx.zoneComposer->setZoneBeatTriggerEnabled(zoneId, doc["enabled"].as<bool>());
        hasUpdates = true;
    }

    // Handle interval field
    if (doc.containsKey("interval")) {
        uint8_t interval = doc["interval"].as<uint8_t>();
        // Clamp to valid values (1, 2, 4, 8, 16, 32)
        if (interval < 1) interval = 1;
        if (interval > 32) interval = 32;
        ctx.zoneComposer->setZoneBeatTriggerInterval(zoneId, interval);
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
            ctx.zoneComposer->setZoneBeatTriggerEffectList(zoneId, effectIds, count);
            hasUpdates = true;
        }
    }

    if (!hasUpdates) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "No beat trigger config fields provided", requestId));
        return;
    }

    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    // Retrieve updated config for response
    bool enabled = false;
    uint8_t interval = 4;
    uint8_t effectIds[8] = {0};
    uint8_t effectCount = 0;
    uint8_t currentIndex = 0;

    ctx.zoneComposer->getZoneBeatTriggerConfig(zoneId, enabled, interval, effectIds, effectCount, currentIndex);

    String response = buildWsResponse("zones.beatTriggerChanged", requestId, [zoneId, enabled, interval, &effectIds, effectCount, currentIndex](JsonObject& data) {
        data["zoneId"] = zoneId;
        data["enabled"] = enabled;
        data["interval"] = interval;
        data["currentIndex"] = currentIndex;
        JsonArray effects = data["effectList"].to<JsonArray>();
        for (uint8_t i = 0; i < effectCount; i++) {
            effects.add(effectIds[i]);
        }
    });
    client->text(response);
}

// Phase 2c.1: Reorder zones with CENTER ORIGIN constraint
static void handleZonesReorder(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";

    // Validate "order" array exists
    if (!doc.containsKey("order") || !doc["order"].is<JsonArray>()) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Required field 'order' must be an array", requestId));
        return;
    }

    JsonArray orderArray = doc["order"];
    uint8_t orderCount = orderArray.size();

    // Validate order array size matches current zone count
    if (orderCount != ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Order array size must match current zone count", requestId));
        return;
    }

    // Validate order array size is within bounds
    if (orderCount == 0 || orderCount > MAX_ZONES) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Order array size out of range (1-4)", requestId));
        return;
    }

    // Extract order values into array
    uint8_t newOrder[MAX_ZONES];
    uint8_t idx = 0;
    for (JsonVariant v : orderArray) {
        if (idx >= MAX_ZONES) break;
        if (!v.is<int>()) {
            client->text(buildWsError(ErrorCodes::INVALID_TYPE, "Order array must contain integers", requestId));
            return;
        }
        int val = v.as<int>();
        if (val < 0 || val >= static_cast<int>(orderCount)) {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Zone ID in order array out of range", requestId));
            return;
        }
        newOrder[idx++] = static_cast<uint8_t>(val);
    }

    // Attempt reorder - ZoneComposer will validate CENTER ORIGIN constraint
    bool success = ctx.zoneComposer->reorderZones(newOrder, orderCount);

    if (!success) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Reorder failed: CENTER ORIGIN constraint violated - Zone 0 must contain LEDs 79/80", requestId));
        return;
    }

    // Broadcast zone state update to all WebSocket clients
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    // Also broadcast a specific reorder event
    if (ctx.ws) {
        StaticJsonDocument<256> eventDoc;
        eventDoc["type"] = "zones.reordered";
        JsonArray orderResp = eventDoc["order"].to<JsonArray>();
        for (uint8_t i = 0; i < orderCount; i++) {
            orderResp.add(newOrder[i]);
        }
        eventDoc["zoneCount"] = orderCount;
        String eventOutput;
        serializeJson(eventDoc, eventOutput);
        ctx.ws->textAll(eventOutput);
    }

    // Send success response to requesting client
    String response = buildWsResponse("zones.reordered", requestId, [&newOrder, orderCount](JsonObject& data) {
        JsonArray orderResp = data["order"].to<JsonArray>();
        for (uint8_t i = 0; i < orderCount; i++) {
            orderResp.add(newOrder[i]);
        }
        data["zoneCount"] = orderCount;
    });
    client->text(response);
}

/**
 * @brief Reorder zones by distance from centre (innermost first)
 * 
 * Ensures zones are ordered centre-outward (Zone 0 = innermost) by sorting
 * by minimum distance from centre pair (LEDs 79/80). This allows users to
 * submit zones in any order while maintaining the centre-origin architecture.
 * 
 * @param segments Array of zone segments (modified in-place)
 * @param count Number of zones
 */
static void reorderZonesByCentreDistance(ZoneSegment* segments, uint8_t count) {
    if (!segments || count == 0 || count > MAX_ZONES) {
        return;
    }
    
    // Simple bubble sort: sort by distance from centre (ascending)
    // Distance calculation: min(79 - s1LeftEnd, s1RightStart - 80)
    // Smaller distance = closer to centre
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - 1 - i; j++) {
            // Calculate distances from centre
            uint8_t leftDist1 = 79 - segments[j].s1LeftEnd;
            uint8_t rightDist1 = segments[j].s1RightStart - 80;
            uint8_t dist1 = (leftDist1 < rightDist1) ? leftDist1 : rightDist1;
            
            uint8_t leftDist2 = 79 - segments[j + 1].s1LeftEnd;
            uint8_t rightDist2 = segments[j + 1].s1RightStart - 80;
            uint8_t dist2 = (leftDist2 < rightDist2) ? leftDist2 : rightDist2;
            
            // Swap if outer zone comes before inner zone
            if (dist1 > dist2) {
                ZoneSegment temp = segments[j];
                segments[j] = segments[j + 1];
                segments[j + 1] = temp;
            }
        }
    }
    
    // Reassign zoneId values sequentially (0, 1, 2, ...)
    for (uint8_t i = 0; i < count; i++) {
        segments[i].zoneId = i;
    }
}

static void handleZonesSetLayout(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";
    
    // Parse zones array
    JsonArray zonesArray = doc["zones"];
    if (!zonesArray || zonesArray.size() == 0 || zonesArray.size() > MAX_ZONES) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Invalid zones array", requestId));
        return;
    }

    // Convert JSON array to ZoneSegment array
    ZoneSegment segments[MAX_ZONES];
    uint8_t zoneCount = zonesArray.size();
    
    for (uint8_t i = 0; i < zoneCount; i++) {
        JsonObject zoneObj = zonesArray[i];
        if (!zoneObj.containsKey("zoneId") || !zoneObj.containsKey("s1LeftStart") ||
            !zoneObj.containsKey("s1LeftEnd") || !zoneObj.containsKey("s1RightStart") ||
            !zoneObj.containsKey("s1RightEnd")) {
            client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Zone segment missing required fields", requestId));
            return;
        }
        
        segments[i].zoneId = zoneObj["zoneId"];
        segments[i].s1LeftStart = zoneObj["s1LeftStart"];
        segments[i].s1LeftEnd = zoneObj["s1LeftEnd"];
        segments[i].s1RightStart = zoneObj["s1RightStart"];
        segments[i].s1RightEnd = zoneObj["s1RightEnd"];
        
        // Calculate totalLeds
        uint8_t leftSize = segments[i].s1LeftEnd - segments[i].s1LeftStart + 1;
        uint8_t rightSize = segments[i].s1RightEnd - segments[i].s1RightStart + 1;
        segments[i].totalLeds = leftSize + rightSize; // Per-strip count (strip 2 mirrors strip 1)
    }

    // Reorder zones by distance from centre (ensures Zone 0 = innermost)
    // This allows users to submit zones in any order while maintaining centre-origin architecture
    reorderZonesByCentreDistance(segments, zoneCount);

    // Set layout with validation
    if (!ctx.zoneComposer->setLayout(segments, zoneCount)) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Layout validation failed", requestId));
        return;
    }
    
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.layoutChanged", requestId, [zoneCount](JsonObject& data) {
        data["zoneCount"] = zoneCount;
    });
    client->text(response);
}

void registerWsZonesCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("zone.enable", handleZoneEnable);
    WsCommandRouter::registerCommand("zone.setEffect", handleZoneSetEffect);
    WsCommandRouter::registerCommand("zone.setBrightness", handleZoneSetBrightness);
    WsCommandRouter::registerCommand("zone.setSpeed", handleZoneSetSpeed);
    WsCommandRouter::registerCommand("zone.setPalette", handleZoneSetPalette);
    WsCommandRouter::registerCommand("zone.setBlend", handleZoneSetBlend);
    WsCommandRouter::registerCommand("zone.loadPreset", handleZoneLoadPreset);
    WsCommandRouter::registerCommand("zones.get", handleZonesGet);
    WsCommandRouter::registerCommand("zones.list", handleZonesList);
    WsCommandRouter::registerCommand("zones.update", handleZonesUpdate);
    WsCommandRouter::registerCommand("zones.setEffect", handleZonesSetEffect);
    WsCommandRouter::registerCommand("zones.setLayout", handleZonesSetLayout);
    WsCommandRouter::registerCommand("getZoneState", handleGetZoneState);

    // Phase 2b.1: Zone Audio Config commands
    WsCommandRouter::registerCommand("zones.getAudio", handleZonesGetAudio);
    WsCommandRouter::registerCommand("zones.setAudio", handleZonesSetAudio);

    // Phase 2b.2: Zone Beat Trigger commands
    WsCommandRouter::registerCommand("zones.getBeatTrigger", handleZonesGetBeatTrigger);
    WsCommandRouter::registerCommand("zones.setBeatTrigger", handleZonesSetBeatTrigger);

    // Phase 2c.1: Zone Reordering command
    WsCommandRouter::registerCommand("zones.reorder", handleZonesReorder);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

