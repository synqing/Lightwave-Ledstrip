/**
 * @file WebServer.cpp
 * @brief Web Server implementation for LightwaveOS v2
 *
 * Implements REST API and WebSocket server integrated with Actor System.
 * All state changes are routed through m_actorSystem for thread safety.
 */

#include "WebServer.h"

#if FEATURE_WEB_SERVER

#define LW_LOG_TAG "WebServer"
#include "utils/Log.h"

#include "ApiResponse.h"
#include "RequestValidator.h"
#include "WiFiManager.h"
#include "webserver/RateLimiter.h"
#include "webserver/LedStreamBroadcaster.h"
#include "webserver/handlers/DeviceHandlers.h"
#include "webserver/handlers/EffectHandlers.h"
#include "webserver/handlers/ZoneHandlers.h"
#include "webserver/handlers/SystemHandlers.h"
#include "webserver/handlers/BatchHandlers.h"
#include "webserver/handlers/ParameterHandlers.h"
#include "webserver/handlers/PaletteHandlers.h"
#include "webserver/handlers/TransitionHandlers.h"
#include "webserver/handlers/NarrativeHandlers.h"
#include "webserver/handlers/AudioHandlers.h"
#include "webserver/WebServerContext.h"
#include "webserver/HttpRouteRegistry.h"
#include "webserver/StaticAssetRoutes.h"
#include "webserver/V1ApiRoutes.h"
#include "webserver/WsGateway.h"
#include "webserver/ws/WsDeviceCommands.h"
#include "webserver/ws/WsEffectsCommands.h"
#include "webserver/ws/WsZonesCommands.h"
#include "webserver/ws/WsTransitionCommands.h"
#include "webserver/ws/WsNarrativeCommands.h"
#include "webserver/ws/WsMotionCommands.h"
#include "webserver/ws/WsColorCommands.h"
#include "webserver/ws/WsPaletteCommands.h"
#include "webserver/ws/WsBatchCommands.h"
#if FEATURE_AUDIO_SYNC
#include "webserver/ws/WsAudioCommands.h"
#include "webserver/ws/WsDebugCommands.h"
#include "webserver/ws/WsTrinityCommands.h"
#endif
#include "webserver/ws/WsStreamCommands.h"
#include "webserver/ws/WsPluginCommands.h"
#include "../config/network_config.h"
#include "../core/actors/ActorSystem.h"
#include "../effects/zones/ZoneDefinition.h"
#include <Update.h>
#include "../core/actors/RendererActor.h"
#include "../core/persistence/ZoneConfigManager.h"
#include "../effects/zones/ZoneComposer.h"
#include "../effects/transitions/TransitionTypes.h"
#include "../palettes/Palettes_Master.h"
#include "../effects/PatternRegistry.h"
#include "../core/narrative/NarrativeEngine.h"
#include "../effects/enhancement/MotionEngine.h"
#include "../effects/enhancement/ColorEngine.h"
#include "../plugins/api/IEffect.h"
#if FEATURE_AUDIO_SYNC
#include "../audio/AudioTuning.h"
#include "../config/audio_config.h"
#include "../core/persistence/AudioTuningManager.h"
#endif
#if FEATURE_AUDIO_BENCHMARK
#include "webserver/BenchmarkStreamBroadcaster.h"
#include "../audio/AudioBenchmarkMetrics.h"
#endif
#if FEATURE_EFFECT_VALIDATION
#include "../validation/ValidationFrameEncoder.h"
#include "../validation/EffectValidationMetrics.h"
#endif
#include <cstring>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Arduino.h>

#if FEATURE_MULTI_DEVICE
#include "../sync/DeviceUUID.h"
#endif

// External zone config manager from main.cpp
extern lightwaveos::persistence::ZoneConfigManager* zoneConfigMgr;

using namespace lightwaveos::actors;
using namespace lightwaveos::zones;
using namespace lightwaveos::transitions;
using namespace lightwaveos::palettes;

namespace lightwaveos {
namespace network {

// Global instance pointer (initialized in setup)
WebServer* webServerInstance = nullptr;

#if FEATURE_EFFECT_VALIDATION
// Validation encoder (uses g_validationRing from EffectValidationMacros.cpp)
// Encoder is lazily initialized to avoid stack overflow during static init
static lightwaveos::validation::ValidationFrameEncoder* s_validationEncoder = nullptr;

// Validation subscriber tracking (max 4 clients)
static AsyncWebSocketClient* s_validationSubscribers[4] = {nullptr, nullptr, nullptr, nullptr};
static constexpr size_t MAX_VALIDATION_SUBSCRIBERS = 4;

static void initValidationEncoder() {
    if (s_validationEncoder == nullptr) {
        lightwaveos::validation::initValidationRing();
        s_validationEncoder = new lightwaveos::validation::ValidationFrameEncoder();
        s_validationEncoder->begin(lightwaveos::validation::g_validationRing);
    }
}
#endif

// ============================================================================
// Constructor / Destructor
// ============================================================================

WebServer::WebServer(ActorSystem& actors, RendererActor* renderer)
    : m_server(nullptr)
    , m_ws(nullptr)
    , m_running(false)
    , m_apMode(false)
    , m_mdnsStarted(false)
    , m_lastBroadcast(0)
    , m_startTime(0)
    , m_lastImmediateBroadcast(0)
    , m_broadcastPending(false)
    , m_zoneComposer(nullptr)
    , m_pluginManager(nullptr)
    , m_lastStateCacheUpdate(0)
    , m_ledBroadcaster(nullptr)
    , m_wsGateway(nullptr)
    , m_actorSystem(actors)
    , m_renderer(renderer)
{
}

WebServer::~WebServer() {
    stop();
    delete m_wsGateway;
#if FEATURE_AUDIO_BENCHCHMARK
    delete m_benchmarkBroadcaster;
#endif
#if FEATURE_AUDIO_SYNC
    delete m_audioBroadcaster;
#endif
    delete m_ledBroadcaster;
    delete m_ws;
    delete m_server;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool WebServer::begin() {
    LW_LOGI("Starting v2 WebServer...");

    // Initialize LittleFS for static file serving
    if (!LittleFS.begin(true)) {
        LW_LOGW("LittleFS mount failed!");
    } else {
        LW_LOGI("LittleFS mounted");
    }

    // Create server instances
    m_server = new AsyncWebServer(WebServerConfig::HTTP_PORT);
    m_ws = new AsyncWebSocket("/ws");
    
    // Create LED stream broadcaster
    m_ledBroadcaster = new webserver::LedStreamBroadcaster(m_ws, WebServerConfig::MAX_WS_CLIENTS);

#if FEATURE_AUDIO_SYNC
    // Create audio stream broadcaster
    m_audioBroadcaster = new webserver::AudioStreamBroadcaster(m_ws);
#endif

#if FEATURE_AUDIO_BENCHMARK
    // Create benchmark metrics broadcaster
    m_benchmarkBroadcaster = new webserver::BenchmarkStreamBroadcaster(m_ws);
#endif

#if FEATURE_EFFECT_VALIDATION
    // Initialize effect validation encoder (lazy init to avoid stack overflow)
    initValidationEncoder();
#endif

    // Initialise WiFi (STA-only: WebServer must never attempt AP fallback)
    // WiFiManager owns all WiFi mode decisions.
    if (!initWiFi()) {
        LW_LOGW("WiFi init failed (STA-only). Continuing without forcing AP mode.");
        // Keep running: routes that require WiFi will naturally be unavailable until connected.
        m_apMode = false;
    }

    // Acquire ZoneComposer before creating any WebServerContext (routes/WS depend on it).
    // This prevents FEATURE_DISABLED responses for zones when the compositor is available.
    if (m_renderer) {
        m_zoneComposer = m_renderer->getZoneComposer();
    }

    // Setup CORS headers
    setupCORS();

    // Setup HTTP routes (before WebSocket, as routes may depend on server state)
    setupRoutes();

    // Set start time before creating gateway (gateway context needs it)
    m_startTime = millis();

    // Setup WebSocket gateway (after routes and startTime, as gateway needs context)
    setupWebSocket();

    // Start mDNS
    startMDNS();

    // Start the server
    m_server->begin();
    m_running = true;

    // Get zone composer reference if available
    if (m_renderer) {
        m_zoneComposer = m_renderer->getZoneComposer();
    }

    LW_LOGI("Server running on port %d", WebServerConfig::HTTP_PORT);
    if (!m_apMode) {
        // Verify IP is valid before logging (defensive check)
        IPAddress ip = WiFi.localIP();
        if (ip != INADDR_NONE && ip != IPAddress(0, 0, 0, 0)) {
            LW_LOGI("Connected - IP: %s", ip.toString().c_str());
        } else {
            LW_LOGW("IP not yet assigned, check WiFiManager status");
        }
    } else {
        // STA-only build: AP mode should not be entered via WebServer.
        LW_LOGW("AP mode reported active; STA-only build expects WiFiManager to manage this.");
    }

    return true;
}

void WebServer::stop() {
    if (m_running) {
        m_ws->closeAll();
        m_server->end();
        m_running = false;
        LW_LOGI("Server stopped");
    }
}

void WebServer::update() {
    if (!m_running) return;

    // Cleanup disconnected WebSocket clients
    m_ws->cleanupClients();

    // WebSocket keepalive ping - prevents mobile network timeouts
    static uint32_t lastPingMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastPingMs >= 30000) {  // Every 30 seconds
        if (m_ws && m_ws->count() > 0) {
            m_ws->pingAll();
        }
        lastPingMs = nowMs;
    }

    // LED frame streaming to subscribed clients (20 FPS)
    broadcastLEDFrame();

#if FEATURE_AUDIO_SYNC
    // Audio frame streaming to subscribed clients (30 FPS)
    broadcastAudioFrame();

    // Beat event streaming (fires on beat_tick/downbeat_tick)
    broadcastBeatEvent();
#endif

#if FEATURE_AUDIO_BENCHMARK
    // Benchmark metrics streaming to subscribed clients (10 FPS)
    broadcastBenchmarkStats();
#endif

#if FEATURE_EFFECT_VALIDATION
    // Effect validation streaming to subscribed clients
    if (s_validationEncoder && s_validationEncoder->tick()) {
        // Send to all subscribed validation clients
        for (size_t i = 0; i < MAX_VALIDATION_SUBSCRIBERS; ++i) {
            AsyncWebSocketClient* client = s_validationSubscribers[i];
            if (client && client->status() == WS_CONNECTED) {
                client->binary(s_validationEncoder->getFrame(), s_validationEncoder->getFrameSize());
            }
        }
        s_validationEncoder->clearFrame();
    }
#endif

    // Periodic status broadcast
    uint32_t now = millis();
    if (now - m_lastBroadcast >= WebServerConfig::STATUS_BROADCAST_INTERVAL_MS) {
        m_lastBroadcast = now;
        m_broadcastPending = true;  // Defer to safe context
    }
    
    // Process deferred broadcasts (safe context - not in AsyncTCP callback)
    // Coalesce: only send if enough time has passed since last broadcast
    // QUEUE PROTECTION: Additional check - skip if we've sent too many broadcasts recently
    if (m_broadcastPending) {
        if (now - m_lastImmediateBroadcast >= BROADCAST_COALESCE_MS) {
            // Additional queue protection: check if we can safely send
            // (AsyncWebSocket doesn't expose queue size, so we use time-based throttling)
            m_lastImmediateBroadcast = now;
            m_broadcastPending = false;
            doBroadcastStatus();  // Actually send the broadcast in safe context
        }
    }
    
    // Update cached renderer state (safe context - not in AsyncTCP callback)
    updateCachedRendererState();
}

// ============================================================================
// Cached Renderer State
// ============================================================================

void WebServer::updateCachedRendererState() {
    // SAFETY: This method is called from update() which runs in a safe context (not in AsyncTCP callback)
    // It's safe to access m_renderer here because we're on the same core or in a controlled context
    
    if (!m_renderer) {
        // Clear cache if renderer is not available
        memset(&m_cachedRendererState, 0, sizeof(m_cachedRendererState));
        return;
    }
    
    uint32_t now = millis();
    // Update cache if TTL expired
    if (now - m_lastStateCacheUpdate < STATE_CACHE_TTL_MS) {
        return;  // Cache still fresh
    }
    
    m_lastStateCacheUpdate = now;
    
    // Cache all const getter values (safe to call from update context)
    m_cachedRendererState.effectCount = m_renderer->getEffectCount();
    m_cachedRendererState.currentEffect = m_renderer->getCurrentEffect();
    m_cachedRendererState.brightness = m_renderer->getBrightness();
    m_cachedRendererState.speed = m_renderer->getSpeed();
    m_cachedRendererState.paletteIndex = m_renderer->getPaletteIndex();
    m_cachedRendererState.hue = m_renderer->getHue();
    m_cachedRendererState.intensity = m_renderer->getIntensity();
    m_cachedRendererState.saturation = m_renderer->getSaturation();
    m_cachedRendererState.complexity = m_renderer->getComplexity();
    m_cachedRendererState.variation = m_renderer->getVariation();
    m_cachedRendererState.mood = m_renderer->getMood();
    m_cachedRendererState.fadeAmount = m_renderer->getFadeAmount();
    m_cachedRendererState.isRunning = m_renderer->isRunning();
    m_cachedRendererState.queueUtilization = m_renderer->getQueueUtilization();
    m_cachedRendererState.queueLength = m_renderer->getQueueLength();
    // Copy stats struct (field-by-field to avoid include dependency)
    const RenderStats& srcStats = m_renderer->getStats();
    m_cachedRendererState.stats.currentFPS = srcStats.currentFPS;
    m_cachedRendererState.stats.cpuPercent = srcStats.cpuPercent;
    m_cachedRendererState.stats.framesRendered = srcStats.framesRendered;
    
    // Cache effect names (pointers to stable strings in RendererActor)
    uint8_t count = m_cachedRendererState.effectCount;
    constexpr uint8_t maxEffects = sizeof(m_cachedRendererState.effectNames) /
                                   sizeof(m_cachedRendererState.effectNames[0]);
    if (count > maxEffects) count = maxEffects;  // Safety: MAX_EFFECTS
    for (uint8_t i = 0; i < count; ++i) {
        m_cachedRendererState.effectNames[i] = m_renderer->getEffectName(i);
    }
    // Clear remaining slots
    for (uint8_t i = count; i < maxEffects; ++i) {
        m_cachedRendererState.effectNames[i] = nullptr;
    }
    
#if FEATURE_AUDIO_SYNC
    // Cache audio tuning (field-by-field to avoid include dependency)
    const audio::AudioContractTuning& srcTuning = m_renderer->getAudioContractTuning();
    m_cachedRendererState.audioTuning.audioStalenessMs = srcTuning.audioStalenessMs;
    m_cachedRendererState.audioTuning.bpmMin = srcTuning.bpmMin;
    m_cachedRendererState.audioTuning.bpmMax = srcTuning.bpmMax;
    m_cachedRendererState.audioTuning.bpmTau = srcTuning.bpmTau;
    m_cachedRendererState.audioTuning.confidenceTau = srcTuning.confidenceTau;
    m_cachedRendererState.audioTuning.phaseCorrectionGain = srcTuning.phaseCorrectionGain;
    m_cachedRendererState.audioTuning.barCorrectionGain = srcTuning.barCorrectionGain;
    m_cachedRendererState.audioTuning.beatsPerBar = srcTuning.beatsPerBar;
    m_cachedRendererState.audioTuning.beatUnit = srcTuning.beatUnit;
    // getLastMusicalGrid() returns a const reference to internal data - safe to cache pointer
    m_cachedRendererState.lastMusicalGrid = &m_renderer->getLastMusicalGrid();
#endif
}

// ============================================================================
// WiFi Initialization
// ============================================================================

bool WebServer::initWiFi() {
    // WiFiManager handles all WiFi mode switching and connections.
    // WebServer just checks the current state - no WiFi.mode() calls here!
    // This avoids mode conflicts (APSTA vs STA vs AP) between components.

    // Check if WiFiManager has already connected
    if (WIFI_MANAGER.isConnected()) {
        LW_LOGI("WiFi already connected via WiFiManager");
        m_apMode = false;
        return true;
    }

    // Check if WiFiManager is in AP mode
    if (WIFI_MANAGER.isAPMode()) {
        LW_LOGI("WiFi in AP mode via WiFiManager");
        m_apMode = true;
        return true;
    }

    // If WiFiManager isn't connected yet, wait briefly for it
    LW_LOGI("Waiting for WiFiManager connection...");
    uint32_t startTime = millis();
    while (!WIFI_MANAGER.isConnected() && !WIFI_MANAGER.isAPMode()) {
        if (millis() - startTime > WebServerConfig::WIFI_CONNECT_TIMEOUT_MS) {
            LW_LOGW("WiFiManager connection timeout");
            return false;
        }
        delay(100);
    }

    m_apMode = WIFI_MANAGER.isAPMode();
    if (m_apMode) {
        LW_LOGI("WiFi in AP mode via WiFiManager");
    } else {
        LW_LOGI("Connected via WiFiManager, IP: %s", WiFi.localIP().toString().c_str());
    }
    return true;
}

bool WebServer::startAPMode() {
    // STA-only: WebServer must never try to start or rely on AP mode.
    // Kept only for legacy callers; return false to surface misuse.
    LW_LOGE("startAPMode is disabled (STA-only). WiFi must be managed by WiFiManager.");
    m_apMode = false;
    return false;
}

void WebServer::setupCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                         "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                         "Content-Type, X-Requested-With");
}

void WebServer::startMDNS() {
    if (MDNS.begin(WebServerConfig::MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WebServerConfig::HTTP_PORT);
        MDNS.addService("ws", "tcp", WebServerConfig::HTTP_PORT);
        MDNS.addServiceTxt("http", "tcp", "version", "2.0.0");
        MDNS.addServiceTxt("http", "tcp", "board", "ESP32-S3");

#if FEATURE_MULTI_DEVICE
        // Add TXT records for multi-device sync discovery
        MDNS.addServiceTxt("ws", "tcp", "board", "ESP32-S3");
        MDNS.addServiceTxt("ws", "tcp", "uuid", DEVICE_UUID.toString());
        MDNS.addServiceTxt("ws", "tcp", "syncver", "1");
        LW_LOGI("Sync UUID: %s", DEVICE_UUID.toString());
#endif

        m_mdnsStarted = true;
        LW_LOGI("mDNS started: http://%s.local", WebServerConfig::MDNS_HOSTNAME);
    } else {
        LW_LOGE("mDNS failed to start");
    }
}

// ============================================================================
// Route Setup
// ============================================================================

void WebServer::setupRoutes() {
    // Create route registry wrapper
    webserver::HttpRouteRegistry registry(m_server);



    // Create WebServer context (m_startTime set later in begin(), use millis() for now)
    webserver::WebServerContext ctx(
        m_actorSystem,
        m_renderer,
        m_zoneComposer,
        m_pluginManager,
        m_rateLimiter,
        m_ledBroadcaster
#if FEATURE_AUDIO_SYNC
        , m_audioBroadcaster
#endif
#if FEATURE_AUDIO_BENCHMARK
        , m_benchmarkBroadcaster
#endif
        , millis()  // Will be updated to m_startTime after server starts
        , m_apMode
    );

    // Register routes using modular registrars
    // Order: V1 first (higher priority), then legacy, then static assets
    webserver::V1ApiRoutes::registerRoutes(
        registry, ctx, this,
        [this](AsyncWebServerRequest* req) { return checkRateLimit(req); },
        [this](AsyncWebServerRequest* req) { return checkAPIKey(req); },
        [this]() { broadcastStatus(); },
        [this]() { broadcastZoneState(); }
    );

    // Legacy API routes removed - all endpoints migrated to V1 API

    webserver::StaticAssetRoutes::registerRoutes(registry);
}

void WebServer::setupWebSocket() {
    // Create WebServer context for gateway
    webserver::WebServerContext ctx(
        m_actorSystem,
        m_renderer,
        m_zoneComposer,
        m_pluginManager,
        m_rateLimiter,
        m_ledBroadcaster
#if FEATURE_AUDIO_SYNC
        , m_audioBroadcaster
#endif
#if FEATURE_AUDIO_BENCHMARK
        , m_benchmarkBroadcaster
#endif
        , m_startTime
        , m_apMode
        , [this]() { broadcastStatus(); }
        , [this]() { broadcastZoneState(); }
        , m_ws
        , [this](AsyncWebSocketClient* client, bool subscribe) { return setLEDStreamSubscription(client, subscribe); }
#if FEATURE_AUDIO_SYNC
        , [this](AsyncWebSocketClient* client, bool subscribe) { return setAudioStreamSubscription(client, subscribe); }
#endif
#if FEATURE_EFFECT_VALIDATION
        , [this](AsyncWebSocketClient* client, bool subscribe) { return setValidationStreamSubscription(client, subscribe); }
#endif
#if FEATURE_AUDIO_BENCHMARK
        , [this](AsyncWebSocketClient* client, bool subscribe) { return setBenchmarkStreamSubscription(client, subscribe); }
#endif
        , [this](const String& action, JsonVariant params) { return executeBatchAction(action, params); }
    );

    // Create WebSocket gateway
    m_wsGateway = new webserver::WsGateway(
        m_ws,
        ctx,
        [this](AsyncWebSocketClient* client) {
            IPAddress clientIP = client->remoteIP();
            if (!m_rateLimiter.checkWebSocket(clientIP)) {
                // Log structured telemetry for rate-limit rejection
                uint32_t tsMonoms = millis();
                char buf[256];
                const int n = snprintf(buf, sizeof(buf),
                    "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"clientId\":%lu,"
                    "\"result\":\"rejected\",\"reason\":\"rate_limit\"}",
                    static_cast<unsigned long>(tsMonoms),
                    static_cast<unsigned long>(client->id())
                );
                if (n > 0 && n < static_cast<int>(sizeof(buf))) {
                    Serial.println(buf);
                }
                
                // Existing error response
                uint32_t retryAfter = m_rateLimiter.getRetryAfterSeconds(clientIP);
                String errorMsg = buildWsRateLimitError(retryAfter);
                client->text(errorMsg);
                return false;
            }
            return true;
        },
        [this](AsyncWebSocketClient* client, JsonDocument& doc) {
            // Auth check (moved from handleWsMessage)
#if FEATURE_API_AUTH
            const char* key = config::NetworkConfig::API_KEY_VALUE;
            if (key != nullptr && key[0] != '\0') {
                if (m_authenticatedClients.find(client->id()) == m_authenticatedClients.end()) {
                    const char* msgType = doc["type"] | "";
                    if (strcmp(msgType, "auth") == 0) {
                        const char* providedKey = doc["apiKey"] | "";
                        if (strcmp(providedKey, key) == 0) {
                            m_authenticatedClients.insert(client->id());
                            client->text("{\"type\":\"auth\",\"success\":true}");
                        } else {
                            client->text(buildWsError(ErrorCodes::UNAUTHORIZED, "Invalid API key").c_str());
                        }
                    } else {
                        client->text("{\"type\":\"error\",\"error\":{\"code\":\"UNAUTHORIZED\",\"message\":\"Authentication required. Send {\\\"type\\\":\\\"auth\\\",\\\"apiKey\\\":\\\"...\\\"}\"}}\n");
                    }
                    return false;
                }
            }
#endif
            return true;
        },
        [this](AsyncWebSocketClient* client) {
            handleWsConnect(client);
        },
        [this](AsyncWebSocketClient* client) {
            handleWsDisconnect(client);
        },
        nullptr  // No fallback handler - all commands are now registered
    );

    // Register WebSocket event handler
    m_ws->onEvent(webserver::WsGateway::onEvent);
    m_server->addHandler(m_ws);
    LW_LOGI("WebSocket handler registered at /ws (max clients: %u)", WebServerConfig::MAX_WS_CLIENTS);

    // Register WS command handlers (Phase 2: modular command registration)
    webserver::ws::registerWsDeviceCommands(ctx);
    webserver::ws::registerWsEffectsCommands(ctx);
    webserver::ws::registerWsZonesCommands(ctx);
    webserver::ws::registerWsTransitionCommands(ctx);
    webserver::ws::registerWsNarrativeCommands(ctx);
    webserver::ws::registerWsMotionCommands(ctx);
    webserver::ws::registerWsColorCommands(ctx);
    webserver::ws::registerWsPaletteCommands(ctx);
    webserver::ws::registerWsBatchCommands(ctx);
#if FEATURE_AUDIO_SYNC
    webserver::ws::registerWsAudioCommands(ctx);
    webserver::ws::registerWsDebugCommands(ctx);
    webserver::ws::registerWsTrinityCommands(ctx);
#endif
    webserver::ws::registerWsStreamCommands(ctx);
    webserver::ws::registerWsPluginCommands(ctx);
}

// ============================================================================
// setupV1Routes() removed - functionality migrated to V1ApiRoutes::registerRoutes()

// ============================================================================
// Handler Methods Migration Summary
// ============================================================================
// All REST API handlers have been moved to dedicated handler classes:
// - ParameterHandlers: handleParametersGet/Set
// - AudioHandlers: handleAudio* (all audio REST endpoints)
// - TransitionHandlers: handleTransitionTypes/Trigger
// - SystemHandlers: handleApiDiscovery, handleHealth, handleOpenApiSpec
// - DeviceHandlers: handleStatus, handleInfo
// - EffectHandlers: handleList, handleCurrent, handleSet, handleMetadata, etc.
// - ZoneHandlers: handleList, handleGet, handleLayout, handleSet*, etc.
// - PaletteHandlers: handleList, handleCurrent, handleSet
// - NarrativeHandlers: handleStatus, handleConfigGet/Set
// - BatchHandlers: handleExecute

bool WebServer::executeBatchAction(const String& action, JsonVariant params) {
    if (action == "setBrightness") {
        if (!params.containsKey("value")) return false;
        m_actorSystem.setBrightness(params["value"]);
        return true;
    }
    else if (action == "setSpeed") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        if (val < 1 || val > 50) return false;
        m_actorSystem.setSpeed(val);
        return true;
    }
    else if (action == "setEffect") {
        if (!params.containsKey("effectId")) return false;
        uint8_t id = params["effectId"];
        if (id >= m_renderer->getEffectCount()) return false;
        m_actorSystem.setEffect(id);
        return true;
    }
    else if (action == "setPalette") {
        if (!params.containsKey("paletteId")) return false;
        m_actorSystem.setPalette(params["paletteId"]);
        return true;
    }
    else if (action == "transition") {
        if (!params.containsKey("toEffect")) return false;
        uint8_t toEffect = params["toEffect"];
        uint8_t type = params["type"] | 0;
        m_renderer->startTransition(toEffect, type);
        return true;
    }
    else if (action == "setZoneEffect" && m_zoneComposer) {
        if (!params.containsKey("zoneId") || !params.containsKey("effectId")) return false;
        uint8_t zoneId = params["zoneId"];
        uint8_t effectId = params["effectId"];
        m_zoneComposer->setZoneEffect(zoneId, effectId);
        return true;
    }

    return false;
}

// ============================================================================
// WebSocket Handlers
// ============================================================================

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    // Delegate to WsGateway (Phase 2: gateway handles all WS events)
    if (webServerInstance && webServerInstance->m_wsGateway) {
        webserver::WsGateway::onEvent(server, client, type, arg, data, len);
    } else {
        // Fallback for legacy path (should not happen after Phase 2 completion)
        switch (type) {
            case WS_EVT_CONNECT:
                if (webServerInstance) webServerInstance->handleWsConnect(client);
                break;
            case WS_EVT_DISCONNECT:
                if (webServerInstance) webServerInstance->handleWsDisconnect(client);
                break;
            case WS_EVT_DATA:
                if (webServerInstance) webServerInstance->handleWsMessage(client, data, len);
                break;
            case WS_EVT_ERROR:
                LW_LOGW("WS: Error from client %u", client->id());
                break;
            case WS_EVT_PONG:
                break;
        }
    }
}

void WebServer::handleWsConnect(AsyncWebSocketClient* client) {
    // SAFETY: Validate m_ws pointer before accessing
    if (!m_ws) {
        LW_LOGW("handleWsConnect: m_ws is null");
        return;
    }
    
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();
    if (m_ws->count() > WebServerConfig::MAX_WS_CLIENTS) {
        LW_LOGW("WS: Max clients reached, rejecting %u", client->id());
        client->close(1008, "Connection limit");
        return;
    }

    LW_LOGI("WS: Client %u connected from %s", client->id(), client->remoteIP().toString().c_str());

    // QUEUE PROTECTION: Defer initial broadcasts to prevent queue saturation on connect
    // Instead of broadcasting immediately (which can queue messages for all clients),
    // we defer to the update() loop which has proper throttling
    // This prevents "Too many messages queued" errors when multiple clients connect rapidly
    m_broadcastPending = true;  // Defer status broadcast
    // Zone state will be sent on next update() tick if needed
}

void WebServer::handleWsDisconnect(AsyncWebSocketClient* client) {
    uint32_t clientId = client->id();
    LW_LOGI("WS: Client %u disconnected", clientId);

    // Cleanup LED stream subscription
    setLEDStreamSubscription(client, false);

#if FEATURE_API_AUTH
    // Cleanup authenticated client tracking
    m_authenticatedClients.erase(clientId);
#endif
}

void WebServer::handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Delegate to WsGateway (Phase 2: gateway handles rate limiting, parsing, auth, routing)
    if (m_wsGateway) {
        m_wsGateway->handleMessage(client, data, len);
    } else {
        // Fallback if gateway not initialized (should not happen)
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Gateway not initialized"));
    }
}

// processWsCommand has been removed - all commands are now handled by modular command handlers
// This function previously contained ~2400 LOC which has been extracted to:
// - WsEffectsCommands, WsZonesCommands, WsAudioCommands, WsStreamCommands,
// - WsTransitionCommands, WsNarrativeCommands, WsMotionCommands,
// - WsColorCommands, WsPaletteCommands, WsBatchCommands
//
// All commands are now registered via WsCommandRouter in the respective command modules.

// ============================================================================
// Broadcasting
// ============================================================================

void WebServer::broadcastStatus() {
    // SAFETY: Always defer broadcasts to avoid calling doBroadcastStatus() from within
    // AsyncTCP/AsyncWebServer callback context. This prevents re-entrancy issues and
    // corrupted pointer access when accessing m_renderer (Core 1) or m_ws from Core 0.
    // All broadcasts are processed in update() which runs in a safe context.
    // 
    // QUEUE PROTECTION: Setting m_broadcastPending is idempotent - multiple calls
    // before the next update() tick will only result in one broadcast, preventing
    // queue buildup from rapid API calls.
    m_broadcastPending = true;
}

void WebServer::doBroadcastStatus() {
    // SAFETY CHECKS: Validate pointers and state before accessing
    if (!m_ws) {
        LW_LOGW("doBroadcastStatus: m_ws is null");
        return;
    }
    
    if (m_ws->count() == 0) return;

    // Cleanup disconnected clients before broadcasting
    m_ws->cleanupClients();
    if (m_ws->count() == 0) return;

    // QUEUE PROTECTION: Conservative approach - limit broadcast frequency
    // AsyncWebSocket has internal queue limits, but we can't check them directly
    // Instead, we use time-based throttling to prevent queue saturation
    static uint32_t lastBroadcastAttempt = 0;
    uint32_t now = millis();
    if (now - lastBroadcastAttempt < 10) {  // Minimum 10ms between broadcast attempts
        return;  // Skip this broadcast to prevent queue buildup
    }
    lastBroadcastAttempt = now;

    StaticJsonDocument<512> doc;
    doc["type"] = "status";

    // SAFE: Use cached state instead of unsafe cross-core access
    const CachedRendererState& cached = m_cachedRendererState;
    doc["effectId"] = cached.currentEffect;
    if (cached.currentEffect < cached.effectCount && cached.effectNames[cached.currentEffect]) {
        doc["effectName"] = cached.effectNames[cached.currentEffect];
    }
    doc["brightness"] = cached.brightness;
    doc["speed"] = cached.speed;
    doc["paletteId"] = cached.paletteIndex;
    doc["paletteName"] = MasterPaletteNames[cached.paletteIndex];
    doc["hue"] = cached.hue;
    doc["intensity"] = cached.intensity;
    doc["saturation"] = cached.saturation;
    doc["complexity"] = cached.complexity;
    doc["variation"] = cached.variation;
    doc["fps"] = cached.stats.currentFPS;
    doc["cpuPercent"] = cached.stats.cpuPercent;

    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    String output;
    serializeJson(doc, output);
    
    // SAFETY: Validate m_ws is still valid before calling textAll()
    // Note: AsyncWebSocket will handle queue overflow internally, but we throttle to prevent it
    if (m_ws && m_ws->count() > 0) {
        m_ws->textAll(output);
    }
}

void WebServer::broadcastZoneState() {
    // SAFETY: Validate pointers before accessing
    if (!m_ws) {
        LW_LOGW("broadcastZoneState: m_ws is null");
        return;
    }
    
    if (m_ws->count() == 0 || !m_zoneComposer) return;

    StaticJsonDocument<1024> doc;  // Increased size for additional fields
    doc["type"] = "zones.list";  // Changed from "zone.state"
    doc["enabled"] = m_zoneComposer->isEnabled();
    doc["zoneCount"] = m_zoneComposer->getZoneCount();

    // Include segment definitions
    JsonArray segmentsArray = doc["segments"].to<JsonArray>();
    const ZoneSegment* segments = m_zoneComposer->getZoneConfig();
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject seg = segmentsArray.add<JsonObject>();
        seg["zoneId"] = segments[i].zoneId;
        seg["s1LeftStart"] = segments[i].s1LeftStart;
        seg["s1LeftEnd"] = segments[i].s1LeftEnd;
        seg["s1RightStart"] = segments[i].s1RightStart;
        seg["s1RightEnd"] = segments[i].s1RightEnd;
        seg["totalLeds"] = segments[i].totalLeds;
    }

    JsonArray zones = doc["zones"].to<JsonArray>();
    for (uint8_t i = 0; i < m_zoneComposer->getZoneCount(); i++) {
        JsonObject zone = zones.add<JsonObject>();
        zone["id"] = i;
        zone["enabled"] = m_zoneComposer->isZoneEnabled(i);
        uint8_t effectId = m_zoneComposer->getZoneEffect(i);
        zone["effectId"] = effectId;
        // SAFE: Use cached state instead of unsafe cross-core access
        const CachedRendererState& cached = m_cachedRendererState;
        if (effectId < cached.effectCount && cached.effectNames[effectId]) {
            zone["effectName"] = cached.effectNames[effectId];
        }
        zone["brightness"] = m_zoneComposer->getZoneBrightness(i);
        zone["speed"] = m_zoneComposer->getZoneSpeed(i);
        zone["paletteId"] = m_zoneComposer->getZonePalette(i);
        zone["blendMode"] = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(i));
        zone["blendModeName"] = getBlendModeName(m_zoneComposer->getZoneBlendMode(i));
    }

    // Add presets array
    JsonArray presets = doc["presets"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        JsonObject preset = presets.add<JsonObject>();
        preset["id"] = i;
        preset["name"] = ZoneComposer::getPresetName(i);
    }

    String output;
    serializeJson(doc, output);
    
    // QUEUE PROTECTION: Throttle zone broadcasts to prevent queue saturation
    static uint32_t lastZoneBroadcastAttempt = 0;
    uint32_t now = millis();
    if (now - lastZoneBroadcastAttempt < 10) {  // Minimum 10ms between zone broadcasts
        return;  // Skip this broadcast to prevent queue buildup
    }
    lastZoneBroadcastAttempt = now;
    
    // SAFETY: Validate m_ws is still valid before calling textAll()
    if (m_ws && m_ws->count() > 0) {
        m_ws->textAll(output);
    }
}

void WebServer::notifyEffectChange(uint8_t effectId, const char* name) {
    // SAFETY: Validate pointers before accessing
    if (!m_ws) {
        LW_LOGW("notifyEffectChange: m_ws is null");
        return;
    }
    
    if (m_ws->count() == 0) return;

    // QUEUE PROTECTION: Throttle notifications to prevent queue saturation
    static uint32_t lastEffectNotifyAttempt = 0;
    uint32_t now = millis();
    if (now - lastEffectNotifyAttempt < 10) {  // Minimum 10ms between notifications
        return;  // Skip this notification to prevent queue buildup
    }
    lastEffectNotifyAttempt = now;

    StaticJsonDocument<512> doc;
    doc["type"] = "effectChanged";
    doc["effectId"] = effectId;
    doc["name"] = name;

    String output;
    serializeJson(doc, output);
    
    // SAFETY: Validate m_ws is still valid before calling textAll()
    if (m_ws && m_ws->count() > 0) {
        m_ws->textAll(output);
    }
}

void WebServer::notifyParameterChange() {
    broadcastStatus();
}

// ============================================================================
// LED Frame Streaming
// ============================================================================

void WebServer::broadcastLEDFrame() {
    if (!m_ledBroadcaster || !m_renderer) return;
    
    // Get LED buffer from renderer (cross-core safe copy)
    CRGB leds[webserver::LedStreamConfig::TOTAL_LEDS];
    m_renderer->getBufferCopy(leds);
    
    // Broadcast to subscribed clients (throttling handled internally)
    m_ledBroadcaster->broadcast(leds);
}

bool WebServer::setLEDStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_ledBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_ledBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to LED stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from LED stream", clientId);
    }

    return success;
}

bool WebServer::hasLEDStreamSubscribers() const {
    return m_ledBroadcaster && m_ledBroadcaster->hasSubscribers();
}

#if FEATURE_AUDIO_SYNC
// ============================================================================
// Audio Frame Streaming
// ============================================================================

void WebServer::broadcastAudioFrame() {
    if (!m_audioBroadcaster || !m_renderer) return;

    // Get audio frame from renderer (cross-core safe - returns by-value copy)
    const audio::ControlBusFrame& frame = m_renderer->getCachedAudioFrame();
    const audio::MusicalGridSnapshot& grid = m_renderer->getLastMusicalGrid();

    // Broadcast to subscribed clients (throttling handled internally)
    m_audioBroadcaster->broadcast(frame, grid);
}

void WebServer::broadcastBeatEvent() {
    // SAFETY: Validate pointers before accessing
    if (!m_ws || m_ws->count() == 0) return;
    
    // QUEUE PROTECTION: Throttle beat events to prevent queue saturation
    static uint32_t lastBeatBroadcastAttempt = 0;
    uint32_t now = millis();
    if (now - lastBeatBroadcastAttempt < 50) {  // Minimum 50ms between beat broadcasts (20 Hz max)
        return;  // Skip this broadcast to prevent queue buildup
    }
    lastBeatBroadcastAttempt = now;
    
    // Note: We need to get musical grid from cached state or another safe source
    // For now, we'll skip if renderer is not available (this should use cached audio state)
    if (!m_renderer) return;
    
    const auto& grid = m_renderer->getLastMusicalGrid();

    // Only broadcast on actual beat/downbeat (single-frame pulses)
    if (!grid.beat_tick && !grid.downbeat_tick) return;

    StaticJsonDocument<512> doc;
    doc["type"] = "beat.event";
    doc["tick"] = grid.beat_tick;
    doc["downbeat"] = grid.downbeat_tick;
    doc["beat_index"] = (uint32_t)(grid.beat_index & 0xFFFFFFFF);
    doc["bar_index"] = (uint32_t)(grid.bar_index & 0xFFFFFFFF);
    doc["beat_in_bar"] = grid.beat_in_bar;
    doc["beat_phase"] = grid.beat_phase01;
    doc["bpm"] = grid.bpm_smoothed;
    doc["confidence"] = grid.tempo_confidence;

    String json;
    serializeJson(doc, json);
    
    // SAFETY: Validate m_ws is still valid before calling textAll()
    if (m_ws) {
        m_ws->textAll(json);
    }
}

bool WebServer::setAudioStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_audioBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_audioBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to audio stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from audio stream", clientId);
    }

    return success;
}

bool WebServer::hasAudioStreamSubscribers() const {
    return m_audioBroadcaster && m_audioBroadcaster->hasSubscribers();
}
#endif

#if FEATURE_AUDIO_BENCHMARK
// ============================================================================
// Benchmark Metrics Streaming
// ============================================================================

void WebServer::broadcastBenchmarkStats() {
    if (!m_benchmarkBroadcaster) return;

    // Get AudioActor to retrieve benchmark stats
    auto* audio = m_actorSystem.getAudio();
    if (!audio) return;

    // Get stats (returns copy - thread safe)
    const audio::AudioBenchmarkStats& stats = audio->getBenchmarkStats();

    // Broadcast to subscribed clients (throttling handled internally at 10 Hz)
    m_benchmarkBroadcaster->broadcastCompact(stats);
}

bool WebServer::setBenchmarkStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_benchmarkBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_benchmarkBroadcaster->setSubscription(clientId, subscribe);

    if (subscribe && success) {
        LW_LOGD("Client %u subscribed to benchmark stream", clientId);
    } else if (!subscribe) {
        LW_LOGD("Client %u unsubscribed from benchmark stream", clientId);
    }

    return success;
}

bool WebServer::hasBenchmarkStreamSubscribers() const {
    return m_benchmarkBroadcaster && m_benchmarkBroadcaster->hasSubscribers();
}
#endif

// ============================================================================
// Rate Limiting
// ============================================================================

bool WebServer::checkRateLimit(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    if (!m_rateLimiter.checkHTTP(clientIP)) {
        uint32_t retryAfter = m_rateLimiter.getRetryAfterSeconds(clientIP);
        sendRateLimitError(request, retryAfter);
        return false;
    }
    return true;
}

bool WebServer::checkWsRateLimit(AsyncWebSocketClient* client) {
    IPAddress clientIP = client->remoteIP();
    return m_rateLimiter.checkWebSocket(clientIP);
}

bool WebServer::checkAPIKey(AsyncWebServerRequest* request) {
#if FEATURE_API_AUTH
    const char* key = config::NetworkConfig::API_KEY_VALUE;
    // If no API key configured, auth is disabled
    if (key == nullptr || key[0] == '\0') {
        return true;
    }

    if (!request->hasHeader("X-API-Key")) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Missing X-API-Key header");
        return false;
    }

    if (request->header("X-API-Key") != key) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Invalid API key");
        return false;
    }
#endif
    return true;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
