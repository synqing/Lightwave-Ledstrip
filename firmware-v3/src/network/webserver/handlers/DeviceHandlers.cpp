/**
 * @file DeviceHandlers.cpp
 * @brief Device handlers implementation
 */

#include "DeviceHandlers.h"
#include "../../../config/version.h"
#include "core/actors/ActorSystem.h"
#include "core/actors/RendererActor.h"
#include <WiFi.h>

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

        // Network info
        JsonObject network = data["network"].to<JsonObject>();
        network["connected"] = WiFi.status() == WL_CONNECTED;
        network["apMode"] = apMode;
        if (WiFi.status() == WL_CONNECTED) {
            network["ip"] = WiFi.localIP().toString();
            network["rssi"] = WiFi.RSSI();
        }

        data["wsClients"] = wsClientCount;
    });
}

void DeviceHandlers::handleInfo(AsyncWebServerRequest* request, 
                                  ActorSystem& actors, 
                                  RendererActor* renderer) {
    (void)actors;
    (void)renderer;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["firmware"] = FIRMWARE_VERSION_STRING;
        data["firmwareVersionNumber"] = FIRMWARE_VERSION_NUMBER;
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
