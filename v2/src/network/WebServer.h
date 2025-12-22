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

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

#include "SubscriptionManager.h"

// Forward declarations
class AsyncWebServer;
class AsyncWebSocket;
class AsyncWebSocketClient;

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
// LED Frame Streaming Configuration
// ============================================================================

namespace LedStreamConfig {
    constexpr uint16_t TOTAL_LEDS = 320;              // Total LEDs (2 strips × 160)
    constexpr uint16_t FRAME_SIZE = TOTAL_LEDS * 3;   // RGB bytes per frame (960 bytes)
    constexpr uint8_t TARGET_FPS = 20;                // Max streaming FPS (throttled)
    constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;  // ~50ms between frames
    constexpr uint8_t MAGIC_BYTE = 0xFE;              // Frame header magic byte
}

// ============================================================================
// Rate Limiter Configuration
// ============================================================================

namespace RateLimitConfig {
    constexpr uint8_t MAX_TRACKED_IPS = 8;         // Number of IPs to track
    constexpr uint16_t HTTP_LIMIT = 20;             // Max HTTP requests per window
    constexpr uint16_t WS_LIMIT = 50;               // Max WebSocket messages per window
    constexpr uint32_t WINDOW_SIZE_MS = 1000;       // 1 second sliding window
    constexpr uint32_t BLOCK_DURATION_MS = 5000;    // Block duration when limit exceeded
    constexpr uint8_t RETRY_AFTER_SECONDS = 5;      // Retry-After header value
}

// ============================================================================
// Rate Limiter
// ============================================================================

/**
 * @brief Per-IP token bucket rate limiter with sliding window
 *
 * Tracks rate limits per IP address for HTTP and per client for WebSocket.
 * Uses LRU eviction when tracking table is full.
 *
 * Features:
 * - Separate limits for HTTP (20/sec) and WebSocket (50/sec)
 * - Automatic blocking for 5 seconds when limit exceeded
 * - LRU eviction when tracking table is full
 * - Provides remaining block time for Retry-After header
 *
 * RAM Cost: ~400 bytes (8 IP entries * ~48 bytes each)
 */
class RateLimiter {
public:
    /**
     * @brief Per-IP rate limiting entry
     */
    struct Entry {
        IPAddress ip;           // Client IP address
        uint32_t windowStart;   // Start of current window (millis)
        uint16_t httpCount;     // HTTP requests in current window
        uint16_t wsCount;       // WebSocket messages in current window
        uint32_t blockedUntil;  // Time when block expires (0 = not blocked)
    };

    RateLimiter() {
        memset(m_entries, 0, sizeof(m_entries));
    }

    /**
     * @brief Check and record an HTTP request
     * @param ip Client IP address
     * @return true if request is allowed, false if rate limited
     */
    bool checkHTTP(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return true; // Can't track, allow the request

        uint32_t now = millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > RateLimitConfig::WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->httpCount >= RateLimitConfig::HTTP_LIMIT) {
            entry->blockedUntil = now + RateLimitConfig::BLOCK_DURATION_MS;
            return false;
        }

        entry->httpCount++;
        return true;
    }

    /**
     * @brief Check and record a WebSocket message
     * @param ip Client IP address
     * @return true if message is allowed, false if rate limited
     */
    bool checkWebSocket(IPAddress ip) {
        Entry* entry = findOrCreate(ip);
        if (!entry) return true; // Can't track, allow the message

        uint32_t now = millis();

        // Check if currently blocked
        if (entry->blockedUntil > now) {
            return false;
        }

        // Reset window if expired
        if (now - entry->windowStart > RateLimitConfig::WINDOW_SIZE_MS) {
            entry->windowStart = now;
            entry->httpCount = 0;
            entry->wsCount = 0;
        }

        // Check limit
        if (entry->wsCount >= RateLimitConfig::WS_LIMIT) {
            entry->blockedUntil = now + RateLimitConfig::BLOCK_DURATION_MS;
            return false;
        }

        entry->wsCount++;
        return true;
    }

    /**
     * @brief Check if an IP is currently blocked
     * @param ip Client IP address
     * @return true if blocked, false if allowed
     */
    bool isBlocked(IPAddress ip) const {
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return m_entries[i].blockedUntil > millis();
            }
        }
        return false;
    }

    /**
     * @brief Get remaining time until block expires
     * @param ip Client IP address
     * @return Remaining block time in seconds (for Retry-After header), 0 if not blocked
     */
    uint32_t getRetryAfterSeconds(IPAddress ip) const {
        uint32_t now = millis();
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip && m_entries[i].blockedUntil > now) {
                return (m_entries[i].blockedUntil - now + 999) / 1000; // Round up to seconds
            }
        }
        return RateLimitConfig::RETRY_AFTER_SECONDS; // Default retry time
    }

    /**
     * @brief Get current HTTP request count for an IP
     * @param ip Client IP address
     * @return Current request count in window, or 0 if not tracked
     */
    uint16_t getHttpCount(IPAddress ip) const {
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                if (millis() - m_entries[i].windowStart <= RateLimitConfig::WINDOW_SIZE_MS) {
                    return m_entries[i].httpCount;
                }
                return 0;
            }
        }
        return 0;
    }

    /**
     * @brief Get current WebSocket message count for an IP
     * @param ip Client IP address
     * @return Current message count in window, or 0 if not tracked
     */
    uint16_t getWsCount(IPAddress ip) const {
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                if (millis() - m_entries[i].windowStart <= RateLimitConfig::WINDOW_SIZE_MS) {
                    return m_entries[i].wsCount;
                }
                return 0;
            }
        }
        return 0;
    }

private:
    Entry m_entries[RateLimitConfig::MAX_TRACKED_IPS];  // ~384 bytes

    /**
     * @brief Find existing entry or create new one for IP
     * Uses LRU eviction when table is full
     */
    Entry* findOrCreate(IPAddress ip) {
        // Find existing entry
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == ip) {
                return &m_entries[i];
            }
        }

        // Find empty slot
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].ip == IPAddress(0, 0, 0, 0)) {
                m_entries[i].ip = ip;
                m_entries[i].windowStart = millis();
                m_entries[i].httpCount = 0;
                m_entries[i].wsCount = 0;
                m_entries[i].blockedUntil = 0;
                return &m_entries[i];
            }
        }

        // Table full - evict oldest (LRU) entry
        uint32_t oldest = 0xFFFFFFFF;
        uint8_t oldestIdx = 0;
        for (uint8_t i = 0; i < RateLimitConfig::MAX_TRACKED_IPS; i++) {
            if (m_entries[i].windowStart < oldest) {
                oldest = m_entries[i].windowStart;
                oldestIdx = i;
            }
        }

        // Reset the oldest entry for new IP
        m_entries[oldestIdx].ip = ip;
        m_entries[oldestIdx].windowStart = millis();
        m_entries[oldestIdx].httpCount = 0;
        m_entries[oldestIdx].wsCount = 0;
        m_entries[oldestIdx].blockedUntil = 0;
        return &m_entries[oldestIdx];
    }
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
     * @brief Broadcast LED frame data to subscribed clients
     *
     * Sends binary WebSocket frame containing RGB data for all 320 LEDs.
     * Frame format: [0xFE magic byte][320 × RGB bytes] = 961 bytes total
     * Throttled to 20 FPS to limit bandwidth (~19KB/sec)
     */
    void broadcastLEDFrame();

    /**
     * @brief Subscribe/unsubscribe a WebSocket client to LED frame streaming
     *
     * Uses a fixed-size subscriber table (max = MAX_WS_CLIENTS). We do NOT rely
     * on client IDs being dense/small, and we avoid version-fragile iteration
     * over all clients.
     *
     * @param client WebSocket client pointer
     * @param subscribe true to subscribe, false to unsubscribe
     * @return true if the subscription table was updated (or already in desired state)
     */
    bool setLEDStreamSubscription(AsyncWebSocketClient* client, bool subscribe);

    /**
     * @brief Check if any clients are subscribed to LED streaming
     */
    bool hasLEDStreamSubscribers() const;

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
    void handleEffectsFamilies(AsyncWebServerRequest* request);

    // Narrative
    void handleNarrativeStatus(AsyncWebServerRequest* request);
    void handleNarrativeConfigGet(AsyncWebServerRequest* request);
    void handleNarrativeConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

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

    // LED frame streaming state
    SubscriptionManager<WebServerConfig::MAX_WS_CLIENTS> m_ledStreamSubscribers;
    uint32_t m_lastLedBroadcast;          // Last LED frame broadcast time
    uint8_t m_ledFrameBuffer[LedStreamConfig::FRAME_SIZE + 1];  // +1 for magic byte
#if defined(ESP32)
    mutable portMUX_TYPE m_ledStreamMux = portMUX_INITIALIZER_UNLOCKED;
#endif

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
