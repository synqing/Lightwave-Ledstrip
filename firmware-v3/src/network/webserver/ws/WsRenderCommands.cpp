/**
 * @file WsRenderCommands.cpp
 * @brief WebSocket render-output command handlers implementation
 */

#include "WsRenderCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleRenderDitheringGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    auto* renderer = ctx.actorSystem.getRenderer();
    const bool enabled = renderer ? renderer->isLedDitheringEnabled() : true;

    String response = buildWsResponse("render.dithering.get", requestId, [enabled](JsonObject& data) {
        data["enabled"] = enabled;
    });
    client->text(response);
}

static void handleRenderDitheringSet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    if (!doc.containsKey("enabled")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "enabled is required", requestId));
        return;
    }

    const bool enabled = doc["enabled"].as<bool>();
    if (!ctx.actorSystem.setLedDithering(enabled)) {
        client->text(buildWsError(ErrorCodes::RATE_LIMITED, "Renderer queue saturated", requestId));
        return;
    }
    if (ctx.broadcastStatus) ctx.broadcastStatus();

    String response = buildWsResponse("render.dithering.set", requestId, [enabled](JsonObject& data) {
        data["enabled"] = enabled;
    });
    client->text(response);
}

void registerWsRenderCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("render.dithering.get", handleRenderDitheringGet);
    WsCommandRouter::registerCommand("render.dithering.set", handleRenderDitheringSet);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
