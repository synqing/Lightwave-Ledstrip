/**
 * @file WebServer.h
 * @brief Web Server for LightwaveOS v2 with Actor System integration
 *
 * Provides REST API (v1 modern + legacy) and WebSocket real-time control.
 * All state changes go through the Actor System for thread-safe operation.
 *
 * Features:
 * - Legacy API (/api/*) for backward compatibility
 * - Modern API v1 (/api/v1/*) with HATEOAS and standardized responses
 * - WebSocket (/ws) for real-time control and events
 * - Rate limiting: 20 req/sec HTTP, 50 msg/sec WebSocket
 * - CORS enabled for browser access
 * - mDNS: lightwaveos.local
 *
 * Architecture:
 * - WebServer runs on Core 0 with WiFi stack
 * - State changes sent as messages to RendererActor on Core 1
 * - Never directly accesses LED buffers
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>

// Forward declarations
namespace lightwaveos {
    namespace actors {
        class ActorSystem;
        class RendererActor;
    }
    namespace zones {
        class ZoneComposer;
    }
}

namespace lightwaveos {
namespace network {

// ============================================================================
// Configuration
// ============================================================================

namespace WebServerConfig {
    constexpr uint16_t HTTP_PORT = 80;
    constexpr const char* MDNS_HOSTNAME = "lightwaveos";
    constexpr const char* AP_SSID_PREFIX = "LightwaveOS-";
    constexpr const char* AP_PASSWORD = "lightwave123";
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
    constexpr uint32_t STATUS_BROADCAST_INTERVAL_MS = 5000;
    constexpr uint8_t MAX_WS_CLIENTS = 4;
    constexpr uint8_t MAX_BATCH_OPERATIONS = 10;
}

// ============================================================================
// Rate Limiter
// ============================================================================

/**
 * @brief Simple token bucket rate limiter
 */
class RateLimiter {
public:
    RateLimiter() : m_httpTokens(20), m_wsTokens(50), m_lastRefill(0) {}

    bool checkHTTP() {
        refillTokens();
        if (m_httpTokens > 0) {
            m_httpTokens--;
            return true;
        }
        return false;
    }

    bool checkWebSocket() {
        refillTokens();
        if (m_wsTokens > 0) {
            m_wsTokens--;
            return true;
        }
        return false;
    }

private:
    void refillTokens() {
        uint32_t now = millis();
        if (now - m_lastRefill >= 1000) {
            m_httpTokens = 20;  // 20 req/sec
            m_wsTokens = 50;    // 50 msg/sec
            m_lastRefill = now;
        }
    }

    uint8_t m_httpTokens;
    uint8_t m_wsTokens;
    uint32_t m_lastRefill;
};

// ============================================================================
// WebServer Class
// ============================================================================

/**
 * @brief Web Server with Actor System integration
 *
 * All state modifications are sent through ACTOR_SYSTEM commands,
 * ensuring thread-safe operation with the RendererActor on Core 1.
 */
class WebServer {
public:
    /**
     * @brief Construct WebServer
     */
    WebServer();

    /**
     * @brief Destructor
     */
    ~WebServer();

    // Prevent copying
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize and start the web server
     *
     * Sets up WiFi (STA or AP mode), HTTP routes, and WebSocket.
     * Call from setup() after ActorSystem is started.
     *
     * @return true if server started successfully
     */
    bool begin();

    /**
     * @brief Stop the web server
     */
    void stop();

    /**
     * @brief Update function (call from loop)
     *
     * Handles WebSocket cleanup and periodic status broadcasts.
     */
    void update();

    // ========================================================================
    // Status
    // ========================================================================

    bool isRunning() const { return m_running; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool isAPMode() const { return m_apMode; }
    size_t getClientCount() const { return m_ws->count(); }

    // ========================================================================
    // Broadcasting
    // ========================================================================

    /**
     * @brief Broadcast current status to all WebSocket clients
     */
    void broadcastStatus();

    /**
     * @brief Broadcast zone state to all WebSocket clients
     */
    void broadcastZoneState();

    /**
     * @brief Notify clients of effect change
     */
    void notifyEffectChange(uint8_t effectId, const char* name);

    /**
     * @brief Notify clients of parameter change
     */
    void notifyParameterChange();

private:
    // ========================================================================
    // Setup Methods
    // ========================================================================

    bool initWiFi();
    bool startAPMode();
    void setupCORS();
    void setupRoutes();
    void setupLegacyRoutes();
    void setupV1Routes();
    void setupWebSocket();
    void startMDNS();

    // ========================================================================
    // Legacy API Handlers (/api/*)
    // ========================================================================

    void handleLegacyStatus(AsyncWebServerRequest* request);
    void handleLegacySetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleLegacySetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleLegacySetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleLegacySetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleLegacyZoneCount(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleLegacyZoneEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ========================================================================
    // V1 API Handlers (/api/v1/*)
    // ========================================================================

    void handleApiDiscovery(AsyncWebServerRequest* request);
    void handleDeviceStatus(AsyncWebServerRequest* request);
    void handleDeviceInfo(AsyncWebServerRequest* request);
    void handleEffectsList(AsyncWebServerRequest* request);
    void handleEffectsCurrent(AsyncWebServerRequest* request);
    void handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleParametersGet(AsyncWebServerRequest* request);
    void handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTransitionTypes(AsyncWebServerRequest* request);
    void handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePalettesList(AsyncWebServerRequest* request);
    void handlePalettesCurrent(AsyncWebServerRequest* request);
    void handlePalettesSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // Effect Metadata
    void handleEffectsMetadata(AsyncWebServerRequest* request);

    // Transition Config
    void handleTransitionConfigGet(AsyncWebServerRequest* request);
    void handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // OpenAPI Specification
    void handleOpenApiSpec(AsyncWebServerRequest* request);

    // Zone v1 REST Handlers
    void handleZonesList(AsyncWebServerRequest* request);
    void handleZonesLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneGet(AsyncWebServerRequest* request);
    void handleZoneSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleZoneSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // Helper to extract zone ID from URL path
    uint8_t extractZoneIdFromPath(AsyncWebServerRequest* request);

    // ========================================================================
    // WebSocket Handlers
    // ========================================================================

    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);

    void handleWsConnect(AsyncWebSocketClient* client);
    void handleWsDisconnect(AsyncWebSocketClient* client);
    void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    void processWsCommand(AsyncWebSocketClient* client, JsonDocument& doc);

    // ========================================================================
    // Batch Operations
    // ========================================================================

    bool executeBatchAction(const String& action, JsonVariant params);

    // ========================================================================
    // Rate Limiting
    // ========================================================================

    bool checkRateLimit(AsyncWebServerRequest* request);
    bool checkWsRateLimit(AsyncWebSocketClient* client);

    // ========================================================================
    // Member Variables
    // ========================================================================

    AsyncWebServer* m_server;
    AsyncWebSocket* m_ws;
    RateLimiter m_rateLimiter;

    bool m_running;
    bool m_apMode;
    bool m_mdnsStarted;
    uint32_t m_lastBroadcast;
    uint32_t m_startTime;

    // Reference to external components (not owned)
    zones::ZoneComposer* m_zoneComposer;
};

// ============================================================================
// Global Instance
// ============================================================================

/**
 * @brief Global WebServer instance
 */
extern WebServer webServer;

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
