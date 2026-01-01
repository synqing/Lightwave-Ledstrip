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
        eventDoc["type"] = "zone.enabledChanged";
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
    
    String response = buildWsResponse("zone.paletteChanged", requestId, [&ctx, zoneId, paletteId](JsonObject& data) {
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
    
    String response = buildWsResponse("zone.blendChanged", requestId, [&ctx, zoneId, blendModeVal, blendMode](JsonObject& data) {
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
    
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, updatedEffect, updatedBrightness, updatedSpeed, updatedPalette, updatedBlend](JsonObject& data) {
        data["zoneId"] = zoneId;
        
        JsonArray updated = data["updated"].to<JsonArray>();
        if (updatedEffect) updated.add("effectId");
        if (updatedBrightness) updated.add("brightness");
        if (updatedSpeed) updated.add("speed");
        if (updatedPalette) updated.add("paletteId");
        if (updatedBlend) updated.add("blendMode");
        
        JsonObject current = data["current"].to<JsonObject>();
        current["effectId"] = ctx.zoneComposer->getZoneEffect(zoneId);
        current["brightness"] = ctx.zoneComposer->getZoneBrightness(zoneId);
        current["speed"] = ctx.zoneComposer->getZoneSpeed(zoneId);
        current["paletteId"] = ctx.zoneComposer->getZonePalette(zoneId);
        current["blendMode"] = static_cast<uint8_t>(ctx.zoneComposer->getZoneBlendMode(zoneId));
        current["blendModeName"] = getBlendModeName(ctx.zoneComposer->getZoneBlendMode(zoneId));
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
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "effectId required", requestId));
        return;
    }
    
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
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

