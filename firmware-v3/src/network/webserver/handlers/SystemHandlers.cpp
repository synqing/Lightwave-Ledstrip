/**
 * @file SystemHandlers.cpp
 * @brief System handlers implementation
 */

#include "SystemHandlers.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For WebServerConfig
#include "../../../core/actors/RendererActor.h"
#include "../../../hal/led/LedDriverConfig.h"
#include "../../../config/network_config.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

using namespace lightwaveos::actors;
using namespace lightwaveos::network;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void SystemHandlers::handleHealth(AsyncWebServerRequest* request,
                                    RendererActor* renderer,
                                    AsyncWebSocket* ws) {
    JsonDocument doc;
    doc["success"] = true;
    doc["status"] = "healthy";
    
    JsonObject data = doc["data"].to<JsonObject>();
    data["uptime"] = millis() / 1000;
    data["freeHeap"] = ESP.getFreeHeap();
    data["totalHeap"] = ESP.getHeapSize();
    data["minFreeHeap"] = ESP.getMinFreeHeap();
    
    if (renderer) {
        data["rendererRunning"] = renderer->isRunning();
        data["queueUtilization"] = renderer->getQueueUtilization();
        data["queueLength"] = renderer->getQueueLength();
        data["queueCapacity"] = 32;  // RendererActor queue size
        
        const RenderStats& stats = renderer->getStats();
        data["fps"] = stats.currentFPS;
        data["cpuPercent"] = stats.cpuPercent;
    }
    
    if (ws) {
        data["wsClients"] = ws->count();
        data["wsMaxClients"] = WebServerConfig::MAX_WS_CLIENTS;
    }
    
    String output;
    serializeJson(doc, output);
    request->send(HttpStatus::OK, "application/json", output);
}

void SystemHandlers::handleApiDiscovery(AsyncWebServerRequest* request) {
    sendSuccessResponseLarge(request, [](JsonObject& data) {
        data["name"] = "LightwaveOS";
        data["apiVersion"] = API_VERSION;
        data["description"] = "ESP32-S3 LED Control System v2";

        // Hardware info
        JsonObject hw = data["hardware"].to<JsonObject>();
        hw["ledsTotal"] = LedConfig::TOTAL_LEDS;
        hw["strips"] = LedConfig::NUM_STRIPS;
        hw["centerPoint"] = LedConfig::CENTER_POINT;
        hw["maxZones"] = 4;

        // HATEOAS links
        JsonObject links = data["_links"].to<JsonObject>();
        links["self"] = "/api/v1/";
        links["device"] = "/api/v1/device/status";
        links["effects"] = "/api/v1/effects";
        links["parameters"] = "/api/v1/parameters";
        links["audioParameters"] = "/api/v1/audio/parameters";
        links["transitions"] = "/api/v1/transitions/types";
        links["batch"] = "/api/v1/batch";
        links["websocket"] = "ws://lightwaveos.local/ws";
    }, 1024);
}

void SystemHandlers::handleOpenApiSpec(AsyncWebServerRequest* request) {
    // Build OpenAPI 3.0 spec dynamically
    JsonDocument spec;
    JsonObject openapi = spec.to<JsonObject>();
    openapi["openapi"] = "3.0.0";
    
    JsonObject info = openapi["info"].to<JsonObject>();
    info["title"] = "LightwaveOS API";
    info["version"] = API_VERSION;
    info["description"] = "REST API for ESP32-S3 LED Control System";
    
    JsonObject servers = openapi["servers"].to<JsonArray>().add<JsonObject>();
    servers["url"] = "http://lightwaveos.local";
    servers["description"] = "Local device";
    
    JsonObject paths = openapi["paths"].to<JsonObject>();
    
    // Health endpoint
    JsonObject health = paths["/api/v1/health"].to<JsonObject>();
    JsonObject healthGet = health["get"].to<JsonObject>();
    healthGet["summary"] = "Health check";
    healthGet["operationId"] = "getHealth";
    JsonObject health200 = healthGet["responses"]["200"].to<JsonObject>();
    health200["description"] = "System is healthy";
    
    // Device status
    JsonObject device = paths["/api/v1/device/status"].to<JsonObject>();
    JsonObject deviceGet = device["get"].to<JsonObject>();
    deviceGet["summary"] = "Get device status";
    deviceGet["operationId"] = "getDeviceStatus";
    
    // Effects list
    JsonObject effects = paths["/api/v1/effects"].to<JsonObject>();
    JsonObject effectsGet = effects["get"].to<JsonObject>();
    effectsGet["summary"] = "List all effects";
    effectsGet["operationId"] = "listEffects";
    
    // Current effect
    JsonObject current = paths["/api/v1/effects/current"].to<JsonObject>();
    JsonObject currentGet = current["get"].to<JsonObject>();
    currentGet["summary"] = "Get current effect";
    currentGet["operationId"] = "getCurrentEffect";
    
    JsonObject currentPut = current["put"].to<JsonObject>();
    currentPut["summary"] = "Set current effect";
    currentPut["operationId"] = "setCurrentEffect";
    
    // Parameters
    JsonObject params = paths["/api/v1/parameters"].to<JsonObject>();
    JsonObject paramsGet = params["get"].to<JsonObject>();
    paramsGet["summary"] = "Get parameters";
    paramsGet["operationId"] = "getParameters";
    
    JsonObject paramsPost = params["post"].to<JsonObject>();
    paramsPost["summary"] = "Set parameters";
    paramsPost["operationId"] = "setParameters";
    
    // Transitions
    JsonObject transitions = paths["/api/v1/transitions/types"].to<JsonObject>();
    JsonObject transGet = transitions["get"].to<JsonObject>();
    transGet["summary"] = "List transition types";
    transGet["operationId"] = "listTransitionTypes";
    
    // Batch
    JsonObject batch = paths["/api/v1/batch"].to<JsonObject>();
    JsonObject batchPost = batch["post"].to<JsonObject>();
    batchPost["summary"] = "Execute batch operations";
    batchPost["operationId"] = "executeBatch";
    
    String output;
    serializeJson(spec, output);
    request->send(HttpStatus::OK, "application/json", output);
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

