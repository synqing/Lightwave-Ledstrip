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
    // Network status
    static void handleStatus(AsyncWebServerRequest* request);

    // Network management (AP-first architecture)
    static void handleListNetworks(AsyncWebServerRequest* request);
    static void handleAddNetwork(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleDeleteNetwork(AsyncWebServerRequest* request, const String& ssid);
    static void handleConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleDisconnect(AsyncWebServerRequest* request);
    
    // Network scanning
    static void handleScanNetworks(AsyncWebServerRequest* request);
    static void handleScanStatus(AsyncWebServerRequest* request);

    // Legacy OTA endpoints (POST bodies are handled in V1ApiRoutes via raw body callback)
    static void handleEnableSTA(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    static void handleEnableAPOnly(AsyncWebServerRequest* request);

private:
    static bool checkOTAToken(AsyncWebServerRequest* request);
    
    // Scan job tracking (static storage for scan results)
    // Scan job tracking (static storage for scan results)
    // NOTE: These are set in handleScanNetworks() and checked in handleScanStatus()
    static uint32_t s_scanJobId;
    static uint32_t s_scanStartTime;
    static bool s_scanInProgress;
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

