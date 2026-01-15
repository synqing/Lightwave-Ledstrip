#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../core/persistence/ZoneConfigManager.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For CachedRendererState

namespace lightwaveos {
namespace nodes {
class NodeOrchestrator;
class RendererNode;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ZoneHandlers {
public:
    static void handleList(AsyncWebServerRequest* request, lightwaveos::nodes::NodeOrchestrator& orchestrator, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer);
    static void handleLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleGet(AsyncWebServerRequest* request, lightwaveos::nodes::NodeOrchestrator& orchestrator, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer);
    static void handleSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::nodes::NodeOrchestrator& orchestrator, const lightwaveos::network::WebServer::CachedRendererState& cachedState, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Persistence API handlers (Phase 1.5)
    static void handleConfigGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager);
    static void handleConfigSave(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager);
    static void handleConfigLoad(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer, lightwaveos::persistence::ZoneConfigManager* configManager, std::function<void()> broadcastZoneState);

    // Timing Metrics API handler (Phase 2a.1)
    static void handleTimingGet(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer);
    static void handleTimingReset(AsyncWebServerRequest* request, lightwaveos::zones::ZoneComposer* composer);

    // Zone Audio Config API handlers (Phase 2b.1)
    static void handleAudioConfigGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer);
    static void handleAudioConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Zone Beat Trigger API handlers (Phase 2b.2)
    static void handleBeatTriggerGet(AsyncWebServerRequest* request, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer);
    static void handleBeatTriggerSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, uint8_t zoneId, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

    // Zone Reordering API handler (Phase 2c.1)
    static void handleReorder(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

private:
    static uint8_t extractZoneIdFromPath(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
