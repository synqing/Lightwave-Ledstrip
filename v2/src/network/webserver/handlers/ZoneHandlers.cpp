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

void ZoneHandlers::handleList(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning() || renderer == nullptr) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    sendSuccessResponseLarge(request, [composer, renderer](JsonObject& data) {
        data["enabled"] = composer->isEnabled();
        data["layout"] = static_cast<uint8_t>(composer->getLayout());
        data["layoutName"] = composer->getLayout() == lightwaveos::zones::ZoneLayout::QUAD ? "QUAD" : "TRIPLE";
        data["zoneCount"] = composer->getZoneCount();

        JsonArray zones = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < composer->getZoneCount(); i++) {
            JsonObject zone = zones.add<JsonObject>();
            zone["id"] = i;
            zone["enabled"] = composer->isZoneEnabled(i);
            zone["effectId"] = composer->getZoneEffect(i);
            zone["effectName"] = renderer->getEffectName(composer->getZoneEffect(i));
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

    // Schema validates zoneCount is 3 or 4
    uint8_t zoneCount = doc["zoneCount"];

    lightwaveos::zones::ZoneLayout layout = (zoneCount == 4) ? lightwaveos::zones::ZoneLayout::QUAD : lightwaveos::zones::ZoneLayout::TRIPLE;
    composer->setLayout(layout);

    sendSuccessResponse(request, [zoneCount, layout](JsonObject& respData) {
        respData["zoneCount"] = zoneCount;
        respData["layout"] = static_cast<uint8_t>(layout);
        respData["layoutName"] = (layout == lightwaveos::zones::ZoneLayout::QUAD) ? "QUAD" : "TRIPLE";
    });

    if (broadcastZoneState) broadcastZoneState();
}

void ZoneHandlers::handleGet(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning() || renderer == nullptr) {
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

    sendSuccessResponse(request, [composer, zoneId, renderer](JsonObject& data) {
        data["id"] = zoneId;
        data["enabled"] = composer->isZoneEnabled(zoneId);
        data["effectId"] = composer->getZoneEffect(zoneId);
        data["effectName"] = renderer->getEffectName(composer->getZoneEffect(zoneId));
        data["brightness"] = composer->getZoneBrightness(zoneId);
        data["speed"] = composer->getZoneSpeed(zoneId);
        data["paletteId"] = composer->getZonePalette(zoneId);
        data["blendMode"] = static_cast<uint8_t>(composer->getZoneBlendMode(zoneId));
        data["blendModeName"] = lightwaveos::zones::getBlendModeName(composer->getZoneBlendMode(zoneId));
    });
}

void ZoneHandlers::handleSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    if (!actors.isRunning() || renderer == nullptr) {
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
    if (effectId >= renderer->getEffectCount()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Effect ID out of range", "effectId");
        return;
    }

    composer->setZoneEffect(zoneId, effectId);

    sendSuccessResponse(request, [zoneId, effectId, renderer](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["effectId"] = effectId;
        respData["effectName"] = renderer->getEffectName(effectId);
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

    composer->setZonePalette(zoneId, paletteId);

    sendSuccessResponse(request, [zoneId, paletteId](JsonObject& respData) {
        respData["zoneId"] = zoneId;
        respData["paletteId"] = paletteId;
        respData["paletteName"] = MasterPaletteNames[paletteId];
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

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
