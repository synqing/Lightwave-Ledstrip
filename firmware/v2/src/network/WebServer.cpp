/**
 * @file WebServer.cpp
 * @brief Web Server implementation for LightwaveOS v2
 *
 * Implements REST API and WebSocket server integrated with Actor System.
 * All state changes are routed through m_orchestrator for thread safety.
 */

#include "WebServer.h"

#if FEATURE_WEB_SERVER

#define LW_LOG_TAG "WebServer"
#include "utils/Log.h"

#include "webserver/LogStreamBroadcaster.h"
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
#include "webserver/UdpStreamer.h"
#include "webserver/WsCommandRouter.h"
#include "webserver/ws/WsDeviceCommands.h"
#include "webserver/ws/WsFilesystemCommands.h"
#include "webserver/ws/WsEffectsCommands.h"
#include "webserver/ws/WsZonesCommands.h"
#include "webserver/ws/WsTransitionCommands.h"
#include "webserver/ws/WsNarrativeCommands.h"
#include "webserver/ws/WsMotionCommands.h"
#include "webserver/ws/WsColorCommands.h"
#include "webserver/ws/WsPaletteCommands.h"
#include "webserver/ws/WsPresetCommands.h"
#include "webserver/ws/WsZonePresetCommands.h"
#include "webserver/ws/WsBatchCommands.h"
#if FEATURE_AUDIO_SYNC
#include "webserver/ws/WsAudioCommands.h"
#include "webserver/ws/WsDebugCommands.h"
#endif
#include "webserver/ws/WsStreamCommands.h"
#include "webserver/ws/WsModifierCommands.h"
#if FEATURE_API_AUTH
#include "webserver/ws/WsAuthCommands.h"
#endif
#include "webserver/ws/WsSysCommands.h"
#include "webserver/ws/WsTrinityCommands.h"
#include "webserver/ws/WsOtaCommands.h"
#include "webserver/ws/WsPluginCommands.h"
#include "../config/network_config.h"
#include "../core/actors/NodeOrchestrator.h"
#include "../effects/zones/ZoneDefinition.h"
#include <Update.h>
#include "../core/actors/RendererNode.h"
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
#include "../audio/AudioNode.h"
#include "../audio/contracts/ControlBus.h"
#include "../audio/tempo/TempoTracker.h"  // For TempoTrackerOutput
#include <cmath>
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

using namespace lightwaveos::nodes;
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

WebServer::WebServer(NodeOrchestrator& orchestrator, RendererNode* renderer)
    : m_server(nullptr)
    , m_ws(nullptr)
    , m_running(false)
    , m_apMode(false)
    , m_mdnsStarted(false)
    , m_littleFSMounted(false)
    , m_lastBroadcast(0)
    , m_startTime(0)
    , m_lastImmediateBroadcast(0)
    , m_broadcastPending(false)
    , m_zoneComposer(nullptr)
    , m_lastStateCacheUpdate(0)
    , m_ledBroadcaster(nullptr)
    , m_udpStreamer(nullptr)
    , m_logBroadcaster(nullptr)
    , m_wsGateway(nullptr)
    , m_orchestrator(orchestrator)
    , m_renderer(renderer)
{
}

WebServer::~WebServer() {
    stop();

    // TODO: Implement logging callback system for WebSocket log streaming
    // clearLogCallback() would unregister the WebSocket log forwarder here
    // lightwaveos::logging::clearLogCallback();

    delete m_wsGateway;
#if FEATURE_AUDIO_BENCHMARK
    delete m_benchmarkBroadcaster;
#endif
#if FEATURE_AUDIO_SYNC
    delete m_audioBroadcaster;
#endif
    delete m_logBroadcaster;
    delete m_udpStreamer;
    delete m_ledBroadcaster;
    delete m_ws;
    delete m_server;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool WebServer::begin() {
    // CRITICAL: Guard against calling begin() multiple times
    // If already running, stop and clean up first (prevents memory leak and port conflict)
    if (m_running) {
        LW_LOGW("WebServer::begin() called when already running - stopping first");
        stop();  // Clean up existing instance
        delay(500);  // Give time for cleanup
    }
    
    // CRITICAL: Verify previous instance was cleaned up
    if (m_server != nullptr) {
        LW_LOGE("CRITICAL: m_server is not nullptr! Memory leak or double-initialization!");
        delete m_server;  // Clean up orphaned instance
        m_server = nullptr;
    }
    if (m_ws != nullptr) {
        LW_LOGE("CRITICAL: m_ws is not nullptr! Memory leak or double-initialization!");
        delete m_ws;  // Clean up orphaned instance
        m_ws = nullptr;
    }
    
    LW_LOGI("Starting v2 WebServer...");

    // Initialize LittleFS for static file serving
    m_littleFSMounted = LittleFS.begin(false);
    if (!m_littleFSMounted) {
        LW_LOGW("LittleFS mount failed - preset saves will not be available");
    } else {
        LW_LOGI("LittleFS mounted");
    }

    // Create server instances
    m_server = new AsyncWebServer(WebServerConfig::HTTP_PORT);
    m_ws = new AsyncWebSocket("/ws");

    // Create log stream broadcaster (wireless serial monitoring) - always created
    m_logBroadcaster = new webserver::LogStreamBroadcaster(m_ws);

#ifdef BOARD_HAS_PSRAM
    // LED and audio stream broadcasters use non-trivial buffers; skip on no-PSRAM (FH4) to avoid OOM
    m_ledBroadcaster = new webserver::LedStreamBroadcaster(m_ws, WebServerConfig::MAX_WS_CLIENTS);

    // UDP streamer for low-latency LED/audio frames (bypasses TCP backpressure)
    m_udpStreamer = new webserver::UdpStreamer();
    if (m_udpStreamer->begin()) {
        LW_LOGI("UDP streamer initialised");
    } else {
        LW_LOGW("UDP streamer failed to initialise");
        delete m_udpStreamer;
        m_udpStreamer = nullptr;
    }
#endif

    // TODO: Implement logging callback system for WebSocket log streaming
    // setLogCallback() would register a callback to forward logs to WebSocket subscribers:
    // lightwaveos::logging::setLogCallback([this](const char* formattedLine) {
    //     if (m_logBroadcaster) {
    //         m_logBroadcaster->broadcastLine(formattedLine);
    //     }
    // });
    LW_LOGI("WebSocket log streaming not yet implemented - requires logging callback system");

#if FEATURE_AUDIO_SYNC && defined(BOARD_HAS_PSRAM)
    // Create audio stream broadcaster (skip on no-PSRAM to save RAM)
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

#if FEATURE_API_AUTH
    // Initialize API key manager (NVS persistence)
    if (!m_apiKeyManager.begin()) {
        LW_LOGW("ApiKeyManager initialization failed - using compile-time default key");
    }
#endif

    // WiFi state is owned by WiFiManager; it may be STA or AP depending on build
    // (AP-only builds force AP, standard builds start in STA and fall back to AP)
    // Check current WiFi state
    if (WIFI_MANAGER.isAPMode()) {
        LW_LOGI("WiFi in AP mode via WiFiManager");
        m_apMode = true;
    } else if (WIFI_MANAGER.isConnected()) {
        LW_LOGI("WiFi connected via WiFiManager, IP: %s", WiFi.localIP().toString().c_str());
        m_apMode = false;
    } else {
        // Default to AP mode if state is unclear
        LW_LOGW("WiFi state unclear, defaulting to AP mode");
        m_apMode = true;
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

    // AP mode: AP IP (192.168.4.1) is available immediately
    // No need for IP validation or delays in AP mode
    IPAddress apIP = WiFi.softAPIP();
    LW_LOGI("Starting AsyncWebServer on port %d (AP IP: %s)...", 
            WebServerConfig::HTTP_PORT, apIP.toString().c_str());

    // Start the server (simple startup like commit 937c9abc)
    m_server->begin();
    m_running = true;

    // Get zone composer reference if available
    if (m_renderer) {
        m_zoneComposer = m_renderer->getZoneComposer();
    }

    // Wire up zone state change callback for real-time WebSocket broadcasts
    if (m_zoneComposer) {
        m_zoneComposer->setStateChangeCallback([this](uint8_t zoneId) {
            broadcastSingleZoneState(zoneId);
        });
        LW_LOGI("Zone state callback registered");
    }

    LW_LOGI("Server running on port %d", WebServerConfig::HTTP_PORT);
    if (m_apMode) {
        LW_LOGI("AP mode - IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        // STA mode (user-initiated connection)
        IPAddress ip = WiFi.localIP();
        if (ip != INADDR_NONE && ip != IPAddress(0, 0, 0, 0)) {
            LW_LOGI("STA mode - IP: %s", ip.toString().c_str());
        } else {
            LW_LOGW("STA mode but IP not assigned, check WiFiManager status");
        }
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

bool WebServer::mountLittleFS() {
    if (m_littleFSMounted) {
        LW_LOGW("LittleFS already mounted");
        return true;
    }
    
    m_littleFSMounted = LittleFS.begin(false);
    if (m_littleFSMounted) {
        LW_LOGI("LittleFS mounted successfully");
    } else {
        LW_LOGE("LittleFS mount failed");
    }
    return m_littleFSMounted;
}

bool WebServer::unmountLittleFS() {
    if (!m_littleFSMounted) {
        LW_LOGW("LittleFS not mounted");
        return true;
    }
    
    // Safety check: Don't unmount if WebServer is running (files may be in use)
    if (m_running) {
        LW_LOGW("Cannot unmount LittleFS while WebServer is running");
        return false;
    }
    
    LittleFS.end();
    m_littleFSMounted = false;
    LW_LOGI("LittleFS unmounted");
    return true;
}

#if FEATURE_API_AUTH
bool WebServer::isClientAuthenticated(uint32_t clientId) const {
    return m_authenticatedClients.find(clientId) != m_authenticatedClients.end();
}
#endif

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

    // UDP recovery service loop (runs regardless of subscriber activity)
    if (m_udpStreamer) {
        m_udpStreamer->service();
    }

    // Restart UDP streamer on WiFi reconnects (keeps socket state fresh)
    if (m_udpStreamer) {
        static bool udpSuspendedForWifi = false;
        static uint32_t lastWifiReconnectRequestMs = 0;
        static uint32_t lastUdpRebootMs = 0;
        bool networkUp = WIFI_MANAGER.isConnected() || WIFI_MANAGER.isAPMode();
        if (!networkUp && !udpSuspendedForWifi) {
            m_udpStreamer->stop();
            udpSuspendedForWifi = true;
            LW_LOGW("UDP streamer suspended (network down)");
        } else if (networkUp && udpSuspendedForWifi) {
            if (m_udpStreamer->begin()) {
                LW_LOGI("UDP streamer restarted after reconnect");
                udpSuspendedForWifi = false;
            } else {
                LW_LOGW("UDP streamer restart failed (will retry)");
            }
        }

        // If WiFi is up but UDP is in a persistent failure state, force a WiFi reconnect.
        // Skip in AP+STA mode — reconnecting STA would disrupt AP clients (Tab5, etc).
        if (networkUp && !WIFI_MANAGER.isAPMode() && WiFi.getMode() != WIFI_MODE_APSTA) {
            webserver::UdpStreamer::UdpStats st;
            m_udpStreamer->getStats(st);
            uint32_t now = millis();
            uint32_t lastFailAgo = st.lastFailureMs > 0 ? (now - st.lastFailureMs) : 0;

            // Only escalate while failures are current (not historical).
            if (st.consecutiveFailures >= 6 && lastFailAgo < 5000) {
                if (now - lastWifiReconnectRequestMs > 15000) {
                    LW_LOGW("UDP: requesting WiFi reconnect (consecutiveFailures=%u)", static_cast<unsigned>(st.consecutiveFailures));
                    WIFI_MANAGER.reconnect();
                    lastWifiReconnectRequestMs = now;
                }
            }

            // Absolute last resort: reboot if the system is stuck in a failure loop.
            if (st.consecutiveFailures >= 12 && lastFailAgo < 5000) {
                if (now - lastUdpRebootMs > 60000) {
                    LW_LOGE("UDP: unrecoverable failure state, rebooting (consecutiveFailures=%u)", static_cast<unsigned>(st.consecutiveFailures));
                    lastUdpRebootMs = now;
                    ESP.restart();
                }
            }
        }
    }

    // LED frame streaming to subscribed clients (20 FPS)
    broadcastLEDFrame();

    // UDP streaming to subscribed clients (bypasses TCP backpressure)
    if (m_udpStreamer && m_renderer) {
        CRGB udpLeds[webserver::LedStreamConfig::TOTAL_LEDS];
        m_renderer->getBufferCopy(udpLeds);
        m_udpStreamer->sendLedFrame(udpLeds);

#if FEATURE_AUDIO_SYNC
        const audio::ControlBusFrame& frame = m_renderer->getCachedAudioFrame();
        const audio::MusicalGridSnapshot& grid = m_renderer->getLastMusicalGrid();
        m_udpStreamer->sendAudioFrame(frame, grid);
#endif
    }

#if FEATURE_AUDIO_SYNC
    // Audio frame streaming to subscribed clients (30 FPS)
    broadcastAudioFrame();

    // FFT frame streaming to subscribed clients (31 Hz)
    broadcastFftFrame();

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
    
    // Re-register mDNS if IP changed (e.g. after WiFi reconnect with new DHCP lease)
    if (m_mdnsStarted) {
        IPAddress currentIP = WiFi.localIP();
        if (currentIP != m_lastRegisteredIP && currentIP != INADDR_NONE && currentIP != IPAddress(0, 0, 0, 0)) {
            MDNS.end();
            if (MDNS.begin(WebServerConfig::MDNS_HOSTNAME)) {
                MDNS.addService("http", "tcp", WebServerConfig::HTTP_PORT);
                MDNS.addService("ws", "tcp", WebServerConfig::HTTP_PORT);
                m_lastRegisteredIP = currentIP;
                LW_LOGI("[MDNS] Re-registered %s.local at %s", WebServerConfig::MDNS_HOSTNAME, currentIP.toString().c_str());
            }
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
    if (count > 102) count = 102;  // Safety: MAX_EFFECTS
    for (uint8_t i = 0; i < count; ++i) {
        m_cachedRendererState.effectNames[i] = m_renderer->getEffectName(i);
    }
    // Clear remaining slots
    for (uint8_t i = count; i < 96; ++i) {
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

// initWiFi() removed - WiFi state is checked inline in begin()

// startAPMode() removed - AP is always managed by WiFiManager

void WebServer::setupCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                         "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                         "Content-Type, X-Requested-With");
}

void WebServer::startMDNS() {
    LW_LOGI("Starting mDNS service...");
    LW_LOGI("  Hostname: %s", WebServerConfig::MDNS_HOSTNAME);
    
    // Use AP IP when in AP mode, STA IP when connected
    IPAddress ip = m_apMode ? WiFi.softAPIP() : WiFi.localIP();
    LW_LOGI("  IP Address: %s", ip.toString().c_str());
    LW_LOGI("  WiFi Mode: %s", WiFi.getMode() == WIFI_MODE_AP ? "AP" :
                               WiFi.getMode() == WIFI_MODE_STA ? "STA" : "UNKNOWN");
    
    // AP IP (192.168.4.1) is valid immediately - no need for validation
    if (MDNS.begin(WebServerConfig::MDNS_HOSTNAME)) {
        LW_LOGI("  mDNS.begin() succeeded");
        
        bool httpOk = MDNS.addService("http", "tcp", WebServerConfig::HTTP_PORT);
        bool wsOk = MDNS.addService("ws", "tcp", WebServerConfig::HTTP_PORT);
        LW_LOGI("  Service registration: http=%s ws=%s", httpOk ? "OK" : "FAIL", wsOk ? "OK" : "FAIL");
        
        MDNS.addServiceTxt("http", "tcp", "version", "2.0.0");
        MDNS.addServiceTxt("http", "tcp", "board", "ESP32-S3");

#if FEATURE_MULTI_DEVICE
        // Add TXT records for multi-device sync discovery
        MDNS.addServiceTxt("ws", "tcp", "board", "ESP32-S3");
        MDNS.addServiceTxt("ws", "tcp", "uuid", DEVICE_UUID.toString());
        MDNS.addServiceTxt("ws", "tcp", "syncver", "1");
        LW_LOGI("  Sync UUID: %s", DEVICE_UUID.toString());
#endif

        m_mdnsStarted = true;
        m_lastRegisteredIP = WiFi.localIP();
        LW_LOGI("mDNS started successfully: http://%s.local", WebServerConfig::MDNS_HOSTNAME);
        LW_LOGI("  WebSocket: ws://%s.local:%d/ws", WebServerConfig::MDNS_HOSTNAME, WebServerConfig::HTTP_PORT);
    } else {
        LW_LOGE("mDNS failed to start");
        m_mdnsStarted = false;
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
        m_orchestrator,
        m_renderer,
        m_zoneComposer,
        this,
        m_rateLimiter,
        m_ledBroadcaster,
        m_logBroadcaster
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
        m_orchestrator,
        m_renderer,
        m_zoneComposer,
        this,
        m_rateLimiter,
        m_ledBroadcaster,
        m_logBroadcaster
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
        , [this](AsyncWebSocketClient* client, bool subscribe) { return setLogStreamSubscription(client, subscribe); }
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
        , m_udpStreamer
    );

    // Create WebSocket gateway
    m_wsGateway = new webserver::WsGateway(
        m_ws,
        ctx,
        [this](AsyncWebSocketClient* client) {
            IPAddress clientIP = client->remoteIP();
            if (!m_rateLimiter.checkWebSocket(clientIP)) {
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
            // Get current API key from ApiKeyManager (NVS or compile-time default)
            String currentKey = m_apiKeyManager.getKey();
            if (currentKey.length() > 0) {
                IPAddress clientIP = client->remoteIP();

                // Check if IP is blocked due to too many failed attempts
                if (m_authRateLimiter.isBlocked(clientIP)) {
                    uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
                    client->text(buildWsAuthRateLimitError(retryAfter));
                    return false;
                }

                if (m_authenticatedClients.find(client->id()) == m_authenticatedClients.end()) {
                    const char* msgType = doc["type"] | "";
                    if (strcmp(msgType, "auth") == 0) {
                        const char* providedKey = doc["apiKey"] | "";
                        // Use ApiKeyManager's constant-time validation
                        if (m_apiKeyManager.validateKey(String(providedKey))) {
                            m_authenticatedClients.insert(client->id());
                            m_authRateLimiter.recordSuccess(clientIP);
                            client->text("{\"type\":\"auth\",\"success\":true}");
                        } else {
                            // Record failure and check if now blocked
                            bool nowBlocked = m_authRateLimiter.recordFailure(clientIP);
                            if (nowBlocked) {
                                uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
                                client->text(buildWsAuthRateLimitError(retryAfter));
                            } else {
                                client->text(buildWsError(ErrorCodes::UNAUTHORIZED, "Invalid API key").c_str());
                            }
                        }
                    } else {
                        // Non-auth message from unauthenticated client - also counts as failure
                        m_authRateLimiter.recordFailure(clientIP);
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
    webserver::ws::registerWsFilesystemCommands(ctx);
    webserver::ws::registerWsEffectsCommands(ctx);
    webserver::ws::registerWsZonesCommands(ctx);
    webserver::ws::registerWsTransitionCommands(ctx);
    webserver::ws::registerWsNarrativeCommands(ctx);
    webserver::ws::registerWsMotionCommands(ctx);
    webserver::ws::registerWsColorCommands(ctx);
    webserver::ws::registerWsPaletteCommands(ctx);
    webserver::ws::registerWsPresetCommands(ctx);
    webserver::ws::registerWsZonePresetCommands(ctx);
    webserver::ws::registerWsBatchCommands(ctx);
#if FEATURE_AUDIO_SYNC
    webserver::ws::registerWsAudioCommands(ctx);
#endif
    webserver::ws::registerWsDebugCommands(ctx);
    webserver::ws::registerWsStreamCommands(ctx);
    webserver::ws::registerWsModifierCommands(ctx);
#if FEATURE_API_AUTH
    webserver::ws::registerWsAuthCommands(ctx);
#endif
    webserver::ws::registerWsSysCommands(ctx);
    webserver::ws::registerWsTrinityCommands(ctx);
    webserver::ws::registerWsOtaCommands(ctx);
    webserver::ws::registerWsPluginCommands(ctx);

    // Log handler registration summary
    size_t handlerCount = webserver::WsCommandRouter::getHandlerCount();
    size_t maxHandlers = webserver::WsCommandRouter::getMaxHandlers();
    LW_LOGI("WebSocket commands registered: %zu/%zu handlers", handlerCount, maxHandlers);
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
        m_orchestrator.setBrightness(params["value"]);
        return true;
    }
    else if (action == "setSpeed") {
        if (!params.containsKey("value")) return false;
        uint8_t val = params["value"];
        if (val < 1 || val > 50) return false;
        m_orchestrator.setSpeed(val);
        return true;
    }
    else if (action == "setEffect") {
        if (!params.containsKey("effectId")) return false;
        uint8_t id = params["effectId"];
        if (id >= m_renderer->getEffectCount()) return false;
        m_orchestrator.setEffect(id);
        return true;
    }
    else if (action == "setPalette") {
        if (!params.containsKey("paletteId")) return false;
        m_orchestrator.setPalette(params["paletteId"]);
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

    // Cleanup UDP stream subscriptions
    if (m_udpStreamer) {
        m_udpStreamer->removeSubscriber(client->remoteIP());
    }

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

#if FEATURE_AUDIO_SYNC
/**
 * @brief Format chord state to musical key string (e.g., "C", "Am", "Dm")
 * 
 * @param rootNote 0-11 (C=0, C#=1, ..., B=11)
 * @param type Chord type (MAJOR, MINOR, DIMINISHED, AUGMENTED)
 * @return Formatted key name string
 */
static const char* formatKeyName(uint8_t rootNote, audio::ChordType type) {
    static const char* NOTE_NAMES[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    
    if (rootNote >= 12) rootNote = rootNote % 12;
    const char* note = NOTE_NAMES[rootNote];
    
    switch (type) {
        case audio::ChordType::MAJOR:
            return note;  // "C", "D", etc.
        case audio::ChordType::MINOR:
            // Format as "Am", "Dm", etc.
            static char minorKey[4];
            snprintf(minorKey, sizeof(minorKey), "%sm", note);
            return minorKey;
        case audio::ChordType::DIMINISHED:
            static char dimKey[5];
            snprintf(dimKey, sizeof(dimKey), "%sdim", note);
            return dimKey;
        case audio::ChordType::AUGMENTED:
            static char augKey[5];
            snprintf(augKey, sizeof(augKey), "%saug", note);
            return augKey;
        default:
            return note;  // Fallback to root note name
    }
}
#endif

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

    JsonDocument doc;  // Increased to accommodate audio metrics
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
    doc["hue"] = cached.hue;
    doc["intensity"] = cached.intensity;
    doc["saturation"] = cached.saturation;
    doc["complexity"] = cached.complexity;
    doc["variation"] = cached.variation;
    doc["fps"] = cached.stats.currentFPS;
    doc["cpuPercent"] = cached.stats.cpuPercent;

    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

#if FEATURE_AUDIO_SYNC
    // Add audio metrics (BPM, KEY, MIC)
    auto* audio = m_orchestrator.getAudio();
    if (audio) {
        if (m_orchestrator.getRenderer() != nullptr) {
            doc["audioSyncMode"] = m_orchestrator.getRenderer()->getAudioSyncMode();
        }
#if FEATURE_AUDIO_BACKEND_ESV11
        audio::ControlBusFrame frame{};
        audio->getControlBusBuffer().ReadLatest(frame);

        doc["bpm"] = frame.es_bpm;

        // Mic level in dB (approx; ES backend publishes mapped RMS 0..1)
        float micLevelDb = (frame.rms > 0.0001f)
            ? (20.0f * log10f(frame.rms))
            : -80.0f;
        doc["mic"] = micLevelDb;

        const audio::ChordState& chord = frame.chordState;
        if (chord.confidence > 0.1f && chord.type != audio::ChordType::NONE) {
            doc["key"] = formatKeyName(chord.rootNote, chord.type);
        } else {
            doc["key"] = "";
        }
#else
        // BPM from TempoTrackerOutput
        audio::TempoTrackerOutput tempo = audio->getTempo().getOutput();
        doc["bpm"] = tempo.bpm;
        
        // Mic level in dB (calculated from rmsPreGain)
        audio::AudioDspState dsp = audio->getDspState();
        float micLevelDb = (dsp.rmsPreGain > 0.0001f) 
            ? (20.0f * log10f(dsp.rmsPreGain)) 
            : -80.0f;
        doc["mic"] = micLevelDb;
        
        // Musical key/chord (formatted string)
        const audio::ControlBus& controlBus = audio->getControlBusRef();
        const audio::ControlBusFrame& frame = controlBus.GetFrame();
        const audio::ChordState& chord = frame.chordState;
        if (chord.confidence > 0.1f && chord.type != audio::ChordType::NONE) {
            doc["key"] = formatKeyName(chord.rootNote, chord.type);
        } else {
            doc["key"] = "";  // Empty string when no valid key detected
        }
#endif
    }
#endif

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

    JsonDocument doc;  // Increased for audio config fields
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

        // Audio config fields (fixes UI revert bug)
        ZoneAudioConfig audioConfig = m_zoneComposer->getZoneAudioConfig(i);
        zone["tempoSync"] = audioConfig.tempoSync;
        zone["beatModulation"] = audioConfig.beatModulation;
        zone["tempoSpeedScale"] = audioConfig.tempoSpeedScale;
        zone["beatDecay"] = audioConfig.beatDecay;
        zone["audioBand"] = audioConfig.audioBand;
        zone["beatTriggerEnabled"] = audioConfig.beatTriggerEnabled;
        zone["beatTriggerInterval"] = audioConfig.beatTriggerInterval;
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

void WebServer::broadcastSingleZoneState(uint8_t zoneId) {
    // SAFETY: Validate pointers before accessing
    if (!m_ws) {
        LW_LOGW("broadcastSingleZoneState: m_ws is null");
        return;
    }

    if (m_ws->count() == 0 || !m_zoneComposer) return;

    // Validate zone ID
    if (zoneId >= m_zoneComposer->getZoneCount()) {
        LW_LOGW("broadcastSingleZoneState: invalid zoneId %d", zoneId);
        return;
    }

    // Build zones.stateChanged message
    JsonDocument doc;
    doc["type"] = "zones.stateChanged";
    doc["zoneId"] = zoneId;
    doc["timestamp"] = millis();

    // Add current zone state
    JsonObject current = doc["current"].to<JsonObject>();
    current["enabled"] = m_zoneComposer->isZoneEnabled(zoneId);

    uint8_t effectId = m_zoneComposer->getZoneEffect(zoneId);
    current["effectId"] = effectId;

    // SAFE: Use cached state for effect name (thread-safe)
    const CachedRendererState& cached = m_cachedRendererState;
    if (effectId < cached.effectCount && cached.effectNames[effectId]) {
        current["effectName"] = cached.effectNames[effectId];
    } else {
        current["effectName"] = "Unknown";
    }

    current["brightness"] = m_zoneComposer->getZoneBrightness(zoneId);
    current["speed"] = m_zoneComposer->getZoneSpeed(zoneId);
    current["paletteId"] = m_zoneComposer->getZonePalette(zoneId);

    uint8_t blendModeValue = static_cast<uint8_t>(m_zoneComposer->getZoneBlendMode(zoneId));
    current["blendMode"] = blendModeValue;
    current["blendModeName"] = zones::getBlendModeName(m_zoneComposer->getZoneBlendMode(zoneId));

    String output;
    serializeJson(doc, output);

    // SAFETY: Validate m_ws is still valid before calling textAll()
    if (m_ws && m_ws->count() > 0) {
        m_ws->textAll(output);
    }

    LW_LOGD("Broadcast zones.stateChanged for zone %d", zoneId);
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

    JsonDocument doc;
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

// ============================================================================
// Log Stream (Wireless Serial Monitoring)
// ============================================================================

bool WebServer::setLogStreamSubscription(AsyncWebSocketClient* client, bool subscribe) {
    if (!client || !m_logBroadcaster) return false;
    uint32_t clientId = client->id();
    bool success = m_logBroadcaster->setSubscription(clientId, subscribe);

    // Note: LW_LOG calls here would be sent to the subscriber too!
    // The LogStreamBroadcaster already logs subscription changes internally.

    return success;
}

bool WebServer::hasLogStreamSubscribers() const {
    return m_logBroadcaster && m_logBroadcaster->hasSubscribers();
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

    JsonDocument doc;
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

void WebServer::broadcastFftFrame() {
    // SAFETY: Validate pointers and check if we have subscribers
    if (!m_ws || !m_renderer) return;

    // Use WsAudioCommands helper to check if FFT subscribers exist
    // This avoids re-checking in broadcastFftFrame implementation
    if (!webserver::ws::hasFftStreamSubscribers()) {
        return;
    }

    // Get cached audio frame from renderer (cross-core safe)
    const audio::ControlBusFrame& frame = m_renderer->getCachedAudioFrame();

    // Delegate to WsAudioCommands implementation which handles:
    // - Subscriber table management
    // - Throttling to 31 Hz
    // - JSON serialization with 64 FFT bins
    // - Cleanup of disconnected clients
    webserver::ws::broadcastFftFrame(frame, m_ws);
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

    // Get AudioNode to retrieve benchmark stats
    auto* audio = m_orchestrator.getAudio();
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
    // Get current API key from ApiKeyManager (NVS or compile-time default)
    String currentKey = m_apiKeyManager.getKey();

    // If no API key configured, auth is disabled
    if (currentKey.length() == 0) {
        return true;
    }

    IPAddress clientIP = request->client()->remoteIP();

    // Check if IP is blocked due to too many failed attempts
    if (m_authRateLimiter.isBlocked(clientIP)) {
        uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
        sendAuthRateLimitError(request, retryAfter);
        return false;
    }

    if (!request->hasHeader("X-API-Key")) {
        // Record failure and check if now blocked
        bool nowBlocked = m_authRateLimiter.recordFailure(clientIP);
        if (nowBlocked) {
            uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
            sendAuthRateLimitError(request, retryAfter);
        } else {
            sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                              ErrorCodes::UNAUTHORIZED, "Missing X-API-Key header");
        }
        return false;
    }

    // Use ApiKeyManager's constant-time validation
    if (!m_apiKeyManager.validateKey(request->header("X-API-Key"))) {
        // Record failure and check if now blocked
        bool nowBlocked = m_authRateLimiter.recordFailure(clientIP);
        if (nowBlocked) {
            uint32_t retryAfter = m_authRateLimiter.getRetryAfterSeconds(clientIP);
            sendAuthRateLimitError(request, retryAfter);
        } else {
            sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                              ErrorCodes::UNAUTHORIZED, "Invalid API key");
        }
        return false;
    }

    // Successful auth - reset failure counter
    m_authRateLimiter.recordSuccess(clientIP);
#endif
    return true;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
