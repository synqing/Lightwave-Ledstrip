/**
 * @file SystemHandlers.h
 * @brief System-related HTTP handlers (health, discovery, OpenAPI)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace nodes {
class RendererNode;
}
namespace network {
class WebServer;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class SystemHandlers {
public:
    static void handleHealth(AsyncWebServerRequest* request,
                              lightwaveos::nodes::RendererNode* renderer,
                              AsyncWebSocket* ws);
    
    static void handleApiDiscovery(AsyncWebServerRequest* request);
    
    static void handleOpenApiSpec(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

