/**
 * @file ColorCorrectionHandlers.h
 * @brief Color correction REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ColorCorrectionHandlers {
public:
    static void handleGetConfig(AsyncWebServerRequest* request);
    static void handleSetMode(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleSave(AsyncWebServerRequest* request);
    static void handleGetPresets(AsyncWebServerRequest* request);
    static void handleSetPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
