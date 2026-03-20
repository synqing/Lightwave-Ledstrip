/**
 * @file SystemInit.cpp
 * @brief Init helper functions extracted from setup() in main.cpp.
 *
 * Phase 4 of the main.cpp decomposition. Each function encapsulates a
 * logical boot phase.  setup() calls them in strict order.
 *
 * IMPORTANT: The call order is load-bearing. See SystemInit.h for the
 * full dependency graph.
 */

#include "SystemInit.h"

#include <Arduino.h>
#ifndef NATIVE_BUILD
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_system.h>
#endif

#define LW_LOG_TAG "SystemInit"
#include "utils/Log.h"

#include "config/features.h"
#include "config/Trace.h"
#include "core/actors/ActorSystem.h"
#include "core/actors/RendererActor.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "effects/CoreEffects.h"
#include "effects/zones/ZoneComposer.h"
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
#endif

#if FEATURE_AUDIO_SYNC
#include "audio/contracts/AudioEffectMapping.h"
#endif

#include "serial/CaptureStreamer.h"

#if FEATURE_WEB_SERVER
#ifndef NATIVE_BUILD
#include <esp_wifi.h>
#endif
#include "network/WiFiManager.h"
#include "network/WiFiCredentialManager.h"
#include "network/WebServer.h"
using namespace lightwaveos::network;
using namespace lightwaveos::config;
#endif

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::plugins;
using namespace lightwaveos::persistence;

namespace lightwaveos::core {

// ─── Phase 1: Serial ────────────────────────────────────────────────────

void initSerial() {
    Serial.setTxBufferSize(4096);   // Tier 0: enlarge HWCDC TX ring buffer (default 256B)
                                     // so Serial.write(1009) enqueues non-blocking instead
                                     // of blocking through ~4 refill cycles.
    Serial.begin(115200);
    delay(1000);

    // Telemetry boot heartbeat (for trace capture verification)
    Serial.println("{\"event\":\"telemetry.boot\",\"ts_mono_ms\":0,\"version\":\"2.0\"}");

#if FEATURE_MABUTRACE
    TRACE_INIT(64);  // 64 KB ring buffer for Perfetto trace
#endif
}

// ─── Phase 2: PSRAM scratch + capture streamer ──────────────────────────

void initPSRAMScratch(lightwaveos::serial::CaptureStreamer& captureStreamer) {
    // Allocate capture streamer PSRAM buffers (Phase 2 extraction).
    captureStreamer.init();
}

// ─── Phase 3: OTA + WiFi deinit ─────────────────────────────────────────

void initOtaAndWiFiReset() {
    // OTA boot verification — check rollback status before anything else
#ifndef NATIVE_BUILD
    lightwaveos::core::system::OtaBootVerifier::init();
#endif

#if FEATURE_WEB_SERVER && !defined(NATIVE_BUILD)
    // Force clean WiFi state before any WiFi use
    // (avoids esp_wifi_init 257 / "Failed to deinit" on no-PSRAM)
    esp_err_t deinit = esp_wifi_deinit();
    if (deinit != ESP_OK && deinit != ESP_ERR_WIFI_NOT_INIT) {
        LW_LOGW("esp_wifi_deinit() returned %d", deinit);
    }
#endif
}

// ─── Phase 4: WDT safe-mode check ──────────────────────────────────────

void checkWdtSafeMode(BootFlags& flags) {
#ifndef NATIVE_BUILD
    {
        esp_reset_reason_t reason = esp_reset_reason();
        if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT) {
            flags.wdtSafeMode = true;
            LW_LOGW("WDT recovery boot — will load safe default (Prism)");
        }
    }
#else
    (void)flags;
#endif
}

// ─── Phase 5: System monitoring ─────────────────────────────────────────

void initSystemMonitoring() {
    LW_LOGI("==========================================");
    LW_LOGI("LightwaveOS v2 - Actor System + Zones");
    LW_LOGI("==========================================");

    // Initialise system monitoring (must be before actors start)
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
    // Reset baseline after all initialisation
    delay(1000);  // Wait for system to stabilise
    lightwaveos::core::system::MemoryLeakDetector::resetBaseline();
#endif
}

// ─── Phase 6: Actor system + effects ────────────────────────────────────

void initActorSystem(
    ActorSystem& actors,
    RendererActor*& renderer,
    lightwaveos::serial::CaptureStreamer& captureStreamer)
{
    LW_LOGI("Initializing Actor System...");
    if (!actors.init()) {
        LW_LOGE("Actor System init failed!");
        while (1) delay(1000);  // Halt
    }
    renderer = actors.getRenderer();
    captureStreamer.setRenderer(renderer);
    LW_LOGI("Actor System: INITIALIZED");

    // Register ALL effects (core + LGP) BEFORE starting actors
    LW_LOGI("Registering effects...");
    uint16_t effectCount = registerAllEffects(renderer);
    LW_LOGI("Effects registered: %d", effectCount);

#if FEATURE_AUDIO_SYNC
    // Initialise AudioMappingRegistry before render tasks begin so the large
    // mapping table can live in PSRAM rather than internal SRAM.
    LW_LOGI("Initialising Audio Mapping Registry...");
    bool mappingOk = lightwaveos::audio::AudioMappingRegistry::instance().begin();
    LW_LOGI("Audio Mapping Registry: %s", mappingOk ? "READY" : "DISABLED");
#endif
}

// ─── Phase 7: NVS + zones ──────────────────────────────────────────────

void initNvsAndZones(
    RendererActor* renderer,
    ZoneComposer& zoneComposer,
    ZoneConfigManager*& zoneConfigMgr)
{
    // Initialise NVS (must be before Zone Composer to load saved config)
    LW_LOGI("Initializing NVS...");
    if (!NVS_MANAGER.init()) {
        LW_LOGW("NVS init failed - settings won't persist!");
    } else {
        LW_LOGI("NVS: INITIALIZED");
    }

    // Initialise per-device OTA token (must be after NVS, before WebServer)
#if FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER
    if (!lightwaveos::core::system::OtaTokenManager::init()) {
        LW_LOGW("OTA Token Manager init failed - using compile-time token");
    } else {
        LW_LOGI("OTA Token Manager: INITIALIZED");
    }
#endif

    // Initialise Zone Composer
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
            // First boot — load default preset
            zoneComposer.loadPreset(1);
            LW_LOGI("Zone Composer: INITIALIZED");
            LW_LOGI("Preset: Dual Split (default)");
        }
    }
}

// ─── Phase 8: Status strip + TTP223 button ──────────────────────────────

void initStatusStripAndButton() {
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
}

// ─── Phase 9a: Start actors + plugin manager ────────────────────────────

void startActorsAndPlugins(
    ActorSystem& actors,
    RendererActor* renderer,
    PluginManagerActor*& pluginManager)
{
    // Start all actors (RendererActor runs on Core 1 at 120 FPS)
    LW_LOGI("Starting Actor System...");
    if (!actors.start()) {
        LW_LOGE("Actor System start failed!");
        while (1) delay(1000);  // Halt
    }
    LW_LOGI("Actor System: RUNNING");
#ifndef NATIVE_BUILD
    LW_LOGI("Boot memory: heap %lu bytes free", (unsigned long)ESP.getFreeHeap());
#ifdef CONFIG_SPIRAM_SUPPORT
    LW_LOGI("Boot memory: PSRAM %lu bytes free", (unsigned long)ESP.getFreePsram());
#endif
#endif

    // Initialise Plugin Manager
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
}

// ─── Phase 10: WiFi AP ──────────────────────────────────────────────────

void initWiFiAP() {
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

        // Initialise WiFi Credential Manager (for saved networks)
        if (!WIFI_CREDENTIALS.begin()) {
            LW_LOGW("WiFiCredentialManager failed to initialize");
        }
    }
#endif
}

// ─── Phase 11: WebServer ────────────────────────────────────────────────

void initWebServer(
    ActorSystem& actors,
    RendererActor* renderer,
    PluginManagerActor* pluginManager)
{
#if FEATURE_WEB_SERVER
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
#else
    (void)actors;
    (void)renderer;
    (void)pluginManager;
#endif
}

// ─── Phase 12: Post-boot validation ─────────────────────────────────────

void postBootValidation(const BootFlags& flags) {
    // OTA boot verification — validate health after all critical subsystems init
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
    if (flags.nvsCorrupted) {
        statusStripTriggerWhiteFlash();
    }
    {
        uint32_t freeHeap = esp_get_free_heap_size();
        if (freeHeap < 38 * 1024) {
            statusStripSetLowHeap(true);
            LW_LOGW("Low heap at boot: %u bytes", freeHeap);
        }
    }
#else
    (void)flags;
#endif
}

// ─── Phase 13: Help banner ──────────────────────────────────────────────

void printHelpBanner() {
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

}  // namespace lightwaveos::core
