/**
 * @file NetworkHandlers.h
 * @brief Network management HTTP handlers for LightwaveOS v2
 *
 * Provides REST API endpoints for WiFi network management:
 * - GET  /api/v1/network/status     - Current WiFi state
 * - GET  /api/v1/network/scan       - Scan for networks
 * - POST /api/v1/network/connect    - Connect to network
 * - POST /api/v1/network/disconnect - Disconnect from WiFi
 * - GET  /api/v1/network/saved      - List saved networks (no passwords)
 * - POST /api/v1/network/saved      - Add saved network
 * - DELETE /api/v1/network/saved/*  - Remove saved network
 *
 * Security:
 * - Passwords are never returned via API
 * - Rate limiting applied at route registration level
 * - API key authentication if FEATURE_API_AUTH enabled
 */

#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Network management HTTP handlers
 */
class NetworkHandlers {
public:
    /**
     * @brief Register network routes
     * @param registry Route registry
     *
     * Note: Routes are registered in V1ApiRoutes.cpp with rate limiting wrappers.
     */
    static void registerRoutes(HttpRouteRegistry& registry);

    // ========================================================================
    // Status & Scanning
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/network/status
     *
     * Returns current WiFi state, SSID, IP, RSSI.
     *
     * Response:
     * {
     *   "connected": true,
     *   "ssid": "MyNetwork",
     *   "ip": "192.168.1.100",
     *   "rssi": -45,
     *   "channel": 6,
     *   "state": "CONNECTED",
     *   "apMode": false
     * }
     */
    static void handleStatus(AsyncWebServerRequest* request);

    /**
     * @brief Handle GET /api/v1/network/scan
     *
     * Performs synchronous WiFi scan and returns results.
     * May take 2-5 seconds.
     *
     * Response:
     * {
     *   "networks": [
     *     {
     *       "ssid": "Network1",
     *       "rssi": -45,
     *       "channel": 6,
     *       "encryption": "WPA2",
     *       "bssid": "AA:BB:CC:DD:EE:FF"
     *     }
     *   ],
     *   "count": 5
     * }
     */
    static void handleScan(AsyncWebServerRequest* request);

    // ========================================================================
    // Connection Management
    // ========================================================================

    /**
     * @brief Handle POST /api/v1/network/connect
     *
     * Initiates WiFi connection. Returns 202 Accepted immediately.
     * Client should poll /status for connection result.
     *
     * Request body:
     * {
     *   "ssid": "NetworkName",
     *   "password": "password123",  // Optional for open networks
     *   "save": true                // Optional: save to NVS
     * }
     *
     * Response: 202 Accepted
     * {
     *   "message": "Connection attempt initiated",
     *   "ssid": "NetworkName"
     * }
     */
    static void handleConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    /**
     * @brief Handle POST /api/v1/network/disconnect
     *
     * Disconnects from current WiFi network.
     *
     * Response:
     * {
     *   "message": "Disconnected"
     * }
     */
    static void handleDisconnect(AsyncWebServerRequest* request);

    // ========================================================================
    // Saved Networks
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/network/saved
     *
     * Returns list of saved networks (SSIDs only, NO passwords).
     *
     * Response:
     * {
     *   "networks": ["Network1", "Network2"],
     *   "count": 2,
     *   "maxNetworks": 10
     * }
     */
    static void handleSavedList(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/network/saved
     *
     * Adds or updates a saved network.
     *
     * Request body:
     * {
     *   "ssid": "NetworkName",
     *   "password": "password123"  // Required (min 8 chars for WPA2)
     * }
     *
     * Response:
     * {
     *   "message": "Network saved",
     *   "ssid": "NetworkName"
     * }
     */
    static void handleSavedAdd(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    /**
     * @brief Handle DELETE /api/v1/network/saved/:ssid
     *
     * Removes a saved network by SSID (URL-encoded in path).
     *
     * Response:
     * {
     *   "message": "Network removed",
     *   "ssid": "NetworkName"
     * }
     */
    static void handleSavedDelete(AsyncWebServerRequest* request);

    // ========================================================================
    // Alternative Endpoints (for V1ApiRoutes compatibility)
    // ========================================================================

    /**
     * @brief Handle GET /api/v1/networks - List saved networks
     */
    static void handleListNetworks(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/networks - Add network
     */
    static void handleAddNetwork(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    /**
     * @brief Handle DELETE /api/v1/networks/:ssid - Delete network
     */
    static void handleDeleteNetwork(AsyncWebServerRequest* request, const String& ssid);

    /**
     * @brief Handle POST /api/v1/network/scan - Start scan
     */
    static void handleScanNetworks(AsyncWebServerRequest* request);

    /**
     * @brief Handle GET /api/v1/network/scan/status - Get scan status
     */
    static void handleScanStatus(AsyncWebServerRequest* request);

    // ========================================================================
    // Network Mode Control
    // ========================================================================

    /**
     * @brief Handle POST /api/v1/network/sta/enable - Enable STA mode
     *
     * Enables station mode to connect to WiFi networks.
     * Optional auto-revert to AP-only after timeout.
     *
     * Request body:
     * {
     *   "durationSeconds": 300,  // Optional: auto-revert after N seconds
     *   "revertToApOnly": true   // Optional: revert to AP mode on timeout
     * }
     */
    static void handleEnableSTA(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    /**
     * @brief Handle POST /api/v1/network/ap/enable - Force AP-only mode
     *
     * Disables station mode and forces AP-only operation.
     */
    static void handleEnableAPOnly(AsyncWebServerRequest* request);

private:
    /**
     * @brief Convert WiFi auth mode to string
     * @param authMode ESP32 wifi_auth_mode_t
     * @return Human-readable encryption type
     */
    static const char* authModeToString(uint8_t authMode);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
