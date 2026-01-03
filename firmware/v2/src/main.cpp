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
#endif
#include <time.h>

#define LW_LOG_TAG "Main"
#include "utils/Log.h"

#include "config/features.h"
#include "core/actors/NodeOrchestrator.h"
#include "hardware/EncoderManager.h"
#include "core/actors/RendererNode.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "core/persistence/PresetManager.h"
#include "effects/CoreEffects.h"
#include "effects/zones/ZoneComposer.h"
#include "effects/PatternRegistry.h"
#include "effects/transitions/TransitionEngine.h"
#include "effects/transitions/TransitionTypes.h"
#include "core/narrative/NarrativeEngine.h"
#include "core/actors/ShowNode.h"
#include "core/shows/BuiltinShows.h"
#include "plugins/api/IEffect.h"
#include "core/system/StackMonitor.h"
#include "core/system/HeapMonitor.h"
#include "core/system/MemoryLeakDetector.h"
#include "core/system/ValidationProfiler.h"

// TempoTracker debug included via AudioNode.h

#if FEATURE_AUDIO_SYNC
#include "audio/AudioDebugConfig.h"
#endif

#if FEATURE_WEB_SERVER
#include "network/WiFiManager.h"
#include "network/WebServer.h"
using namespace lightwaveos::network;
using namespace lightwaveos::config;

// Global WebServer instance pointer
WebServer* webServerInstance = nullptr;
#endif

using namespace lightwaveos::persistence;

using namespace lightwaveos::nodes;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::transitions;
using namespace lightwaveos::narrative;
using namespace lightwaveos::plugins;

// ==================== Global Zone Composer ====================

ZoneComposer zoneComposer;
ZoneConfigManager* zoneConfigMgr = nullptr;
PresetManager* presetMgr = nullptr;

// Global Node Orchestrator Access
NodeOrchestrator& orchestrator = NodeOrchestrator::instance();
RendererNode* renderer = nullptr;

// Effect count is now dynamic via renderer->getEffectCount()
// Effect names retrieved via renderer->getEffectName(id)

// Current show index for serial navigation
static uint8_t currentShowIndex = 0;

// ==================== Setup ====================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);

    LW_LOGI("==========================================");
    LW_LOGI("LightwaveOS v2 - Actor System + Zones");
    LW_LOGI("==========================================");

    // Initialize Node System (creates RendererNode)
    // Initialize system monitoring (must be before actors start)
    lightwaveos::core::system::StackMonitor::init();
    LW_LOGI("Stack Monitor: INITIALIZED");
    
    // Start stack profiling
    lightwaveos::core::system::StackMonitor::startProfiling();
    LW_LOGI("Stack Profiling: STARTED");
    
    lightwaveos::core::system::HeapMonitor::init();
    LW_LOGI("Heap Monitor: INITIALIZED");
    
    lightwaveos::core::system::MemoryLeakDetector::init();
    LW_LOGI("Memory Leak Detector: INITIALIZED");
    
    lightwaveos::core::system::ValidationProfiler::init();
    LW_LOGI("Validation Profiler: INITIALIZED");
    
    // Reset baseline after all initialization
    delay(1000);  // Wait for system to stabilize
    lightwaveos::core::system::MemoryLeakDetector::resetBaseline();

    LW_LOGI("Initializing Node Orchestrator...");
    if (!orchestrator.init()) {
        LW_LOGE("Node Orchestrator init failed!");
        while(1) delay(1000);  // Halt
    }
    renderer = orchestrator.getRenderer();
    LW_LOGI("Node Orchestrator: INITIALIZED");

    // Register ALL effects (core + LGP) BEFORE starting actors
    LW_LOGI("Registering effects...");
    uint8_t effectCount = registerAllEffects(renderer);
    LW_LOGI("Effects registered: %d", effectCount);

    // Initialize NVS (must be before Zone Composer to load saved config)
    LW_LOGI("Initializing NVS...");
    if (!NVS_MANAGER.init()) {
        LW_LOGW("NVS init failed - settings won't persist!");
    } else {
        LW_LOGI("NVS: INITIALIZED");
    }

    // Initialize Zone Composer
    LW_LOGI("Initializing Zone Composer...");
    if (!zoneComposer.init(renderer)) {
        LW_LOGE("Zone Composer init failed!");
    } else {
        // Attach zone composer to renderer
        renderer->setZoneComposer(&zoneComposer);

        // Create config manager
        zoneConfigMgr = new ZoneConfigManager(&zoneComposer);

        // Create preset manager (lazy init - will initialize when LittleFS is mounted)
        presetMgr = new PresetManager();
        // Don't call init() here - LittleFS isn't mounted yet (mounted in WebServer::begin())
        // PresetManager will auto-initialize on first use when LittleFS is available

        // Try to load saved zone configuration
        if (zoneConfigMgr->loadFromNVS()) {
            LW_LOGI("Zone Composer: INITIALIZED (restored from NVS)");
        } else {
            // First boot - load default preset
            zoneComposer.loadPreset(2);
            LW_LOGI("Zone Composer: INITIALIZED");
            LW_LOGI("Preset: Triple Rings (default)");
        }
    }

    // Start all nodes (RendererNode runs on Core 1 at 120 FPS)
    LW_LOGI("Starting Node Orchestrator...");
    if (!orchestrator.start()) {
        LW_LOGE("Node Orchestrator start failed!");
        while(1) delay(1000);  // Halt
    }
    LW_LOGI("Node Orchestrator: RUNNING");

    // Load or set initial state
    LW_LOGI("Loading system state...");
    uint8_t savedEffect, savedBrightness, savedSpeed, savedPalette;
    if (zoneConfigMgr && zoneConfigMgr->loadSystemState(savedEffect, savedBrightness, savedSpeed, savedPalette)) {
        orchestrator.setEffect(savedEffect);
        orchestrator.setBrightness(savedBrightness);
        orchestrator.setSpeed(savedSpeed);
        orchestrator.setPalette(savedPalette);
        LW_LOGI("Restored: Effect=%d, Brightness=%d, Speed=%d, Palette=%d",
                savedEffect, savedBrightness, savedSpeed, savedPalette);
    } else {
        // First boot defaults
        orchestrator.setEffect(0);       // Fire
        orchestrator.setBrightness(128); // 50% brightness
        orchestrator.setSpeed(15);       // Medium speed
        orchestrator.setPalette(0);      // Party colors
        LW_LOGI("Using defaults (first boot)");
    }

    // Initialize Network (if enabled)
#if FEATURE_WEB_SERVER
    // Start WiFiManager BEFORE WebServer
    LW_LOGI("Initializing WiFiManager...");
    WIFI_MANAGER.setCredentials(
        NetworkConfig::WIFI_SSID_VALUE,
        NetworkConfig::WIFI_PASSWORD_VALUE
    );
    // STA-only: do not enable SoftAP fallback here.
    // If you want AP provisioning, explicitly re-enable this call.

    if (!WIFI_MANAGER.begin()) {
        LW_LOGE("WiFiManager failed to start!");
    } else {
        LW_LOGI("WiFiManager: STARTED");

        // Wait for connection or AP mode (with timeout)
        LW_LOGI("Connecting to WiFi...");
        uint32_t waitStart = millis();
        while (!WIFI_MANAGER.isConnected() && !WIFI_MANAGER.isAPMode()) {
            if (millis() - waitStart > 30000) {
                LW_LOGW("WiFi connection timeout, continuing...");
                break;
            }
            delay(100);
        }

        if (WIFI_MANAGER.isConnected()) {
            LW_LOGI("WiFi: CONNECTED to %s", NetworkConfig::WIFI_SSID_VALUE);
            LW_LOGI("IP: %s", WiFi.localIP().toString().c_str());
        } else if (WIFI_MANAGER.isAPMode()) {
            LW_LOGI("WiFi: AP MODE (connect to LightwaveOS-Setup)");
            LW_LOGI("IP: %s", WiFi.softAPIP().toString().c_str());
        }
    }

    // Start WebServer
    LW_LOGI("Starting Web Server...");

    // Instantiate WebServer with dependencies
    webServerInstance = new WebServer(orchestrator, renderer);

    if (!webServerInstance->begin()) {
        LW_LOGW("Web Server failed to start!");
    } else {
        LW_LOGI("Web Server: RUNNING");
        LW_LOGI("REST API: http://lightwaveos.local/api/v1/");
        LW_LOGI("WebSocket: ws://lightwaveos.local/ws");
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
    Serial.println("  l       - List effects");
    Serial.println("  P       - List palettes");
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
    Serial.println("  S       - Save settings to NVS and also save a timestamped preset");
    Serial.println("  R       - List saved presets");
    Serial.println("\nTransition Commands:");
    Serial.println("  t       - Transition to next effect (random type)");
    Serial.println("  T       - Transition to next effect (fade)");
    Serial.println("  !       - List transition types");
    Serial.println("\nAuto-Play (Narrative) Commands:");
    Serial.println("  A       - Toggle auto-play mode");
    Serial.println("  @       - Print narrative status");
    Serial.println("\nShow Playback Commands:");
    Serial.println("  W       - List all shows (10 presets)");
    Serial.println("  w       - Toggle show playback");
    Serial.println("  </>     - Previous/Next show");
    Serial.println("  {/}     - Seek backward/forward 30s");
    Serial.println("  #       - Print show status");
    Serial.println("\nColor Correction Commands:");
    Serial.println("  c       - Cycle correction mode (OFF→HSV→RGB→BOTH→OFF)");
    Serial.println("  C       - Show color correction status");
    Serial.println("  e       - Toggle auto-exposure");
    Serial.println("  g       - Toggle/cycle gamma (off→2.2→2.5→2.8→off)");
    Serial.println("  B       - Toggle brown guardrail");
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
    Serial.println("  adbg         - Show current level and intervals");
    Serial.println("  adbg <0-5>   - Set level (0=off, 2=status, 4=medium, 5=verbose)");
    Serial.println("  adbg interval <N> - Set base interval in frames");
#endif
#if FEATURE_WEB_SERVER
    Serial.println("\nWeb API:");
    Serial.println("  GET  /api/v1/effects - List effects");
    Serial.println("  POST /api/v1/effects/set - Set effect");
    Serial.println("  WS   /ws - Real-time control");
#endif
    Serial.println();
}

// ==================== Loop ====================

void loop() {
    static uint32_t lastStatus = 0;
    static uint8_t currentEffect = 0;
    static uint8_t lastAudioEffectIndex = 0;  // Track which audio effect (0=Waveform, 1=Bloom)

    // Effect register state (for filtered effect cycling)
    static EffectRegister currentRegister = EffectRegister::ALL;
    static uint8_t reactiveRegisterIndex = 0;   // Index within reactive effects
    static uint8_t ambientRegisterIndex = 0;    // Index within ambient effects
    static uint8_t ambientEffectIds[80];        // Cached ambient effect IDs
    static uint8_t ambientEffectCount = 0;      // Number of ambient effects
    static bool registersInitialized = false;

    // One-time initialization of effect registers
    if (!registersInitialized && renderer) {
        uint8_t effectCount = renderer->getEffectCount();
        ambientEffectCount = PatternRegistry::buildAmbientEffectArray(
            ambientEffectIds, sizeof(ambientEffectIds), effectCount);
        registersInitialized = true;
        LW_LOGI("Effect registers: %d reactive, %d ambient, %d total",
                PatternRegistry::getReactiveEffectCount(), ambientEffectCount, effectCount);
    }

    uint32_t now = millis();

#if FEATURE_ROTATE8_ENCODER
    // Handle encoder events
    using namespace lightwaveos::hardware;
    EncoderEvent event;
    while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
        handleEncoderEvent(event, orchestrator, renderer);
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
                case '<': case '>':  // Show navigation
                case '{': case '}':  // Seek
                    isImmediate = true;
                    break;
            }
            if (isImmediate) {
                // Process immediately without buffering
                serialCmdBuffer = String(c);
                break; // Exit while loop to process
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

        // Use firstChar for single immediate commands, trimmed input for multi-char
        if (input.length() == 0 && firstChar != ' ' && firstChar != '+' && firstChar != '-' &&
            firstChar != '=' && firstChar != '[' && firstChar != ']' &&
            firstChar != ',' && firstChar != '.' && firstChar != '<' && firstChar != '>' &&
            firstChar != '{' && firstChar != '}') {
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

        // Color correction commands (cc, ae, gamma, brown) and capture commands
        if (peekChar == 'c' && input.length() > 1) {

            // -----------------------------------------------------------------
            // Capture commands: capture on/off/status/dump
            // -----------------------------------------------------------------
            if (inputLower.startsWith("capture")) {
                handledMulti = true;

                // Ack what we received
                Serial.printf("[CAPTURE] recv='%s'\n", input.c_str());

                // Strip the leading keyword
                String subcmd = inputLower.substring(7); // after "capture"
                subcmd.trim();

                if (subcmd == "off") {
                    renderer->setCaptureMode(false, 0);
                    Serial.println("Capture mode disabled");
                }
                else if (subcmd.startsWith("on")) {
                    uint8_t tapMask = 0x07;  // default all taps
                    if (subcmd.length() > 2) {
                        tapMask = 0;
                        if (subcmd.indexOf('a') >= 0) tapMask |= 0x01;
                        if (subcmd.indexOf('b') >= 0) tapMask |= 0x02;
                        if (subcmd.indexOf('c') >= 0) tapMask |= 0x04;
                    }
                    renderer->setCaptureMode(true, tapMask);
                    Serial.printf("Capture mode enabled (tapMask=0x%02X: %s%s%s)\n",
                                  tapMask,
                                  (tapMask & 0x01) ? "A" : "",
                                  (tapMask & 0x02) ? "B" : "",
                                  (tapMask & 0x04) ? "C" : "");
                }
                else if (subcmd.startsWith("dump")) {
                    using namespace lightwaveos::nodes;
                    RendererNode::CaptureTap tap;
                    bool valid = false;

                    if (subcmd.indexOf('a') >= 0) { tap = RendererNode::CaptureTap::TAP_A_PRE_CORRECTION; valid = true; }
                    else if (subcmd.indexOf('b') >= 0) { tap = RendererNode::CaptureTap::TAP_B_POST_CORRECTION; valid = true; }
                    else if (subcmd.indexOf('c') >= 0) { tap = RendererNode::CaptureTap::TAP_C_PRE_WS2812; valid = true; }

                    if (valid) {
                        CRGB frame[320];
                        // If we have no captured frame yet (common right after enabling capture or switching effects),
                        // force a one-shot capture at the requested tap and retry.
                        if (!renderer->getCapturedFrame(tap, frame)) {
                            renderer->forceOneShotCapture(tap);
                            delay(10);  // allow capture to complete
                        }

                        if (renderer->getCapturedFrame(tap, frame)) {
                            auto metadata = renderer->getCaptureMetadata();

                            Serial.write(0xFD);  // Magic
                            Serial.write(0x01);  // Version
                            Serial.write((uint8_t)tap);
                            Serial.write(metadata.effectId);
                            Serial.write(metadata.paletteId);
                            Serial.write(metadata.brightness);
                            Serial.write(metadata.speed);
                            Serial.write((uint8_t)(metadata.frameIndex & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 8) & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 16) & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 24) & 0xFF));
                            Serial.write((uint8_t)(metadata.timestampUs & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 8) & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 16) & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 24) & 0xFF));
                            uint16_t frameLen = 320 * 3;  // RGB × 320 LEDs
                            Serial.write((uint8_t)(frameLen & 0xFF));
                            Serial.write((uint8_t)((frameLen >> 8) & 0xFF));

                            // Payload
                            Serial.write((uint8_t*)frame, frameLen);

                            Serial.printf("\nFrame dumped: tap=%d, effect=%d, palette=%d, frame=%u\n",
                                          (int)tap, metadata.effectId, metadata.paletteId, (unsigned int)metadata.frameIndex);
                        } else {
                            Serial.println("No frame captured for this tap");
                        }
                    } else {
                        Serial.println("Usage: capture dump <a|b|c>");
                    }
                }
                else if (subcmd == "status") {
                    auto metadata = renderer->getCaptureMetadata();
                    Serial.println("\n=== Capture Status ===");
                    Serial.printf("  Enabled: %s\n", renderer->isCaptureModeEnabled() ? "YES" : "NO");
                    Serial.printf("  Last capture: effect=%d, palette=%d, frame=%lu\n",
                                  metadata.effectId, metadata.paletteId, metadata.frameIndex);
                    Serial.println();
                }
                else {
                    Serial.println("Usage: capture <on [a|b|c]|off|dump <a|b|c>|status>");
                }
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

                int effectId = input.substring(7).toInt();
                uint8_t effectCount = renderer->getEffectCount();
                if (effectId < 0 || effectId >= effectCount) {
                    Serial.printf("ERROR: Invalid effect ID. Valid range: 0-%d\n", effectCount - 1);
                } else {
                    currentEffect = (uint8_t)effectId;
                    orchestrator.setEffect((uint8_t)effectId);
                    Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", effectId, renderer->getEffectName(effectId));
                }
            }
        }
        else if (peekChar == 'a' && input.length() > 1) {
            if (inputLower.startsWith("validation_stats") || inputLower == "val_stats") {
                lightwaveos::core::system::ValidationProfiler::generateReport();
                handledMulti = true;
            } else if (inputLower.startsWith("stack_usage") || inputLower == "stack_profile") {
                lightwaveos::core::system::StackMonitor::generateProfileReport();
                handledMulti = true;
            } else if (input.startsWith("ae")) {
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
            // Audio Debug Verbosity: adbg, adbg <0-5>, adbg interval <N>
            else if (inputLower.startsWith("adbg")) {
                handledMulti = true;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();

                if (inputLower == "adbg") {
                    // Show current settings
                    Serial.printf("Audio debug: level=%d interval=%d (8band=%d, 64bin=%d, dma=%d)\n",
                                  dbgCfg.verbosity, dbgCfg.baseInterval,
                                  dbgCfg.interval8Band(), dbgCfg.interval64Bin(), dbgCfg.intervalDMA());
                    Serial.println("Levels: 0=off, 1=errors, 2=status(10s), 3=+DMA, 4=+64bin, 5=+8band");
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
                        const char* names[] = {"off", "errors", "status", "low(+DMA)", "medium(+64bin)", "high(+8band)"};
                        Serial.printf("Audio debug level: %d (%s)\n", level, names[level]);
                    } else {
                        Serial.println("Usage: adbg [0-5] | adbg interval <frames>");
                    }
                }
            }
#endif
        }
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

                // Parse effect ID
                int effectId = input.substring(9).toInt();
                uint8_t effectCount = renderer->getEffectCount();

                if (effectId < 0 || effectId >= effectCount) {
                    Serial.printf("ERROR: Invalid effect ID. Valid range: 0-%d\n", effectCount - 1);
                } else {
                    // Save current effect
                    uint8_t savedEffect = renderer->getCurrentEffect();
                    const char* effectName = renderer->getEffectName(effectId);

                    // Memory baseline
                    uint32_t heapBefore = ESP.getFreeHeap();

                    Serial.printf("\n=== Validating Effect: %s (ID %d) ===\n", effectName, effectId);

                    // Switch to effect temporarily
                    orchestrator.setEffect(effectId);
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
                    CRGB ledBuffer[TOTAL_LEDS];

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
                    orchestrator.setEffect(savedEffect);
                    delay(50);

                    Serial.println("========================================\n");
                }
            } else {
                // Treat non-validate 'v...' input as handled to avoid consuming and then reading a stale single-char command
                handledMulti = true;
                Serial.println("Unknown command. Use: validate <effect_id>");
            }
        }
        else if (peekChar == 'c' && input.length() > 1) {
            if (inputLower.startsWith("capture ")) {
                handledMulti = true;
                String subcmd = inputLower.substring(8);
                
                if (subcmd == "off") {
                    renderer->setCaptureMode(false, 0);
                    Serial.println("Capture mode disabled");
                }
                else if (subcmd.startsWith("on")) {
                    // Parse tap mask: "on" (all), "on a" (tap A), "on b" (tap B), "on c" (tap C), "on ab" (taps A+B), etc.
                    uint8_t tapMask = 0x07;  // Default: all taps
                    if (subcmd.length() > 3) {
                        tapMask = 0;
                        if (subcmd.indexOf('a') >= 0) tapMask |= 0x01;  // Tap A
                        if (subcmd.indexOf('b') >= 0) tapMask |= 0x02;  // Tap B
                        if (subcmd.indexOf('c') >= 0) tapMask |= 0x04;  // Tap C
                    }
                    renderer->setCaptureMode(true, tapMask);
                    Serial.printf("Capture mode enabled (tapMask=0x%02X: %s%s%s)\n",
                                 tapMask,
                                 (tapMask & 0x01) ? "A" : "",
                                 (tapMask & 0x02) ? "B" : "",
                                 (tapMask & 0x04) ? "C" : "");
                }
                else if (subcmd.startsWith("dump ")) {
                    // Dump captured frame: "dump a", "dump b", "dump c"
                    using namespace lightwaveos::nodes;
                    RendererNode::CaptureTap tap;
                    bool valid = false;
                    
                    if (subcmd.indexOf(" a") >= 0) {
                        tap = RendererNode::CaptureTap::TAP_A_PRE_CORRECTION;
                        valid = true;
                    } else if (subcmd.indexOf(" b") >= 0) {
                        tap = RendererNode::CaptureTap::TAP_B_POST_CORRECTION;
                        valid = true;
                    } else if (subcmd.indexOf(" c") >= 0) {
                        tap = RendererNode::CaptureTap::TAP_C_PRE_WS2812;
                        valid = true;
                    }
                    
                    if (valid) {
                        CRGB frame[320];
                        if (renderer->getCapturedFrame(tap, frame)) {
                            auto metadata = renderer->getCaptureMetadata();
                            
                            // Binary frame format:
                            // Header: [MAGIC=0xFD][VERSION=0x01][TAP_ID][EFFECT_ID][PALETTE_ID][BRIGHTNESS][SPEED][FRAME_INDEX(4)][TIMESTAMP(4)][FRAME_LEN(2)]
                            // Payload: [RGB×320]
                            Serial.write(0xFD);  // Magic
                            Serial.write(0x01);  // Version
                            Serial.write((uint8_t)tap);
                            Serial.write(metadata.effectId);
                            Serial.write(metadata.paletteId);
                            Serial.write(metadata.brightness);
                            Serial.write(metadata.speed);
                            Serial.write((uint8_t)(metadata.frameIndex & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 8) & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 16) & 0xFF));
                            Serial.write((uint8_t)((metadata.frameIndex >> 24) & 0xFF));
                            Serial.write((uint8_t)(metadata.timestampUs & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 8) & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 16) & 0xFF));
                            Serial.write((uint8_t)((metadata.timestampUs >> 24) & 0xFF));
                            uint16_t frameLen = 320 * 3;  // RGB × 320 LEDs
                            Serial.write((uint8_t)(frameLen & 0xFF));
                            Serial.write((uint8_t)((frameLen >> 8) & 0xFF));
                            
                            // Payload: RGB data
                            Serial.write((uint8_t*)frame, frameLen);
                            
                            Serial.printf("\nFrame dumped: tap=%d, effect=%d, palette=%d, frame=%u\n",
                                         (int)tap, metadata.effectId, metadata.paletteId, (unsigned int)metadata.frameIndex);
                        } else {
                            Serial.println("No frame captured for this tap");
                        }
                    } else {
                        Serial.println("Usage: capture dump <a|b|c>");
                    }
                }
                else if (subcmd == "status") {
                    handledMulti = true;
                    auto metadata = renderer->getCaptureMetadata();
                    Serial.println("\n=== Capture Status ===");
                    Serial.printf("  Enabled: %s\n", renderer->isCaptureModeEnabled() ? "YES" : "NO");
                    Serial.printf("  Last capture: effect=%d, palette=%d, frame=%lu\n",
                                 metadata.effectId, metadata.paletteId, metadata.frameIndex);
                    Serial.println();
                }
                else {
                    Serial.println("Usage: capture <on [a|b|c]|off|dump <a|b|c>|status>");
                }
            }
        }

        // -----------------------------------------------------------------
        // Tempo Debug Commands: tempo
        // -----------------------------------------------------------------
#if FEATURE_AUDIO_SYNC
        else if (inputLower.startsWith("tempo")) {
            handledMulti = true;

            // Get TempoTracker from AudioNode
            auto* audio = orchestrator.getAudio();
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

        if (handledMulti) {
            // Do not process single-character commands after consuming a multi-char command
        } else if (input.length() == 1) {
            // Single character commands (input is already in 'input' from line buffering)
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
                    const uint8_t audioEffects[] = {72, 73};  // Audio Waveform, Audio Bloom
                    const uint8_t audioEffectCount = sizeof(audioEffects) / sizeof(audioEffects[0]);
                    
                    // Cycle to next audio effect
                    lastAudioEffectIndex = (lastAudioEffectIndex + 1) % audioEffectCount;
                    uint8_t audioEffectId = audioEffects[lastAudioEffectIndex];
                    
                    if (audioEffectId < renderer->getEffectCount()) {
                        currentEffect = audioEffectId;
                        orchestrator.setEffect(audioEffectId);
                        Serial.printf("Audio Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", audioEffectId, renderer->getEffectName(audioEffectId));
                    } else {
                        Serial.printf("ERROR: Audio effect %d not available (effect count: %d)\n", 
                                     audioEffectId, renderer->getEffectCount());
                    }
                } else {
                    // Normal numeric effect selection (0-5, 7-9)
                if (e < renderer->getEffectCount()) {
                    currentEffect = e;
                    orchestrator.setEffect(e);
                    Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", e, renderer->getEffectName(e));
                    }
                }
            } else {
                // Check if this is an effect selection key (a-k = effects 10-20, excludes command letters)
                // Command letters: c, e, g, n, l, p, s, t, z are handled in switch below
                bool isEffectKey = false;
                if (!inZoneMode && cmd >= 'a' && cmd <= 'k' &&
                    cmd != 'c' && cmd != 'e' && cmd != 'g' &&
                    cmd != 'n' && cmd != 'l' && cmd != 'p' && cmd != 's' && cmd != 't' && cmd != 'z') {
                    uint8_t e = 10 + (cmd - 'a');
                    if (e < renderer->getEffectCount()) {
                        currentEffect = e;
                        orchestrator.setEffect(e);
                        Serial.printf("Effect %d: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n", e, renderer->getEffectName(e));
                        isEffectKey = true;
                    }
                }

                if (!isEffectKey)
                switch (cmd) {
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
                    // Save all settings to NVS (auto-load on boot)
                    if (zoneConfigMgr) {
                        Serial.println("Saving settings to NVS...");
                        bool zoneOk = zoneConfigMgr->saveToNVS();
                        bool sysOk = zoneConfigMgr->saveSystemState(
                            renderer->getCurrentEffect(),
                            renderer->getBrightness(),
                            renderer->getSpeed(),
                            renderer->getPaletteIndex()
                        );
                        
                        // Also save to preset library with timestamp name
                        if (presetMgr && zoneOk) {
                            ZoneConfigData config;
                            zoneConfigMgr->exportConfig(config);
                            
                            // Generate timestamp name
                            struct tm timeinfo;
                            char presetName[32];
                            bool timeValid = false;
                            
                            // Try to get local time (requires NTP sync)
                            if (getLocalTime(&timeinfo)) {
                                strftime(presetName, sizeof(presetName), "preset-%Y%m%d-%H%M%S", &timeinfo);
                                timeValid = true;
                            } else {
                                // Fallback: use millis-based name if time not available
                                uint32_t millisNow = millis();
                                snprintf(presetName, sizeof(presetName), "preset-%lu", millisNow);
                            }
                            
                            if (presetMgr->savePreset(presetName, config)) {
                                Serial.printf("  Preset saved: %s\n", presetName);
                            } else {
                                Serial.printf("  Preset save failed: %s\n", presetName);
                            }
                        }
                        
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

                case ' ':  // Spacebar - quick next effect (no Enter needed)
                case 'n':
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        uint8_t newEffectId = currentEffect;

                        switch (currentRegister) {
                            case EffectRegister::ALL:
                                currentEffect = (currentEffect + 1) % effectCount;
                                newEffectId = currentEffect;
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

                        if (newEffectId != 0xFF && newEffectId < effectCount) {
                            currentEffect = newEffectId;
                            orchestrator.setEffect(currentEffect);
                            const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                                 (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                            Serial.printf("Effect %d%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
                                          currentEffect, suffix, renderer->getEffectName(currentEffect));
                        }
                    }
                    break;

                case 'N':
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        uint8_t newEffectId = currentEffect;

                        switch (currentRegister) {
                            case EffectRegister::ALL:
                                currentEffect = (currentEffect + effectCount - 1) % effectCount;
                                newEffectId = currentEffect;
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

                        if (newEffectId != 0xFF && newEffectId < effectCount) {
                            currentEffect = newEffectId;
                            orchestrator.setEffect(currentEffect);
                            const char* suffix = (currentRegister == EffectRegister::REACTIVE) ? "[R]" :
                                                 (currentRegister == EffectRegister::AMBIENT) ? "[M]" : "";
                            Serial.printf("Effect %d%s: " LW_CLR_GREEN "%s" LW_ANSI_RESET "\n",
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
                        uint8_t reactiveId = PatternRegistry::getReactiveEffectId(reactiveRegisterIndex);
                        if (reactiveId != 0xFF && reactiveId < renderer->getEffectCount()) {
                            currentEffect = reactiveId;
                            orchestrator.setEffect(reactiveId);
                            Serial.printf("  Current: %s (ID %d)\n",
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
                        uint8_t ambientId = ambientEffectIds[ambientRegisterIndex];
                        if (ambientId < renderer->getEffectCount()) {
                            currentEffect = ambientId;
                            orchestrator.setEffect(ambientId);
                            Serial.printf("  Current: %s (ID %d)\n",
                                          renderer->getEffectName(ambientId), ambientId);
                        }
                    }
                    break;

                case '*':  // Switch back to All effects register (default)
                    currentRegister = EffectRegister::ALL;
                    Serial.println("Switched to " LW_CLR_GREEN "All Effects" LW_ANSI_RESET " register");
                    Serial.printf("  %d effects available\n", renderer->getEffectCount());
                    Serial.printf("  Current: %s (ID %d)\n",
                                  renderer->getEffectName(currentEffect), currentEffect);
                    break;

                case '+':
                case '=':
                    {
                        uint8_t b = renderer->getBrightness();
                        if (b < 250) {
                            b = min((int)b + 16, 250);
                            orchestrator.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '-':
                    {
                        uint8_t b = renderer->getBrightness();
                        if (b > 16) {
                            b = max((int)b - 16, 16);
                            orchestrator.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '[':
                    {
                        uint8_t s = renderer->getSpeed();
                        if (s > 1) {
                            s = max((int)s - 1, 1);
                            orchestrator.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case ']':
                    {
                        uint8_t s = renderer->getSpeed();
                        if (s < 100) {
                            s = min((int)s + 1, 100);
                            orchestrator.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case '.':  // Next palette (quick key)
                case 'p':
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t p = (renderer->getPaletteIndex() + 1) % paletteCount;
                        orchestrator.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
                    }
                    break;

                case ',':  // Previous palette
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t current = renderer->getPaletteIndex();
                        uint8_t p = (current + paletteCount - 1) % paletteCount;
                        orchestrator.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
                    }
                    break;

                case 'l':
                    {
                        uint8_t effectCount = renderer->getEffectCount();
                        Serial.printf("\n=== Effects (%d total) ===\n", effectCount);
                        for (uint8_t i = 0; i < effectCount; i++) {
                            char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                            const char* type = (renderer->getEffectInstance(i) != nullptr) ? " [IEffect]" : " [Legacy]";
                            Serial.printf("  %2d [%c]: %s%s%s\n", i, key, renderer->getEffectName(i), type,
                                          (!inZoneMode && i == currentEffect) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 'R':
                    // List all saved presets
                    if (presetMgr) {
                        if (!presetMgr->init()) {
                            Serial.println("ERROR: Preset manager not initialized");
                            break;
                        }
                        std::vector<String> presets = presetMgr->listPresets();
                        Serial.printf("\n=== Saved Presets (%d total) ===\n", presets.size());
                        if (presets.empty()) {
                            Serial.println("  No presets saved yet");
                        } else {
                            for (size_t i = 0; i < presets.size(); i++) {
                                lightwaveos::persistence::PresetMetadata metadata;
                                if (presetMgr->getPresetMetadata(presets[i].c_str(), metadata)) {
                                    Serial.printf("  %s", presets[i].c_str());
                                    if (metadata.description.length() > 0) {
                                        Serial.printf(" - %s", metadata.description.c_str());
                                    }
                                    if (metadata.created.length() > 0) {
                                        Serial.printf(" (%s)", metadata.created.c_str());
                                    }
                                    Serial.println();
                                } else {
                                    Serial.printf("  %s\n", presets[i].c_str());
                                }
                            }
                        }
                        Serial.println();
                    } else {
                        Serial.println("ERROR: Preset manager not available");
                    }
                    break;

                case 'P':
                    // List all palettes
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t currentPalette = renderer->getPaletteIndex();
                        Serial.printf("\n=== Palettes (%d total) ===\n", paletteCount);
                        Serial.println("--- Artistic (cpt-city) ---");
                        for (uint8_t i = 0; i <= 32; i++) {
                            Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println("--- Scientific (Crameri) ---");
                        for (uint8_t i = 33; i <= 56; i++) {
                            Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println("--- LGP-Optimized (viridis family) ---");
                        for (uint8_t i = 57; i <= 74; i++) {
                            Serial.printf("  %2d: %s%s\n", i, renderer->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 's':
                    orchestrator.printStatus();
                    if (zoneComposer.isEnabled()) {
                        zoneComposer.printStatus();
                    }
                    if (renderer->isTransitionActive()) {
                        Serial.println("  Transition: ACTIVE");
                    }
                    // Show IEffect status for current effect
                    {
                        uint8_t current = renderer->getCurrentEffect();
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

                case 't':
                    // Random transition to next effect
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        uint8_t nextEffect = (currentEffect + 1) % effectCount;
                        renderer->startRandomTransition(nextEffect);
                        currentEffect = nextEffect;
                        Serial.printf("Transition to: %s\n", renderer->getEffectName(currentEffect));
                    }
                    break;

                case 'T':
                    // Fade transition to next effect
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        uint8_t nextEffect = (currentEffect + 1) % effectCount;
                        renderer->startTransition(nextEffect, 0);  // 0 = FADE
                        currentEffect = nextEffect;
                        Serial.printf("Fade to: %s\n", renderer->getEffectName(currentEffect));
                    }
                    break;

                case '!':
                    // List transition types
                    Serial.println("\n=== Transition Types ===");
                    for (uint8_t i = 0; i < static_cast<uint8_t>(TransitionType::TYPE_COUNT); i++) {
                        Serial.printf("  %2d: %s (%dms)\n", i,
                                      getTransitionName(static_cast<TransitionType>(i)),
                                      getDefaultDuration(static_cast<TransitionType>(i)));
                    }
                    Serial.println();
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

                // ========== Show Playback Commands ==========

                case 'W':
                    // List all shows
                    {
                        Serial.printf("\n=== Shows (%d available) ===\n", BUILTIN_SHOW_COUNT);
                        for (uint8_t i = 0; i < BUILTIN_SHOW_COUNT; i++) {
                            // Read show info from PROGMEM
                            ShowDefinition show;
                            memcpy_P(&show, &BUILTIN_SHOWS[i], sizeof(ShowDefinition));
                            char nameBuf[20];
                            strncpy_P(nameBuf, show.name, sizeof(nameBuf) - 1);
                            nameBuf[sizeof(nameBuf) - 1] = '\0';

                            uint32_t mins = show.totalDurationMs / 60000;
                            uint32_t secs = (show.totalDurationMs % 60000) / 1000;

                            Serial.printf("  %d: %-12s %d:%02d %s%s\n",
                                          i, nameBuf, mins, secs,
                                          show.looping ? "[loops]" : "",
                                          (i == currentShowIndex) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 'w':
                    // Toggle show playback
                    {
                        ShowNode* showDir = orchestrator.getShowDirector();
                        if (showDir) {
                            if (showDir->isPlaying()) {
                                // Stop the show
                                Message stopMsg(MessageType::SHOW_STOP);
                                showDir->send(stopMsg);
                                Serial.println("Show: STOPPED");
                            } else {
                                // Load and start the show
                                Message loadMsg(MessageType::SHOW_LOAD, currentShowIndex);
                                showDir->send(loadMsg);
                                delay(10);  // Allow load to process
                                Message startMsg(MessageType::SHOW_START);
                                showDir->send(startMsg);

                                // Get show name for display
                                ShowDefinition show;
                                memcpy_P(&show, &BUILTIN_SHOWS[currentShowIndex], sizeof(ShowDefinition));
                                char nameBuf[20];
                                strncpy_P(nameBuf, show.name, sizeof(nameBuf) - 1);
                                nameBuf[sizeof(nameBuf) - 1] = '\0';
                                Serial.printf("Show: PLAYING '%s'\n", nameBuf);
                            }
                        } else {
                            Serial.println("ERROR: ShowDirector not available");
                        }
                    }
                    break;

                case '<':
                    // Previous show
                    {
                        currentShowIndex = (currentShowIndex + BUILTIN_SHOW_COUNT - 1) % BUILTIN_SHOW_COUNT;
                        ShowDefinition show;
                        memcpy_P(&show, &BUILTIN_SHOWS[currentShowIndex], sizeof(ShowDefinition));
                        char nameBuf[20];
                        strncpy_P(nameBuf, show.name, sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        Serial.printf("Show %d: %s\n", currentShowIndex, nameBuf);
                    }
                    break;

                case '>':
                    // Next show
                    {
                        currentShowIndex = (currentShowIndex + 1) % BUILTIN_SHOW_COUNT;
                        ShowDefinition show;
                        memcpy_P(&show, &BUILTIN_SHOWS[currentShowIndex], sizeof(ShowDefinition));
                        char nameBuf[20];
                        strncpy_P(nameBuf, show.name, sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        Serial.printf("Show %d: %s\n", currentShowIndex, nameBuf);
                    }
                    break;

                case '{':
                    // Seek backward 30s
                    {
                        ShowNode* showDir = orchestrator.getShowDirector();
                        if (showDir && showDir->isPlaying()) {
                            uint32_t elapsed = showDir->getElapsedMs();
                            uint32_t newTime = (elapsed > 30000) ? (elapsed - 30000) : 0;
                            Message seekMsg(MessageType::SHOW_SEEK, 0, 0, 0, newTime);
                            showDir->send(seekMsg);
                            Serial.printf("Seek: %d:%02d\n", newTime / 60000, (newTime % 60000) / 1000);
                        } else {
                            Serial.println("No show playing");
                        }
                    }
                    break;

                case '}':
                    // Seek forward 30s
                    {
                        ShowNode* showDir = orchestrator.getShowDirector();
                        if (showDir && showDir->isPlaying()) {
                            uint32_t elapsed = showDir->getElapsedMs();
                            uint32_t remaining = showDir->getRemainingMs();
                            uint32_t newTime = (remaining > 30000) ? (elapsed + 30000) : (elapsed + remaining);
                            Message seekMsg(MessageType::SHOW_SEEK, 0, 0, 0, newTime);
                            showDir->send(seekMsg);
                            Serial.printf("Seek: %d:%02d\n", newTime / 60000, (newTime % 60000) / 1000);
                        } else {
                            Serial.println("No show playing");
                        }
                    }
                    break;

                case '#':
                    // Print show status
                    {
                        ShowNode* showDir = orchestrator.getShowDirector();
                        if (showDir) {
                            if (showDir->hasShow()) {
                                ShowDefinition show;
                                memcpy_P(&show, &BUILTIN_SHOWS[showDir->getCurrentShowId()], sizeof(ShowDefinition));
                                char nameBuf[20];
                                strncpy_P(nameBuf, show.name, sizeof(nameBuf) - 1);
                                nameBuf[sizeof(nameBuf) - 1] = '\0';

                                uint32_t elapsed = showDir->getElapsedMs();
                                uint32_t total = show.totalDurationMs;

                                Serial.println("\n=== Show Status ===");
                                Serial.printf("  Show: %s\n", nameBuf);
                                Serial.printf("  State: %s\n",
                                              showDir->isPlaying() ? (showDir->isPaused() ? "PAUSED" : "PLAYING") : "STOPPED");
                                Serial.printf("  Progress: %d:%02d / %d:%02d (%.0f%%)\n",
                                              elapsed / 60000, (elapsed % 60000) / 1000,
                                              total / 60000, (total % 60000) / 1000,
                                              showDir->getProgress() * 100.0f);
                                Serial.printf("  Chapter: %d\n", showDir->getCurrentChapter());
                                Serial.println();
                            } else {
                                Serial.printf("No show loaded. Selected: %d\n", currentShowIndex);
                            }
                        } else {
                            Serial.println("ShowDirector not available");
                        }
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

                case 'e':
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

                case 'B':
                    // Toggle brown guardrail
                    {
                        auto& engine = lightwaveos::enhancement::ColorCorrectionEngine::getInstance();
                        auto& cfg = engine.getConfig();
                        cfg.brownGuardrailEnabled = !cfg.brownGuardrailEnabled;
                        Serial.printf("Brown guardrail: %s\n", cfg.brownGuardrailEnabled ? "ON" : "OFF");
                    }
                    break;

                case 'I':
                    // Show IEffect pilot status
                    {
                        Serial.println("\n=== IEffect Pilot Status ===");
                        uint8_t effectCount = renderer->getEffectCount();
                        uint8_t ieffectCount = 0;
                        uint8_t legacyCount = 0;
                        
                        // Count all effects first
                        for (uint8_t i = 0; i < effectCount; i++) {
                            if (renderer->getEffectInstance(i) != nullptr) {
                                ieffectCount++;
                            } else {
                                legacyCount++;
                            }
                        }
                        
                        Serial.println("\nPilot Effects (IEffect Native):");
                        uint8_t pilotIds[] = {15, 22, 67};  // Modal Resonance, Chevron Waves, Chromatic Interference
                        const char* pilotNames[] = {"LGP Modal Resonance", "LGP Chevron Waves", "LGP Chromatic Interference"};
                        
                        for (uint8_t i = 0; i < 3; i++) {
                            uint8_t id = pilotIds[i];
                            if (id < effectCount) {
                                lightwaveos::plugins::IEffect* effect = renderer->getEffectInstance(id);
                                if (effect != nullptr) {
                                    const lightwaveos::plugins::EffectMetadata& meta = effect->getMetadata();
                                    Serial.printf("  [✓] ID %2d: %s\n", id, pilotNames[i]);
                                    Serial.printf("      Type: IEffect Native\n");
                                    Serial.printf("      Metadata: %s - %s\n", meta.name, meta.description);
                                } else {
                                    Serial.printf("  [✗] ID %2d: %s - NOT REGISTERED AS IEffect!\n", id, pilotNames[i]);
                                }
                            }
                        }
                        
                        Serial.println("\nAll Effects Summary:");
                        Serial.printf("  IEffect Native: %d effects\n", ieffectCount);
                        Serial.printf("  Legacy (function pointer): %d effects\n", legacyCount);
                        Serial.printf("  Total: %d effects\n", effectCount);
                        
                        uint8_t current = renderer->getCurrentEffect();
                        lightwaveos::plugins::IEffect* currentEffect = renderer->getEffectInstance(current);
                        Serial.printf("\nCurrent Effect (ID %d):\n", current);
                        Serial.printf("  Name: %s\n", renderer->getEffectName(current));
                        Serial.printf("  Type: %s\n", currentEffect ? "IEffect Native" : "Legacy (function pointer)");
                        if (currentEffect) {
                            const lightwaveos::plugins::EffectMetadata& meta = currentEffect->getMetadata();
                            Serial.printf("  Category: %d\n", (int)meta.category);
                            Serial.printf("  Version: %d\n", meta.version);
                        }
                        Serial.println();
                    }
                    break;
            }
                }
        }
        }
    }

    // Update NarrativeEngine (auto-play mode)
    NARRATIVE.update();

    // Print status every 10 seconds
    if (now - lastStatus > 10000) {
        const RenderStats& stats = renderer->getStats();
        if (zoneComposer.isEnabled()) {
            Serial.printf("[Status] FPS: %d, CPU: %d%%, Mode: ZONES (%d zones)\n",
                          stats.currentFPS,
                          stats.cpuPercent,
                          zoneComposer.getZoneCount());
        } else {
            Serial.printf("[Status] FPS: %d, CPU: %d%%, Effect: %s\n",
                          stats.currentFPS,
                          stats.cpuPercent,
                          renderer->getEffectName(currentEffect));
        }
        
        // Periodic system health checks
        lightwaveos::core::system::StackMonitor::checkAllTasks();
        lightwaveos::core::system::HeapMonitor::checkHeapIntegrity();
        
        // Scan for memory leaks (every 10 seconds)
        static uint32_t lastLeakScan = 0;
        if (now - lastLeakScan > 10000) {
            lightwaveos::core::system::MemoryLeakDetector::scanForLeaks();
            lastLeakScan = now;
        }
        
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
