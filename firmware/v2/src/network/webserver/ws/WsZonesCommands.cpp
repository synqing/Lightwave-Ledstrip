/**
 * @file WsZonesCommands.cpp
 * @brief WebSocket zones command handlers implementation
 */

#include "WsZonesCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../codec/WsZonesCodec.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../effects/zones/BlendMode.h"
#include "../../../config/effect_ids.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::zones;

static void handleZoneEnable(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone composer not available", requestId));
        return;
    }
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneEnableDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneEnable(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::ZoneEnableRequest& req = decodeResult.request;
    ctx.zoneComposer->setEnabled(req.enable);
    
    // Send immediate zone.enabledChanged event
    if (ctx.ws) {
        String eventOutput = buildWsResponse("zone.enabledChanged", "", [&req](JsonObject& data) {
            codec::WsZonesCodec::encodeZoneEnabledChanged(req.enable, data);
        });
        ctx.ws->textAll(eventOutput);
    }
    
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

/**
 * @brief Per-zone enable/disable command
 *
 * Request:  {"type": "zone.enableZone", "zoneId": 0, "enabled": true}
 * Response: {"type": "zone.zoneEnabledChanged", "success": true, "data": {"zoneId": 0, "enabled": true}}
 * Broadcast: Full zone state via broadcastZoneState()
 */
static void handleZoneEnableZone(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone composer not available", requestId));
        return;
    }

    const char* requestId = doc["requestId"] | "";

    // Validate zoneId
    if (!doc["zoneId"].is<uint8_t>()) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'zoneId' field", requestId));
        return;
    }
    uint8_t zoneId = doc["zoneId"].as<uint8_t>();

    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    // Validate enabled
    if (!doc["enabled"].is<bool>()) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "Missing 'enabled' field", requestId));
        return;
    }
    bool enabled = doc["enabled"].as<bool>();

    ctx.zoneComposer->setZoneEnabled(zoneId, enabled);

    // Broadcast to all clients
    if (ctx.ws) {
        String eventOutput = buildWsResponse("zone.zoneEnabledChanged", requestId, [zoneId, enabled](JsonObject& data) {
            data["zoneId"] = zoneId;
            data["enabled"] = enabled;
        });
        ctx.ws->textAll(eventOutput);
    }

    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

static void handleZoneSetEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetEffectDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetEffect(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::ZoneSetEffectRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    EffectId effectId = req.effectId;
    const char* requestId = req.requestId ? req.requestId : "";

    // DEFENSIVE CHECK: Validate zoneId and effectId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    if (!ctx.renderer->isEffectRegistered(effectId)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    ctx.zoneComposer->setZoneEffect(zoneId, effectId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    String response = buildWsResponse("zones.effectChanged", requestId, [&ctx, zoneId, effectId](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesEffectChanged(zoneId, effectId, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZoneSetBrightness(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetBrightnessDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetBrightness(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZoneSetBrightnessRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    uint8_t brightness = req.brightness;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneBrightness(zoneId, brightness);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    const char* updatedFields[] = {"brightness"};
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, updatedFields](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesChanged(zoneId, updatedFields, 1, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZoneSetSpeed(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetSpeedDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetSpeed(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZoneSetSpeedRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    uint8_t speed = req.speed;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    ctx.zoneComposer->setZoneSpeed(zoneId, speed);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    const char* updatedFields[] = {"speed"};
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, updatedFields](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesChanged(zoneId, updatedFields, 1, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZoneSetPalette(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetPaletteDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetPalette(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZoneSetPaletteRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    uint8_t paletteId = req.paletteId;
    const char* requestId = req.requestId ? req.requestId : "";
    
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
        codec::WsZonesCodec::encodeZonePaletteChanged(zoneId, paletteId, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZoneSetBlend(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetBlendDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetBlend(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZoneSetBlendRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    uint8_t blendModeVal = req.blendMode;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // DEFENSIVE CHECK: Validate zoneId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    
    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }
    
    lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(blendModeVal);
    ctx.zoneComposer->setZoneBlendMode(zoneId, blendMode);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    String response = buildWsResponse("zone.blendChanged", requestId, [&ctx, zoneId, blendModeVal](JsonObject& data) {
        codec::WsZonesCodec::encodeZoneBlendChanged(zoneId, blendModeVal, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZoneLoadPreset(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (!ctx.zoneComposer) {
        return; // Silently ignore if zones not available
    }
    
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneLoadPresetDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneLoadPreset(root);
    
    if (!decodeResult.success) {
        // Silently ignore decode errors for loadPreset (matches original behavior)
        return;
    }
    
    const codec::ZoneLoadPresetRequest& req = decodeResult.request;
    ctx.zoneComposer->loadPreset(req.presetId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

static void handleZonesGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZonesGetDecodeResult decodeResult = codec::WsZonesCodec::decodeZonesGet(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::ZonesGetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    String response = buildWsResponse("zones", requestId, [&ctx](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesGet(*ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZonesList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZonesGetDecodeResult decodeResult = codec::WsZonesCodec::decodeZonesGet(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::ZonesGetRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    String response = buildWsResponse("zones.list", requestId, [&ctx](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesList(*ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZonesUpdate(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZonesUpdateDecodeResult decodeResult = codec::WsZonesCodec::decodeZonesUpdate(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZonesUpdateRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    const char* requestId = req.requestId ? req.requestId : "";
    
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
    
    if (req.hasEffectId) {
        EffectId effectId = req.effectId;
        // DEFENSIVE CHECK: Validate effectId before array access
        effectId = lightwaveos::network::validateEffectIdInRequest(effectId);
        if (ctx.renderer->isEffectRegistered(effectId)) {
            ctx.zoneComposer->setZoneEffect(zoneId, effectId);
            updatedEffect = true;
        }
    }
    
    if (req.hasBrightness) {
        ctx.zoneComposer->setZoneBrightness(zoneId, req.brightness);
        updatedBrightness = true;
    }
    
    if (req.hasSpeed) {
        ctx.zoneComposer->setZoneSpeed(zoneId, req.speed);
        updatedSpeed = true;
    }
    
    if (req.hasPaletteId) {
        uint8_t paletteId = req.paletteId;
        // DEFENSIVE CHECK: Validate paletteId before array access
        paletteId = lightwaveos::network::validatePaletteIdInRequest(paletteId);
        ctx.zoneComposer->setZonePalette(zoneId, paletteId);
        updatedPalette = true;
    }
    
    if (req.hasBlendMode) {
        lightwaveos::zones::BlendMode blendMode = static_cast<lightwaveos::zones::BlendMode>(req.blendMode);
        ctx.zoneComposer->setZoneBlendMode(zoneId, blendMode);
        updatedBlend = true;
    }
    
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
    
    const char* updatedFields[5];
    uint8_t updatedCount = 0;
    if (updatedEffect) updatedFields[updatedCount++] = "effectId";
    if (updatedBrightness) updatedFields[updatedCount++] = "brightness";
    if (updatedSpeed) updatedFields[updatedCount++] = "speed";
    if (updatedPalette) updatedFields[updatedCount++] = "paletteId";
    if (updatedBlend) updatedFields[updatedCount++] = "blendMode";
    
    String response = buildWsResponse("zones.changed", requestId, [&ctx, zoneId, updatedFields, updatedCount](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesChanged(zoneId, updatedFields, updatedCount, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleZonesSetEffect(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZoneSetEffectDecodeResult decodeResult = codec::WsZonesCodec::decodeZoneSetEffect(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    const codec::ZoneSetEffectRequest& req = decodeResult.request;
    uint8_t zoneId = req.zoneId;
    EffectId effectId = req.effectId;
    const char* requestId = req.requestId ? req.requestId : "";

    // DEFENSIVE CHECK: Validate zoneId and effectId before array access
    zoneId = lightwaveos::network::validateZoneIdInRequest(zoneId);
    effectId = lightwaveos::network::validateEffectIdInRequest(effectId);

    if (zoneId >= ctx.zoneComposer->getZoneCount()) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid zoneId", requestId));
        return;
    }

    if (!ctx.renderer->isEffectRegistered(effectId)) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Invalid effectId", requestId));
        return;
    }

    ctx.zoneComposer->setZoneEffect(zoneId, effectId);
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    String response = buildWsResponse("zones.effectChanged", requestId, [&ctx, zoneId, effectId](JsonObject& data) {
        codec::WsZonesCodec::encodeZonesEffectChanged(zoneId, effectId, *ctx.zoneComposer, ctx.renderer, data);
    });
    client->text(response);
}

static void handleGetZoneState(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();
}

static void handleZonesSetLayout(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::ZonesSetLayoutDecodeResult decodeResult = codec::WsZonesCodec::decodeZonesSetLayout(root);
    
    if (!decodeResult.success) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, decodeResult.errorMsg, requestId));
        return;
    }
    
    if (!ctx.zoneComposer) {
        const char* requestId = decodeResult.request.requestId ? decodeResult.request.requestId : "";
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }
    
    const codec::ZonesSetLayoutRequest& req = decodeResult.request;
    const char* requestId = req.requestId ? req.requestId : "";
    
    // Convert codec segments to ZoneSegment array
    ZoneSegment segments[lightwaveos::zones::MAX_ZONES];
    uint8_t zoneCount = req.zoneCount;
    
    for (uint8_t i = 0; i < zoneCount; i++) {
        segments[i].zoneId = req.zones[i].zoneId;
        segments[i].s1LeftStart = req.zones[i].s1LeftStart;
        segments[i].s1LeftEnd = req.zones[i].s1LeftEnd;
        segments[i].s1RightStart = req.zones[i].s1RightStart;
        segments[i].s1RightEnd = req.zones[i].s1RightEnd;
        
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
        codec::WsZonesCodec::encodeZonesLayoutChanged(zoneCount, data);
    });
    client->text(response);
}

void registerWsZonesCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("zone.enable", handleZoneEnable);
    WsCommandRouter::registerCommand("zone.enableZone", handleZoneEnableZone);
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

