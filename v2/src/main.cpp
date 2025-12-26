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
#include "config/features.h"
#include "core/actors/ActorSystem.h"
#include "hardware/EncoderManager.h"
#include "core/actors/RendererActor.h"
#include "core/persistence/NVSManager.h"
#include "core/persistence/ZoneConfigManager.h"
#include "effects/CoreEffects.h"
#include "effects/zones/ZoneComposer.h"
#include "effects/transitions/TransitionEngine.h"
#include "effects/transitions/TransitionTypes.h"
#include "core/narrative/NarrativeEngine.h"
#include "core/actors/ShowDirectorActor.h"
#include "core/shows/BuiltinShows.h"
#include "plugins/api/IEffect.h"

#if FEATURE_WEB_SERVER
#include "network/WiFiManager.h"
#include "network/WebServer.h"
using namespace lightwaveos::network;
using namespace lightwaveos::config;

// Global WebServer instance pointer
WebServer* webServerInstance = nullptr;
#endif

using namespace lightwaveos::persistence;

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::transitions;
using namespace lightwaveos::narrative;
using namespace lightwaveos::plugins;

// ==================== Global Zone Composer ====================

ZoneComposer zoneComposer;
ZoneConfigManager* zoneConfigMgr = nullptr;

// Global Actor System Access
ActorSystem& actors = ActorSystem::instance();
RendererActor* renderer = nullptr;

// Effect count is now dynamic via renderer->getEffectCount()
// Effect names retrieved via renderer->getEffectName(id)

// Current show index for serial navigation
static uint8_t currentShowIndex = 0;

// ==================== Setup ====================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n==========================================");
    Serial.println("LightwaveOS v2 - Actor System + Zones");
    Serial.println("==========================================\n");

    // Initialize Actor System (creates RendererActor)
    Serial.println("Initializing Actor System...");
    if (!actors.init()) {
        Serial.println("ERROR: Actor System init failed!");
        while(1) delay(1000);  // Halt
    }
    renderer = actors.getRenderer();
    Serial.println("  Actor System: INITIALIZED\n");

    // Register ALL effects (core + LGP) BEFORE starting actors
    Serial.println("Registering effects...");
    uint8_t effectCount = registerAllEffects(renderer);
    Serial.printf("  Effects registered: %d\n\n", effectCount);

    // Initialize NVS (must be before Zone Composer to load saved config)
    Serial.println("Initializing NVS...");
    if (!NVS_MANAGER.init()) {
        Serial.println("WARNING: NVS init failed - settings won't persist!");
    } else {
        Serial.println("  NVS: INITIALIZED\n");
    }

    // Initialize Zone Composer
    Serial.println("Initializing Zone Composer...");
    if (!zoneComposer.init(renderer)) {
        Serial.println("ERROR: Zone Composer init failed!");
    } else {
        // Attach zone composer to renderer
        renderer->setZoneComposer(&zoneComposer);

        // Create config manager
        zoneConfigMgr = new ZoneConfigManager(&zoneComposer);

        // Try to load saved zone configuration
        if (zoneConfigMgr->loadFromNVS()) {
            Serial.println("  Zone Composer: INITIALIZED (restored from NVS)");
        } else {
            // First boot - load default preset
            zoneComposer.loadPreset(2);
            Serial.println("  Zone Composer: INITIALIZED");
            Serial.println("  Preset: Triple Rings (default)");
        }
        Serial.println();
    }

    // Start all actors (RendererActor runs on Core 1 at 120 FPS)
    Serial.println("Starting Actor System...");
    if (!actors.start()) {
        Serial.println("ERROR: Actor System start failed!");
        while(1) delay(1000);  // Halt
    }
    Serial.println("  Actor System: RUNNING\n");

    // Load or set initial state
    Serial.println("Loading system state...");
    uint8_t savedEffect, savedBrightness, savedSpeed, savedPalette;
    if (zoneConfigMgr && zoneConfigMgr->loadSystemState(savedEffect, savedBrightness, savedSpeed, savedPalette)) {
        actors.setEffect(savedEffect);
        actors.setBrightness(savedBrightness);
        actors.setSpeed(savedSpeed);
        actors.setPalette(savedPalette);
        Serial.printf("  Restored: Effect=%d, Brightness=%d, Speed=%d, Palette=%d\n",
                      savedEffect, savedBrightness, savedSpeed, savedPalette);
    } else {
        // First boot defaults
        actors.setEffect(0);       // Fire
        actors.setBrightness(128); // 50% brightness
        actors.setSpeed(15);       // Medium speed
        actors.setPalette(0);      // Party colors
        Serial.println("  Using defaults (first boot)");
    }
    Serial.println();

    // Initialize Network (if enabled)
#if FEATURE_WEB_SERVER
    // Start WiFiManager BEFORE WebServer
    Serial.println("Initializing WiFiManager...");
    WIFI_MANAGER.setCredentials(
        NetworkConfig::WIFI_SSID_VALUE,
        NetworkConfig::WIFI_PASSWORD_VALUE
    );
    WIFI_MANAGER.enableSoftAP("LightwaveOS-Setup", "lightwave123");

    if (!WIFI_MANAGER.begin()) {
        Serial.println("ERROR: WiFiManager failed to start!");
    } else {
        Serial.println("  WiFiManager: STARTED");

        // Wait for connection or AP mode (with timeout)
        Serial.println("  Connecting to WiFi...");
        uint32_t waitStart = millis();
        while (!WIFI_MANAGER.isConnected() && !WIFI_MANAGER.isAPMode()) {
            if (millis() - waitStart > 30000) {
                Serial.println("  WiFi connection timeout, continuing...");
                break;
            }
            delay(100);
        }

        if (WIFI_MANAGER.isConnected()) {
            Serial.printf("  WiFi: CONNECTED to %s\n", NetworkConfig::WIFI_SSID_VALUE);
            Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (WIFI_MANAGER.isAPMode()) {
            Serial.println("  WiFi: AP MODE (connect to LightwaveOS-Setup)");
            Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());
        }
    }
    Serial.println();

    // Start WebServer
    Serial.println("Starting Web Server...");
    
    // Instantiate WebServer with dependencies
    webServerInstance = new WebServer(actors, renderer);
    
    if (!webServerInstance->begin()) {
        Serial.println("WARNING: Web Server failed to start!");
    } else {
        Serial.println("  Web Server: RUNNING");
        Serial.printf("  REST API: http://lightwaveos.local/api/v1/\n");
        Serial.printf("  WebSocket: ws://lightwaveos.local/ws\n\n");
    }
#endif

    Serial.println("==========================================");
    Serial.println("ACTOR SYSTEM: OPERATIONAL");
    Serial.println("==========================================\n");
    Serial.println("Commands:");
    Serial.println("  SPACE   - Next effect (quick tap)");
    Serial.println("  0-9/a-k - Select effect by key");
    Serial.println("  n/N     - Next/Prev effect");
    Serial.println("  +/-     - Adjust brightness");
    Serial.println("  [/]     - Adjust speed");
    Serial.println("  ,/.     - Prev/Next palette (75 total)");
    Serial.println("  l       - List effects");
    Serial.println("  P       - List palettes");
    Serial.println("  s       - Print status");
    Serial.println("\nZone Commands:");
    Serial.println("  z       - Toggle zone mode");
    Serial.println("  Z       - Print zone status");
    Serial.println("  1-5     - Load zone preset (in zone mode)");
    Serial.println("  S       - Save all settings to NVS");
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
    uint32_t now = millis();

#if FEATURE_ROTATE8_ENCODER
    // Handle encoder events
    using namespace lightwaveos::hardware;
    EncoderEvent event;
    while (xQueueReceive(encoderManager.getEventQueue(), &event, 0) == pdTRUE) {
        handleEncoderEvent(event, actors, renderer);
    }
#endif

    // Handle serial commands
    if (Serial.available()) {
        bool handledMulti = false;
        int peekChar = Serial.peek();

        // Color correction commands (cc, ae, gamma, brown) and capture commands
        if (peekChar == 'c' && Serial.available() > 1) {
            // Peek ahead to check for 'cc' command
            String input = Serial.readStringUntil('\n');
            input.trim();
            String inputLower = input;
            inputLower.toLowerCase();

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
                    using namespace lightwaveos::actors;
                    RendererActor::CaptureTap tap;
                    bool valid = false;

                    if (subcmd.indexOf('a') >= 0) { tap = RendererActor::CaptureTap::TAP_A_PRE_CORRECTION; valid = true; }
                    else if (subcmd.indexOf('b') >= 0) { tap = RendererActor::CaptureTap::TAP_B_POST_CORRECTION; valid = true; }
                    else if (subcmd.indexOf('c') >= 0) { tap = RendererActor::CaptureTap::TAP_C_PRE_WS2812; valid = true; }

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
        else if (peekChar == 'e' && Serial.available() > 1) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            String inputLower = input;
            inputLower.toLowerCase();

            if (inputLower.startsWith("effect ")) {
                handledMulti = true;

                int effectId = input.substring(7).toInt();
                uint8_t effectCount = renderer->getEffectCount();
                if (effectId < 0 || effectId >= effectCount) {
                    Serial.printf("ERROR: Invalid effect ID. Valid range: 0-%d\n", effectCount - 1);
                } else {
                    currentEffect = (uint8_t)effectId;
                    actors.setEffect((uint8_t)effectId);
                    Serial.printf("Effect %d: %s\n", effectId, renderer->getEffectName(effectId));
                }
            }
        }
        else if (peekChar == 'a' && Serial.available() > 1) {
            String input = Serial.readStringUntil('\n');
            input.trim();

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
        }
        else if (peekChar == 'g') {
            String input = Serial.readStringUntil('\n');
            input.trim();

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
        else if (peekChar == 'b' && Serial.available() > 1) {
            String input = Serial.readStringUntil('\n');
            input.trim();

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
        else if (peekChar == 'C' && Serial.available() > 1) {
            String input = Serial.readStringUntil('\n');
            input.trim();

            if (input == "Csave") {
                handledMulti = true;
                lightwaveos::enhancement::ColorCorrectionEngine::getInstance().saveToNVS();
                Serial.println("Color correction settings saved to NVS");
            }
        }
        else if (peekChar == 'v' || peekChar == 'V') {
            String input = Serial.readStringUntil('\n');
            input.trim();
            String inputLower = input;
            inputLower.toLowerCase();

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
        else if (peekChar == 'c' && Serial.available() > 1) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            String inputLower = input;
            inputLower.toLowerCase();

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
                    using namespace lightwaveos::actors;
                    RendererActor::CaptureTap tap;
                    bool valid = false;
                    
                    if (subcmd.indexOf(" a") >= 0) {
                        tap = RendererActor::CaptureTap::TAP_A_PRE_CORRECTION;
                        valid = true;
                    } else if (subcmd.indexOf(" b") >= 0) {
                        tap = RendererActor::CaptureTap::TAP_B_POST_CORRECTION;
                        valid = true;
                    } else if (subcmd.indexOf(" c") >= 0) {
                        tap = RendererActor::CaptureTap::TAP_C_PRE_WS2812;
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

        if (handledMulti) {
            // Do not process single-character commands after consuming a full-line command
        } else {
            // Single character commands
            char cmd = Serial.read();

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
                        actors.setEffect(audioEffectId);
                        Serial.printf("Audio Effect %d: %s\n", audioEffectId, renderer->getEffectName(audioEffectId));
                    } else {
                        Serial.printf("ERROR: Audio effect %d not available (effect count: %d)\n", 
                                     audioEffectId, renderer->getEffectCount());
                    }
                } else {
                    // Normal numeric effect selection (0-5, 7-9)
                if (e < renderer->getEffectCount()) {
                    currentEffect = e;
                    actors.setEffect(e);
                    Serial.printf("Effect %d: %s\n", e, renderer->getEffectName(e));
                    }
                }
            } else {
                // Check if this is an effect selection key (a-k = effects 10-20, excludes command letters)
                // Command letters: n, l, p, s, t, z are handled in switch below
                bool isEffectKey = false;
                if (!inZoneMode && cmd >= 'a' && cmd <= 'k' &&
                    cmd != 'n' && cmd != 'l' && cmd != 'p' && cmd != 's' && cmd != 't' && cmd != 'z') {
                    uint8_t e = 10 + (cmd - 'a');
                    if (e < renderer->getEffectCount()) {
                        currentEffect = e;
                        actors.setEffect(e);
                        Serial.printf("Effect %d: %s\n", e, renderer->getEffectName(e));
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

                case ' ':  // Spacebar - quick next effect (no Enter needed)
                case 'n':
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        currentEffect = (currentEffect + 1) % effectCount;
                        actors.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, renderer->getEffectName(currentEffect));
                    }
                    break;

                case 'N':
                    if (!inZoneMode) {
                        uint8_t effectCount = renderer->getEffectCount();
                        currentEffect = (currentEffect + effectCount - 1) % effectCount;
                        actors.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, renderer->getEffectName(currentEffect));
                    }
                    break;

                case '+':
                case '=':
                    {
                        uint8_t b = renderer->getBrightness();
                        if (b < 160) {
                            b = min((int)b + 16, 160);
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
                            s = max((int)s - 5, 1);
                            actors.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case ']':
                    {
                        uint8_t s = renderer->getSpeed();
                        if (s < 50) {
                            s = min((int)s + 5, 50);
                            actors.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case '.':  // Next palette (quick key)
                case 'p':
                    {
                        uint8_t paletteCount = renderer->getPaletteCount();
                        uint8_t p = (renderer->getPaletteIndex() + 1) % paletteCount;
                        actors.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, renderer->getPaletteName(p));
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
                    actors.printStatus();
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
                        ShowDirectorActor* showDir = actors.getShowDirector();
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
                        ShowDirectorActor* showDir = actors.getShowDirector();
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
                        ShowDirectorActor* showDir = actors.getShowDirector();
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
                        ShowDirectorActor* showDir = actors.getShowDirector();
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
