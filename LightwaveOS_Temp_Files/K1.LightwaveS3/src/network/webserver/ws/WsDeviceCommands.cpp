/**
 * @file WsDeviceCommands.cpp
 * @brief WebSocket device command handlers implementation
 */

#include "WsDeviceCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/RendererNode.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// Legacy compatibility: original on-device UI sends {"type":"getStatus"} and expects a "status" event.
// We keep this as a lightweight alias that triggers the existing status broadcast.
static void handleLegacyGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    (void)doc;
    if (ctx.broadcastStatus) {
        ctx.broadcastStatus();  // Broadcasts "status" to all clients (includes the requester).
        return;
    }
    const char* requestId = doc["requestId"] | "";
    client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Status broadcaster not available", requestId));
}

static void handleDeviceGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("device.status", requestId, [&ctx](JsonObject& data) {
        data["uptime"] = (millis() - ctx.startTime) / 1000;
        data["freeHeap"] = ESP.getFreeHeap();
        data["heapSize"] = ESP.getHeapSize();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        // Render stats
        if (ctx.renderer) {
            const auto& stats = ctx.renderer->getStats();
            data["fps"] = stats.currentFPS;
            data["cpuPercent"] = stats.cpuPercent;
            data["framesRendered"] = stats.framesRendered;
        }

        // Network info
        JsonObject network = data["network"].to<JsonObject>();
        network["connected"] = WiFi.status() == WL_CONNECTED;
        network["apMode"] = ctx.apMode;
        if (WiFi.status() == WL_CONNECTED) {
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
        }
        // Note: wsClients count not available in context - can be added via callback if needed
    });
    client->text(response);
}

static void handleDeviceGetInfo(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    String response = buildWsResponse("device.info", requestId, [&ctx](JsonObject& data) {
        data["chipModel"] = ESP.getChipModel();
        data["chipRevision"] = ESP.getChipRevision();
        data["chipCores"] = ESP.getChipCores();
        data["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        data["flashSize"] = ESP.getFlashChipSize();
        data["freeHeap"] = ESP.getFreeHeap();
        data["heapSize"] = ESP.getHeapSize();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketchSpace"] = ESP.getFreeSketchSpace();
        
        if (ctx.renderer) {
            data["effectCount"] = ctx.renderer->getEffectCount();
        }
    });
    client->text(response);
}

void registerWsDeviceCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("getStatus", handleLegacyGetStatus);
    WsCommandRouter::registerCommand("device.getStatus", handleDeviceGetStatus);
    WsCommandRouter::registerCommand("device.getInfo", handleDeviceGetInfo);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos

