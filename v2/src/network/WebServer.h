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

#if FEATURE_API_AUTH
#include <set>
#endif

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

#include "SubscriptionManager.h"
#include "webserver/RateLimiter.h"
#include "webserver/LedFrameEncoder.h"
#include "webserver/LedStreamBroadcaster.h"
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
    // Allow multiple open dashboard tabs + dev tools without immediately thrashing connections.
    // This also bounds subscriber tables and per-frame broadcast iteration.
    constexpr uint8_t MAX_WS_CLIENTS = 8;
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
 * @brief Web Server with Actor System integration
 *
 * All state modifications are sent through ActorSystem commands,
 * ensuring thread-safe operation with the RendererActor on Core 1.
 */
class WebServer {
public:
    /**
     * @brief Construct WebServer
     * @param actors Reference to the ActorSystem
     * @param renderer Pointer to the RendererActor
     */
    WebServer(lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer);

    /**
     * @brief Destructor
     */
    ~WebServer();

    // Prevent copying
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    
private:
    lightwaveos::actors::ActorSystem& m_actorSystem;
    lightwaveos::actors::RendererActor* m_renderer;

    // ========================================================================
    // Lifecycle
    // ========================================================================

public:
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
    void handleHealth(AsyncWebServerRequest* request);
    void handleParametersGet(AsyncWebServerRequest* request);
    void handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleAudioParametersGet(AsyncWebServerRequest* request);
    void handleAudioParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleAudioControl(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleAudioStateGet(AsyncWebServerRequest* request);
    void handleAudioTempoGet(AsyncWebServerRequest* request);
    void handleAudioPresetsList(AsyncWebServerRequest* request);
    void handleAudioPresetGet(AsyncWebServerRequest* request, uint8_t presetId);
    void handleAudioPresetSave(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleAudioPresetApply(AsyncWebServerRequest* request, uint8_t presetId);
    void handleAudioPresetDelete(AsyncWebServerRequest* request, uint8_t presetId);

    // Audio-Effect Mapping (Phase 4)
    void handleAudioMappingsListSources(AsyncWebServerRequest* request);
    void handleAudioMappingsListTargets(AsyncWebServerRequest* request);
    void handleAudioMappingsListCurves(AsyncWebServerRequest* request);
    void handleAudioMappingsList(AsyncWebServerRequest* request);
    void handleAudioMappingsGet(AsyncWebServerRequest* request, uint8_t effectId);
    void handleAudioMappingsSet(AsyncWebServerRequest* request, uint8_t effectId, uint8_t* data, size_t len);
    void handleAudioMappingsDelete(AsyncWebServerRequest* request, uint8_t effectId);
    void handleAudioMappingsEnable(AsyncWebServerRequest* request, uint8_t effectId, bool enable);
    void handleAudioMappingsStats(AsyncWebServerRequest* request);

    // Zone AGC endpoints
    void handleAudioZoneAGCGet(AsyncWebServerRequest* request);
    void handleAudioZoneAGCSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // Spike detection endpoints
    void handleAudioSpikeDetectionGet(AsyncWebServerRequest* request);
    void handleAudioSpikeDetectionReset(AsyncWebServerRequest* request);

    void handleTransitionTypes(AsyncWebServerRequest* request);
    void handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePalettesList(AsyncWebServerRequest* request);
    void handlePalettesCurrent(AsyncWebServerRequest* request);
    void handlePalettesSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

#if FEATURE_AUDIO_BENCHMARK
    // Audio Benchmark API
    void handleBenchmarkGet(AsyncWebServerRequest* request);
    void handleBenchmarkStart(AsyncWebServerRequest* request);
    void handleBenchmarkStop(AsyncWebServerRequest* request);
    void handleBenchmarkHistory(AsyncWebServerRequest* request);
#endif



    // Narrative
    void handleNarrativeStatus(AsyncWebServerRequest* request);
    void handleNarrativeConfigGet(AsyncWebServerRequest* request);
    void handleNarrativeConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // Transition Config
    void handleTransitionConfigGet(AsyncWebServerRequest* request);
    void handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // OpenAPI Specification
    void handleOpenApiSpec(AsyncWebServerRequest* request);

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
    // API Key Authentication
    // ========================================================================

    bool checkAPIKey(AsyncWebServerRequest* request);

    // ========================================================================
    // Member Variables
    // ========================================================================

    AsyncWebServer* m_server;
    AsyncWebSocket* m_ws;
    webserver::RateLimiter m_rateLimiter;

    bool m_running;
    bool m_apMode;
    bool m_mdnsStarted;
    uint32_t m_lastBroadcast;
    uint32_t m_startTime;
    
    // Broadcast coalescing (prevent spam from rapid commands)
    uint32_t m_lastImmediateBroadcast;
    bool m_broadcastPending;
    static constexpr uint32_t BROADCAST_COALESCE_MS = 50;

    // LED frame streaming (extracted to LedStreamBroadcaster)
    webserver::LedStreamBroadcaster* m_ledBroadcaster;

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
