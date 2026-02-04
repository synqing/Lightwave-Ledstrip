/**
 * @file WebServer.h
 * @brief Web Server for LightwaveOS v2 with Actor System integration
 *
 * Provides REST API (v1) and WebSocket real-time control.
 * All state changes go through the Actor System for thread-safe operation.
 *
 * Features:
 * - V1 REST API (/api/v1/*)
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

#if FEATURE_API_AUTH
#include <set>
#include "ApiKeyManager.h"
#include "webserver/AuthRateLimiter.h"
#endif

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

#include "SubscriptionManager.h"
#include "webserver/RateLimiter.h"
#include "webserver/LedFrameEncoder.h"
#include "webserver/LedStreamBroadcaster.h"
#include "webserver/LogStreamBroadcaster.h"
#include "../config/network_config.h"
#if FEATURE_AUDIO_SYNC
#include "webserver/AudioStreamBroadcaster.h"
#endif
#if FEATURE_AUDIO_BENCHMARK
#include "webserver/BenchmarkStreamBroadcaster.h"
#endif

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
    // Type aliases for legacy handler compatibility
    // The codebase was originally designed with NodeOrchestrator/RendererNode
    // but evolved to use ActorSystem/RendererActor. These aliases maintain
    // compatibility with existing handler code.
    namespace nodes {
        using NodeOrchestrator = actors::ActorSystem;
        using RendererNode = actors::RendererActor;
    }
}

namespace lightwaveos {
namespace network {
namespace webserver {
    class V1ApiRoutes;  // Forward declaration for friend
    class WsGateway;    // WebSocket gateway (implementation in webserver/WsGateway.*)
    class UdpStreamer;  // UDP streaming (bypasses TCP for LED/audio frames)
}
}
} // namespace lightwaveos

namespace lightwaveos {
namespace network {

// ============================================================================
// Configuration
// ============================================================================

namespace WebServerConfig {
    constexpr uint16_t HTTP_PORT = config::NetworkConfig::WEB_SERVER_PORT;
    constexpr const char* MDNS_HOSTNAME = config::NetworkConfig::MDNS_HOSTNAME;
    constexpr const char* AP_SSID_PREFIX = "LightwaveOS-";
    constexpr const char* AP_PASSWORD = config::NetworkConfig::AP_PASSWORD;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = config::NetworkConfig::WIFI_CONNECT_TIMEOUT_MS;
    constexpr uint32_t STATUS_BROADCAST_INTERVAL_MS = 5000;
    // Allow multiple open dashboard tabs + dev tools without immediately thrashing connections.
    // On no-PSRAM (FH4) use fewer clients to reduce RAM; PSRAM builds get full capacity.
    constexpr uint8_t MAX_WS_CLIENTS =
#ifdef BOARD_HAS_PSRAM
        8;
#else
        2;
#endif
    constexpr uint8_t MAX_BATCH_OPERATIONS = 10;
}

// ============================================================================
// Rate Limiter and LED Streaming (extracted to webserver/*)
// ============================================================================

// Re-export LedStreamConfig for backward compatibility
namespace LedStreamConfig {
    constexpr uint16_t LEDS_PER_STRIP = webserver::LedStreamConfig::LEDS_PER_STRIP;
    constexpr uint8_t NUM_STRIPS = webserver::LedStreamConfig::NUM_STRIPS;
    constexpr uint16_t TOTAL_LEDS = webserver::LedStreamConfig::TOTAL_LEDS;
    constexpr uint8_t FRAME_VERSION = webserver::LedStreamConfig::FRAME_VERSION;
    constexpr uint8_t MAGIC_BYTE = webserver::LedStreamConfig::MAGIC_BYTE;
    constexpr uint8_t FRAME_HEADER_SIZE = webserver::LedStreamConfig::FRAME_HEADER_SIZE;
    constexpr uint16_t FRAME_SIZE_PER_STRIP = webserver::LedStreamConfig::FRAME_SIZE_PER_STRIP;
    constexpr uint16_t FRAME_PAYLOAD_SIZE = webserver::LedStreamConfig::FRAME_PAYLOAD_SIZE;
    constexpr uint16_t FRAME_SIZE = webserver::LedStreamConfig::FRAME_SIZE;
    constexpr uint16_t LEGACY_FRAME_SIZE = webserver::LedStreamConfig::LEGACY_FRAME_SIZE;
    constexpr uint8_t TARGET_FPS = webserver::LedStreamConfig::TARGET_FPS;
    constexpr uint32_t FRAME_INTERVAL_MS = webserver::LedStreamConfig::FRAME_INTERVAL_MS;
}

// Re-export RateLimitConfig for backward compatibility
namespace RateLimitConfig {
    constexpr uint8_t MAX_TRACKED_IPS = webserver::RateLimitConfig::MAX_TRACKED_IPS;
    constexpr uint16_t HTTP_LIMIT = webserver::RateLimitConfig::HTTP_LIMIT;
    constexpr uint16_t WS_LIMIT = webserver::RateLimitConfig::WS_LIMIT;
    constexpr uint32_t WINDOW_SIZE_MS = webserver::RateLimitConfig::WINDOW_SIZE_MS;
    constexpr uint32_t BLOCK_DURATION_MS = webserver::RateLimitConfig::BLOCK_DURATION_MS;
    constexpr uint8_t RETRY_AFTER_SECONDS = webserver::RateLimitConfig::RETRY_AFTER_SECONDS;
}

// ============================================================================
// WebServer Class
// ============================================================================

/**
 * @brief Web Server with Node System integration
 *
 * All state modifications are sent through NodeOrchestrator commands,
 * ensuring thread-safe operation with the RendererActor on Core 1.
 */
class WebServer {
    friend class webserver::V1ApiRoutes;  // Allow V1ApiRoutes to access private handlers

public:
    /**
     * @brief Construct WebServer
     * @param orchestrator Reference to the NodeOrchestrator
     * @param renderer Pointer to the RendererActor
     */
    WebServer(lightwaveos::nodes::NodeOrchestrator& orchestrator, lightwaveos::nodes::RendererNode* renderer);

    /**
     * @brief Destructor
     */
    ~WebServer();

    // Prevent copying
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    
private:
    lightwaveos::nodes::NodeOrchestrator& m_orchestrator;
    lightwaveos::nodes::RendererNode* m_renderer;

    // ========================================================================
    // Lifecycle
    // ========================================================================

public:
    /**
     * @brief Initialize and start the web server
     *
     * Sets up WiFi (STA mode via WiFiManager), HTTP routes, and WebSocket.
     * Call from setup() after NodeOrchestrator is started.
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
    AsyncWebSocket* getWebSocket() const { return m_ws; }
    
    /**
     * @brief Check if LittleFS is mounted
     * @return true if filesystem is mounted
     */
    bool isLittleFSMounted() const { return m_littleFSMounted; }
    
    /**
     * @brief Attempt to mount LittleFS
     * @return true if mount succeeded
     */
    bool mountLittleFS();
    
    /**
     * @brief Unmount LittleFS (with safety checks)
     * @return true if unmount succeeded
     */
    bool unmountLittleFS();

#if FEATURE_API_AUTH
    /**
     * @brief Check if a WebSocket client is authenticated
     * @param clientId The WebSocket client ID
     * @return true if client is authenticated, false otherwise
     */
    bool isClientAuthenticated(uint32_t clientId) const;

    /**
     * @brief Get the API key manager (for WebSocket auth commands)
     * @return Reference to the ApiKeyManager instance
     */
    ApiKeyManager& getApiKeyManager() { return m_apiKeyManager; }
#endif

    // ========================================================================
    // Cached Renderer State (thread-safe read-only access from request handlers)
    // ========================================================================
    
    /**
     * @brief Cached read-only state from RendererActor
     * 
     * Updated in update() (safe context) to avoid cross-core access from request handlers.
     * All request handlers should use this cached state instead of direct renderer-> calls.
     */
    struct CachedRendererState {
        uint8_t effectCount;
        uint8_t currentEffect;
        uint8_t brightness;
        uint8_t speed;
        uint8_t paletteIndex;
        uint8_t hue;
        uint8_t intensity;
        uint8_t saturation;
        uint8_t complexity;
        uint8_t variation;
        uint8_t mood;
        uint8_t fadeAmount;
        bool isRunning;
        uint8_t queueUtilization;
        uint16_t queueLength;
        // Stats - simplified struct to avoid include dependency
        struct {
            uint16_t currentFPS;
            uint8_t cpuPercent;
            uint32_t framesRendered;
        } stats;
        // Effect names - pointers to stable strings in RendererActor (valid until next cache update)
        const char* effectNames[102];  // MAX_EFFECTS (keep in sync with RendererNode::MAX_EFFECTS)
        // Audio tuning (if available) - simplified to avoid include dependency
#if FEATURE_AUDIO_SYNC
        struct {
            float audioStalenessMs;
            float bpmMin;
            float bpmMax;
            float bpmTau;
            float confidenceTau;
            float phaseCorrectionGain;
            float barCorrectionGain;
            uint8_t beatsPerBar;
            uint8_t beatUnit;
        } audioTuning;
        const void* lastMusicalGrid;  // Opaque pointer to avoid include dependency
#endif
    };
    
    /**
     * @brief Get cached renderer state (thread-safe read-only access)
     * 
     * Returns a const reference to the cached renderer state, which is updated
     * in update() (safe context). Safe to call from request handlers.
     */
    const CachedRendererState& getCachedRendererState() const { return m_cachedRendererState; }

    // ========================================================================
    // Broadcasting
    // ========================================================================

    /**
     * @brief Broadcast current status to all WebSocket clients
     * 
     * Coalesces rapid calls - if called within BROADCAST_COALESCE_MS (50ms),
     * the broadcast is deferred to the next update() tick.
     */
    void broadcastStatus();
    
    /**
     * @brief Internal: Actually perform the status broadcast
     * 
     * Called by broadcastStatus() after coalescing check, or by update()
     * when flushing a pending broadcast.
     */
    void doBroadcastStatus();

    /**
     * @brief Broadcast zone state to all WebSocket clients
     */
    void broadcastZoneState();

    /**
     * @brief Broadcast a single zone's state change to all WebSocket clients
     *
     * Sends a "zones.stateChanged" event with the current state of the specified zone.
     * Called from ZoneComposer callback when any zone property is modified.
     * Throttling is handled by ZoneComposer (max 10/sec per zone).
     *
     * @param zoneId Zone that changed (0-3)
     */
    void broadcastSingleZoneState(uint8_t zoneId);

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
     * @brief Subscribe/unsubscribe a WebSocket client to log streaming
     *
     * Enables wireless serial monitoring by streaming log messages to
     * WebSocket clients. New subscribers receive backfill of recent logs.
     *
     * @param client WebSocket client pointer
     * @param subscribe true to subscribe, false to unsubscribe
     * @return true if the subscription table was updated
     */
    bool setLogStreamSubscription(AsyncWebSocketClient* client, bool subscribe);

    /**
     * @brief Check if any clients are subscribed to log streaming
     */
    bool hasLogStreamSubscribers() const;

#if FEATURE_AUDIO_SYNC
    /**
     * @brief Broadcast audio frame data to subscribed clients
     *
     * Sends binary WebSocket frame containing audio metrics.
     * Throttled to 30 FPS to match audio hop rate.
     */
    void broadcastAudioFrame();

    /**
     * @brief Broadcast beat event to all WebSocket clients
     *
     * Sends JSON message when beat_tick or downbeat_tick is true.
     * Called from update() at render rate, but only sends on actual beats.
     */
    void broadcastBeatEvent();

    /**
     * @brief Subscribe/unsubscribe a WebSocket client to audio stream
     * @param client WebSocket client pointer
     * @param subscribe true to subscribe, false to unsubscribe
     * @return true if the subscription was updated
     */
    bool setAudioStreamSubscription(AsyncWebSocketClient* client, bool subscribe);

    /**
     * @brief Check if any clients are subscribed to audio streaming
     */
    bool hasAudioStreamSubscribers() const;

    /**
     * @brief Broadcast FFT frame data to subscribed clients (Feature C)
     *
     * Sends JSON WebSocket frame containing 64-bin FFT data.
     * Throttled to 31 Hz (~32ms intervals).
     * Internally manages subscriber table and frame throttling.
     */
    void broadcastFftFrame();
#endif

#if FEATURE_AUDIO_BENCHMARK
    /**
     * @brief Broadcast benchmark metrics to subscribed clients
     *
     * Sends binary WebSocket frame containing audio pipeline timing.
     * Throttled to 10 Hz for low overhead.
     */
    void broadcastBenchmarkStats();

    /**
     * @brief Subscribe/unsubscribe a WebSocket client to benchmark stream
     * @param client WebSocket client pointer
     * @param subscribe true to subscribe, false to unsubscribe
     * @return true if the subscription was updated
     */
    bool setBenchmarkStreamSubscription(AsyncWebSocketClient* client, bool subscribe);

    /**
     * @brief Check if any clients are subscribed to benchmark streaming
     */
    bool hasBenchmarkStreamSubscribers() const;
#endif

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

    void setupCORS();
    void setupRoutes();
    void setupWebSocket();
    void startMDNS();


    // ========================================================================
    // V1 API Handlers (/api/v1/*)
    // ========================================================================
    // All REST API handlers moved to dedicated handler classes (see WebServer.cpp comments)

    // ========================================================================
    // WebSocket Handlers
    // ========================================================================

    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);

    void handleWsConnect(AsyncWebSocketClient* client);
    void handleWsDisconnect(AsyncWebSocketClient* client);
    void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);

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
    // API Key Authentication
    // ========================================================================

    bool checkAPIKey(AsyncWebServerRequest* request);

    // ========================================================================
    // Member Variables
    // ========================================================================

    AsyncWebServer* m_server;
    AsyncWebSocket* m_ws;
    webserver::RateLimiter m_rateLimiter;
    webserver::WsGateway* m_wsGateway;  // WebSocket gateway (Phase 2)

    bool m_running;
    bool m_apMode;
    bool m_mdnsStarted;
    bool m_littleFSMounted;
    uint32_t m_lastBroadcast;
    uint32_t m_startTime;
    IPAddress m_lastRegisteredIP;   // Track IP for mDNS refresh on DHCP change
    
    // Broadcast coalescing (prevent spam from rapid commands)
    uint32_t m_lastImmediateBroadcast;
    bool m_broadcastPending;
    static constexpr uint32_t BROADCAST_COALESCE_MS = 50;

    // LED frame streaming (extracted to LedStreamBroadcaster)
    webserver::LedStreamBroadcaster* m_ledBroadcaster;

    // UDP streaming (bypasses TCP backpressure for LED/audio frames)
    webserver::UdpStreamer* m_udpStreamer;

    // Log streaming (wireless serial monitoring)
    webserver::LogStreamBroadcaster* m_logBroadcaster;

#if FEATURE_AUDIO_SYNC
    // Audio frame streaming
    webserver::AudioStreamBroadcaster* m_audioBroadcaster;
#endif

#if FEATURE_AUDIO_BENCHMARK
    // Benchmark metrics streaming
    webserver::BenchmarkStreamBroadcaster* m_benchmarkBroadcaster;
#endif

#if FEATURE_API_AUTH
    // Authenticated WebSocket clients (by client ID)
    std::set<uint32_t> m_authenticatedClients;

    // API key manager (NVS persistence)
    ApiKeyManager m_apiKeyManager;

    // Auth rate limiter (brute force protection)
    webserver::AuthRateLimiter m_authRateLimiter;
#endif

    // Reference to external components (not owned)
    zones::ZoneComposer* m_zoneComposer;

    // ========================================================================
    // Cached Renderer State (private implementation)
    // ========================================================================
    
    CachedRendererState m_cachedRendererState;
    uint32_t m_lastStateCacheUpdate;
    static constexpr uint32_t STATE_CACHE_TTL_MS = 100;  // Update every 100ms
    
    /**
     * @brief Update cached renderer state (called from update() in safe context)
     */
    void updateCachedRendererState();
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

// ============================================================================
// Effect Validation Global Ring Buffer
// ============================================================================

#if FEATURE_EFFECT_VALIDATION
#include "../validation/ValidationFrameEncoder.h"

namespace lightwaveos {
namespace network {

/**
 * @brief Global validation ring buffer for effect validation streaming
 *
 * Effects can push samples to this ring buffer, and the WebServer
 * will drain and transmit them to subscribed WebSocket clients.
 */
extern lightwaveos::validation::EffectValidationRing<128> g_validationRing;

/**
 * @brief Global validation frame encoder
 */
extern lightwaveos::validation::ValidationFrameEncoder g_validationEncoder;

} // namespace network
} // namespace lightwaveos
#endif // FEATURE_EFFECT_VALIDATION

#endif // FEATURE_WEB_SERVER
