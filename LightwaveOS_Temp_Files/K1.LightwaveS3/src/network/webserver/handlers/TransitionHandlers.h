/**
 * @file TransitionHandlers.h
 * @brief Transition-related HTTP handlers
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

class TransitionHandlers {
public:
    static void handleTypes(AsyncWebServerRequest* request);
    
    static void handleTrigger(AsyncWebServerRequest* request,
                               uint8_t* data, size_t len,
                               lightwaveos::nodes::NodeOrchestrator& orchestrator,
                               const lightwaveos::network::WebServer::CachedRendererState& cachedState,
                               std::function<void()> broadcastStatus);
    
    static void handleConfigGet(AsyncWebServerRequest* request,
                                 lightwaveos::nodes::RendererNode* renderer);
    
    static void handleConfigSet(AsyncWebServerRequest* request,
                                 uint8_t* data, size_t len);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
