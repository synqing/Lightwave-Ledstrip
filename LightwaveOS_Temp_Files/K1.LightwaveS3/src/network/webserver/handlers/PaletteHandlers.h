/**
 * @file PaletteHandlers.h
 * @brief Palette-related HTTP handlers
 */

#pragma once

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

class PaletteHandlers {
public:
    static void handleList(AsyncWebServerRequest* request,
                           lightwaveos::nodes::RendererNode* renderer);
    
    static void handleCurrent(AsyncWebServerRequest* request,
                               lightwaveos::nodes::RendererNode* renderer);
    
    static void handleSet(AsyncWebServerRequest* request,
                          uint8_t* data, size_t len,
                          lightwaveos::nodes::NodeOrchestrator& orchestrator,
                          std::function<void()> broadcastStatus);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

