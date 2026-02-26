/**
 * @file DeviceHandlers.cpp
 * @brief Device handlers implementation
 */

#include "DeviceHandlers.h"
#include "../../../config/features.h"
#include "../../../config/version.h"
#include "../ws/WsStreamCommands.h"
#include "core/actors/ActorSystem.h"
#include "core/actors/RendererActor.h"
#include <WiFi.h>
#if FEATURE_CONTROL_LEASE
#include "../../../core/system/ControlLeaseManager.h"
#endif

using namespace lightwaveos::actors;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void DeviceHandlers::registerRoutes(HttpRouteRegistry& registry,
                                     std::function<bool(AsyncWebServerRequest*)> checkRateLimit) {
    // Intentionally empty for now – WebServer still owns route wiring.
    // When routes are migrated, this function will mirror the behaviour of
    // WebServer::handleDeviceStatus / handleDeviceInfo.
    (void)registry;
    (void)checkRateLimit;
}

void DeviceHandlers::handleStatus(AsyncWebServerRequest* request, 
                                    ActorSystem& actors, 
                                    RendererActor* renderer, 
                                    uint32_t startTime, bool apMode, size_t wsClientCount) {
    if (!actors.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "System not ready");
        return;
    }

    sendSuccessResponse(request, [startTime, apMode, wsClientCount, renderer](JsonObject& data) {
        data["uptime"] = (millis() - startTime) / 1000;
        data["freeHeap"] = ESP.getFreeHeap();
        data["heapSize"] = ESP.getHeapSize();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        // Render stats
        const RenderStats& stats = renderer->getStats();
        data["fps"] = stats.currentFPS;
        data["cpuPercent"] = stats.cpuPercent;
        data["framesRendered"] = stats.framesRendered;

        // Audio sync mode (local ES backend vs external Trinity sync)
        data["audioSyncMode"] = renderer->getAudioSyncMode();

        // Network info
        JsonObject network = data["network"].to<JsonObject>();
        network["connected"] = WiFi.status() == WL_CONNECTED;
        network["apMode"] = apMode;
        if (WiFi.status() == WL_CONNECTED) {
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
        }

        data["wsClients"] = wsClientCount;

#if FEATURE_CONTROL_LEASE
        using lightwaveos::core::system::ControlLeaseManager;
        const ControlLeaseManager::LeaseState leaseState = ControlLeaseManager::getState();
        const ControlLeaseManager::StatusCounters counters = ControlLeaseManager::getCounters();

        JsonObject controlLease = data["controlLease"].to<JsonObject>();
        controlLease["active"] = leaseState.active;
        controlLease["leaseId"] = leaseState.leaseId;
        controlLease["scope"] = leaseState.scope;
        controlLease["ownerClientName"] = leaseState.ownerClientName;
        controlLease["ownerInstanceId"] = leaseState.ownerInstanceId;
        controlLease["ownerWsClientId"] = leaseState.ownerWsClientId;
        controlLease["remainingMs"] = ControlLeaseManager::getRemainingMs();
        controlLease["ttlMs"] = leaseState.ttlMs;
        controlLease["heartbeatIntervalMs"] = leaseState.heartbeatIntervalMs;
        controlLease["takeoverAllowed"] = leaseState.takeoverAllowed;

        JsonObject controlCounters = data["controlCounters"].to<JsonObject>();
        controlCounters["blockedWsCommands"] = counters.blockedWsCommands;
        controlCounters["blockedRestRequests"] = counters.blockedRestRequests;
        controlCounters["blockedLocalEncoderInputs"] = counters.blockedLocalEncoderInputs;
        controlCounters["blockedLocalSerialInputs"] = counters.blockedLocalSerialInputs;
        controlCounters["lastLeaseEventMs"] = counters.lastLeaseEventMs;
#endif

        const ws::RenderStreamStatusSnapshot stream = ws::getRenderStreamStatusSnapshot();
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
}

void DeviceHandlers::handleInfo(AsyncWebServerRequest* request, 
                                  ActorSystem& actors, 
                                  RendererActor* renderer) {
    (void)actors;
    (void)renderer;
    sendSuccessResponse(request, [](JsonObject& data) {
        // Discovery signature fields (used by Tab5.encoder fallback network scan).
        // Tab5 searches for "LightwaveOS"/"lightwaveos" in the /api/v1/device/info
        // response when mDNS resolution fails, so keep these stable.
        data["name"] = "LightwaveOS";
        data["hostname"] = "lightwaveos";

        data["firmware"] = FIRMWARE_VERSION_STRING;
        data["firmwareVersionNumber"] = FIRMWARE_VERSION_NUMBER;
        // Keep a generic board family for clients that do string matching.
        data["boardFamily"] = "ESP32-S3";
        data["board"] = "ESP32-S3-DevKitC-1";
        data["sdk"] = ESP.getSdkVersion();
        data["flashSize"] = ESP.getFlashChipSize();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketch"] = ESP.getFreeSketchSpace();
        data["architecture"] = "Actor System v2";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
