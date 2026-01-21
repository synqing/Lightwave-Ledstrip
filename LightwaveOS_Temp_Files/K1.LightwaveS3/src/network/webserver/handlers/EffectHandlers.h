#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For CachedRendererState
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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

class EffectHandlers {
public:
    static void registerRoutes(HttpRouteRegistry& registry);

    static void handleList(AsyncWebServerRequest* request, lightwaveos::nodes::RendererNode* renderer);
    static void handleCurrent(AsyncWebServerRequest* request, lightwaveos::nodes::RendererNode* renderer);
    static void handleSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::nodes::NodeOrchestrator& orchestrator, const lightwaveos::network::WebServer::CachedRendererState& cachedState, std::function<void()> broadcastStatus);
    static void handleMetadata(AsyncWebServerRequest* request, lightwaveos::nodes::RendererNode* renderer);
    static void handleFamilies(AsyncWebServerRequest* request);
    static void handleParametersGet(AsyncWebServerRequest* request, lightwaveos::nodes::RendererNode* renderer);
    static void handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::nodes::RendererNode* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
