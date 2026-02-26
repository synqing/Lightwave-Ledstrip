/**
 * @file WsDeviceCommands.cpp
 * @brief WebSocket device command handlers implementation
 */

#include "WsDeviceCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "WsStreamCommands.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"
#include "../../../codec/WsDeviceCodec.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../config/features.h"
#if FEATURE_CONTROL_LEASE
#include "../../../core/system/ControlLeaseManager.h"
#endif
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

    // UNICAST: Build status JSON and send directly to the requesting client.
    // The previous implementation called ctx.broadcastStatus() which used
    // textAll(), overwhelming SoftAP TCP send buffers on connect and causing
    // immediate disconnection within 100-250ms. Unicast sends only to the
    // client that asked, avoiding broadcast amplification.
    if (!ctx.webServer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Status not available", requestId));
        return;
    }
    {
        const auto& cached = ctx.webServer->getCachedRendererState();
        JsonDocument response;
        response["type"] = "status";
        response["effectId"] = cached.currentEffect;
        const char* curEffName = cached.findEffectName(cached.currentEffect);
        if (curEffName) {
            response["effectName"] = curEffName;
        }
        response["brightness"] = cached.brightness;
        response["speed"] = cached.speed;
        response["paletteId"] = cached.paletteIndex;
        response["hue"] = cached.hue;
        response["intensity"] = cached.intensity;
        response["saturation"] = cached.saturation;
        response["complexity"] = cached.complexity;
        response["variation"] = cached.variation;
        response["fps"] = cached.stats.currentFPS;
        response["cpuPercent"] = cached.stats.cpuPercent;
        response["freeHeap"] = ESP.getFreeHeap();
        response["uptime"] = millis() / 1000;

#if FEATURE_CONTROL_LEASE
        using lightwaveos::core::system::ControlLeaseManager;
        const ControlLeaseManager::LeaseState leaseState = ControlLeaseManager::getState();
        const ControlLeaseManager::StatusCounters counters = ControlLeaseManager::getCounters();
        JsonObject lease = response["controlLease"].to<JsonObject>();
        lease["active"] = leaseState.active;
        lease["leaseId"] = leaseState.leaseId;
        lease["scope"] = leaseState.scope;
        lease["ownerClientName"] = leaseState.ownerClientName;
        lease["ownerInstanceId"] = leaseState.ownerInstanceId;
        lease["ownerWsClientId"] = leaseState.ownerWsClientId;
        lease["remainingMs"] = ControlLeaseManager::getRemainingMs();
        lease["ttlMs"] = leaseState.ttlMs;
        lease["heartbeatIntervalMs"] = leaseState.heartbeatIntervalMs;
        lease["takeoverAllowed"] = leaseState.takeoverAllowed;

        JsonObject controlCounters = response["controlCounters"].to<JsonObject>();
        controlCounters["blockedWsCommands"] = counters.blockedWsCommands;
        controlCounters["blockedRestRequests"] = counters.blockedRestRequests;
        controlCounters["blockedLocalEncoderInputs"] = counters.blockedLocalEncoderInputs;
        controlCounters["blockedLocalSerialInputs"] = counters.blockedLocalSerialInputs;
        controlCounters["lastLeaseEventMs"] = counters.lastLeaseEventMs;
#endif

        const RenderStreamStatusSnapshot stream = getRenderStreamStatusSnapshot();
        JsonObject renderStream = response["renderStream"].to<JsonObject>();
        renderStream["active"] = stream.active;
        renderStream["sessionId"] = stream.sessionId;
        renderStream["ownerWsClientId"] = stream.ownerWsClientId;
        renderStream["lastFrameSeq"] = stream.lastFrameSeq;
        renderStream["lastFrameRxMs"] = stream.lastFrameRxMs;
        renderStream["staleTimeoutMs"] = stream.staleTimeoutMs;
        renderStream["targetFps"] = stream.targetFps;
        renderStream["frameContractVersion"] = stream.frameContractVersion;
        renderStream["pixelFormat"] = "rgb888";
        renderStream["ledCount"] = stream.ledCount;
        renderStream["headerBytes"] = stream.headerBytes;
        renderStream["payloadBytes"] = stream.payloadBytes;
        renderStream["maxPayloadBytes"] = stream.maxPayloadBytes;
        renderStream["mailboxDepth"] = stream.mailboxDepth;

        JsonObject renderCounters = response["renderCounters"].to<JsonObject>();
        renderCounters["framesRx"] = stream.framesRx;
        renderCounters["framesRendered"] = stream.framesRendered;
        renderCounters["framesDroppedMailbox"] = stream.framesDroppedMailbox;
        renderCounters["framesInvalid"] = stream.framesInvalid;
        renderCounters["framesBlockedLease"] = stream.framesBlockedLease;
        renderCounters["staleTimeouts"] = stream.staleTimeouts;
        String output;
        serializeJson(response, output);
        client->text(output);
    }

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

#if FEATURE_CONTROL_LEASE
        using lightwaveos::core::system::ControlLeaseManager;
        const ControlLeaseManager::LeaseState leaseState = ControlLeaseManager::getState();
        const ControlLeaseManager::StatusCounters counters = ControlLeaseManager::getCounters();
        JsonObject lease = data["controlLease"].to<JsonObject>();
        lease["active"] = leaseState.active;
        lease["leaseId"] = leaseState.leaseId;
        lease["scope"] = leaseState.scope;
        lease["ownerClientName"] = leaseState.ownerClientName;
        lease["ownerInstanceId"] = leaseState.ownerInstanceId;
        lease["ownerWsClientId"] = leaseState.ownerWsClientId;
        lease["remainingMs"] = ControlLeaseManager::getRemainingMs();
        lease["ttlMs"] = leaseState.ttlMs;
        lease["heartbeatIntervalMs"] = leaseState.heartbeatIntervalMs;
        lease["takeoverAllowed"] = leaseState.takeoverAllowed;

        JsonObject controlCounters = data["controlCounters"].to<JsonObject>();
        controlCounters["blockedWsCommands"] = counters.blockedWsCommands;
        controlCounters["blockedRestRequests"] = counters.blockedRestRequests;
        controlCounters["blockedLocalEncoderInputs"] = counters.blockedLocalEncoderInputs;
        controlCounters["blockedLocalSerialInputs"] = counters.blockedLocalSerialInputs;
        controlCounters["lastLeaseEventMs"] = counters.lastLeaseEventMs;
#endif

        const RenderStreamStatusSnapshot stream = getRenderStreamStatusSnapshot();
        JsonObject renderStream = data["renderStream"].to<JsonObject>();
        renderStream["active"] = stream.active;
        renderStream["sessionId"] = stream.sessionId;
        renderStream["ownerWsClientId"] = stream.ownerWsClientId;
        renderStream["lastFrameSeq"] = stream.lastFrameSeq;
        renderStream["lastFrameRxMs"] = stream.lastFrameRxMs;
        renderStream["staleTimeoutMs"] = stream.staleTimeoutMs;
        renderStream["targetFps"] = stream.targetFps;
        renderStream["frameContractVersion"] = stream.frameContractVersion;
        renderStream["pixelFormat"] = "rgb888";
        renderStream["ledCount"] = stream.ledCount;
        renderStream["headerBytes"] = stream.headerBytes;
        renderStream["payloadBytes"] = stream.payloadBytes;
        renderStream["maxPayloadBytes"] = stream.maxPayloadBytes;
        renderStream["mailboxDepth"] = stream.mailboxDepth;

        JsonObject renderCounters = data["renderCounters"].to<JsonObject>();
        renderCounters["framesRx"] = stream.framesRx;
        renderCounters["framesRendered"] = stream.framesRendered;
        renderCounters["framesDroppedMailbox"] = stream.framesDroppedMailbox;
        renderCounters["framesInvalid"] = stream.framesInvalid;
        renderCounters["framesBlockedLease"] = stream.framesBlockedLease;
        renderCounters["staleTimeouts"] = stream.staleTimeouts;
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
