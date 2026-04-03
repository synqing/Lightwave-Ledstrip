/**
 * @file WsStmCommands.cpp
 * @brief WebSocket STM stream subscription command handlers implementation
 */

#include "WsStmCommands.h"

#if FEATURE_AUDIO_SYNC

#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../StmStreamBroadcaster.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LW_LOG_TAG "WsStmCommands"
#include "../../../utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleStmSubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (!ctx.setStmStreamSubscription) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "STM streaming not available", requestId));
        return;
    }

    bool ok = ctx.setStmStreamSubscription(client, true);

    if (ok) {
        String response = buildWsResponse("stm.subscribed", requestId, [clientId](JsonObject& data) {
            data["clientId"] = clientId;
            data["frameSize"] = webserver::StmStreamConfig::FRAME_SIZE;
            data["spectralBins"] = webserver::StmStreamConfig::SPECTRAL_BINS;
            data["temporalBands"] = webserver::StmStreamConfig::TEMPORAL_BANDS;
            data["targetFps"] = webserver::StmStreamConfig::TARGET_FPS;
            data["magic"] = webserver::StmStreamConfig::MAGIC_BYTE;
        });
        client->text(response);
        LW_LOGI("STM stream: client %u subscribed", clientId);
    } else {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "STM subscriber table full or low memory", requestId));
    }
}

static void handleStmUnsubscribe(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    uint32_t clientId = client->id();
    const char* requestId = doc["requestId"] | "";

    if (ctx.setStmStreamSubscription) {
        ctx.setStmStreamSubscription(client, false);
    }

    String response = buildWsResponse("stm.unsubscribed", requestId, [clientId](JsonObject& data) {
        data["clientId"] = clientId;
    });
    client->text(response);
    LW_LOGI("STM stream: client %u unsubscribed", clientId);
}

void registerWsStmCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("stm.subscribe", handleStmSubscribe);
    WsCommandRouter::registerCommand("stm.unsubscribe", handleStmUnsubscribe);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
