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

// Forward declaration for Arduino-ESP32's loopTask WDT subscribe helper
// (defined in esp32-hal-misc.c, C linkage).
extern "C" void enableLoopWDT(void);
#endif

#define LW_LOG_TAG "Main"
#include "utils/Log.h"

#include "config/features.h"
#include "config/Trace.h"
#if HAS_TEMP_SENSOR && !defined(NATIVE_BUILD)
#include <driver/temp_sensor.h>  // Surface 5: legacy ESP-IDF 4.x API (arduino-esp32 v3.x)
#endif
#include "core/actors/ActorSystem.h"
#include "hardware/EncoderManager.h"
#include "core/actors/RendererActor.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "effects/zones/ZoneComposer.h"
#include "core/narrative/NarrativeEngine.h"
#include "core/actors/ShowDirectorActor.h"
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

#ifndef NATIVE_BUILD
#include "hal/esp32s3/StatusStripTouch.h"
#include <esp_system.h>
#endif
#include "config/factory_presets.h"

#include "serial/CaptureStreamer.h"
#include "serial/SerialCLI.h"
#include "core/shows/DynamicShowStore.h"

#if FEATURE_WEB_SERVER
#include "network/WebServer.h"
#include "network/webserver/WsGateway.h"
using namespace lightwaveos::network;
#endif

#include "core/SystemInit.h"

using namespace lightwaveos::persistence;

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::narrative;
using namespace lightwaveos::plugins;

// ==================== Global Zone Composer ====================

ZoneComposer zoneComposer;
ZoneConfigManager* zoneConfigMgr = nullptr;

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

// Serial CLI — command line interface (Phase 3 extraction)
static lightwaveos::serial::SerialCLI serialCLI;

// ==================== Setup ====================

void setup() {
    using namespace lightwaveos::core;

    // Boot flags shared across init phases
    BootFlags bootFlags;

    // Phase 1: Serial + telemetry heartbeat
    initSerial();

    // Phase 2: PSRAM scratch buffers
    initLoopScratchBuffers();
    initPSRAMScratch(captureStreamer);

    // Phase 3: OTA boot verification + WiFi deinit
    initOtaAndWiFiReset();

    // Phase 4: WDT safe-mode check (DEC-011 § 2.6)
    checkWdtSafeMode(bootFlags);

    // Phase 5: System monitoring (must be before actors start)
    initSystemMonitoring();

#if HAS_TEMP_SENSOR && !defined(NATIVE_BUILD)
    // Surface 5: arm ESP32-S3 die temperature sensor (legacy IDF 4.x API).
    // L2 range covers -10..80 °C with ±1 °C accuracy — adequate for thermal
    // throttle detection. Sensor stays running; reads cost ~50–100 µs.
    {
        temp_sensor_config_t tsCfg = TSENS_CONFIG_DEFAULT();
        tsCfg.dac_offset = TSENS_DAC_L2;  // -10..80 °C, error <1 °C
        if (temp_sensor_set_config(tsCfg) != ESP_OK ||
            temp_sensor_start() != ESP_OK) {
            LW_LOGW("Die temp sensor init failed; thermal counter disabled");
        } else {
            LW_LOGI("Die temp sensor armed (range -10..80 C, L2 DAC offset)");
        }
    }
#endif

    // Phase 6: Actor system + effects + audio mapping
    initActorSystem(actors, renderer, captureStreamer);

    // Phase 7: NVS + OTA token + Zone Composer
    initNvsAndZones(renderer, zoneComposer, zoneConfigMgr);

    // Phase 8: Status strip + TTP223 button
    initStatusStripAndButton();

    // Phase 9a: Start actors + Plugin Manager
    startActorsAndPlugins(actors, renderer, pluginManager);

    // Phase 9b: Load or set initial state (depends on applyFactoryPreset
    // which is local to main.cpp and also used by loop)
    LW_LOGI("Loading system state...");

    if (bootFlags.wdtSafeMode) {
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
            bootFlags.nvsCorrupted = true;
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

    // Phase 10: WiFi AP-only boot
    initWiFiAP();

    // Phase 11: WebServer
    initWebServer(actors, renderer, pluginManager);

    // Phase 12: OTA health + degraded-mode signals
    postBootValidation(bootFlags);

    // Initialise Serial CLI (Phase 3 extraction) — all dependencies ready.
    {
        lightwaveos::serial::SerialCLIDeps cliDeps;
        cliDeps.actors           = &actors;
        cliDeps.renderer         = renderer;
        cliDeps.zoneComposer     = &zoneComposer;
        cliDeps.zoneConfigMgr    = zoneConfigMgr;
        cliDeps.captureStreamer   = &captureStreamer;
        cliDeps.showStore        = &serialShowStore;
        cliDeps.effectIdScratch  = s_loopEffectIdScratch;
        cliDeps.validationScratch = s_validationFrameScratch;
        cliDeps.effectIdScratchCap = LOOP_SCRATCH_EFFECT_ID_CAP;
        serialCLI.init(cliDeps);
    }

    // Phase 13: Help banner
    printHelpBanner();

    // Phase 14: Subscribe the Arduino loopTask to the task watchdog so the
    // existing esp_task_wdt_reset() in loop() becomes effective. Any loop-level
    // hang (serial CLI, NVS, WebServer update, encoder poll) will now trigger
    // a task-WDT reset with a backtrace, enabling automatic recovery.
    //
    // Raise the TWDT timeout from the 5 s Arduino default to 10 s. RendererActor
    // on CPU 1 can hog the core during heavy effect init (PSRAM lookup tables,
    // oscillator fields, Fresnel harmonic sums) for multiple hundreds of ms;
    // paired with the vTaskDelay(1) yield discipline added inside RendererActor
    // (pre-/post-init + over-budget frame yield), 10 s gives loopTask enough
    // scheduler headroom to feed its own WDT even when every effect in a rapid
    // cycle is expensive. Genuine hangs (>10 s wedged) still panic.
#ifndef NATIVE_BUILD
    esp_task_wdt_init(10, true);  // 10 s timeout, panic on trip
    enableLoopWDT();
#endif
}

// ==================== Loop ====================

void loop() {
    static uint32_t lastStatus = 0;

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
    // Heap guard: refuse NVS writes when internal heap is critically low.
    // NVS commit can fail or corrupt data under memory pressure.
    static constexpr size_t NVS_SAVE_MIN_HEAP = 8192;
    if (g_nvsSavePending && (now - g_nvsSaveRequestMs) >= NVS_SAVE_DEBOUNCE_MS) {
        const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (freeHeap < NVS_SAVE_MIN_HEAP) {
            // Defer — do NOT clear g_nvsSavePending, retry next loop when heap recovers.
            // Rate-limit the warning to 1 Hz — loop runs at ~100 Hz and would otherwise
            // flood the serial log whilst heap remains below the minimum threshold.
            static uint32_t lastNvsDeferLogMs = 0;
            if (now - lastNvsDeferLogMs >= 1000) {
                lastNvsDeferLogMs = now;
                LW_LOGW("NVS save deferred: internal heap %u < %u minimum",
                        (unsigned)freeHeap, (unsigned)NVS_SAVE_MIN_HEAP);
            }
        } else {
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
    }

#if FEATURE_ROTATE8_ENCODER
    // Handle encoder events
    using namespace lightwaveos::hardware;
    EncoderEvent event;
    while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
        handleEncoderEvent(event, actors, renderer);
    }
#endif

    // Serial CLI tick — reads serial, dispatches commands (Phase 3 extraction)
    serialCLI.tick();

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

    // Phase 1B trace health gauges. These deliberately run from loopTask at
    // 1 Hz, never from audio/render hot paths.
    static uint32_t lastTraceHealthMs = 0;
    if (now - lastTraceHealthMs >= 1000) {
        lastTraceHealthMs = now;
#ifndef NATIVE_BUILD
        const uint32_t heapFreeIntKb =
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024U;
        const uint32_t heapLargestIntKb =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024U;
        const uint32_t heapFreePsKb =
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) / 1024U;
        const uint32_t heapLargestPsKb =
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) / 1024U;
        TRACE_COUNTER("heap_free_internal_kb",    static_cast<int32_t>(heapFreeIntKb));
        TRACE_COUNTER("heap_largest_internal_kb", static_cast<int32_t>(heapLargestIntKb));
        TRACE_COUNTER("heap_free_psram_kb",       static_cast<int32_t>(heapFreePsKb));
        TRACE_COUNTER("heap_largest_psram_kb",    static_cast<int32_t>(heapLargestPsKb));
        TRACE_COUNTER("task_stack_hwm_loop",
                      static_cast<int32_t>(uxTaskGetStackHighWaterMark(nullptr)));

        // Surface 5 Tier 4: heap-pressure thresholds with hysteresis.
        // OOM warn: trip <20 KB free internal; rearm only after recovery >30 KB.
        static bool oomWarnLatched = false;
        if (!oomWarnLatched && heapFreeIntKb < 20U) {
            TRACE_INSTANT("oom_warning");
            oomWarnLatched = true;
        } else if (oomWarnLatched && heapFreeIntKb >= 30U) {
            oomWarnLatched = false;
        }
        // PSRAM pressure: trip <256 KB free; rearm above 320 KB.
        static bool psramWarnLatched = false;
        if (!psramWarnLatched && heapFreePsKb < 256U) {
            TRACE_INSTANT("psram_pressure_warn");
            psramWarnLatched = true;
        } else if (psramWarnLatched && heapFreePsKb >= 320U) {
            psramWarnLatched = false;
        }

#if HAS_TEMP_SENSOR
        // Surface 5: die temperature in 0.1 °C fixed-point (e.g. 75.3 °C → 753).
        // Skips emission silently on read failure — no garbage values.
        float dieTempC = 0.0f;
        if (temp_sensor_read_celsius(&dieTempC) == ESP_OK) {
            TRACE_COUNTER("temp_celsius_x10",
                          static_cast<int32_t>(dieTempC * 10.0f));
            // Tier 4: thermal-throttle warn — trip ≥80 °C; rearm below 75 °C.
            static bool thermalWarnLatched = false;
            if (!thermalWarnLatched && dieTempC >= 80.0f) {
                TRACE_INSTANT("thermal_throttle_warn");
                thermalWarnLatched = true;
            } else if (thermalWarnLatched && dieTempC < 75.0f) {
                thermalWarnLatched = false;
            }
        }
#endif
#endif
        if (renderer) {
            const auto& led = renderer->getLedDriverStats();
#ifndef NATIVE_BUILD
            TRACE_COUNTER("task_stack_hwm_renderer",
                          static_cast<int32_t>(renderer->getStackHighWaterMark()));
#endif
            TRACE_COUNTER("led_show_skips_total", static_cast<int32_t>(led.showSkips));
            TRACE_COUNTER("led_show_avg_us", static_cast<int32_t>(led.avgShowUs));
            TRACE_COUNTER("led_show_max_us", static_cast<int32_t>(led.maxShowUs));
            TRACE_COUNTER("led_fastled_show_call_avg_us",
                          static_cast<int32_t>(led.avgFastLedShowCallUs));
            TRACE_COUNTER("led_rmt_fence_avg_us", static_cast<int32_t>(led.avgRmtFenceUs));
            TRACE_COUNTER("led_latch_wait_avg_us", static_cast<int32_t>(led.avgLatchWaitUs));
            TRACE_COUNTER("led_show_failures_total", static_cast<int32_t>(led.ledShowFailures));
            TRACE_COUNTER("rmt_errors_total", static_cast<int32_t>(led.rmtErrors));
            TRACE_COUNTER("rmt_underruns_total", static_cast<int32_t>(led.rmtUnderruns));
        }
#ifndef NATIVE_BUILD
#if FEATURE_AUDIO_SYNC
        if (auto* audioActor = ::actors.getAudio()) {
            TRACE_COUNTER("task_stack_hwm_audio",
                          static_cast<int32_t>(audioActor->getStackHighWaterMark()));
        }
#endif
        if (auto* showDirector = ::actors.getShowDirector()) {
            TRACE_COUNTER("task_stack_hwm_show_director",
                          static_cast<int32_t>(showDirector->getStackHighWaterMark()));
        }
#endif
#if FEATURE_WEB_SERVER
        if (webServerInstance) {
            TRACE_COUNTER("ws_client_count",
                          static_cast<int32_t>(webServerInstance->getClientCount()));
            TRACE_COUNTER("wifi_ap_mode", webServerInstance->isAPMode() ? 1 : 0);
            // Surface 4 Tier 1: AP-only baseline + WS gateway counters.
            TRACE_COUNTER("wifi_clients",
                          static_cast<int32_t>(WiFi.softAPgetStationNum()));
            if (auto* gw = webServerInstance->getWsGateway()) {
                const auto stats = gw->getStats();  // by-value copy
                TRACE_COUNTER("ws_clients",
                              static_cast<int32_t>(stats.connectAccepted));
                TRACE_COUNTER("ws_dispatch_count",
                              static_cast<int32_t>(stats.dispatchCount));
                TRACE_COUNTER("ws_errors",
                              static_cast<int32_t>(stats.parseErrors + stats.unknownCommands));
            }
        }
#endif
        TRACE_COUNTER("trace_mode_enabled", TRACE_IS_ENABLED() ? 1 : 0);
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
