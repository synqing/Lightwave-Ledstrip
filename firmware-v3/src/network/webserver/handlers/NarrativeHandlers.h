/**
 * @file NarrativeHandlers.h
 * @brief Narrative-related HTTP handlers
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class NarrativeHandlers {
public:
    static void handleStatus(AsyncWebServerRequest* request);
    
    static void handleConfigGet(AsyncWebServerRequest* request);
    
    static void handleConfigSet(AsyncWebServerRequest* request,
                                  uint8_t* data, size_t len);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

