/**
 * LightwaveOS Hub (Tab5) - Full Dashboard
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <lvgl.h>
#include <Wire.h>

#include "config/Config.h"
#include "config/AnsiColors.h"
#include "hub_integration.h"
#include "ui/lvgl_bridge.h"
#include "ui/HubDashboard.h"
#include "input/I2CRecovery.h"
#include "input/DualEncoderService.h"
#include "input/ButtonHandler.h"
#include "input/CoarseModeManager.h"
#include "parameters/ParameterHandler.h"
#include "input/ClickDetector.h"
#include "presets/PresetManager.h"
#include "storage/NvsStorage.h"

// Global instances
HubDashboard g_dashboard;

static DualEncoderService* g_encoders = nullptr;
static ButtonHandler* g_buttonHandler = nullptr;
static CoarseModeManager g_coarseModeManager;
static ParameterHandler* g_paramHandler = nullptr;
static PresetManager* g_presetManager = nullptr;
static ClickDetector g_presetClicks[PRESET_SLOT_COUNT];

// =====================================================================
// UI-safe registry event bridging (async_tcp → main loop)
// =====================================================================
// HubRegistry events can originate from AsyncWebSocket callbacks (async_tcp task).
// LVGL must only be touched from the Arduino main loop thread.
// Keep this heap-free and bounded.
struct UiRegistryEvent {
    uint8_t nodeId;
    uint8_t eventType;
    char message[96];
};

static constexpr uint8_t kUiEventQueueMax = 16;
static UiRegistryEvent g_uiEvents[kUiEventQueueMax];
static uint8_t g_uiEvtHead = 0;
static uint8_t g_uiEvtTail = 0;
static uint8_t g_uiEvtCount = 0;
static portMUX_TYPE g_uiEvtMux = portMUX_INITIALIZER_UNLOCKED;

static void enqueueUiRegistryEvent(uint8_t nodeId, uint8_t eventType, const char* message) {
    portENTER_CRITICAL(&g_uiEvtMux);
    if (g_uiEvtCount < kUiEventQueueMax) {
        UiRegistryEvent& slot = g_uiEvents[g_uiEvtTail];
        slot.nodeId = nodeId;
        slot.eventType = eventType;
        strlcpy(slot.message, message ? message : "", sizeof(slot.message));
        g_uiEvtTail = (uint8_t)((g_uiEvtTail + 1) % kUiEventQueueMax);
        g_uiEvtCount++;
    }
    portEXIT_CRITICAL(&g_uiEvtMux);
}

static bool dequeueUiRegistryEvent(UiRegistryEvent& out) {
    bool ok = false;
    portENTER_CRITICAL(&g_uiEvtMux);
    if (g_uiEvtCount > 0) {
        out = g_uiEvents[g_uiEvtHead];
        g_uiEvtHead = (uint8_t)((g_uiEvtHead + 1) % kUiEventQueueMax);
        g_uiEvtCount--;
        ok = true;
    }
    portEXIT_CRITICAL(&g_uiEvtMux);
    return ok;
}

#if ENABLE_ENCODER_DIAGNOSTICS
static void scanI2C(TwoWire& wire, const char* label) {
    Serial.printf("[I2C] Scan (%s) start\n", label);
    uint8_t found = 0;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        wire.beginTransmission(addr);
        uint8_t err = wire.endTransmission();
        if (err == 0) {
            Serial.printf("[I2C] Scan (%s) found: 0x%02X\n", label, addr);
            found++;
        }
        delay(2);
    }

    if (found == 0) {
        Serial.printf("[I2C] Scan (%s) no devices\n", label);
    }
}
#endif

static void onEncoderChange(uint8_t index, uint16_t value, bool wasReset) {
    if (g_paramHandler) {
        g_paramHandler->onEncoderChanged(index, value, wasReset);
    }
}

static void processSerialCommands() {
    static char buf[96];
    static uint8_t len = 0;

    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;

        if (c == '\r') continue;
        if (c == '\n') {
            buf[len] = '\0';
            len = 0;

            // Commands:
            //   tslog on|off
            //   fanout on|off
            //   udplog on|off
            //   udplog <ms>
            //   ctrllog on|off
            //   set effect <id>
            //   set palette <id>
            //   set bright <0-255>
            //   set speed <0-255>
            //   get global
            //   help
            if (strcmp(buf, "help") == 0) {
                LOG_HUB("Commands: tslog on|off, fanout on|off, udplog on|off, udplog <ms>, ctrllog on|off, set effect/palette/bright/speed <v>, get global");
                return;
            }

            const char* on = "tslog on";
            const char* off = "tslog off";
            if (strcmp(buf, on) == 0) {
                if (g_hubMain) g_hubMain->setTimeSyncUdpVerbose(true);
                LOG_HUB("TS-UDP verbose: ON");
                return;
            }
            if (strcmp(buf, off) == 0) {
                if (g_hubMain) g_hubMain->setTimeSyncUdpVerbose(false);
                LOG_HUB("TS-UDP verbose: OFF");
                return;
            }

            if (strcmp(buf, "fanout on") == 0) {
                if (g_hubMain) g_hubMain->setFanoutEnabled(true);
                LOG_HUB("Hub UDP fanout: ON");
                return;
            }
            if (strcmp(buf, "fanout off") == 0) {
                if (g_hubMain) g_hubMain->setFanoutEnabled(false);
                LOG_HUB("Hub UDP fanout: OFF");
                return;
            }

            if (strcmp(buf, "udplog on") == 0) {
                if (g_hubMain) g_hubMain->setFanoutVerbose(true);
                LOG_HUB("Hub UDP verbose: ON");
                return;
            }
            if (strcmp(buf, "udplog off") == 0) {
                if (g_hubMain) g_hubMain->setFanoutVerbose(false);
                LOG_HUB("Hub UDP verbose: OFF");
                return;
            }
            if (strncmp(buf, "udplog ", 7) == 0) {
                uint32_t intervalMs = (uint32_t)atoi(buf + 7);
                if (intervalMs >= 250 && intervalMs <= 60000) {
                    if (g_hubMain) g_hubMain->setFanoutLogIntervalMs(intervalMs);
                    LOG_HUB("Hub UDP interval: %lu ms", (unsigned long)intervalMs);
                } else {
                    LOG_HUB("udplog <ms> out of range (250..60000)");
                }
                return;
            }

            if (strcmp(buf, "ctrllog on") == 0) {
                if (g_hubMain) g_hubMain->setControlVerbose(true);
                LOG_HUB("Hub control trace: ON");
                return;
            }
            if (strcmp(buf, "ctrllog off") == 0) {
                if (g_hubMain) g_hubMain->setControlVerbose(false);
                LOG_HUB("Hub control trace: OFF");
                return;
            }

            if (strcmp(buf, "get global") == 0) {
                if (!g_hubMain) return;
                HubState::GlobalParams g = g_hubMain->getState()->getGlobalSnapshot();
                LOG_HUB("Global: effect=%u palette=%u bright=%u speed=%u hue=%u intensity=%u saturation=%u complexity=%u variation=%u",
                        (unsigned)g.effectId, (unsigned)g.paletteId, (unsigned)g.brightness, (unsigned)g.speed,
                        (unsigned)g.hue, (unsigned)g.intensity, (unsigned)g.saturation,
                        (unsigned)g.complexity, (unsigned)g.variation);
                return;
            }

            if (strncmp(buf, "set ", 4) == 0) {
                if (!g_hubMain) return;
                HubState* state = g_hubMain->getState();
                const char* arg = buf + 4;

                if (strncmp(arg, "effect ", 7) == 0) {
                    int v = atoi(arg + 7);
                    if (v < 0 || v > 255) { LOG_HUB("set effect out of range (0..255)"); return; }
                    state->setGlobalEffect((uint8_t)v);
                    LOG_HUB("Set effect=%d", v);
                    return;
                }
                if (strncmp(arg, "palette ", 8) == 0) {
                    int v = atoi(arg + 8);
                    if (v < 0 || v > 255) { LOG_HUB("set palette out of range (0..255)"); return; }
                    state->setGlobalPalette((uint8_t)v);
                    LOG_HUB("Set palette=%d", v);
                    return;
                }
                if (strncmp(arg, "bright ", 7) == 0) {
                    int v = atoi(arg + 7);
                    if (v < 0 || v > 255) { LOG_HUB("set bright out of range (0..255)"); return; }
                    state->setGlobalBrightness((uint8_t)v);
                    LOG_HUB("Set brightness=%d", v);
                    return;
                }
                if (strncmp(arg, "speed ", 6) == 0) {
                    int v = atoi(arg + 6);
                    if (v < 0 || v > 255) { LOG_HUB("set speed out of range (0..255)"); return; }
                    state->setGlobalSpeed((uint8_t)v);
                    LOG_HUB("Set speed=%d", v);
                    return;
                }
            }

            if (buf[0] != '\0') {
                LOG_HUB("Unknown command: %s (try: help)", buf);
            }
            return;
        }

        if (len < (uint8_t)(sizeof(buf) - 1)) {
            buf[len++] = (char)c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n================================");
    Serial.println("  LightwaveOS Hub (Tab5)");
    Serial.println("================================");
    
    WiFi.setPins(TAB5_WIFI_SDIO_CLK, TAB5_WIFI_SDIO_CMD, 
                 TAB5_WIFI_SDIO_D0, TAB5_WIFI_SDIO_D1, TAB5_WIFI_SDIO_D2, TAB5_WIFI_SDIO_D3,
                 TAB5_WIFI_SDIO_RST);
    LOG_WIFI("SDIO configured");

    auto cfg = M5.config();
    M5.begin(cfg);
    
    // Set display rotation BEFORE LVGL init (landscape, USB on left)
    M5.Display.setRotation(3);
    
    // Set byte swapping for BGR565 format BEFORE LVGL init
    M5.Display.setSwapBytes(true);
    delay(50);
    
    Serial.printf("[M5] Display: %dx%d (rotation 3, swap bytes enabled)\n", 
                  M5.Display.width(), M5.Display.height());

    if (!LVGLBridge::init()) {
        Serial.println("[FATAL] LVGL init failed!");
        M5.Display.fillScreen(TFT_RED);
        M5.Display.setCursor(40, 200);
        M5.Display.println("LVGL INIT FAILED");
        while (1) { delay(1000); }
    }
    LOG_LVGL("OK");

    // External I2C (Grove Port.A) for M5ROTATE8 encoders
    int8_t extSDA = M5.Ex_I2C.getSDA();
    int8_t extSCL = M5.Ex_I2C.getSCL();
    LOG_I2C("External pins: SDA=%d SCL=%d", extSDA, extSCL);

    Wire.begin(extSDA, extSCL, I2C::FREQ_HZ);
    Wire.setTimeOut(I2C::TIMEOUT_MS);
    I2CRecovery::init(&Wire, extSDA, extSCL, I2C::FREQ_HZ);

    // Storage (NVS) – initialise early so encoder values can be restored.
    NvsStorage::init();

    // Startup-only I2C recovery pass before touching M5ROTATE8.
    // Note: avoid I2CRecovery::isBusHealthy() here because pinMode() can deconfigure
    // the I2C peripheral on ESP32P4 Arduino, causing "bus is not initialized".
    I2CRecovery::forceRecovery();
    uint32_t startMs = millis();
    while (I2CRecovery::isRecovering() && (millis() - startMs) < 1200) {
        I2CRecovery::update();
        delay(5);
    }
    LOG_I2C("Recovery stats: attempts=%u successes=%u",
            static_cast<unsigned>(I2CRecovery::getRecoveryAttempts()),
            static_cast<unsigned>(I2CRecovery::getRecoverySuccesses()));
#if ENABLE_ENCODER_DIAGNOSTICS
    scanI2C(Wire, "EXT");
#endif

    // Encoders
    // Prefer unit A at 0x41 (factory) with optional unit B at 0x42 if reprogrammed.
    // If only one device is present on the bus, DualEncoderService gracefully
    // degrades to 8 encoders (indices 0-7).
    g_encoders = new DualEncoderService(&Wire, 0x41, 0x42);
    if (g_encoders) {
        g_encoders->setChangeCallback(onEncoderChange);
        bool ok = g_encoders->begin();
        LOG_ENC("DualEncoderService begin: %s", ok ? "OK" : "FAILED");
    }

    // Button handler (required by DualEncoderService linkage)
    g_buttonHandler = new ButtonHandler();
    if (g_encoders && g_buttonHandler) {
        g_encoders->setButtonHandler(g_buttonHandler);
    }

    LOG_HUB("Starting...");
    if (!initHubCoordinator()) {
        LOG_WDT("Hub init failed!");
        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);
        lv_obj_t* label = lv_label_create(scr);
        lv_label_set_text(label, "HUB INIT FAILED");
        lv_obj_set_style_text_color(label, lv_color_make(255, 0, 0), 0);
        lv_obj_center(label);
        while (1) { 
            LVGLBridge::update();
            delay(5); 
        }
    }

    // Initialize dashboard with registry and OTA dispatch
    g_dashboard.init(g_hubMain->getRegistry(), g_hubMain->getOtaDispatch());
    
    // Wire registry events to dashboard log
    g_hubMain->getRegistry()->setEventCallback([](uint8_t nodeId, uint8_t eventType, const char* message) {
        enqueueUiRegistryEvent(nodeId, eventType, message);
    });

    // Parameter handler wired to HubState (hub mode)
    if (g_encoders) {
        // Restore last-saved encoder values before wiring ParameterHandler cache.
        uint16_t stored[NvsStorage::PARAM_COUNT];
        uint8_t loaded = NvsStorage::loadAllParameters(stored);
        if (loaded > 0) {
            for (uint8_t i = 0; i < NvsStorage::PARAM_COUNT; ++i) {
                g_encoders->setValue(i, stored[i], false);
            }
            LOG_NVS("Restored %u/%u encoder values", (unsigned)loaded, (unsigned)NvsStorage::PARAM_COUNT);
        }

        g_paramHandler = new ParameterHandler(g_encoders, nullptr, g_hubMain->getState());
        if (g_paramHandler && g_buttonHandler) {
            g_paramHandler->setButtonHandler(g_buttonHandler);
        }
        LOG_ENC("ParameterHandler wired to HubState");

        // Apply restored values into HubState once (without re-saving to NVS),
        // so nodes joining later get the right snapshot.
        if (g_hubMain->getState()) {
            HubState* s = g_hubMain->getState();
            s->setGlobalEffect((uint8_t)g_encoders->getValue((uint8_t)ParameterId::EffectId));
            s->setGlobalPalette((uint8_t)g_encoders->getValue((uint8_t)ParameterId::PaletteId));
            s->setGlobalSpeed((uint8_t)g_encoders->getValue((uint8_t)ParameterId::Speed));
            s->setGlobalIntensity((uint8_t)g_encoders->getValue((uint8_t)ParameterId::Mood));
            s->setGlobalSaturation((uint8_t)g_encoders->getValue((uint8_t)ParameterId::FadeAmount));
            s->setGlobalComplexity((uint8_t)g_encoders->getValue((uint8_t)ParameterId::Complexity));
            s->setGlobalVariation((uint8_t)g_encoders->getValue((uint8_t)ParameterId::Variation));
            s->setGlobalBrightness((uint8_t)g_encoders->getValue((uint8_t)ParameterId::Brightness));
            // Zones are opt-in: do not push any zone values on boot.
            // Zone updates will be applied only after the user explicitly uses zone controls
            // (or recalls a preset with zoneModeEnabled=1).
        }

        // Presets: Unit-B buttons (8-15) act as preset slots 0-7.
        g_presetManager = new PresetManager(g_paramHandler, g_hubMain->getState());
        if (g_presetManager && g_presetManager->init()) {
            g_presetManager->setFeedbackCallback([](uint8_t slot, PresetAction action, bool success) {
                const char* a = (action == PresetAction::SAVE) ? "SAVE" :
                                (action == PresetAction::RECALL) ? "RECALL" :
                                (action == PresetAction::DELETE) ? "DELETE" : "ERROR";
                LOG_PRESET("slot=%u action=%s ok=%d", (unsigned)slot, a, success ? 1 : 0);
            });
        }
    }
    
    LOG_HUB("Dashboard ready");
}

void loop() {
    M5.update();
    LVGLBridge::update();
    g_dashboard.update();

    // Drain registry events onto the UI thread (LVGL-safe).
    UiRegistryEvent ev;
    while (dequeueUiRegistryEvent(ev)) {
        g_dashboard.logRegistryEvent(ev.nodeId, ev.eventType, ev.message);
    }

    processSerialCommands();

    // Poll encoders at a modest cadence; hub networking runs in FreeRTOS tasks.
    static uint32_t lastEncMs = 0;
    uint32_t now = millis();
    if (g_encoders && (now - lastEncMs) >= 5) {
        lastEncMs = now;
        I2CRecovery::update();
        g_encoders->update();

        // Preset click handling (Unit B buttons 8..15).
        if (g_presetManager && g_encoders->isUnitBAvailable()) {
            for (uint8_t slot = 0; slot < PRESET_SLOT_COUNT; ++slot) {
                const uint8_t globalIdx = 8 + slot;
                const bool pressed = g_encoders->isButtonPressed(globalIdx);
                ClickType click = g_presetClicks[slot].update(pressed, now);
                if (click == ClickType::SINGLE_CLICK) {
                    g_presetManager->recallPreset(slot);
                } else if (click == ClickType::DOUBLE_CLICK) {
                    g_presetManager->savePreset(slot);
                } else if (click == ClickType::LONG_HOLD) {
                    g_presetManager->deletePreset(slot);
                }
            }
        }
    }

    // Storage debounce flush (2s per parameter).
    NvsStorage::update();

    delay(1);
}
