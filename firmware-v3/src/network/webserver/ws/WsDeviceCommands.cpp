// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WsDeviceCommands.cpp
 * @brief WebSocket device command handlers implementation
 */

#include "WsDeviceCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../codec/WsDeviceCodec.h"
#include "../../../core/actors/RendererActor.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

static void handleLegacyGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::DeviceDecodeResult decodeResult = codec::WsDeviceCodec::decode(root);
    
    const char* requestId = (decodeResult.success && decodeResult.request.requestId) 
        ? decodeResult.request.requestId : "";
    
    // Extract optional ping metadata from envelope
    int64_t pingId = 0;
    bool hasPingId = false;
    if (root.containsKey("_id")) {
        pingId = root["_id"].as<int64_t>();
        if (pingId > 0) {
            hasPingId = true;
        }
    }

    int64_t clientTime = 0;
    bool hasClientTime = false;
    if (root.containsKey("_time")) {
        clientTime = root["_time"].as<int64_t>();
        if (clientTime > 0) {
            hasClientTime = true;
        }
    }

    if (!ctx.broadcastStatus) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Status broadcaster not available", requestId));
        return;
    }

    ctx.broadcastStatus();

    // Send a dedicated pong response back to the requesting client so PRISM can
    // measure RTT and clock drift. For now we operate in "echo" mode: if the
    // client supplied _time we echo it back, which yields near-zero drift while
    // still giving an accurate RTT measurement.
    if (hasPingId) {
        JsonDocument response;
        response["_type"] = "pong";
        response["_id"] = pingId;
        if (hasClientTime) {
            response["_time"] = clientTime;
        } else {
            response["_time"] = static_cast<int64_t>(millis());
        }
        String output;
        serializeJson(response, output);
        client->text(output);
    }
}

static void handleDeviceGetStatus(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    // Decode using codec (extracts requestId only)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::DeviceDecodeResult decodeResult = codec::WsDeviceCodec::decode(root);
    const char* requestId = (decodeResult.request.requestId) ? decodeResult.request.requestId : "";
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
    // Decode using codec (extracts requestId only)
    JsonObjectConst root = doc.as<JsonObjectConst>();
    codec::DeviceDecodeResult decodeResult = codec::WsDeviceCodec::decode(root);
    const char* requestId = (decodeResult.request.requestId) ? decodeResult.request.requestId : "";
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
