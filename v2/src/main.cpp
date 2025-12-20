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

// ==================== Effect Names for Menu ====================

static const char* effectNames[] = {
    "Fire", "Ocean", "Plasma", "Confetti", "Sinelon",
    "Juggle", "BPM", "Wave", "Ripple", "Heartbeat",
    "Interference", "Breathing", "Pulse"
};
static const uint8_t NUM_EFFECTS = sizeof(effectNames) / sizeof(effectNames[0]);

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
    Serial.println("  0-9/a-c - Select effect (single mode)");
    Serial.println("  n/N     - Next/Prev effect");
    Serial.println("  +/-     - Adjust brightness");
    Serial.println("  [/]     - Adjust speed");
    Serial.println("  p       - Next palette");
    Serial.println("  l       - List effects");
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
            if (e < NUM_EFFECTS) {
                currentEffect = e;
                ACTOR_SYSTEM.setEffect(e);
                Serial.printf("Effect %d: %s\n", e, effectNames[e]);
            }
        } else if (!inZoneMode && cmd >= 'a' && cmd <= 'c') {
            uint8_t e = 10 + (cmd - 'a');
            if (e < NUM_EFFECTS) {
                currentEffect = e;
                ACTOR_SYSTEM.setEffect(e);
                Serial.printf("Effect %d: %s\n", e, effectNames[e]);
            }
        } else {
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

                case 'n':
                    if (!inZoneMode) {
                        currentEffect = (currentEffect + 1) % NUM_EFFECTS;
                        ACTOR_SYSTEM.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, effectNames[currentEffect]);
                    }
                    break;

                case 'N':
                    if (!inZoneMode) {
                        currentEffect = (currentEffect + NUM_EFFECTS - 1) % NUM_EFFECTS;
                        ACTOR_SYSTEM.setEffect(currentEffect);
                        Serial.printf("Effect %d: %s\n", currentEffect, effectNames[currentEffect]);
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

                case 'p':
                    {
                        uint8_t p = (RENDERER->getPaletteIndex() + 1) % 8;
                        ACTOR_SYSTEM.setPalette(p);
                        Serial.printf("Palette: %d\n", p);
                    }
                    break;

                case 'l':
                    Serial.println("\n=== Effects ===");
                    for (uint8_t i = 0; i < NUM_EFFECTS; i++) {
                        char key = (i < 10) ? ('0' + i) : ('a' + i - 10);
                        Serial.printf("  %c: %s%s\n", key, effectNames[i],
                                      (!inZoneMode && i == currentEffect) ? " <--" : "");
                    }
                    Serial.println();
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
                        uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
                        RENDERER->startRandomTransition(nextEffect);
                        currentEffect = nextEffect;
                        Serial.printf("Transition to: %s\n", effectNames[currentEffect]);
                    }
                    break;

                case 'T':
                    // Fade transition to next effect
                    if (!inZoneMode) {
                        uint8_t nextEffect = (currentEffect + 1) % NUM_EFFECTS;
                        RENDERER->startTransition(nextEffect, 0);  // 0 = FADE
                        currentEffect = nextEffect;
                        Serial.printf("Fade to: %s\n", effectNames[currentEffect]);
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
                          effectNames[currentEffect]);
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
