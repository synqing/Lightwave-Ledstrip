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
