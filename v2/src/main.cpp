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
#include "config/features.h"
#include "core/actors/ActorSystem.h"
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

#if FEATURE_WEB_SERVER
#include "network/WiFiManager.h"
#include "network/WebServer.h"
using namespace lightwaveos::network;
using namespace lightwaveos::config;
#endif

using namespace lightwaveos::persistence;

using namespace lightwaveos::actors;
using namespace lightwaveos::effects;
using namespace lightwaveos::zones;
using namespace lightwaveos::transitions;
using namespace lightwaveos::narrative;

// ==================== Global Zone Composer ====================

ZoneComposer zoneComposer;
ZoneConfigManager* zoneConfigMgr = nullptr;

// Effect count is now dynamic via RENDERER->getEffectCount()
// Effect names retrieved via RENDERER->getEffectName(id)

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
    if (!ACTOR_SYSTEM.init()) {
        Serial.println("ERROR: Actor System init failed!");
        while(1) delay(1000);  // Halt
    }
    Serial.println("  Actor System: INITIALIZED\n");

    // Register ALL effects (core + LGP) BEFORE starting actors
    Serial.println("Registering effects...");
    uint8_t effectCount = registerAllEffects(RENDERER);
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
    if (!zoneComposer.init(RENDERER)) {
        Serial.println("ERROR: Zone Composer init failed!");
    } else {
        // Attach zone composer to renderer
        RENDERER->setZoneComposer(&zoneComposer);

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
    if (!ACTOR_SYSTEM.start()) {
        Serial.println("ERROR: Actor System start failed!");
        while(1) delay(1000);  // Halt
    }
    Serial.println("  Actor System: RUNNING\n");

    // Load or set initial state
    Serial.println("Loading system state...");
    uint8_t savedEffect, savedBrightness, savedSpeed, savedPalette;
    if (zoneConfigMgr && zoneConfigMgr->loadSystemState(savedEffect, savedBrightness, savedSpeed, savedPalette)) {
        ACTOR_SYSTEM.setEffect(savedEffect);
        ACTOR_SYSTEM.setBrightness(savedBrightness);
        ACTOR_SYSTEM.setSpeed(savedSpeed);
        ACTOR_SYSTEM.setPalette(savedPalette);
        Serial.printf("  Restored: Effect=%d, Brightness=%d, Speed=%d, Palette=%d\n",
                      savedEffect, savedBrightness, savedSpeed, savedPalette);
    } else {
        // First boot defaults
        ACTOR_SYSTEM.setEffect(0);       // Fire
        ACTOR_SYSTEM.setBrightness(128); // 50% brightness
        ACTOR_SYSTEM.setSpeed(15);       // Medium speed
        ACTOR_SYSTEM.setPalette(0);      // Party colors
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
    if (!webServer.begin()) {
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
    uint32_t now = millis();

    // Handle serial commands
    if (Serial.available()) {
        bool handledMulti = false;
        int peekChar = Serial.peek();
        if (peekChar == 'v' || peekChar == 'V') {
            String input = Serial.readStringUntil('\n');
            input.trim();
            String inputLower = input;
            inputLower.toLowerCase();

            if (inputLower.startsWith("validate ")) {
                handledMulti = true;

                // Parse effect ID
                int effectId = input.substring(9).toInt();
                uint8_t effectCount = RENDERER->getEffectCount();

                if (effectId < 0 || effectId >= effectCount) {
                    Serial.printf("ERROR: Invalid effect ID. Valid range: 0-%d\n", effectCount - 1);
                } else {
                    // Save current effect
                    uint8_t savedEffect = RENDERER->getCurrentEffect();
                    const char* effectName = RENDERER->getEffectName(effectId);

                    // Memory baseline
                    uint32_t heapBefore = ESP.getFreeHeap();

                    Serial.printf("\n=== Validating Effect: %s (ID %d) ===\n", effectName, effectId);

                    // Switch to effect temporarily
                    ACTOR_SYSTEM.setEffect(effectId);
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
                        RENDERER->getBufferCopy(ledBuffer);

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
                    const RenderStats& finalStats = RENDERER->getStats();
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
                    ACTOR_SYSTEM.setEffect(savedEffect);
                    delay(50);

                    Serial.println("========================================\n");
                }
            } else {
                // Treat non-validate 'v...' input as handled to avoid consuming and then reading a stale single-char command
                handledMulti = true;
                Serial.println("Unknown command. Use: validate <effect_id>");
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
                if (e < RENDERER->getEffectCount()) {
                    currentEffect = e;
                    ACTOR_SYSTEM.setEffect(e);
                    Serial.printf("Effect %d: %s\n", e, RENDERER->getEffectName(e));
                }
            } else {
                // Check if this is an effect selection key (a-k = effects 10-20, excludes command letters)
                // Command letters: n, l, p, s, t, z are handled in switch below
                bool isEffectKey = false;
                if (!inZoneMode && cmd >= 'a' && cmd <= 'k' &&
                    cmd != 'n' && cmd != 'l' && cmd != 'p' && cmd != 's' && cmd != 't' && cmd != 'z') {
                    uint8_t e = 10 + (cmd - 'a');
                    if (e < RENDERER->getEffectCount()) {
                        currentEffect = e;
                        ACTOR_SYSTEM.setEffect(e);
                        Serial.printf("Effect %d: %s\n", e, RENDERER->getEffectName(e));
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
                            RENDERER->getCurrentEffect(),
                            RENDERER->getBrightness(),
                            RENDERER->getSpeed(),
                            RENDERER->getPaletteIndex()
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
                        uint8_t effectCount = RENDERER->getEffectCount();
                        currentEffect = (currentEffect + 1) % effectCount;
                        ACTOR_SYSTEM.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, RENDERER->getEffectName(currentEffect));
                    }
                    break;

                case 'N':
                    if (!inZoneMode) {
                        uint8_t effectCount = RENDERER->getEffectCount();
                        currentEffect = (currentEffect + effectCount - 1) % effectCount;
                        ACTOR_SYSTEM.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, RENDERER->getEffectName(currentEffect));
                    }
                    break;

                case '+':
                case '=':
                    {
                        uint8_t b = RENDERER->getBrightness();
                        if (b < 160) {
                            b = min((int)b + 16, 160);
                            ACTOR_SYSTEM.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '-':
                    {
                        uint8_t b = RENDERER->getBrightness();
                        if (b > 16) {
                            b = max((int)b - 16, 16);
                            ACTOR_SYSTEM.setBrightness(b);
                            Serial.printf("Brightness: %d\n", b);
                        }
                    }
                    break;

                case '[':
                    {
                        uint8_t s = RENDERER->getSpeed();
                        if (s > 1) {
                            s = max((int)s - 5, 1);
                            ACTOR_SYSTEM.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case ']':
                    {
                        uint8_t s = RENDERER->getSpeed();
                        if (s < 50) {
                            s = min((int)s + 5, 50);
                            ACTOR_SYSTEM.setSpeed(s);
                            Serial.printf("Speed: %d\n", s);
                        }
                    }
                    break;

                case '.':  // Next palette (quick key)
                case 'p':
                    {
                        uint8_t paletteCount = RENDERER->getPaletteCount();
                        uint8_t p = (RENDERER->getPaletteIndex() + 1) % paletteCount;
                        ACTOR_SYSTEM.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, RENDERER->getPaletteName(p));
                    }
                    break;

                case ',':  // Previous palette
                    {
                        uint8_t paletteCount = RENDERER->getPaletteCount();
                        uint8_t current = RENDERER->getPaletteIndex();
                        uint8_t p = (current + paletteCount - 1) % paletteCount;
                        ACTOR_SYSTEM.setPalette(p);
                        Serial.printf("Palette %d/%d: %s\n", p, paletteCount, RENDERER->getPaletteName(p));
                    }
                    break;

                case 'l':
                    {
                        uint8_t effectCount = RENDERER->getEffectCount();
                        Serial.printf("\n=== Effects (%d total) ===\n", effectCount);
                        for (uint8_t i = 0; i < effectCount; i++) {
                            char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                            Serial.printf("  %2d [%c]: %s%s\n", i, key, RENDERER->getEffectName(i),
                                          (!inZoneMode && i == currentEffect) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 'P':
                    // List all palettes
                    {
                        uint8_t paletteCount = RENDERER->getPaletteCount();
                        uint8_t currentPalette = RENDERER->getPaletteIndex();
                        Serial.printf("\n=== Palettes (%d total) ===\n", paletteCount);
                        Serial.println("--- Artistic (cpt-city) ---");
                        for (uint8_t i = 0; i <= 32; i++) {
                            Serial.printf("  %2d: %s%s\n", i, RENDERER->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println("--- Scientific (Crameri) ---");
                        for (uint8_t i = 33; i <= 56; i++) {
                            Serial.printf("  %2d: %s%s\n", i, RENDERER->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println("--- LGP-Optimized (viridis family) ---");
                        for (uint8_t i = 57; i <= 74; i++) {
                            Serial.printf("  %2d: %s%s\n", i, RENDERER->getPaletteName(i),
                                          (i == currentPalette) ? " <--" : "");
                        }
                        Serial.println();
                    }
                    break;

                case 's':
                    ACTOR_SYSTEM.printStatus();
                    if (zoneComposer.isEnabled()) {
                        zoneComposer.printStatus();
                    }
                    if (RENDERER->isTransitionActive()) {
                        Serial.println("  Transition: ACTIVE");
                    }
                    break;

                case 't':
                    // Random transition to next effect
                    if (!inZoneMode) {
                        uint8_t effectCount = RENDERER->getEffectCount();
                        uint8_t nextEffect = (currentEffect + 1) % effectCount;
                        RENDERER->startRandomTransition(nextEffect);
                        currentEffect = nextEffect;
                        Serial.printf("Transition to: %s\n", RENDERER->getEffectName(currentEffect));
                    }
                    break;

                case 'T':
                    // Fade transition to next effect
                    if (!inZoneMode) {
                        uint8_t effectCount = RENDERER->getEffectCount();
                        uint8_t nextEffect = (currentEffect + 1) % effectCount;
                        RENDERER->startTransition(nextEffect, 0);  // 0 = FADE
                        currentEffect = nextEffect;
                        Serial.printf("Fade to: %s\n", RENDERER->getEffectName(currentEffect));
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
                        ShowDirectorActor* showDir = ACTOR_SYSTEM.getShowDirector();
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
                        ShowDirectorActor* showDir = ACTOR_SYSTEM.getShowDirector();
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
                        ShowDirectorActor* showDir = ACTOR_SYSTEM.getShowDirector();
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
                        ShowDirectorActor* showDir = ACTOR_SYSTEM.getShowDirector();
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
            }
        }
        }
    }

    // Update NarrativeEngine (auto-play mode)
    NARRATIVE.update();

    // Print status every 10 seconds
    if (now - lastStatus > 10000) {
        const RenderStats& stats = RENDERER->getStats();
        if (zoneComposer.isEnabled()) {
            Serial.printf("[Status] FPS: %d, CPU: %d%%, Mode: ZONES (%d zones)\n",
                          stats.currentFPS,
                          stats.cpuPercent,
                          zoneComposer.getZoneCount());
        } else {
            Serial.printf("[Status] FPS: %d, CPU: %d%%, Effect: %s\n",
                          stats.currentFPS,
                          stats.cpuPercent,
                          RENDERER->getEffectName(currentEffect));
        }
        lastStatus = now;
    }

    // Update WebServer (if enabled)
    // Note: WiFiManager runs on its own FreeRTOS task
#if FEATURE_WEB_SERVER
    webServer.update();
#endif

    // Main loop is mostly idle - actors run in background
    delay(10);
}
