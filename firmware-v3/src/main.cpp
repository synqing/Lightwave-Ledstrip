/**
 * LightwaveOS v2 - Main Entry Point
 *
 * Next-gen LED control platform with:
 * - Actor model for cross-core communication
 * - CQRS state management
 * - Plugin system for extensible effects
 * - 120 FPS rendering on Core 1
 * - Multi-zone composition
 * - REST API and WebSocket real-time control
 *
 * This version uses the full Actor system architecture.
 */

#include <Arduino.h>
#ifndef NATIVE_BUILD
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#endif

#define LW_LOG_TAG "Main"
#include "utils/Log.h"

#include "config/features.h"
#include "config/Trace.h"
#include "core/actors/ActorSystem.h"
#include "effects/enhancement/EdgeMixer.h"
#include "hardware/EncoderManager.h"
#include "core/actors/RendererActor.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "effects/CoreEffects.h"
#include "effects/zones/ZoneComposer.h"
#include "effects/PatternRegistry.h"
#include "config/display_order.h"
#include "effects/ieffect/BeatPulseBloomEffect.h"  // For debug toggle
#include "effects/ieffect/BloomParityEffect.h"     // For runtime PostFX tuning
#include "effects/ieffect/LGPFilmPost.h"           // For cinema post toggle
#if FEATURE_TRANSITIONS
#include "effects/transitions/TransitionEngine.h"
#include "effects/transitions/TransitionTypes.h"
#endif
#include "core/narrative/NarrativeEngine.h"
#include "core/actors/ShowDirectorActor.h"
#include "core/shows/BuiltinShows.h"
#include "core/shows/Prim8Adapter.h"
#include "core/shows/ShowBundleParser.h"
#include "core/shows/DynamicShowStore.h"
#include "effects/zones/BlendMode.h"
#include "plugins/api/IEffect.h"
#include "plugins/PluginManagerActor.h"
#if FEATURE_STACK_PROFILING
#include "core/system/StackMonitor.h"
#endif
#if FEATURE_HEAP_MONITORING
#include "core/system/HeapMonitor.h"
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
#include "core/system/MemoryLeakDetector.h"
#endif
#if FEATURE_VALIDATION_PROFILING
#include "core/system/ValidationProfiler.h"
#endif
#include "core/system/OtaBootVerifier.h"
#include "core/system/OtaTokenManager.h"

#ifndef NATIVE_BUILD
#include "hal/esp32s3/StatusStripTouch.h"
#include <esp_system.h>
#endif
#include "config/factory_presets.h"

// TempoTracker debug included via AudioActor.h

#if FEATURE_AUDIO_SYNC
#include "audio/AudioDebugConfig.h"
#include "audio/contracts/AudioEffectMapping.h"
#endif

#include "config/DebugConfig.h"
#include "serial/CaptureStreamer.h"
#include <ArduinoJson.h>

#if FEATURE_WEB_SERVER
#ifndef NATIVE_BUILD
#include <esp_wifi.h>
#endif
#include "network/WiFiManager.h"
#include "network/WiFiCredentialManager.h"
#include "network/WebServer.h"
using namespace lightwaveos::network;
using namespace lightwaveos::config;
// webServerInstance declared in WebServer.h, defined in WebServer.cpp
#endif

using namespace lightwaveos::persistence;

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
#if FEATURE_TRANSITIONS
using namespace lightwaveos::transitions;
#endif
using namespace lightwaveos::narrative;
using namespace lightwaveos::plugins;

// ==================== Global Zone Composer ====================

ZoneComposer zoneComposer;
ZoneConfigManager* zoneConfigMgr = nullptr;

// PresetManager removed — stub handlers remain in V1ApiRoutes until implemented

// ==================== Global Plugin Manager ====================

PluginManagerActor* pluginManager = nullptr;

// Global Actor System Access
ActorSystem& actors = ActorSystem::instance();
RendererActor* renderer = nullptr;

// Effect count is now dynamic via renderer->getEffectCount()
// Effect names retrieved via renderer->getEffectName(id)

// Current show index for serial navigation
static uint8_t currentShowIndex = 0;

// ==================== Factory Preset + Expression Persistence ====================

uint8_t g_factoryPresetIndex = 0;  // non-static: accessed by V1ApiRoutes for REST endpoint
static bool    g_wdtSafeMode        = false;
static bool    g_nvsCorrupted       = false;

// Debounced NVS save state (DEC-011/E2: 500ms coalesced writes)
static bool     g_nvsSavePending     = false;
static uint32_t g_nvsSaveRequestMs   = 0;
static constexpr uint32_t NVS_SAVE_DEBOUNCE_MS = 500;

// Audio failure tracking for degraded-mode relay
static bool g_audioFailureActive = false;

// External NVS save trigger (set by REST/WS handlers via persistence_trigger.h)
#include "config/persistence_trigger.h"
std::atomic<bool> g_externalNvsSaveRequest{false};

// Forced idle state (D4 § 2.7: long press toggles idle/active)
static bool    g_forcedIdle            = false;
static uint8_t g_preIdleBrightness     = 128;
static constexpr uint8_t IDLE_BRIGHTNESS_FLOOR = 20;  // ~8% of 255 (AC-14)

/// Schedule a debounced NVS save. Call whenever effect/palette/expression changes.
static void requestDebouncedSave(uint32_t now) {
    g_nvsSavePending = true;
    g_nvsSaveRequestMs = now;
}

/// Apply a factory preset: sets effect, palette, and all 7 expression params.
static void applyFactoryPreset(uint8_t index) {
    if (index >= lightwaveos::FACTORY_PRESET_COUNT) return;
    const lightwaveos::FactoryPreset& p = lightwaveos::FACTORY_PRESETS[index];

    ActorSystem& sys = ActorSystem::instance();
    sys.setEffect(p.effectId);
    sys.setPalette(p.paletteIndex);
    sys.setHue(p.hue);
    sys.setSaturation(p.saturation);
    sys.setMood(p.mood);
    sys.setIntensity(p.intensity);
    sys.setComplexity(p.complexity);
    sys.setVariation(p.variation);
    // trails → SET_FADE_AMOUNT
    if (renderer) {
        renderer->send(lightwaveos::actors::Message(
            lightwaveos::actors::MessageType::SET_FADE_AMOUNT, p.trails));
    }

    g_factoryPresetIndex = index;

#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
    statusStripShowPalette(p.paletteIndex);
#endif

    LW_LOGI("Factory preset %u: %s", index, p.name);
}

// Dynamic show store for Serial (PRISM Studio) show uploads
static prism::DynamicShowStore serialShowStore;

namespace {

constexpr uint16_t LOOP_SCRATCH_EFFECT_ID_CAP = 170;
constexpr uint16_t LOOP_SCRATCH_LED_COUNT = 320;

EffectId s_loopEffectIdFallback[LOOP_SCRATCH_EFFECT_ID_CAP] = {};
CRGB s_validationFrameFallback[LOOP_SCRATCH_LED_COUNT] = {};

EffectId* s_loopEffectIdScratch = s_loopEffectIdFallback;
CRGB* s_validationFrameScratch = s_validationFrameFallback;
bool s_loopScratchInitialised = false;

void initLoopScratchBuffers() {
    if (s_loopScratchInitialised) return;
    s_loopScratchInitialised = true;

#if !defined(NATIVE_BUILD) && defined(BOARD_HAS_PSRAM)
    if (auto* ids = static_cast<EffectId*>(
            heap_caps_calloc(LOOP_SCRATCH_EFFECT_ID_CAP, sizeof(EffectId),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) {
        s_loopEffectIdScratch = ids;
    }
    if (auto* validation = static_cast<CRGB*>(
            heap_caps_calloc(LOOP_SCRATCH_LED_COUNT, sizeof(CRGB),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) {
        s_validationFrameScratch = validation;
    }
#endif

    LW_LOGI("Loop scratch buffers: effectIds=%s validation=%s",
            (s_loopEffectIdScratch != s_loopEffectIdFallback) ? "PSRAM" : "DRAM",
            (s_validationFrameScratch != s_validationFrameFallback) ? "PSRAM" : "DRAM");
}

}  // namespace

// Capture streamer — binary frame streaming over Serial (Phase 2 extraction)
static lightwaveos::serial::CaptureStreamer captureStreamer;

// ==================== Setup ====================

void setup() {
    // Initialize serial
    Serial.setTxBufferSize(4096);   // Tier 0: enlarge HWCDC TX ring buffer (default 256B)
                                     // so Serial.write(1009) enqueues non-blocking instead
                                     // of blocking through ~4 refill cycles.
    Serial.begin(115200);
    delay(1000);

    // Allocate loop command scratch once so loopTask avoids large transient stack arrays.
    initLoopScratchBuffers();

    // Allocate capture streamer PSRAM buffers (Phase 2 extraction).
    captureStreamer.init();

    // Telemetry boot heartbeat (for trace capture verification)
    Serial.println("{\"event\":\"telemetry.boot\",\"ts_mono_ms\":0,\"version\":\"2.0\"}");

#if FEATURE_MABUTRACE
    TRACE_INIT(64);  // 64 KB ring buffer for Perfetto trace
#endif

    // OTA boot verification -- check rollback status before anything else
#ifndef NATIVE_BUILD
    lightwaveos::core::system::OtaBootVerifier::init();
#endif

#if FEATURE_WEB_SERVER && !defined(NATIVE_BUILD)
    // Force clean WiFi state before any WiFi use (avoids esp_wifi_init 257 / "Failed to deinit" on no-PSRAM)
    esp_err_t deinit = esp_wifi_deinit();
    if (deinit != ESP_OK && deinit != ESP_ERR_WIFI_NOT_INIT) {
        LW_LOGW("esp_wifi_deinit() returned %d", deinit);
    }
#endif

    // ── WDT safe-mode check (DEC-011 § 2.6) ────────────────────────────
#ifndef NATIVE_BUILD
    {
        esp_reset_reason_t reason = esp_reset_reason();
        if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT) {
            g_wdtSafeMode = true;
            LW_LOGW("WDT recovery boot — will load safe default (Prism)");
        }
    }
#endif

    LW_LOGI("==========================================");
    LW_LOGI("LightwaveOS v2 - Actor System + Zones");
    LW_LOGI("==========================================");

    // Initialize Actor System (creates RendererActor)
    // Initialize system monitoring (must be before actors start)
#if FEATURE_STACK_PROFILING
    lightwaveos::core::system::StackMonitor::init();
    LW_LOGI("Stack Monitor: INITIALIZED");
    lightwaveos::core::system::StackMonitor::startProfiling();
    LW_LOGI("Stack Profiling: STARTED");
#endif
#if FEATURE_HEAP_MONITORING
    lightwaveos::core::system::HeapMonitor::init();
    LW_LOGI("Heap Monitor: INITIALIZED");
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
    lightwaveos::core::system::MemoryLeakDetector::init();
    LW_LOGI("Memory Leak Detector: INITIALIZED");
#endif
#if FEATURE_VALIDATION_PROFILING
    lightwaveos::core::system::ValidationProfiler::init();
    LW_LOGI("Validation Profiler: INITIALIZED");
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
    // Reset baseline after all initialization
    delay(1000);  // Wait for system to stabilize
    lightwaveos::core::system::MemoryLeakDetector::resetBaseline();
#endif

    LW_LOGI("Initializing Actor System...");
    if (!actors.init()) {
        LW_LOGE("Actor System init failed!");
        while(1) delay(1000);  // Halt
    }
    renderer = actors.getRenderer();
    captureStreamer.setRenderer(renderer);
    LW_LOGI("Actor System: INITIALIZED");

    // Register ALL effects (core + LGP) BEFORE starting actors
    LW_LOGI("Registering effects...");
    uint16_t effectCount = registerAllEffects(renderer);
    LW_LOGI("Effects registered: %d", effectCount);

#if FEATURE_AUDIO_SYNC
    // Initialise AudioMappingRegistry before render tasks begin so the large mapping
    // table can live in PSRAM rather than internal SRAM.
    LW_LOGI("Initialising Audio Mapping Registry...");
    bool mappingOk = lightwaveos::audio::AudioMappingRegistry::instance().begin();
    LW_LOGI("Audio Mapping Registry: %s", mappingOk ? "READY" : "DISABLED");
#endif

    // Initialize NVS (must be before Zone Composer to load saved config)
    LW_LOGI("Initializing NVS...");
    if (!NVS_MANAGER.init()) {
        LW_LOGW("NVS init failed - settings won't persist!");
    } else {
        LW_LOGI("NVS: INITIALIZED");
    }

    // Initialize per-device OTA token (must be after NVS, before WebServer)
#if FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER
    if (!lightwaveos::core::system::OtaTokenManager::init()) {
        LW_LOGW("OTA Token Manager init failed - using compile-time token");
    } else {
        LW_LOGI("OTA Token Manager: INITIALIZED");
    }
#endif

    // Initialize Zone Composer
    LW_LOGI("Initializing Zone Composer...");
    if (!zoneComposer.init(renderer)) {
        LW_LOGE("Zone Composer init failed!");
    } else {
        // Attach zone composer to renderer
        renderer->setZoneComposer(&zoneComposer);

        // Create config manager
        zoneConfigMgr = new ZoneConfigManager(&zoneComposer);

        // Try to load saved zone configuration
        if (zoneConfigMgr->loadFromNVS()) {
            LW_LOGI("Zone Composer: INITIALIZED (restored from NVS)");
        } else {
            // First boot - load default preset
            zoneComposer.loadPreset(1);
            LW_LOGI("Zone Composer: INITIALIZED");
            LW_LOGI("Preset: Dual Split (default)");
        }
    }

    // Status strip + touch: register with FastLED *before* actors start,
    // so no show() occurs with a partially built controller list.
#if CHIP_ESP32_S3 && !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
    statusStripTouchSetup();
#endif

    // TTP223 button init (independent of status strip)
#if !defined(NATIVE_BUILD) && defined(K1_TTP223_PIN)
    pinMode(K1_TTP223_PIN, INPUT);
    LW_LOGI("TTP223 button: GPIO %d", K1_TTP223_PIN);
#endif

    // Start all actors (RendererActor runs on Core 1 at 120 FPS)
    LW_LOGI("Starting Actor System...");
    if (!actors.start()) {
        LW_LOGE("Actor System start failed!");
        while(1) delay(1000);  // Halt
    }
    LW_LOGI("Actor System: RUNNING");
#ifndef NATIVE_BUILD
    LW_LOGI("Boot memory: heap %lu bytes free", (unsigned long)ESP.getFreeHeap());
#ifdef CONFIG_SPIRAM_SUPPORT
    LW_LOGI("Boot memory: PSRAM %lu bytes free", (unsigned long)ESP.getFreePsram());
#endif
#endif

    // Initialize Plugin Manager
    LW_LOGI("Initializing Plugin Manager...");
    pluginManager = new PluginManagerActor();
    
    // Wire to RendererActor for effect forwarding
    if (renderer) {
        pluginManager->setTargetRegistry(renderer);
        LW_LOGI("Plugin Manager: Target registry set to RendererActor");
    }
    
    // Start plugin manager (loads manifests from LittleFS)
    pluginManager->onStart();
    LW_LOGI("Plugin Manager: INITIALIZED");

    // Load or set initial state
    LW_LOGI("Loading system state...");

    if (g_wdtSafeMode) {
        // WDT recovery: force safe default, ignore NVS (may have caused crash)
        applyFactoryPreset(lightwaveos::FACTORY_PRESET_DEFAULT_INDEX);
        actors.setBrightness(128);
        actors.setSpeed(15);
        LW_LOGW("WDT safe-mode: loaded Prism factory preset");
    } else {
        EffectId savedEffect; uint8_t savedBrightness, savedSpeed, savedPalette;
        uint8_t savedPresetIdx = 0;
        lightwaveos::persistence::SystemExpressionParams savedExpr;

        bool loaded = zoneConfigMgr && zoneConfigMgr->loadSystemState(
            savedEffect, savedBrightness, savedSpeed, savedPalette,
            &savedPresetIdx, &savedExpr);

        if (loaded) {
            actors.setEffect(savedEffect);
            actors.setBrightness(savedBrightness);
            actors.setSpeed(savedSpeed);
            actors.setPalette(savedPalette);
            actors.setHue(savedExpr.hue);
            actors.setSaturation(savedExpr.saturation);
            actors.setMood(savedExpr.mood);
            actors.setIntensity(savedExpr.intensity);
            actors.setComplexity(savedExpr.complexity);
            actors.setVariation(savedExpr.variation);
            // trails
            if (renderer) {
                renderer->send(lightwaveos::actors::Message(
                    lightwaveos::actors::MessageType::SET_FADE_AMOUNT, savedExpr.trails));
            }
            g_factoryPresetIndex = savedPresetIdx;
            LW_LOGI("Restored: Effect=0x%04X, Bri=%d, Spd=%d, Pal=%d, Preset=%d",
                    savedEffect, savedBrightness, savedSpeed, savedPalette, savedPresetIdx);
            LW_LOGI("Expression: H=%d S=%d M=%d T=%d I=%d C=%d V=%d",
                    savedExpr.hue, savedExpr.saturation, savedExpr.mood,
                    savedExpr.trails, savedExpr.intensity, savedExpr.complexity,
                    savedExpr.variation);
        } else if (zoneConfigMgr && zoneConfigMgr->getLastError() ==
                   lightwaveos::persistence::NVSResult::CHECKSUM_ERROR) {
            // NVS corruption detected
            g_nvsCorrupted = true;
            applyFactoryPreset(lightwaveos::FACTORY_PRESET_DEFAULT_INDEX);
            actors.setBrightness(128);
            actors.setSpeed(15);
            LW_LOGW("NVS corruption — loaded Prism factory preset");
        } else {
            // First boot defaults
            applyFactoryPreset(lightwaveos::FACTORY_PRESET_DEFAULT_INDEX);
            actors.setBrightness(128);
            actors.setSpeed(15);
            LW_LOGI("Using defaults (first boot)");
        }
    }

    // Initialize Network (if enabled)
#if FEATURE_WEB_SERVER
    // Start WiFiManager — boots into AP-only mode.
    // STA is activated ONLY via serial `wifi connect` command.
    LW_LOGI("Initializing WiFiManager (AP-only boot)...");
    WIFI_MANAGER.setCredentials(
        NetworkConfig::WIFI_SSID_VALUE,
        NetworkConfig::WIFI_PASSWORD_VALUE
    );
    WIFI_MANAGER.enableSoftAP(NetworkConfig::AP_SSID, NetworkConfig::AP_PASSWORD);

    if (!WIFI_MANAGER.begin()) {
        LW_LOGE("WiFiManager failed to start!");
    } else {
        LW_LOGI("WiFiManager: STARTED (AP: %s, IP: %s)",
                NetworkConfig::AP_SSID, WiFi.softAPIP().toString().c_str());
        LW_LOGI("Use serial 'wifi connect SSID PASS' to enable STA mode");

        // Initialize WiFi Credential Manager (for saved networks)
        if (!WIFI_CREDENTIALS.begin()) {
            LW_LOGW("WiFiCredentialManager failed to initialize");
        }
    }

    // Start WebServer (skip when heap too low to avoid OOM abort on no-PSRAM)
    const size_t kMinHeapForWebServer = 38000;
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < kMinHeapForWebServer) {
        LW_LOGW("Insufficient heap for WebServer (%u bytes free, need %u), skipping",
                (unsigned)freeHeap, (unsigned)kMinHeapForWebServer);
    } else {
        LW_LOGI("Starting Web Server... (free heap: %u)", (unsigned)freeHeap);

        // Instantiate WebServer with dependencies
        webServerInstance = new WebServer(actors, renderer);

        // Note: PluginManager wiring to WebServer not yet implemented
        // TODO: Add setPluginManager() to WebServer when plugin UI wiring is needed
        if (pluginManager) {
            LW_LOGI("Plugin Manager: Created (WebServer integration pending)");
        }

        if (!webServerInstance->begin()) {
            LW_LOGW("Web Server failed to start!");
            delete webServerInstance;
            webServerInstance = nullptr;
        } else {
            LW_LOGI("Web Server: RUNNING");
            LW_LOGI("REST API: http://lightwaveos.local/api/v1/");
            LW_LOGI("WebSocket: ws://lightwaveos.local/ws");
        }
    }
#endif

    // OTA boot verification -- validate health after all critical subsystems init
#ifndef NATIVE_BUILD
    {
#if FEATURE_WEB_SERVER
        bool wifiOk = WIFI_MANAGER.isConnected() || WIFI_MANAGER.isAPMode();
        bool wsOk = (webServerInstance != nullptr);
#else
        bool wifiOk = false;
        bool wsOk = false;
#endif
        lightwaveos::core::system::OtaBootVerifier::markAppValidIfHealthy(wifiOk, wsOk);
    }
#endif

    // ── Boot-time degraded-mode signals (DEC-011 § 2.6) ────────────────
#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
    if (g_nvsCorrupted) {
        statusStripTriggerWhiteFlash();
    }
    {
        uint32_t freeHeap = esp_get_free_heap_size();
        if (freeHeap < 38 * 1024) {
            statusStripSetLowHeap(true);
            LW_LOGW("Low heap at boot: %u bytes", freeHeap);
        }
    }
#endif

    LW_LOGI("==========================================");
    LW_LOGI("ACTOR SYSTEM: OPERATIONAL");
    LW_LOGI("==========================================");
    Serial.println("\nCommands:");
    Serial.println("  SPACE   - Next effect (quick tap)");
    Serial.println("  0-9/a-k - Select effect by key");
    Serial.println("  n/N     - Next/Prev effect");
    Serial.println("  +/-     - Adjust brightness");
    Serial.println("  [/]     - Adjust speed");
    Serial.println("  ,/.     - Prev/Next palette (75 total)");
    Serial.println("  p/P     - Bloom prism opacity +/-");
    Serial.println("  o/O     - Bloom bulb opacity +/-");
    Serial.println("  i/I     - Mood +/- (transport speed)");
    Serial.println("  f/F     - Bloom alpha +/- (persistence)");
    Serial.println("  h/H     - Bloom square iter +/- (contrast)");
    Serial.println("  j/J     - Bloom prism iter +/- (ghost layers)");
    Serial.println("  k/K     - Bloom gHue speed +/- (palette sweep)");
    Serial.println("  u/U     - Bloom spatial spread +/- (palette gradient)");
    Serial.println("  v/V     - Bloom intensity coupling +/- (spatial↔heatmap)");
    Serial.println("  l       - List effects");
    Serial.println("  s       - Print status");
    Serial.println("\nEffect Registers:");
    Serial.println("  r       - Reactive effects only (audio-responsive)");
    Serial.println("  m       - Ambient effects only (time-based)");
    Serial.println("  *       - All effects (default)");
    Serial.println("\nZone Commands:");
    Serial.println("  z       - Toggle zone mode");
    Serial.println("  Z       - Print zone status");
    Serial.println("  zs      - Set zone speed: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>");
    Serial.println("  1-5     - Load zone preset (in zone mode)");
    Serial.println("  S       - Save all settings to NVS");
    Serial.println("\nTransition Commands:");
    Serial.println("  t/T     - RD Triangle F +/- (auto-selects RD Triangle)");
    Serial.println("  !       - List transition types");
    Serial.println("\nAuto-Play (Narrative) Commands:");
    Serial.println("  A       - Toggle auto-play mode");
    Serial.println("  @       - Print narrative status");
    Serial.println("\nEdgeMixer Commands:");
    Serial.println("  e       - Cycle mode (mirror/analog/comp/split_comp/sat_veil)");
    Serial.println("  w/W     - Spread +/- (hue width, 0-60)");
    Serial.println("  </>     - Strength -/+ (0-255)");
    Serial.println("  y       - Toggle spatial (uniform/centre_gradient)");
    Serial.println("  Y       - Toggle temporal (static/rms_gate)");
    Serial.println("  }       - Save EdgeMixer to NVS");
    Serial.println("  #       - Print EdgeMixer status");
    Serial.println("\nColor Correction Commands:");
    Serial.println("  c       - Cycle correction mode (OFF→HSV→RGB→BOTH→OFF)");
    Serial.println("  C       - Show color correction status");
    Serial.println("  E       - Toggle auto-exposure");
    Serial.println("  g       - Toggle/cycle gamma (off→2.2→2.5→2.8→off)");
    Serial.println("  b/B     - RD Triangle K +/- (auto-selects RD Triangle)");
    Serial.println("  cc      - Show correction mode (0=OFF,1=HSV,2=RGB,3=BOTH)");
    Serial.println("  cc N    - Set correction mode (0-3, accepts 'cc1' or 'cc 1')");
    Serial.println("  ae      - Show auto-exposure status");
    Serial.println("  ae 0/1  - Disable/enable auto-exposure (accepts 'ae0' or 'ae 0')");
    Serial.println("  gamma   - Show gamma status");
    Serial.println("  gamma N - Set gamma (0=off, 1.0-3.0, accepts 'gamma1.5' or 'gamma 1.5')");
    Serial.println("  brown   - Show brown guardrail status");
    Serial.println("  brown 0/1 - Disable/enable brown guardrail (accepts 'brown0' or 'brown 0')");
    Serial.println("  Csave   - Save color settings to NVS");
#if FEATURE_AUDIO_SYNC
    Serial.println("\nTempoTracker Debug:");
    Serial.println("  tempo   - Show BPM, confidence, phase, lock state");
#endif
#if FEATURE_AUDIO_SYNC
    Serial.println("\nAudio Debug Verbosity:");
    Serial.println("  x              - Print 8-band + bass/mid/treble/rms/flux (one-shot; alias: bands)");
    Serial.println("  adbg           - Show current level");
    Serial.println("  adbg <0-5>     - Set level (0=off, 2=warnings, 5=trace)");
    Serial.println("  adbg status    - Print health summary (one-shot)");
    Serial.println("  adbg spectrum  - Print 8-band + 64-bin spectrum (one-shot)");
    Serial.println("  adbg beat      - Print BPM, phase, confidence (one-shot)");
    Serial.println("  adbg interval <N> - Set base interval in frames");
#endif
    Serial.println("\nDebug Commands (unified):");
    Serial.println("  d             - Toggle Bloom effect debug (spd/vel/mood/beat)");
    Serial.println("  dbg           - Show debug config");
    Serial.println("  dbg <0-5>     - Set global level (0=off,2=warn,3=info,4=debug,5=trace)");
    Serial.println("  dbg audio <0-5>   - Set audio domain level");
    Serial.println("  dbg render <0-5>  - Set render domain level");
    Serial.println("  dbg network <0-5> - Set network domain level");
    Serial.println("  dbg actor <0-5>   - Set actor domain level");
    Serial.println("  dbg status    - Print audio health NOW");
    Serial.println("  dbg spectrum  - Print 64-bin spectrum NOW");
    Serial.println("  dbg beat      - Print beat tracking NOW");
    Serial.println("  dbg memory    - Print heap/stack NOW");
    Serial.println("  dbg interval status <N>  - Auto status every N sec (0=off)");
    Serial.println("  dbg interval spectrum <N>- Auto spectrum every N sec (0=off)");
#if FEATURE_MABUTRACE
    Serial.println("\nTrace Commands:");
    Serial.println("  trace         - Flush + dump Perfetto trace JSON to serial");
#endif
#if FEATURE_WEB_SERVER
    Serial.println("\nWiFi Commands:");
    Serial.println("  wifi              - Show status (mode, SSID, IP, RSSI, saved networks)");
    Serial.println("  wifi ap           - Switch to AP-only mode (no STA)");
    Serial.println("  wifi connect SSID PASS - Connect to network + save to NVS");
    Serial.println("  wifi connect SSID - Connect using saved NVS creds");
    Serial.println("  wifi connect      - Reconnect to last/first saved");
    Serial.println("  wifi scan         - Trigger network scan");
    Serial.println("\nWeb API:");
    Serial.println("  GET  /api/v1/effects - List effects");
    Serial.println("  POST /api/v1/effects/set - Set effect");
    Serial.println("  WS   /ws - Real-time control");
#endif
    Serial.println();
}

// Serial JSON Gateway moved to serial/SerialJsonGateway.cpp
#include "serial/SerialJsonGateway.h"

// ==================== Loop ====================

void loop() {
    static uint32_t lastStatus = 0;
    static EffectId currentEffect = lightwaveos::EID_FIRE;
    static uint8_t lastAudioEffectIndex = 0;  // Track which audio effect (0=Waveform, 1=Bloom)

    // Effect register state (for filtered effect cycling)
    static EffectRegister currentRegister = EffectRegister::ALL;
    static uint8_t reactiveRegisterIndex = 0;   // Index within reactive effects
    static uint16_t ambientRegisterIndex = 0;   // Index within ambient effects
    static EffectId ambientEffectIds[170];      // Cached ambient effect IDs
    static uint16_t ambientEffectCount = 0;     // Number of ambient effects
    static bool registersInitialized = false;

    // One-time initialization of effect registers
    if (!registersInitialized && renderer) {
        uint16_t effectCount = renderer->getEffectCount();
        const uint16_t cappedCount =
            (effectCount <= LOOP_SCRATCH_EFFECT_ID_CAP) ? effectCount : LOOP_SCRATCH_EFFECT_ID_CAP;
        // Build array of all registered EffectIds for ambient filtering
        for (uint16_t i = 0; i < cappedCount; i++) {
            s_loopEffectIdScratch[i] = renderer->getEffectIdAt(i);
        }
        ambientEffectCount = PatternRegistry::buildAmbientEffectArray(
            ambientEffectIds, LOOP_SCRATCH_EFFECT_ID_CAP, s_loopEffectIdScratch, cappedCount);
        registersInitialized = true;
        LW_LOGI("Effect registers: %d reactive, %d ambient, %d total",
                PatternRegistry::getReactiveEffectCount(), ambientEffectCount, effectCount);
    }

    uint32_t now = millis();
    uint32_t nowUs = micros();

    // --- Capture streaming tick (sync fallback) ---
    captureStreamer.tick(nowUs);

#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
    statusStripTouchLoop(now);

    // ── Audio failure relay to status strip (D4 § 2.6) ───────────────────
    {
        auto* audioActor = ::actors.getAudio();
        if (audioActor) {
            bool audioFailed = (audioActor->getState() == lightwaveos::audio::AudioActorState::ERROR);
            if (audioFailed != g_audioFailureActive) {
                g_audioFailureActive = audioFailed;
                statusStripSetAudioFailure(audioFailed);
            }
        }
    }
#endif

    // ── Button polling: factory preset cycling (D4 § 2.3, § 2.7) ────────
#if !defined(NATIVE_BUILD) && defined(K1_TTP223_PIN)
    {
        ButtonEvent evt = statusStripPollButton(now);
        if (evt == ButtonEvent::TAP) {
            if (g_forcedIdle) {
                // Exit idle on tap — restore brightness before applying preset
                g_forcedIdle = false;
                ActorSystem::instance().setBrightness(g_preIdleBrightness);
                LW_LOGI("Idle OFF (tap exit, brightness → %u)", g_preIdleBrightness);
            }
            g_factoryPresetIndex = (g_factoryPresetIndex + 1) % lightwaveos::FACTORY_PRESET_COUNT;
            applyFactoryPreset(g_factoryPresetIndex);
            requestDebouncedSave(now);
            if (webServerInstance) webServerInstance->broadcastStatus();
        }
        if (evt == ButtonEvent::LONG_PRESS) {
            g_forcedIdle = !g_forcedIdle;
            if (g_forcedIdle) {
                g_preIdleBrightness = renderer ? renderer->getBrightness() : 128;
                ActorSystem::instance().setBrightness(IDLE_BRIGHTNESS_FLOOR);
                LW_LOGI("Idle ON (brightness %u → %u)", g_preIdleBrightness, IDLE_BRIGHTNESS_FLOOR);
            } else {
                ActorSystem::instance().setBrightness(g_preIdleBrightness);
                LW_LOGI("Idle OFF (brightness → %u)", g_preIdleBrightness);
            }
            if (webServerInstance) webServerInstance->broadcastStatus();
        }
    }
#endif

    // ── External save trigger from REST/WS handlers (D4 § 2.4) ────────────
    if (g_externalNvsSaveRequest.exchange(false)) {
        requestDebouncedSave(now);
    }

    // ── Debounced NVS save (DEC-011/E2: 500ms coalesced writes) ──────────
    if (g_nvsSavePending && (now - g_nvsSaveRequestMs) >= NVS_SAVE_DEBOUNCE_MS) {
        g_nvsSavePending = false;
        if (zoneConfigMgr && renderer) {
            lightwaveos::persistence::SystemExpressionParams expr;
            expr.hue        = renderer->getHue();
            expr.saturation = renderer->getSaturation();
            expr.mood       = renderer->getMood();
            expr.trails     = renderer->getFadeAmount();
            expr.intensity  = renderer->getIntensity();
            expr.complexity = renderer->getComplexity();
            expr.variation  = renderer->getVariation();
            zoneConfigMgr->saveSystemState(
                renderer->getCurrentEffect(),
                renderer->getBrightness(),
                renderer->getSpeed(),
                renderer->getPaletteIndex(),
                g_factoryPresetIndex,
                &expr);
        }
    }

#if FEATURE_ROTATE8_ENCODER
    // Handle encoder events
    using namespace lightwaveos::hardware;
    EncoderEvent event;
    while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
        handleEncoderEvent(event, actors, renderer);
    }
#endif

    // Handle serial commands with proper line buffering
    // This fixes the issue where typing "adbg 2" character-by-character
    // would trigger effect selection for '2' instead of the adbg command
    static String serialCmdBuffer = "";

    while (Serial.available()) {
        char c = Serial.read();

        // Immediate single-char commands (no Enter needed, no buffering)
        // These work even mid-buffer - they're "hotkeys"
        if (serialCmdBuffer.length() == 0) {
            bool isImmediate = false;
            switch (c) {
                case ' ':   // Next effect
                case '+': case '=':  // Brightness up
                case '-': case '_':  // Brightness down
                case '[': case ']':  // Speed
                case ',': case '.':  // Palette
                case 'e':            // EdgeMixer mode cycle
                case 'w': case 'W':  // EdgeMixer spread +/-
                case '<': case '>':  // EdgeMixer strength -/+
                case 'y':            // EdgeMixer spatial toggle
                case 'Y':            // EdgeMixer temporal toggle
                case '}':            // EdgeMixer save to NVS
                case 'a':            // Audio debug toggle
                case 'p': case 'P':  // Bloom prism opacity
                case 'o': case 'O':  // Bloom bulb opacity
                case 'i': case 'I':  // Mood
                case 'f': case 'F':  // Bloom alpha (persistence)
                case 'h': case 'H':  // Bloom square iter (contrast)
                case 'j': case 'J':  // Bloom prism iterations
                case 'k': case 'K':  // Bloom gHue speed (palette sweep)
                case 'u': case 'U':  // Bloom spatial spread
                case 'v': case 'V':  // Bloom intensity coupling
                case 'b': case 'B':  // RD Triangle K +/-
                case 't': case 'T':  // RD Triangle F +/-
                case 'x': case 'X':  // Bands observability (one-shot dump)
                case 'q':            // Cinema post-processing toggle
                case '`':            // Status strip idle mode cycle
                    isImmediate = true;
                    break;
            }
            if (isImmediate) {
                // Check if more chars are pending — if so, this might be
                // the start of a multi-char command (e.g. 'a' in "adbg 5")
                if (Serial.available() > 0) {
                    // Buffer it instead of processing immediately
                    serialCmdBuffer += c;
                } else {
                    // Process immediately without buffering
                    serialCmdBuffer = String(c);
                    break; // Exit while loop to process
                }
                continue;
            }
        }

        if (c == '\n' || c == '\r') {
            // End of line - process buffered command
            break;
        } else if (c == 0x7F || c == 0x08) {
            // Backspace - remove last char
            if (serialCmdBuffer.length() > 0) {
                serialCmdBuffer.remove(serialCmdBuffer.length() - 1);
            }
        } else if (c >= 32 && c < 127) {
            // Printable ASCII - add to buffer
            serialCmdBuffer += c;
        }
    }

    // Process buffered command if we have one
    if (serialCmdBuffer.length() == 0) {
        // No command to process
    } else {
        String input = serialCmdBuffer;
        char firstChar = input[0]; // Save before trim (for space, etc.)
        serialCmdBuffer = ""; // Clear for next command
        input.trim();

        // Intercept VAL: commands → forward to RendererActor validation mode
        if (input.startsWith("VAL:")) {
            auto* ren = actors.getRenderer();
            if (ren) {
                ren->enqueueValidationCommand(input.c_str());
            }
        }
        else // Use firstChar for single immediate commands, trimmed input for multi-char
        if (input.length() == 0 && firstChar != ' ' && firstChar != '+' && firstChar != '-' &&
            firstChar != '=' && firstChar != '[' && firstChar != ']' &&
            firstChar != ',' && firstChar != '.' && firstChar != '<' && firstChar != '>' &&
            firstChar != '}' && firstChar != 'x' && firstChar != 'X') {
            // Empty after trim and not an immediate command - ignore
        } else {
        // Restore single-char immediate commands that got trimmed
        if (input.length() == 0) {
            input = String(firstChar);
        }
        String inputLower = input;
        inputLower.toLowerCase();
        bool handledMulti = false;
        int peekChar = input[0];

        // Serial JSON command gateway (for PRISM Studio Web Serial).
        // Any line beginning with '{' is routed to the JSON handler and
        // does not fall through to the text command matching below.
        if (input.length() > 0 && input.charAt(0) == '{') {
            lightwaveos::serial::SerialJsonGatewayDeps jsonDeps{
                actors, renderer, zoneComposer, serialShowStore};
            lightwaveos::serial::processSerialJsonCommand(input, jsonDeps);
            handledMulti = true;
        }

#if FEATURE_AUDIO_SYNC
        // Observability: single key "x" (or "bands") — must be top-level so "x" and "bands" are not gated by peekChar == 'a'
        if (inputLower == "x" || inputLower == "bands") {
            handledMulti = true;
            RendererActor* ren = actors.getRenderer();
            if (ren) {
                RendererActor::BandsDebugSnapshot snap;
                ren->getBandsDebugSnapshot(snap);
                if (snap.valid) {
                    Serial.println("\n=== Bands (renderer snapshot) ===");
                    Serial.printf("  bands[0..7]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                        snap.bands[0], snap.bands[1], snap.bands[2], snap.bands[3],
                        snap.bands[4], snap.bands[5], snap.bands[6], snap.bands[7]);
                    Serial.printf("  bass=%.3f mid=%.3f treble=%.3f (ctx.audio accessors)\n",
                        snap.bass, snap.mid, snap.treble);
                    Serial.printf("  rms=%.3f flux=%.3f hop_seq=%lu\n",
                        snap.rms, snap.flux, (unsigned long)snap.hop_seq);
                } else {
                    Serial.println("Bands: no live snapshot (audio buffer or frame not ready)");
                }
            } else {
                Serial.println("Renderer not available");
            }
        }
        else
#endif
        // Color correction commands (cc, ae, gamma, brown) and capture commands
        if (peekChar == 'c' && input.length() > 1) {

            // Capture commands — delegated to CaptureStreamer (Phase 2 extraction).
            if (inputLower.startsWith("capture")) {
                handledMulti = true;
                captureStreamer.handleCommand(input);
            }

            if (input == "c") {
                // Single 'c' - let single-char handler process it (don't set handledMulti)
                // But we've already consumed it, so handle it here
                handledMulti = true;
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto currentMode = engine.getMode();
                uint8_t modeInt = (uint8_t)currentMode;
                modeInt = (modeInt + 1) % 4;  // Cycle: 0→1→2→3→0
                engine.setMode((lightwaveos::enhancement::CorrectionMode)modeInt);
                const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
                Serial.printf("Color correction mode: %d (%s)\n", modeInt, modeNames[modeInt]);
            } else if (input.startsWith("cc")) {
                handledMulti = true;
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();

                if (input == "cc") {
                    // Show current mode
                    auto mode = engine.getMode();
                    Serial.printf("Color correction mode: %d\n", (int)mode);
                    Serial.println("  0=OFF, 1=HSV, 2=RGB, 3=BOTH");
                } else if (input.length() > 2) {
                    // Set mode - handle both "cc1" and "cc 1" formats
                    // Find first digit after "cc" (skip whitespace)
                    int startIdx = 2;
                    while (startIdx < input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                        startIdx++;
                    }
                    if (startIdx < input.length()) {
                        uint8_t mode = input.substring(startIdx).toInt();
                        if (mode <= 3) {
                            engine.setMode((lightwaveos::enhancement::CorrectionMode)mode);
                            Serial.printf("Color correction mode set to: %d\n", mode);
                        } else {
                            Serial.println("Invalid mode. Use 0-3");
                        }
                    }
                }
            }
            // If input doesn't start with 'c' or 'cc', let it fall through (but this shouldn't happen)
        }
        // -----------------------------------------------------------------
        // Effect selection command (multi-digit safe): "effect <id>"
        // -----------------------------------------------------------------
        else if (peekChar == 'e' && input.length() > 1) {
            if (inputLower.startsWith("effect ")) {
                handledMulti = true;

                // Parse as EffectId - accepts both namespaced IDs (0x0100) and display indices (0-161)
                // Supports hex input with 0x/0X prefix (e.g. "effect 0x1403")
                String idStr = input.substring(7);
                idStr.trim();
                long rawId;
                if (idStr.startsWith("0x") || idStr.startsWith("0X")) {
                    rawId = strtol(idStr.c_str(), nullptr, 16);
                } else {
                    rawId = idStr.toInt();
                }
                EffectId effectId;
                uint16_t effectCount = renderer->getEffectCount();
                if (rawId >= 0 && rawId < effectCount) {
                    // Treat small numbers as display-order index
                    effectId = renderer->getEffectIdAt(rawId);
                } else {
                    // Treat as direct EffectId
                    effectId = static_cast<EffectId>(rawId);
                }
                if (!renderer->isEffectRegistered(effectId)) {
                    Serial.printf("ERROR: Effect 0x%04X not registered\n", effectId);
                } else {
                    currentEffect = effectId;
                    actors.setEffect(effectId);
                    Serial.printf("Effect 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", effectId, renderer->getEffectName(effectId));
                }
            }
        }
        else if (peekChar == 'a' && input.length() > 1) {
#if FEATURE_VALIDATION_PROFILING
            if (inputLower.startsWith("validation_stats") || inputLower == "val_stats") {
                lightwaveos::core::system::ValidationProfiler::generateReport();
                handledMulti = true;
            } else
#endif
#if FEATURE_STACK_PROFILING
            if (inputLower.startsWith("stack_usage") || inputLower == "stack_profile") {
                lightwaveos::core::system::StackMonitor::generateProfileReport();
                handledMulti = true;
            } else
#endif
            if (input.startsWith("ae")) {
                handledMulti = true;
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto& cfg = engine.getConfig();

                if (input == "ae") {
                    Serial.printf("Auto-exposure: %s, target=%d\n",
                                  cfg.autoExposureEnabled ? "ON" : "OFF",
                                  cfg.autoExposureTarget);
                } else if (input.length() > 2) {
                    // Handle both "ae0"/"ae1" and "ae 0"/"ae 1" formats
                    int startIdx = 2;
                    while (startIdx < input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                        startIdx++;
                    }
                    if (startIdx < input.length()) {
                        String arg = input.substring(startIdx);
                        if (arg == "0") {
                            cfg.autoExposureEnabled = false;
                            Serial.println("Auto-exposure: OFF");
                        } else if (arg == "1") {
                            cfg.autoExposureEnabled = true;
                            Serial.println("Auto-exposure: ON");
                        } else {
                            int target = arg.toInt();
                            if (target > 0 && target <= 255) {
                                cfg.autoExposureTarget = target;
                                Serial.printf("Auto-exposure target: %d\n", target);
                            }
                        }
                    }
                }
            }
#if FEATURE_AUDIO_SYNC
            // adbg        -> show audio debug config
            // (x | bands handled at top level so "x" and "bands" are not gated by peekChar == 'a')
            if (inputLower.startsWith("adbg")) {
                handledMulti = true;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                auto& unifiedCfg = lightwaveos::config::getDebugConfig();

                if (inputLower == "adbg") {
                    // Show current settings
                    Serial.printf("Audio debug: level=%d interval=%d frames\n",
                                  dbgCfg.verbosity, dbgCfg.baseInterval);
                    Serial.println("Levels: 0=off, 1=errors, 2=warnings, 3=info, 4=debug, 5=trace");
                    Serial.println("One-shot: adbg status | adbg spectrum | adbg beat");
                } else if (inputLower == "adbg status") {
                    // One-shot status via AudioActor method
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printStatus();
                    } else {
                        Serial.println("Audio not available");
                    }
                } else if (inputLower == "adbg spectrum") {
                    // One-shot spectrum via AudioActor method
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printSpectrum();
                    } else {
                        Serial.println("Audio not available");
                    }
                } else if (inputLower == "adbg beat") {
                    // One-shot beat tracking via AudioActor method
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printBeat();
                    } else {
                        Serial.println("Audio not available");
                    }
                } else if (inputLower.startsWith("adbg interval ")) {
                    int val = inputLower.substring(14).toInt();
                    if (val > 0 && val < 1000) {
                        dbgCfg.baseInterval = val;
                        Serial.printf("Base interval set to %d frames (~%.1fs)\n", val, val / 62.5f);
                    } else {
                        Serial.println("Invalid interval (1-999)");
                    }
                } else {
                    // Parse level: adbg0, adbg1, ..., adbg5 or adbg 0, adbg 1, etc.
                    int level = -1;
                    if (inputLower.length() == 5 && inputLower[4] >= '0' && inputLower[4] <= '5') {
                        level = inputLower[4] - '0';
                    } else if (inputLower.length() > 5) {
                        level = inputLower.substring(5).toInt();
                        if (inputLower.substring(4, 5) != " " && inputLower[4] < '0') level = -1;
                    }
                    if (level >= 0 && level <= 5) {
                        dbgCfg.verbosity = level;
                        // Also sync with unified DebugConfig
                        unifiedCfg.setDomainLevel(lightwaveos::config::DebugDomain::AUDIO, level);
                        const char* names[] = {"off", "errors", "warnings", "info", "debug", "trace"};
                        Serial.printf("Audio debug level: %d (%s)\n", level, names[level]);
                    } else {
                        Serial.println("Usage: adbg [0-5] | adbg status | adbg spectrum | adbg beat | adbg interval <N>");
                    }
                }
            }
#endif
        }
#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11
        // Runtime tempo parameter tuning: "tempo" shows current, "tempo <param> <value>" sets.
        else if (peekChar == 't' && inputLower.startsWith("tempo")) {
            handledMulti = true;
            auto* aa = ::actors.getAudio();
            if (aa) {
                auto& tp = aa->esBackend().tempoParams();
                String subcmd = input.substring(5);
                subcmd.trim();
                if (subcmd.length() == 0) {
                    Serial.printf("tempo params:\n"
                        "  gate_base=%.2f gate_scale=%.2f gate_tau=%.1f\n"
                        "  conf_floor=%.2f valid_thr=%.2f stab_tau=%.1f\n"
                        "  hold_us=%lu oct_runs=%u decay_floor=%.4f\n"
                        "  oct_ratio_lo=%.2f oct_ratio_hi=%.2f\n"
                        "  ws_sep_floor=%.4f conf_decay=%.4f\n"
                        "  generic_persist_us=%lu\n",
                        tp.gateBase, tp.gateScale, tp.gateTau,
                        tp.confFloor, tp.validationThr, tp.stabilityTau,
                        (unsigned long)tp.holdUs, (unsigned)tp.octaveRuns, tp.decayFloor,
                        tp.octRatioLo, tp.octRatioHi,
                        tp.wsSepFloor, tp.confDecay,
                        (unsigned long)tp.genericPersistUs);
                } else {
                    int sp = subcmd.indexOf(' ');
                    if (sp > 0) {
                        String key = subcmd.substring(0, sp); key.trim();
                        float val = subcmd.substring(sp + 1).toFloat();
                        if (key == "gate_base") tp.gateBase = val;
                        else if (key == "gate_scale") tp.gateScale = val;
                        else if (key == "gate_tau") tp.gateTau = val;
                        else if (key == "conf_floor") tp.confFloor = val;
                        else if (key == "valid_thr") tp.validationThr = val;
                        else if (key == "stab_tau") tp.stabilityTau = val;
                        else if (key == "hold_us") tp.holdUs = static_cast<uint32_t>(val);
                        else if (key == "oct_runs") tp.octaveRuns = static_cast<uint8_t>(val);
                        else if (key == "decay_floor") tp.decayFloor = val;
                        else if (key == "oct_ratio_lo") tp.octRatioLo = val;
                        else if (key == "oct_ratio_hi") tp.octRatioHi = val;
                        else if (key == "ws_sep_floor") tp.wsSepFloor = val;
                        else if (key == "conf_decay") tp.confDecay = val;
                        else if (key == "generic_persist_us") tp.genericPersistUs = static_cast<uint32_t>(val);
                        else { Serial.printf("Unknown param: %s\n", key.c_str()); }
                        Serial.printf("tempo %s = %.4f\n", key.c_str(), val);
                    } else {
                        Serial.println("Usage: tempo <param> <value>");
                        Serial.println("Params: gate_base gate_scale gate_tau conf_floor valid_thr");
                        Serial.println("        stab_tau hold_us oct_runs decay_floor");
                        Serial.println("        oct_ratio_lo oct_ratio_hi ws_sep_floor conf_decay");
                        Serial.println("        generic_persist_us");
                    }
                }
            } else {
                Serial.println("AudioActor not available");
            }
        }
#endif
        else if (peekChar == 'g' && input.length() > 1) {
            if (input.startsWith("gamma")) {
                handledMulti = true;
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto& cfg = engine.getConfig();

                if (input == "gamma") {
                    Serial.printf("Gamma: %s, value=%.1f\n",
                                  cfg.gammaEnabled ? "ON" : "OFF",
                                  cfg.gammaValue);
                } else if (input.length() > 5) {
                    // Handle both "gamma1.5" and "gamma 1.5" formats
                    int startIdx = 5;
                    while (startIdx < input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                        startIdx++;
                    }
                    if (startIdx < input.length()) {
                        float val = input.substring(startIdx).toFloat();
                        if (val == 0) {
                            cfg.gammaEnabled = false;
                            Serial.println("Gamma: OFF");
                        } else if (val >= 1.0f && val <= 3.0f) {
                            cfg.gammaEnabled = true;
                            cfg.gammaValue = val;
                            Serial.printf("Gamma set to: %.1f\n", val);
                        } else {
                            Serial.println("Invalid gamma. Use 0 (off) or 1.0-3.0");
                        }
                    }
                }
            }
        }
        // -----------------------------------------------------------------
        // Debug Commands: dbg
        // -----------------------------------------------------------------
        else if (peekChar == 'd' && input.length() >= 3) {
            if (inputLower.startsWith("dbg")) {
                handledMulti = true;
                auto& cfg = lightwaveos::config::getDebugConfig();
                String subcmd = inputLower.substring(3);
                subcmd.trim();

                if (subcmd.length() == 0) {
                    // dbg - show config
                    lightwaveos::config::printDebugConfig();
                }
                else if (subcmd.length() == 1 && subcmd[0] >= '0' && subcmd[0] <= '5') {
                    // dbg <0-5> - set global level
                    cfg.globalLevel = subcmd[0] - '0';
                    Serial.printf("Global debug level: %d (%s)\n",
                                  cfg.globalLevel,
                                  lightwaveos::config::DebugConfig::levelName(cfg.globalLevel));
                }
                else if (subcmd.startsWith("audio ")) {
                    // dbg audio <0-5>
                    int level = subcmd.substring(6).toInt();
                    if (level >= 0 && level <= 5) {
                        cfg.setDomainLevel(lightwaveos::config::DebugDomain::AUDIO, level);
                        Serial.printf("Audio debug level: %d (%s)\n", level,
                                      lightwaveos::config::DebugConfig::levelName((uint8_t)level));
#if FEATURE_AUDIO_SYNC
                        // Also sync with legacy AudioDebugConfig for backwards compatibility
                        lightwaveos::audio::getAudioDebugConfig().verbosity = level;
#endif
                    } else {
                        Serial.println("Invalid level. Use 0-5.");
                    }
                }
                else if (subcmd.startsWith("render ")) {
                    // dbg render <0-5>
                    int level = subcmd.substring(7).toInt();
                    if (level >= 0 && level <= 5) {
                        cfg.setDomainLevel(lightwaveos::config::DebugDomain::RENDER, level);
                        Serial.printf("Render debug level: %d (%s)\n", level,
                                      lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                    } else {
                        Serial.println("Invalid level. Use 0-5.");
                    }
                }
                else if (subcmd.startsWith("network ")) {
                    // dbg network <0-5>
                    int level = subcmd.substring(8).toInt();
                    if (level >= 0 && level <= 5) {
                        cfg.setDomainLevel(lightwaveos::config::DebugDomain::NETWORK, level);
                        Serial.printf("Network debug level: %d (%s)\n", level,
                                      lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                    } else {
                        Serial.println("Invalid level. Use 0-5.");
                    }
                }
                else if (subcmd.startsWith("actor ")) {
                    // dbg actor <0-5>
                    int level = subcmd.substring(6).toInt();
                    if (level >= 0 && level <= 5) {
                        cfg.setDomainLevel(lightwaveos::config::DebugDomain::ACTOR, level);
                        Serial.printf("Actor debug level: %d (%s)\n", level,
                                      lightwaveos::config::DebugConfig::levelName((uint8_t)level));
                    } else {
                        Serial.println("Invalid level. Use 0-5.");
                    }
                }
                else if (subcmd.startsWith("motion ")) {
                    // dbg motion <0-5>
                    int level = subcmd.substring(7).toInt();
                    if (level >= 0 && level <= 5) {
                        cfg.setDomainLevel(lightwaveos::config::DebugDomain::MOTION, level);
                        Serial.printf("Motion debug level: %d (%s)\n", level,
                                      lightwaveos::config::DebugConfig::levelName(static_cast<uint8_t>(level)));
                    } else {
                        Serial.println("Invalid level (0-5)");
                    }
                }
                else if (subcmd == "motion") {
                    // One-shot motion-semantic field print
#if FEATURE_AUDIO_SYNC
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        lightwaveos::audio::ControlBusFrame bus;
                        audio->getControlBusBuffer().ReadLatest(bus);
                        Serial.println("\n=== Motion-Semantic Fields ===");
                        Serial.println("  --- Layer 1 (raw DSP) ---");
                        Serial.printf("  timing_jitter:    %.4f\n", bus.timing_jitter);
                        Serial.printf("  syncopation_level:%.4f\n", bus.syncopation_level);
                        Serial.printf("  pitch_contour_dir:%.4f\n", bus.pitch_contour_dir);
                        auto* ren = actors.getRenderer();
                        if (ren) {
                            const auto& mf = ren->getMotionFrame();
                            Serial.println("  --- Layer 2 (Laban 6-axis) ---");
                            Serial.printf("  Weight:    %.3f\n", mf.weight);
                            Serial.printf("  Time:      %.3f\n", mf.time_quality);
                            Serial.printf("  Space:     %.3f\n", mf.space);
                            Serial.printf("  Flow:      %.3f\n", mf.flow);
                            Serial.printf("  Fluidity:  %.3f\n", mf.fluidity);
                            Serial.printf("  Impulse:   %.3f\n", mf.impulse_strength);
                            Serial.printf("  Confidence:%u\n", mf.confidence_min);
                            const auto& ms = ren->getMotionShaping();
                            Serial.println("  --- Layer 3 (temporal shaping) ---");
                            Serial.printf("  Intensity: %.3f\n", ms.intensity);
                            Serial.printf("  DecayMs:   %.0f\n", ms.decayMs);
                            Serial.printf("  Accent:    %.2f\n", ms.accentScale);
                            Serial.printf("  EnvType:   %u\n", ms.envType);
                            Serial.printf("  Active:    %s\n", ms.active ? "yes" : "no");
                        }
                        Serial.println();
                    } else {
                        Serial.println("[DBG] Audio not available");
                    }
#else
                    Serial.println("[DBG] Audio not enabled in this build");
#endif
                }
                else if (subcmd == "status") {
                    // One-shot status print - use AudioActor's printStatus() method
#if FEATURE_AUDIO_SYNC
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printStatus();
                    } else {
                        Serial.println("[DBG] Audio not available");
                    }
#else
                    Serial.println("[DBG] Audio not enabled in this build");
#endif
                }
                else if (subcmd == "spectrum") {
                    // One-shot spectrum print - use AudioActor's printSpectrum() method
#if FEATURE_AUDIO_SYNC
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printSpectrum();
                    } else {
                        Serial.println("[DBG] Audio not available");
                    }
#else
                    Serial.println("[DBG] Audio not enabled in this build");
#endif
                }
                else if (subcmd == "beat") {
                    // One-shot beat print - use AudioActor's printBeat() method
#if FEATURE_AUDIO_SYNC
                    auto* audio = ::actors.getAudio();
                    if (audio) {
                        audio->printBeat();
                    } else {
                        Serial.println("[DBG] Audio not available");
                    }
#else
                    Serial.println("[DBG] Audio not enabled in this build");
#endif
                }
                else if (subcmd == "memory") {
                    // One-shot memory print
                    Serial.println("\n=== Memory Status ===");
                    Serial.printf("  Free heap: %lu bytes\n", ESP.getFreeHeap());
                    Serial.printf("  Min free heap: %lu bytes\n", ESP.getMinFreeHeap());
                    Serial.printf("  Max alloc heap: %lu bytes\n", ESP.getMaxAllocHeap());
#ifdef CONFIG_SPIRAM_SUPPORT
                    Serial.printf("  Free PSRAM: %lu bytes\n", ESP.getFreePsram());
#endif
                    Serial.println();
                }
                else if (subcmd.startsWith("interval ")) {
                    // dbg interval status <N> or dbg interval spectrum <N>
                    String rest = subcmd.substring(9);
                    rest.trim();
                    if (rest.startsWith("status ")) {
                        cfg.statusIntervalSec = rest.substring(7).toInt();
                        if (cfg.statusIntervalSec > 0) {
                            Serial.printf("Status interval: %u seconds\n", cfg.statusIntervalSec);
                        } else {
                            Serial.println("Status interval: disabled");
                        }
                    }
                    else if (rest.startsWith("spectrum ")) {
                        cfg.spectrumIntervalSec = rest.substring(9).toInt();
                        if (cfg.spectrumIntervalSec > 0) {
                            Serial.printf("Spectrum interval: %u seconds\n", cfg.spectrumIntervalSec);
                        } else {
                            Serial.println("Spectrum interval: disabled");
                        }
                    }
                    else {
                        Serial.println("Usage: dbg interval <status|spectrum> <seconds>");
                    }
                }
                else {
                    Serial.println("Usage: dbg [0-5|audio|render|network|actor|motion|status|spectrum|beat|memory|interval]");
                    Serial.println("  dbg           - Show debug config");
                    Serial.println("  dbg <0-5>     - Set global level");
                    Serial.println("  dbg audio <0-5>   - Set audio domain level");
                    Serial.println("  dbg render <0-5>  - Set render domain level");
                    Serial.println("  dbg network <0-5> - Set network domain level");
                    Serial.println("  dbg actor <0-5>   - Set actor domain level");
                    Serial.println("  dbg motion <0-5>  - Set motion-semantic domain level");
                    Serial.println("  dbg motion        - Print motion-semantic fields NOW");
                    Serial.println("  dbg status    - Print audio health NOW");
                    Serial.println("  dbg spectrum  - Print spectrum NOW");
                    Serial.println("  dbg beat      - Print beat tracking NOW");
                    Serial.println("  dbg memory    - Print heap/stack NOW");
                    Serial.println("  dbg interval status <N>   - Auto status every N sec (0=off)");
                    Serial.println("  dbg interval spectrum <N> - Auto spectrum every N sec (0=off)");
                }
            }
        }
        else if (peekChar == 'b' && input.length() > 1) {
            if (input.startsWith("brown")) {
                handledMulti = true;
                auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                auto& cfg = engine.getConfig();

                if (input == "brown") {
                    Serial.printf("Brown guardrail: %s\n",
                                  cfg.brownGuardrailEnabled ? "ON" : "OFF");
                    Serial.printf("  Max green: %d%% of red\n", cfg.maxGreenPercentOfRed);
                    Serial.printf("  Max blue: %d%% of red\n", cfg.maxBluePercentOfRed);
                } else if (input.length() > 5) {
                    // Handle both "brown0"/"brown1" and "brown 0"/"brown 1" formats
                    int startIdx = 5;
                    while (startIdx < input.length() && (input[startIdx] == ' ' || input[startIdx] == '\t')) {
                        startIdx++;
                    }
                    if (startIdx < input.length()) {
                        String arg = input.substring(startIdx);
                        if (arg == "0") {
                            cfg.brownGuardrailEnabled = false;
                            Serial.println("Brown guardrail: OFF");
                        } else if (arg == "1") {
                            cfg.brownGuardrailEnabled = true;
                            Serial.println("Brown guardrail: ON");
                        }
                    }
                }
            }
        }
        else if (peekChar == 'C' && input.length() > 1) {
            if (input == "Csave") {
                handledMulti = true;
                lightwaveos::enhancement::ColorCorrectionEngine::getInstance().saveToNVS();
                Serial.println("Color correction settings saved to NVS");
            }
        }
        else if ((peekChar == 'v' || peekChar == 'V') && input.length() > 1) {
            if (inputLower.startsWith("validate ")) {
                handledMulti = true;

                // Parse effect ID - accepts display index or direct EffectId
                int rawId = input.substring(9).toInt();
                uint16_t effectCount = renderer->getEffectCount();
                EffectId effectId;
                if (rawId >= 0 && rawId < effectCount) {
                    effectId = renderer->getEffectIdAt(rawId);
                } else {
                    effectId = static_cast<EffectId>(rawId);
                }

                if (!renderer->isEffectRegistered(effectId)) {
                    Serial.printf("ERROR: Effect 0x%04X not registered\n", effectId);
                } else {
                    // Save current effect
                    EffectId savedEffect = renderer->getCurrentEffect();
                    const char* effectName = renderer->getEffectName(effectId);

                    // Memory baseline
                    uint32_t heapBefore = ESP.getFreeHeap();

                    Serial.printf("\n=== Validating Effect: %s (ID %d) ===\n", effectName, effectId);

                    // Switch to effect temporarily
                    actors.setEffect(effectId);
                    delay(100);  // Allow effect to initialize

                    // Validation checks
                    bool centreOriginPass = false;
                    bool hueSpanPass = false;
                    bool frameRatePass = false;
                    bool memoryPass = false;

                    // Centre-origin check: Measure brightness at LEDs 79-80 vs edges
                    uint32_t centreBrightnessSum = 0;
                    uint32_t edgeBrightnessSum = 0;
                    uint16_t centreSamples = 0;
                    uint16_t edgeSamples = 0;

                    // Hue span tracking
                    float minHue = 360.0f;
                    float maxHue = 0.0f;
                    bool hueInitialized = false;

                    // Capture LED buffer without heap allocations
                    constexpr uint16_t TOTAL_LEDS = 320;  // 2 strips × 160 LEDs
                    CRGB* ledBuffer = s_validationFrameScratch;

                    // Capture for 2 seconds total:
                    // - first 1 second: brightness + hue sampling
                    // - full 2 seconds: FPS stabilisation
                    uint32_t validationStart = millis();
                    uint32_t validationEnd = validationStart + 2000;
                    uint16_t samplesCollected = 0;

                    while (millis() < validationEnd) {
                        delay(8);  // ~120 FPS pacing

                        // Snapshot the LED buffer
                        renderer->getBufferCopy(ledBuffer);

                        // Only sample brightness/hue for first second to limit compute cost
                        if (millis() - validationStart < 1000 && samplesCollected < 120) {
                            samplesCollected++;

                            // Centre-origin: LEDs 79-80 vs edges (0-10, 150-159)
                            for (int i = 79; i <= 80; i++) {
                                uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                                centreBrightnessSum += brightness;
                                centreSamples++;
                            }

                            for (int i = 0; i <= 10; i++) {
                                uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                                edgeBrightnessSum += brightness;
                                edgeSamples++;
                            }
                            for (int i = 150; i <= 159; i++) {
                                uint16_t brightness = (uint16_t)ledBuffer[i].r + ledBuffer[i].g + ledBuffer[i].b;
                                edgeBrightnessSum += brightness;
                                edgeSamples++;
                            }

                            // Hue span: convert RGB to hue (degrees), track min/max
                            for (int i = 0; i < TOTAL_LEDS; i++) {
                                CRGB rgb = ledBuffer[i];
                                if (rgb.r == 0 && rgb.g == 0 && rgb.b == 0) continue;  // Skip black

                                uint8_t maxVal = max(max(rgb.r, rgb.g), rgb.b);
                                uint8_t minVal = min(min(rgb.r, rgb.g), rgb.b);
                                if (maxVal == 0 || maxVal == minVal) continue;

                                float delta = (maxVal - minVal) / 255.0f;
                                float hue = 0.0f;

                                if (maxVal == rgb.r) {
                                    hue = 60.0f * fmod((((rgb.g - rgb.b) / 255.0f) / delta), 6.0f);
                                } else if (maxVal == rgb.g) {
                                    hue = 60.0f * (((rgb.b - rgb.r) / 255.0f) / delta + 2.0f);
                                } else {
                                    hue = 60.0f * (((rgb.r - rgb.g) / 255.0f) / delta + 4.0f);
                                }
                                if (hue < 0) hue += 360.0f;

                                if (!hueInitialized) {
                                    minHue = maxHue = hue;
                                    hueInitialized = true;
                                } else {
                                    if (hue < minHue) minHue = hue;
                                    if (hue > maxHue) maxHue = hue;
                                }
                            }
                        }
                    }

                    // Frame rate check (after 2s run)
                    const RenderStats& finalStats = renderer->getStats();
                    frameRatePass = (finalStats.currentFPS >= 120);

                    // Centre-origin validation
                    float centreAvg = 0.0f;
                    float edgeAvg = 0.0f;
                    float ratio = 0.0f;
                    if (centreSamples > 0 && edgeSamples > 0) {
                        centreAvg = centreBrightnessSum / (float)centreSamples;
                        edgeAvg = edgeBrightnessSum / (float)edgeSamples;
                        ratio = (edgeAvg > 0.0f) ? (centreAvg / edgeAvg) : 0.0f;
                        centreOriginPass = (centreAvg > edgeAvg * 1.2f);
                    }

                    // Hue span validation
                    float hueSpan = 0.0f;
                    if (hueInitialized) {
                        hueSpan = maxHue - minHue;
                        if (hueSpan > 180.0f) hueSpan = 360.0f - hueSpan;  // wrap-around
                        hueSpanPass = (hueSpan < 60.0f);
                    }

                    // Memory check (heap delta should be near-zero; allow small drift)
                    uint32_t heapAfter = ESP.getFreeHeap();
                    int32_t heapDelta = (int32_t)heapAfter - (int32_t)heapBefore;
                    memoryPass = (abs(heapDelta) <= 256);

                    // Output validation report
                    Serial.println("\n--- Validation Results ---");
                    Serial.printf("[%s] Centre-origin: %s (centre: %.0f, edge: %.0f, ratio: %.2fx)\n",
                                  centreOriginPass ? "✓" : "✗",
                                  centreOriginPass ? "PASS" : "FAIL",
                                  centreAvg, edgeAvg, ratio);

                    Serial.printf("[%s] Hue span: %s (hue range: %.1f°, limit: <60°)\n",
                                  hueSpanPass ? "✓" : "✗",
                                  hueSpanPass ? "PASS" : "FAIL",
                                  hueSpan);

                    Serial.printf("[%s] Frame rate: %s (%d FPS, target: ≥120 FPS)\n",
                                  frameRatePass ? "✓" : "✗",
                                  frameRatePass ? "PASS" : "FAIL",
                                  finalStats.currentFPS);

                    Serial.printf("[%s] Memory: %s (free heap Δ %ld bytes)\n",
                                  memoryPass ? "✓" : "✗",
                                  memoryPass ? "PASS" : "FAIL",
                                  (long)heapDelta);

                    int passCount = (centreOriginPass ? 1 : 0) + (hueSpanPass ? 1 : 0) +
                                    (frameRatePass ? 1 : 0) + (memoryPass ? 1 : 0);
                    int totalChecks = 4;

                    Serial.printf("\nOverall: %s (%d/%d checks passed)\n",
                                  (passCount == totalChecks) ? "PASS" : "PARTIAL",
                                  passCount, totalChecks);

                    // Restore original effect
                    actors.setEffect(savedEffect);
                    delay(50);

                    Serial.println("========================================\n");
                }
            } else {
                // Treat non-validate 'v...' input as handled to avoid consuming and then reading a stale single-char command
                handledMulti = true;
                Serial.println("Unknown command. Use: validate <effect_id>");
            }
        }

        // -----------------------------------------------------------------
        // Trace Commands: trace (MabuTrace Perfetto dump)
        // -----------------------------------------------------------------
#if FEATURE_MABUTRACE
        else if (inputLower == "trace") {
            handledMulti = true;
            Serial.println(F("[TRACE] Flushing trace buffer..."));
            TRACE_FLUSH();
            get_json_trace_chunked(nullptr, [](void* /*ctx*/, const char* chunk, size_t len) {
                Serial.write(chunk, len);
            });
            Serial.println();
            Serial.println(F("[TRACE] Done."));
        }
#endif

        // -----------------------------------------------------------------
        // Tempo Debug Commands: tempo
        // -----------------------------------------------------------------
#if FEATURE_AUDIO_SYNC && !FEATURE_AUDIO_BACKEND_ESV11 && !FEATURE_AUDIO_BACKEND_PIPELINECORE
        else if (inputLower.startsWith("tempo")) {
            handledMulti = true;

            // Get TempoTracker from AudioActor
            auto* audio = ::actors.getAudio();
            if (audio) {
                const auto& tempo = audio->getTempo();
                auto output = tempo.getOutput();
                Serial.println("=== TempoTracker Status ===");
                Serial.printf("  BPM: %.1f\n", output.bpm);
                Serial.printf("  Phase: %.3f\n", output.phase01);
                Serial.printf("  Confidence: %.2f\n", output.confidence);
                Serial.printf("  Locked: %s\n", output.locked ? "YES" : "NO");
                Serial.printf("  Beat Strength: %.2f\n", output.beat_strength);
                Serial.printf("  Winner Bin: %u\n", tempo.getWinnerBin());
            } else {
                Serial.println("TempoTracker not available (audio not enabled)");
            }
        }
#endif

        // -----------------------------------------------------------------
        // Zone Speed Commands: zs
        // -----------------------------------------------------------------
        else if (inputLower.startsWith("zs ")) {
            handledMulti = true;

            if (!zoneComposer.isEnabled()) {
                Serial.println("ERROR: Zone mode not enabled. Press 'z' to enable.");
            } else {
                // Parse: "zs <zoneId> <speed>" or "zs <speed0> <speed1> <speed2>"
                String args = input.substring(3); // After "zs "
                args.trim();

                int zoneId = -1;
                int speed0 = -1, speed1 = -1, speed2 = -1;

                // Try parsing as "zs <zoneId> <speed>"
                int firstSpace = args.indexOf(' ');
                if (firstSpace > 0) {
                    String first = args.substring(0, firstSpace);
                    String rest = args.substring(firstSpace + 1);
                    rest.trim();

                    int parsedZone = first.toInt();
                    int parsedSpeed = rest.toInt();

                    if (parsedZone >= 0 && parsedZone < zoneComposer.getZoneCount() &&
                        parsedSpeed >= 1 && parsedSpeed <= 50) {
                        zoneId = parsedZone;
                        zoneComposer.setZoneSpeed(zoneId, parsedSpeed);
                        Serial.printf("Zone %d speed set to %d\n", zoneId, parsedSpeed);
                    } else {
                        Serial.println("ERROR: Usage: zs <zoneId> <speed> (zoneId: 0-2, speed: 1-50)");
                    }
                } else {
                    // Try parsing as "zs <speed0> <speed1> <speed2>"
                    int space1 = args.indexOf(' ');
                    if (space1 > 0) {
                        String s0 = args.substring(0, space1);
                        String rest1 = args.substring(space1 + 1);
                        rest1.trim();
                        int space2 = rest1.indexOf(' ');
                        if (space2 > 0) {
                            String s1 = rest1.substring(0, space2);
                            String s2 = rest1.substring(space2 + 1);
                            s2.trim();

                            speed0 = s0.toInt();
                            speed1 = s1.toInt();
                            speed2 = s2.toInt();

                            if (speed0 >= 1 && speed0 <= 50 &&
                                speed1 >= 1 && speed1 <= 50 &&
                                speed2 >= 1 && speed2 <= 50 &&
                                zoneComposer.getZoneCount() >= 3) {
                                zoneComposer.setZoneSpeed(0, speed0);
                                zoneComposer.setZoneSpeed(1, speed1);
                                zoneComposer.setZoneSpeed(2, speed2);
                                Serial.printf("Zone speeds set: Zone 0=%d, Zone 1=%d, Zone 2=%d\n",
                                            speed0, speed1, speed2);
                            } else {
                                Serial.println("ERROR: Usage: zs <speed0> <speed1> <speed2> (speeds: 1-50)");
                            }
                        } else {
                            Serial.println("ERROR: Usage: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>");
                        }
                    } else {
                        Serial.println("ERROR: Usage: zs <zoneId> <speed> OR zs <speed0> <speed1> <speed2>");
                    }
                }
            }
        }

        // -----------------------------------------------------------------
        // WiFi commands: wifi, wifi connect SSID PASS, wifi ap, wifi scan
        // Uses original `input` (not inputLower) to preserve SSID case.
        // -----------------------------------------------------------------
        else if (inputLower.startsWith("wifi")) {
            handledMulti = true;
            // Use original input to preserve SSID/password case
            String args = input.substring(4);
            args.trim();
            String argsLower = args;
            argsLower.toLowerCase();

            if (args.length() == 0 || argsLower == "status") {
                // Print WiFi status
                Serial.println("\n=== WiFi Status ===");
                Serial.printf("  Mode: %s\n",
                    WiFi.getMode() == WIFI_MODE_AP ? "AP" :
                    WiFi.getMode() == WIFI_MODE_STA ? "STA" :
                    WiFi.getMode() == WIFI_MODE_APSTA ? "AP+STA" : "OFF");
                if (WiFi.isConnected()) {
                    Serial.printf("  Connected: YES\n");
                    Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
                    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
                    Serial.printf("  Channel: %d\n", WiFi.channel());
                } else {
                    Serial.printf("  Connected: NO\n");
                }
                Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
                Serial.printf("  AP Clients: %d\n", WiFi.softAPgetStationNum());
                // List saved networks (NVS)
                WiFiCredentialsStorage::NetworkCredential nets[8];
                uint8_t count = WIFI_MANAGER.getSavedNetworks(nets, 8);
                Serial.printf("  Saved Networks (NVS): %d\n", count);
                for (uint8_t i = 0; i < count; i++) {
                    Serial.printf("    [%d] %s\n", i, nets[i].ssid.c_str());
                }
                // Show compile-time config SSIDs
                using namespace lightwaveos::config;
                Serial.printf("  Config SSID 1: %s\n", NetworkConfig::WIFI_SSID_VALUE);
                if (strlen(NetworkConfig::WIFI_SSID_2_VALUE) > 0)
                    Serial.printf("  Config SSID 2: %s\n", NetworkConfig::WIFI_SSID_2_VALUE);
                Serial.println();
            }
            else if (argsLower.startsWith("connect")) {
                // Parse: "connect SSID PASSWORD" or "connect SSID" (for saved) or "connect" (auto)
                String connectArgs = args.substring(7); // after "connect"
                connectArgs.trim();

                // Split into SSID and PASSWORD by first space
                String ssid = "";
                String password = "";
                int spaceIdx = connectArgs.indexOf(' ');
                if (spaceIdx > 0) {
                    ssid = connectArgs.substring(0, spaceIdx);
                    password = connectArgs.substring(spaceIdx + 1);
                    password.trim();
                } else {
                    ssid = connectArgs;
                }

                // No SSID given -- try last connected or first saved
                if (ssid.length() == 0) {
                    String lastSSID = WIFI_MANAGER.getLastConnectedSSID();
                    if (lastSSID.length() > 0) {
                        ssid = lastSSID;
                    } else {
                        WiFiCredentialsStorage::NetworkCredential nets[8];
                        uint8_t count = WIFI_MANAGER.getSavedNetworks(nets, 8);
                        if (count > 0) ssid = nets[0].ssid;
                    }
                }

                if (ssid.length() == 0) {
                    Serial.println("ERROR: No SSID specified and no saved networks");
                    Serial.println("Usage: wifi connect SSID PASSWORD");
                } else if (password.length() > 0) {
                    // Have both SSID and password -- connect directly and save to NVS
                    Serial.printf("Connecting to '%s' (saving to NVS)...\n", ssid.c_str());
                    bool ok = WIFI_MANAGER.connectToNetwork(ssid, password);
                    if (ok) {
                        Serial.println("Connection initiated (AP+STA). Check 'wifi' in ~10s.");
                    } else {
                        Serial.println("Connect request failed");
                    }
                } else {
                    // SSID only -- try saved credentials (NVS)
                    Serial.printf("Connecting to saved network: '%s'...\n", ssid.c_str());
                    bool ok = WIFI_MANAGER.connectToSavedNetwork(ssid);
                    if (ok) {
                        Serial.println("Connection initiated (AP+STA). Check 'wifi' in ~10s.");
                    } else {
                        Serial.printf("'%s' not in NVS. Use: wifi connect %s YOUR_PASSWORD\n", ssid.c_str(), ssid.c_str());
                    }
                }
            }
            else if (argsLower == "ap") {
                Serial.println("Switching to AP-only mode...");
                WIFI_MANAGER.requestAPOnly();
                Serial.println("AP-only mode active");
            }
            else if (argsLower == "scan") {
                Serial.println("Triggering WiFi scan...");
                WIFI_MANAGER.scanNetworks();
                Serial.println("Scan initiated. Check 'wifi' in ~5s for results.");
            }
            else {
                Serial.println("WiFi commands:");
                Serial.println("  wifi                        -- show status");
                Serial.println("  wifi connect SSID PASSWORD  -- connect + save to NVS");
                Serial.println("  wifi connect SSID           -- connect using saved NVS creds");
                Serial.println("  wifi connect                -- reconnect to last/first saved");
                Serial.println("  wifi ap                     -- switch to AP-only mode");
                Serial.println("  wifi scan                   -- trigger network scan");
            }
        }

        if (!handledMulti && input.length() == 1) {
            char cmd = input[0];

            // Check if in zone mode for special handling
            bool inZoneMode = zoneComposer.isEnabled();

            // Zone mode: 1-5 selects presets
            if (inZoneMode && cmd >= '1' && cmd <= '5') {
                uint8_t preset = cmd - '1';
                zoneComposer.loadPreset(preset);
                Serial.printf("Zone Preset: %s\n", ZoneComposer::getPresetName(preset));
            }
            // Numeric and alpha effect selection (single mode)
            else if (!inZoneMode && cmd >= '0' && cmd <= '9') {
                uint8_t e = cmd - '0';
                
                // Special case: '6' cycles through audio effects (72=Waveform, 73=Bloom)
                if (e == 6) {
                    const EffectId audioEffects[] = {lightwaveos::EID_AUDIO_WAVEFORM, lightwaveos::EID_AUDIO_BLOOM};
                    const uint8_t audioEffectCount = sizeof(audioEffects) / sizeof(audioEffects[0]);

                    // Cycle to next audio effect
                    lastAudioEffectIndex = (lastAudioEffectIndex + 1) % audioEffectCount;
                    EffectId audioEffectId = audioEffects[lastAudioEffectIndex];

                    if (renderer->isEffectRegistered(audioEffectId)) {
                        currentEffect = audioEffectId;
                        actors.setEffect(audioEffectId);
                        Serial.printf("Audio Effect 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", audioEffectId, renderer->getEffectName(audioEffectId));
                    } else {
                        Serial.printf("ERROR: Audio effect 0x%04X not registered\n", audioEffectId);
                    }
                } else {
                    // Normal numeric effect selection (0-5, 7-9) - map to display order
                    EffectId eid = (e < lightwaveos::DISPLAY_COUNT) ? lightwaveos::DISPLAY_ORDER[e] : lightwaveos::INVALID_EFFECT_ID;
                if (renderer->isEffectRegistered(eid)) {
                    currentEffect = eid;
                    actors.setEffect(eid);
                    Serial.printf("Effect [%d] 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", e, eid, renderer->getEffectName(eid));
                    }
                }
            } else {
                // Check if this is an effect selection key (a-k = effects 10-20, excludes command letters)
                // Command letters: c, e, g, n, l, p, s, t, z are handled in switch below
                bool isEffectKey = false;
                if (!inZoneMode && cmd >= 'a' && cmd <= 'k' &&
                    cmd != 'a' && cmd != 'b' && cmd != 'c' && cmd != 'd' && cmd != 'e' && cmd != 'f' && cmd != 'g' &&
                    cmd != 'h' && cmd != 'i' && cmd != 'j' && cmd != 'k' &&
                    cmd != 'n' && cmd != 'l' && cmd != 'p' && cmd != 's' && cmd != 't' && cmd != 'z') {
                    uint16_t displayIdx = 10 + (cmd - 'a');
                    EffectId eid = (displayIdx < lightwaveos::DISPLAY_COUNT) ? lightwaveos::DISPLAY_ORDER[displayIdx] : lightwaveos::INVALID_EFFECT_ID;
                    if (renderer->isEffectRegistered(eid)) {
                        currentEffect = eid;
                        actors.setEffect(eid);
                        Serial.printf("Effect [%d] 0x%04X: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", displayIdx, eid, renderer->getEffectName(eid));
                        isEffectKey = true;
                    }
                }

                if (!isEffectKey)
                switch (cmd) {
#if FEATURE_AUDIO_SYNC
                case 'x': case 'X':
                    // Bands observability (same as top-level "x" / "bands")
                    {
                        RendererActor* ren = actors.getRenderer();
                        if (ren) {
                            RendererActor::BandsDebugSnapshot snap;
                            ren->getBandsDebugSnapshot(snap);
                            if (snap.valid) {
                                Serial.println("\n=== Bands (renderer snapshot) ===");
                                Serial.printf("  bands[0..7]: %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                    snap.bands[0], snap.bands[1], snap.bands[2], snap.bands[3],
                                    snap.bands[4], snap.bands[5], snap.bands[6], snap.bands[7]);
                                Serial.printf("  bass=%.3f mid=%.3f treble=%.3f (ctx.audio accessors)\n",
                                    snap.bass, snap.mid, snap.treble);
                                Serial.printf("  rms=%.3f flux=%.3f hop_seq=%lu\n",
                                    snap.rms, snap.flux, (unsigned long)snap.hop_seq);
                            } else {
                                Serial.println("Bands: no live snapshot (audio buffer or frame not ready)");
                            }
                        } else {
                            Serial.println("Renderer not available");
                        }
                    }
                    break;
#endif
                case 'z':
                    // Toggle zone mode
                    zoneComposer.setEnabled(!zoneComposer.isEnabled());
                    Serial.printf("Zone Mode: %s\n",
                                  zoneComposer.isEnabled() ? "ENABLED" : "DISABLED");
                    if (zoneComposer.isEnabled()) {
                        Serial.println("  Press 1-5 to load presets");
                    }
                    break;

                case 'Z':
                    // Print zone status
                    zoneComposer.printStatus();
                    break;

                case 'S':
                    // Save all settings to NVS
                    if (zoneConfigMgr) {
                        Serial.println("Saving settings to NVS...");
                        bool zoneOk = zoneConfigMgr->saveToNVS();
                        bool sysOk = zoneConfigMgr->saveSystemState(
                            renderer->getCurrentEffect(),
                            renderer->getBrightness(),
                            renderer->getSpeed(),
                            renderer->getPaletteIndex()
                        );
                        if (zoneOk && sysOk) {
                            Serial.println("  All settings saved!");
                        } else {
                            Serial.printf("  Save result: zones=%s, system=%s\n",
                                          zoneOk ? "OK" : "FAIL",
                                          sysOk ? "OK" : "FAIL");
                        }
                    } else {
                        Serial.println("ERROR: Config manager not initialized");
                    }
                    break;

                case '`':
#if !defined(NATIVE_BUILD) && FEATURE_STATUS_STRIP_TOUCH
                    statusStripNextIdleMode();
#endif
                    break;

                case ' ':  // Spacebar - quick next effect (no Enter needed)
                case 'n':
                    if (!inZoneMode) {
                        EffectId newEffectId = currentEffect;

                        switch (currentRegister) {
                            case EffectRegister::ALL:
                                newEffectId = lightwaveos::getNextDisplay(currentEffect);
                                break;

                            case EffectRegister::REACTIVE:
                                if (PatternRegistry::getReactiveEffectCount() > 0) {
                                    reactiveRegisterIndex = (reactiveRegisterIndex + 1) %
                                                            PatternRegistry::getReactiveEffectCount();
                                    newEffectId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                                }
                                break;

                            case EffectRegister::AMBIENT:
                                if (ambientEffectCount > 0) {
                                    ambientRegisterIndex = (ambientRegisterIndex + 1) % ambientEffectCount;
                                    newEffectId = ambientEffectIds[ambientRegisterIndex];
                                }
                                break;
                        }

                        if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                            currentEffect = newEffectId;
                            actors.setEffect(currentEffect);
                            const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                                 (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                            Serial.printf("Effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                          currentEffect, suffix, renderer->getEffectName(currentEffect));
                        }
                    }
                    break;

                case 'N':
                    if (!inZoneMode) {
                        EffectId newEffectId = currentEffect;

                        switch (currentRegister) {
                            case EffectRegister::ALL:
                                newEffectId = lightwaveos::getPrevDisplay(currentEffect);
                                break;

                            case EffectRegister::REACTIVE: {
                                uint8_t count = PatternRegistry::getReactiveEffectCount();
                                if (count > 0) {
                                    reactiveRegisterIndex = (reactiveRegisterIndex + count - 1) % count;
                                    newEffectId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                                }
                                break;
                            }

                            case EffectRegister::AMBIENT:
                                if (ambientEffectCount > 0) {
                                    ambientRegisterIndex = (ambientRegisterIndex + ambientEffectCount - 1) %
                                                           ambientEffectCount;
                                    newEffectId = ambientEffectIds[ambientRegisterIndex];
                                }
                                break;
                        }

                        if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                            currentEffect = newEffectId;
                            actors.setEffect(currentEffect);
                            const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                                 (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                            Serial.printf("Effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                          currentEffect, suffix, renderer->getEffectName(currentEffect));
                        }
                    }
                    break;

                case 'r':  // Switch to Reactive register (audio-reactive effects)
                    currentRegister = EffectRegister::REACTIVE;
                    Serial.println("Switched to " LW_CLR_CYAN "Reactive" LW_ANSI_RESET " register");
                    Serial.printf("  %d audio-reactive effects available\n",
                                  PatternRegistry::getReactiveEffectCount());
                    // Switch to current reactive effect
                    if (PatternRegistry::getReactiveEffectCount() > 0) {
                        EffectId reactiveId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                        if (reactiveId != lightwaveos::INVALID_EFFECT_ID) {
                            currentEffect = reactiveId;
                            actors.setEffect(reactiveId);
                            Serial.printf("  Current: %s (ID 0x%04X)\n",
                                          renderer->getEffectName(reactiveId), reactiveId);
                        }
                    }
                    break;

                case 'm':  // Switch to aMbient register (time-based effects)
                    currentRegister = EffectRegister::AMBIENT;
                    Serial.println("Switched to " LW_CLR_MAGENTA "Ambient" LW_ANSI_RESET " register");
                    Serial.printf("  %d ambient effects available\n", ambientEffectCount);
                    // Switch to current ambient effect
                    if (ambientEffectCount > 0 && ambientRegisterIndex < ambientEffectCount) {
                        EffectId ambientId = ambientEffectIds[ambientRegisterIndex];
                        if (ambientId != lightwaveos::INVALID_EFFECT_ID) {
                            currentEffect = ambientId;
                            actors.setEffect(ambientId);
                            Serial.printf("  Current: %s (ID 0x%04X)\n",
                                          renderer->getEffectName(ambientId), ambientId);
                        }
                    }
                    break;

                case '*':  // Switch back to All effects register (default)
                    currentRegister = EffectRegister::ALL;
                    Serial.println("Switched to " LW_CLR_GREEN "All Effects" LW_ANSI_RESET " register");
                    Serial.printf("  %d effects available\n", renderer->getEffectCount());
                    Serial.printf("  Current: %s (ID 0x%04X)\n",
                                  renderer->getEffectName(currentEffect), currentEffect);
                    break;

                case 'L': {  // Jump to last effect in current register
                    EffectId newEffectId = lightwaveos::INVALID_EFFECT_ID;

                    switch (currentRegister) {
                        case EffectRegister::ALL: {
                            // Last effect in display order
                            uint16_t dc = lightwaveos::DISPLAY_COUNT;
                            if (dc > 0) {
                                newEffectId = lightwaveos::DISPLAY_ORDER[dc - 1];
                            }
                            break;
                        }

                        case EffectRegister::REACTIVE: {
                            uint8_t count = PatternRegistry::getReactiveEffectCount();
                            if (count > 0) {
                                reactiveRegisterIndex = count - 1;
                                newEffectId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                            }
                            break;
                        }

                        case EffectRegister::AMBIENT:
                            if (ambientEffectCount > 0) {
                                ambientRegisterIndex = ambientEffectCount - 1;
                                newEffectId = ambientEffectIds[ambientRegisterIndex];
                            }
                            break;
                    }

                    if (newEffectId != lightwaveos::INVALID_EFFECT_ID) {
                        currentEffect = newEffectId;
                        actors.setEffect(currentEffect);
                        const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                             (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                        Serial.printf("Last effect 0x%04X%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                      currentEffect, suffix, renderer->getEffectName(currentEffect));
                    }
                    break;
                }

                case '+':
                case '=':
                    {
                        uint8_t b = renderer->getBrightness();
                        if (b < 255) {
                            b = min((int)b + 16, 255);
                            actors.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '-':
                    {
                        uint8_t b = renderer->getBrightness();
                        if (b > 16) {
                            b = max((int)b - 16, 16);
                            actors.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '[':
                    {
                        uint8_t s = renderer->getSpeed();
                        if (s > 1) {
                            s = max((int)s - 1, 1);
                            actors.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case ']':
                    {
                        uint8_t s = renderer->getSpeed();
                        if (s < 100) {
                            s = min((int)s + 1, 100);
                            actors.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case '.':  // Next palette (quick key)
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t pal = (renderer->getPaletteIndex() + 1) % paletteCount;
                        actors.setPalette(pal);
                        Serial.printf("Palette %d/%d: %s\n", pal, paletteCount, renderer->getPaletteName(pal));
                    }
                    break;

                case ',':  // Previous palette
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t current = renderer->getPaletteIndex();
                        uint8_t p = (current + paletteCount - 1) % paletteCount;
                        actors.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
                    }
                    break;

                case 'l':
                    {
                        uint16_t effectCount = renderer->getEffectCount();
                        Serial.printf("\n=== Effects (%d total) ===\n", effectCount);
                        for (uint16_t i = 0; i < effectCount; i++) {
                            EffectId eid = renderer->getEffectIdAt(i);
                            char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                            const char* type = (renderer->getEffectInstance(eid) != nullptr) ? " [IEffect]" : " [Legacy]";
                            Serial.printf("  %3d [%c] 0x%04X: %s%s%s\n", i, key, eid, renderer->getEffectName(eid), type,
                                          (!inZoneMode && eid == currentEffect) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 'p':
                    // Bloom prism opacity +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setPrismOpacity(BP::getPrismOpacity() + 0.05f);
                        Serial.printf("Bloom Prism: %.2f\n", BP::getPrismOpacity());
                    }
                    break;

                case 'P':
                    // Bloom prism opacity -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setPrismOpacity(BP::getPrismOpacity() - 0.05f);
                        Serial.printf("Bloom Prism: %.2f\n", BP::getPrismOpacity());
                    }
                    break;

                case 'o':
                    // Bloom bulb opacity +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setBulbOpacity(BP::getBulbOpacity() + 0.05f);
                        Serial.printf("Bloom Bulb: %.2f\n", BP::getBulbOpacity());
                    }
                    break;

                case 'O':
                    // Bloom bulb opacity -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setBulbOpacity(BP::getBulbOpacity() - 0.05f);
                        Serial.printf("Bloom Bulb: %.2f\n", BP::getBulbOpacity());
                    }
                    break;

                case 'a':
                    // Toggle audio debug logging
                    {
                        bool enabled = !renderer->isAudioDebugEnabled();
                        renderer->setAudioDebugEnabled(enabled);
                        Serial.printf("Audio debug: %s\n", enabled ? "ON" : "OFF");
                    }
                    break;

                case 'i':
                    // Mood +
                    {
                        uint8_t m = renderer->getMood();
                        m = static_cast<uint8_t>(min(static_cast<int>(m) + 16, 255));
                        actors.setMood(m);
                        Serial.printf("Mood: %d (%.0f%%)\n", m, m * 100.0f / 255.0f);
                    }
                    break;

                case 's':
                    actors.printStatus();
                    if (zoneComposer.isEnabled()) {
                        zoneComposer.printStatus();
                    }
                    if (renderer->isTransitionActive()) {
                        Serial.println("  Transition: ACTIVE");
                    }
                    // Show IEffect status for current effect
                    {
                        EffectId current = renderer->getCurrentEffect();
                        IEffect* effect = renderer->getEffectInstance(current);
                        if (effect != nullptr) {
                            Serial.printf("  Current effect type: IEffect (native)\n");
                            const EffectMetadata& meta = effect->getMetadata();
                            Serial.printf("  Metadata: %s - %s\n", meta.name, meta.description);
                        } else {
                            Serial.printf("  Current effect type: Legacy (function pointer)\n");
                        }
                    }
                    break;

                case 'd':
                    // Toggle Bloom effect debug output
                    lightwaveos::effects::ieffect::g_bloomDebugEnabled =
                        !lightwaveos::effects::ieffect::g_bloomDebugEnabled;
                    Serial.printf("[BLOOM DEBUG] %s\n",
                        lightwaveos::effects::ieffect::g_bloomDebugEnabled ? "ENABLED (select effect 120)" : "DISABLED");
                    break;

                case 't':
                case 'T':
                    // RD Triangle: F - / +
                    {
                        EffectId rdTriangleId = lightwaveos::INVALID_EFFECT_ID;
                        uint16_t effectCount = renderer->getEffectCount();
                        for (uint16_t i = 0; i < effectCount; i++) {
                            EffectId eid = renderer->getEffectIdAt(i);
                            const char* name = renderer->getEffectName(eid);
                            if (name && strcmp(name, "LGP RD Triangle") == 0) {
                                rdTriangleId = eid;
                                break;
                            }
                        }
                        if (rdTriangleId == lightwaveos::INVALID_EFFECT_ID) {
                            Serial.println("RD Triangle not found");
                            break;
                        }
                        if (renderer->getCurrentEffect() != rdTriangleId) {
                            currentEffect = rdTriangleId;
                            actors.setEffect(rdTriangleId);
                        }
                        IEffect* effect = renderer->getEffectInstance(rdTriangleId);
                        if (!effect) {
                            Serial.println("RD Triangle not available");
                            break;
                        }
                        float f = effect->getParameter("F");
                        if (f <= 0.0f) f = 0.0380f;
                        if (cmd == 't') f -= 0.0010f;
                        else f += 0.0010f;
                        if (f < 0.0300f) f = 0.0300f;
                        if (f > 0.0500f) f = 0.0500f;
                        effect->setParameter("F", f);
                        Serial.printf("RD Triangle F: %.4f\n", f);
                    }
                    break;

                case '!':
#if FEATURE_TRANSITIONS
                    // List transition types
                    Serial.println("\n=== Transition Types ===");
                    for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                        Serial.printf("  %2d: %s (%dms)\n", i,
                                      getTransitionName(static_cast<TransitionType>(i)),
                                      getDefaultDuration(static_cast<TransitionType>(i)));
                    }
                    Serial.println();
#else
                    Serial.println("Transitions disabled (FH4 build)");
#endif
                    break;

                case 'A':
                    // Toggle auto-play (narrative) mode
                    if (NARRATIVE.isEnabled()) {
                        NARRATIVE.disable();
                        Serial.println("Auto-play: DISABLED");
                    } else {
                        NARRATIVE.enable();
                        Serial.println("Auto-play: ENABLED (4s cycle)");
                    }
                    break;

                case '@':
                    // Print narrative status
                    NARRATIVE.printStatus();
                    break;

                // ========== EdgeMixer Commands ==========

                case 'e':
                    // Cycle EdgeMixer mode: mirror -> analogous -> complementary -> split_comp -> sat_veil -> mirror
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t next = (static_cast<uint8_t>(mixer.getMode()) + 1) % 5;
                        actors.setEdgeMixerMode(next);
                        Serial.printf("EdgeMixer mode: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                                      lightwaveos::enhancement::EdgeMixer::modeName(
                                          static_cast<lightwaveos::enhancement::EdgeMixerMode>(next)));
                    }
                    break;

                case 'w':
                    // EdgeMixer spread +
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t spread = mixer.getSpread();
                        spread = (spread + 5 > 60) ? 60 : spread + 5;
                        actors.setEdgeMixerSpread(spread);
                        Serial.printf("EdgeMixer spread: %d\n", spread);
                    }
                    break;

                case 'W':
                    // EdgeMixer spread -
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t spread = mixer.getSpread();
                        spread = (spread < 5) ? 0 : spread - 5;
                        actors.setEdgeMixerSpread(spread);
                        Serial.printf("EdgeMixer spread: %d\n", spread);
                    }
                    break;

                case '<':
                    // EdgeMixer strength -
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t strength = mixer.getStrength();
                        strength = (strength < 15) ? 0 : strength - 15;
                        actors.setEdgeMixerStrength(strength);
                        Serial.printf("EdgeMixer strength: %d\n", strength);
                    }
                    break;

                case '>':
                    // EdgeMixer strength +
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t strength = mixer.getStrength();
                        strength = (strength + 15 > 255) ? 255 : strength + 15;
                        actors.setEdgeMixerStrength(strength);
                        Serial.printf("EdgeMixer strength: %d\n", strength);
                    }
                    break;

                case 'y':
                    // Toggle EdgeMixer spatial: uniform ↔ centre_gradient
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t next = (static_cast<uint8_t>(mixer.getSpatial()) == 0) ? 1 : 0;
                        actors.setEdgeMixerSpatial(next);
                        Serial.printf("EdgeMixer spatial: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                                      lightwaveos::enhancement::EdgeMixer::spatialName(
                                          static_cast<lightwaveos::enhancement::EdgeMixerSpatial>(next)));
                    }
                    break;

                case 'Y':
                    // Toggle EdgeMixer temporal: static ↔ rms_gate
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        uint8_t next = (static_cast<uint8_t>(mixer.getTemporal()) == 0) ? 1 : 0;
                        actors.setEdgeMixerTemporal(next);
                        Serial.printf("EdgeMixer temporal: " LW_CLR_CYAN "%s" LW_ANSI_RESET "\n",
                                      lightwaveos::enhancement::EdgeMixer::temporalName(
                                          static_cast<lightwaveos::enhancement::EdgeMixerTemporal>(next)));
                    }
                    break;

                case '}':
                    // Save EdgeMixer to NVS
                    {
                        actors.saveEdgeMixerToNVS();
                        Serial.println("EdgeMixer: " LW_CLR_GREEN "saved to NVS" LW_ANSI_RESET);
                    }
                    break;

                case '#':
                    // Print EdgeMixer status
                    {
                        auto& mixer = lightwaveos::enhancement::EdgeMixer::getInstance();
                        Serial.println("\n=== EdgeMixer Status ===");
                        Serial.printf("  Mode:     %s\n", mixer.modeName());
                        Serial.printf("  Spread:   %d\n", mixer.getSpread());
                        Serial.printf("  Strength: %d\n", mixer.getStrength());
                        Serial.printf("  Spatial:  %s\n", mixer.spatialName());
                        Serial.printf("  Temporal: %s\n", mixer.temporalName());
                        Serial.println();
                    }
                    break;

                // ========== Color Correction Keystrokes ==========

                case 'c':
                    // Cycle color correction mode (OFF → HSV → RGB → BOTH → OFF)
                    {
                        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                        auto currentMode = engine.getMode();
                        uint8_t modeInt = (uint8_t)currentMode;
                        modeInt = (modeInt + 1) % 4;  // Cycle: 0→1→2→3→0
                        engine.setMode((lightwaveos::enhancement::CorrectionMode)modeInt);
                        const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
                        Serial.printf("Color correction mode: %d (%s)\n", modeInt, modeNames[modeInt]);
                    }
                    break;

                case 'C':
                    // Show color correction status
                    {
                        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                        auto mode = engine.getMode();
                        auto& cfg = engine.getConfig();
                        const char* modeNames[] = {"OFF", "HSV", "RGB", "BOTH"};
                        Serial.println("\n=== Color Correction Status ===");
                        Serial.printf("  Mode: %d (%s)\n", (int)mode, modeNames[(int)mode]);
                        Serial.printf("  Auto-exposure: %s, target=%d\n",
                                      cfg.autoExposureEnabled ? "ON" : "OFF",
                                      cfg.autoExposureTarget);
                        Serial.printf("  Gamma: %s, value=%.1f\n",
                                      cfg.gammaEnabled ? "ON" : "OFF",
                                      cfg.gammaValue);
                        Serial.printf("  Brown guardrail: %s\n",
                                      cfg.brownGuardrailEnabled ? "ON" : "OFF");
                        Serial.println();
                    }
                    break;

                case 'E':
                    // Toggle auto-exposure
                    {
                        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                        auto& cfg = engine.getConfig();
                        cfg.autoExposureEnabled = !cfg.autoExposureEnabled;
                        Serial.printf("Auto-exposure: %s\n", cfg.autoExposureEnabled ? "ON" : "OFF");
                    }
                    break;

                case 'g':
                    // Toggle gamma or cycle common values
                    {
                        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                        auto& cfg = engine.getConfig();
                        if (!cfg.gammaEnabled) {
                            // Enable with default 2.2
                            cfg.gammaEnabled = true;
                            cfg.gammaValue = 2.2f;
                            Serial.printf("Gamma: ON (%.1f)\n", cfg.gammaValue);
                        } else {
                            // Cycle through common values: 2.2 → 2.5 → 2.8 → off
                            if (cfg.gammaValue < 2.3f) {
                                cfg.gammaValue = 2.5f;
                                Serial.printf("Gamma: %.1f\n", cfg.gammaValue);
                            } else if (cfg.gammaValue < 2.6f) {
                                cfg.gammaValue = 2.8f;
                                Serial.printf("Gamma: %.1f\n", cfg.gammaValue);
                            } else {
                                cfg.gammaEnabled = false;
                                Serial.println("Gamma: OFF");
                            }
                        }
                    }
                    break;

                case 'q':
                    // Toggle cinema post-processing (A/B visual comparison)
                    {
                        namespace cine = lightwaveos::effects::cinema;
                        cine::setEnabled(!cine::isEnabled());
                        Serial.printf("Cinema post: %s\n", cine::isEnabled() ? "ON" : "OFF");
                    }
                    break;

                case 'b':
                case 'B':
                    // RD Triangle: K - / +
                    {
                        EffectId rdTriangleId = lightwaveos::INVALID_EFFECT_ID;
                        uint16_t effectCount = renderer->getEffectCount();
                        for (uint16_t i = 0; i < effectCount; i++) {
                            EffectId eid = renderer->getEffectIdAt(i);
                            const char* name = renderer->getEffectName(eid);
                            if (name && strcmp(name, "LGP RD Triangle") == 0) {
                                rdTriangleId = eid;
                                break;
                            }
                        }
                        if (rdTriangleId == lightwaveos::INVALID_EFFECT_ID) {
                            Serial.println("RD Triangle not found");
                            break;
                        }
                        if (renderer->getCurrentEffect() != rdTriangleId) {
                            currentEffect = rdTriangleId;
                            actors.setEffect(rdTriangleId);
                        }
                        IEffect* effect = renderer->getEffectInstance(rdTriangleId);
                        if (!effect) {
                            Serial.println("RD Triangle not available");
                            break;
                        }
                        float k = effect->getParameter("K");
                        if (k <= 0.0f) k = 0.0630f;
                        if (cmd == 'b') k -= 0.0010f;
                        else k += 0.0010f;
                        if (k < 0.0550f) k = 0.0550f;
                        if (k > 0.0750f) k = 0.0750f;
                        effect->setParameter("K", k);
                        Serial.printf("RD Triangle K: %.4f\n", k);
                    }
                    break;

                case 'I':
                    // Mood -
                    {
                        uint8_t m = renderer->getMood();
                        m = static_cast<uint8_t>(max(static_cast<int>(m) - 16, 0));
                        actors.setMood(m);
                        Serial.printf("Mood: %d (%.0f%%)\n", m, m * 100.0f / 255.0f);
                    }
                    break;

                case 'f':
                    // Alpha (persistence) +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setAlpha(BP::getAlpha() + 0.01f);
                        Serial.printf("Bloom Alpha: %.3f\n", BP::getAlpha());
                    }
                    break;

                case 'F':
                    // Alpha (persistence) -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setAlpha(BP::getAlpha() - 0.01f);
                        Serial.printf("Bloom Alpha: %.3f\n", BP::getAlpha());
                    }
                    break;

                case 'h':
                    // Square iterations (contrast) +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setSquareIter(BP::getSquareIter() + 1);
                        Serial.printf("Bloom Square Iter: %d\n", BP::getSquareIter());
                    }
                    break;

                case 'H':
                    // Square iterations (contrast) -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        uint8_t si = BP::getSquareIter();
                        BP::setSquareIter(si > 0 ? si - 1 : 0);
                        Serial.printf("Bloom Square Iter: %d\n", BP::getSquareIter());
                    }
                    break;

                case 'j':
                    // Prism iterations +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setPrismIterations(BP::getPrismIterations() + 1);
                        Serial.printf("Bloom Prism Iter: %d\n", BP::getPrismIterations());
                    }
                    break;

                case 'J':
                    // Prism iterations -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        uint8_t pi = BP::getPrismIterations();
                        BP::setPrismIterations(pi > 0 ? pi - 1 : 0);
                        Serial.printf("Bloom Prism Iter: %d\n", BP::getPrismIterations());
                    }
                    break;

                case 'k':
                    // gHue speed +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setGHueSpeed(BP::getGHueSpeed() + 0.25f);
                        Serial.printf("Bloom gHue Speed: %.2f\n", BP::getGHueSpeed());
                    }
                    break;

                case 'K':
                    // gHue speed -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setGHueSpeed(BP::getGHueSpeed() - 0.25f);
                        Serial.printf("Bloom gHue Speed: %.2f\n", BP::getGHueSpeed());
                    }
                    break;

                case 'u':
                    // Spatial spread +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setSpatialSpread(BP::getSpatialSpread() + 16.0f);
                        Serial.printf("Bloom Spatial Spread: %.0f\n", BP::getSpatialSpread());
                    }
                    break;

                case 'U':
                    // Spatial spread -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        float ss = BP::getSpatialSpread();
                        BP::setSpatialSpread(ss > 16.0f ? ss - 16.0f : 0.0f);
                        Serial.printf("Bloom Spatial Spread: %.0f\n", BP::getSpatialSpread());
                    }
                    break;

                case 'v':
                    // Intensity coupling +
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        BP::setIntensityCoupling(BP::getIntensityCoupling() + 0.1f);
                        Serial.printf("Bloom Intensity Coupling: %.1f\n", BP::getIntensityCoupling());
                    }
                    break;

                case 'V':
                    // Intensity coupling -
                    {
                        using BP = lightwaveos::effects::ieffect::BloomParityEffect;
                        float ic = BP::getIntensityCoupling();
                        BP::setIntensityCoupling(ic > 0.1f ? ic - 0.1f : 0.0f);
                        Serial.printf("Bloom Intensity Coupling: %.1f\n", BP::getIntensityCoupling());
                    }
                    break;
            }
                }
        }
        }
    }

    // Update NarrativeEngine (auto-play mode)
    NARRATIVE.update();

    // Periodic system health checks every 10 seconds
    if (now - lastStatus > 10000) {
        // Periodic system health checks
#if FEATURE_STACK_PROFILING
        lightwaveos::core::system::StackMonitor::checkAllTasks();
#endif
#if FEATURE_HEAP_MONITORING
        lightwaveos::core::system::HeapMonitor::checkHeapIntegrity();
#endif
#if FEATURE_MEMORY_LEAK_DETECTION
        // Scan for memory leaks (every 10 seconds)
        static uint32_t lastLeakScan = 0;
        if (now - lastLeakScan > 10000) {
            lightwaveos::core::system::MemoryLeakDetector::scanForLeaks();
            lastLeakScan = now;
        }
#endif

        lastStatus = now;
    }

    // Update WebServer (if enabled)
    // Note: WiFiManager runs on its own FreeRTOS task
#if FEATURE_WEB_SERVER
    if (webServerInstance) {
        webServerInstance->update();
    }
#endif

    // Feed watchdog timer (prevents system reset if tasks block)
#ifndef NATIVE_BUILD
    esp_task_wdt_reset();
#endif

    // Main loop is mostly idle - actors run in background
    delay(10);
}
