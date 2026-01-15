/**
 * @file ParameterHandlers.h
 * @brief Parameter-related HTTP handlers
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
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

class ParameterHandlers {
public:
    static void handleGet(AsyncWebServerRequest* request,
                           const lightwaveos::network::WebServer::CachedRendererState& cachedState);
    
    static void handleSet(AsyncWebServerRequest* request,
                           uint8_t* data, size_t len,
                           lightwaveos::nodes::NodeOrchestrator& orchestrator,
                           std::function<void()> broadcastStatus);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

