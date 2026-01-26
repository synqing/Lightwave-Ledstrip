/**
 * @file FirmwareHandlers.h
 * @brief Firmware/OTA update REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class FirmwareHandlers {
public:
    static void handleVersion(AsyncWebServerRequest* request);
    static void handleV1Update(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken);
    static void handleLegacyUpdate(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken);
    static void handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
    static bool checkOTAToken(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
