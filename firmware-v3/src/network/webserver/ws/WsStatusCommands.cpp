/**
 * @file WsStatusCommands.cpp
 * @brief WebSocket status subscription command handlers implementation
 *
 * Handles status.subscribe / status.unsubscribe and acknowledges via
 * status.subscribed / status.unsubscribed. Mirrors the beat.subscribe
 * pattern in WsStreamCommands.cpp: idempotent add, table-bounded reject.
 *
 * The actual subscriber table lives on the WebServer instance (alongside
 * the LED/log subscription helpers) so it can be consulted from
 * doBroadcastStatus() and cleaned up from handleWsDisconnect().
 */

#include "WsStatusCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LW_LOG_TAG "WsStatus"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleStatusSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (!ctx.webServer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED,
                                  "Status subscription not available", requestId));
        return;
    }

    bool ok = ctx.webServer->setStatusSubscription(client, true);

    if (ok) {
        LW_LOGI("[WsStatus] Client %u subscribed to status (subscribers=%u)",
                clientId,
                static_cast<unsigned>(ctx.webServer->getStatusSubscriberCount()));
        String response = buildWsResponse("status.subscribed", requestId,
                                          [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            // Periodic broadcast cadence is locked in WebServerConfig (5 s).
            // Surface it for clients that wish to detect a stalled feed.
            data["periodMs"] = 5000;
        });
        client->text(response);
    } else {
        LW_LOGW("[WsStatus] Client %u status.subscribe rejected (table full)", clientId);
        // Mirror audio.subscribe rejection format: literal RESOURCE_EXHAUSTED
        // string under an explicit error envelope (no constexpr in ErrorCodes).
        JsonDocument response;
        response["type"] = "status.subscribed";
        if (requestId != nullptr && strlen(requestId) > 0) {
            response["requestId"] = requestId;
        }
        response["success"] = false;
        JsonObject error = response["error"].to<JsonObject>();
        error["code"] = "RESOURCE_EXHAUSTED";
        error["message"] = "Status subscriber table full";

        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleStatusUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (ctx.webServer) {
        ctx.webServer->setStatusSubscription(client, false);
    }

    LW_LOGI("[WsStatus] Client %u unsubscribed from status", clientId);

    String response = buildWsResponse("status.unsubscribed", requestId,
                                      [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
}

// ============================================================================
// Registration
// ============================================================================

void registerWsStatusCommands(const WebServerContext& ctx) {
    (void)ctx;
    WsCommandRouter::registerCommand("status.subscribe", handleStatusSubscribe);
    WsCommandRouter::registerCommand("status.unsubscribe", handleStatusUnsubscribe);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
