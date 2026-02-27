// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsBatchCommands.cpp
 * @brief WebSocket batch command handlers implementation
 */

#include "WsBatchCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleBatch(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    
    if (!doc.containsKey("operations") || !doc["operations"].is<JsonArray>()) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "operations array required", requestId));
        return;
    }
    
    JsonArray ops = doc["operations"];
    if (ops.size() > WebServerConfig::MAX_BATCH_OPERATIONS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Max 10 operations per batch", requestId));
        return;
    }
    
    uint8_t processed = 0;
    uint8_t failed = 0;
    
    for (JsonVariant op : ops) {
        String action = op["action"] | "";
        if (ctx.executeBatchAction && ctx.executeBatchAction(action, op)) {
            processed++;
        } else {
            failed++;
        }
    }
    
    String response = buildWsResponse("batch.result", requestId, [processed, failed](JsonObject& data) {
        data["processed"] = processed;
        data["failed"] = failed;
    });
    client->text(response);
    
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}

void registerWsBatchCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("batch", handleBatch);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

