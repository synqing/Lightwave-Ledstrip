/**
 * @file NetworkHandlers.h
 * @brief Network mode/status handlers (AP/STA) for LightwaveOS v2
 *
 * Purpose:
 * - Allow temporarily enabling STA mode for OTA updates while AP-only is the default.
 * - Expose network status (AP IP, STA IP, connection state).
 *
 * Security:
 * - Mode-changing endpoints require X-OTA-Token header (same as OTA update endpoints).
 */

#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class NetworkHandlers {
public:
    static void handleStatus(AsyncWebServerRequest* request);

    // POST bodies are handled in V1ApiRoutes via raw body callback (data/len).
    static void handleEnableSTA(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleEnableAPOnly(AsyncWebServerRequest* request);

private:
    static bool checkOTAToken(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

