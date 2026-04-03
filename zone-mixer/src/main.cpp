/**
 * Zone Mixer — K1 Zone Composer Physical Controller
 *
 * Hardware: AtomS3 (ESP32-S3) → PaHub v2.1 (PCA9548A) → 4x Unit-Scroll + 2x M5Rotate8
 * Architecture: DJ-mixer paradigm for 3-zone LED art control
 *
 * PaHub Channel Map (from hardware scan):
 *   CH0: 8Encoder (0x42)  — Zone selection bank A
 *   CH1: 8Encoder (0x41)  — Mix & config bank B
 *   CH2: Scroll (0x40)    — Zone 1 brightness
 *   CH3: Scroll (0x40)    — Zone 2 brightness
 *   CH4: Scroll (0x40)    — Zone 3 brightness
 *   CH5: Scroll (0x40)    — Master brightness
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 SpectraSynq
 */

#include <M5Unified.h>
#include "config.h"
#include "network/WiFiManager.h"
#include "network/WebSocketClient.h"
#include "input/InputManager.h"
#include "parameters/ParameterMapper.h"
#include "sync/EchoGuard.h"
#include "led/LedFeedback.h"
#include "display/Display.h"

// ============================================================================
// Globals
// ============================================================================

static ZMWiFiManager wifiMgr;
static ZMWebSocketClient wsClient;
static InputManager inputMgr;
static ParameterMapper paramMapper;
static EchoGuard echoGuard;
static LedFeedback ledFeedback;
static ZMDisplay display;
static bool wsStarted = false;

// ============================================================================
// WebSocket Message Handler
// ============================================================================

static void onWsMessage(JsonDocument& doc) {
    const char* type = doc["type"] | "";
    uint32_t now = millis();

    if (strcmp(type, "status") == 0) {
        paramMapper.applyStatus(doc, now);
        // Sync display with K1 status
        display.setMasterBrightness(paramMapper.masterBrightness);
        display.setSpeed(paramMapper.masterSpeed);
        if (doc["effectName"]) display.setEffectName(doc["effectName"]);
        // Log summary
        uint8_t bri = doc["brightness"] | 0;
        const char* name = doc["effectName"] | "?";
        Serial.printf("[Status] %s bri=%d spd=%d\n", name, bri, doc["speed"] | 0);
    }
    else if (strcmp(type, "zones.list") == 0 || strcmp(type, "zones") == 0) {
        paramMapper.applyZoneState(doc, now);
        // Sync display with zone state
        display.setZoneCount(paramMapper.zoneCount);
        display.setZonesEnabled(paramMapper.zoneSystemOn);
        for (uint8_t i = 0; i < 3; i++) {
            display.setZoneBrightness(i, paramMapper.zoneBrightness[i]);
        }
        Serial.printf("[Zones] count=%d enabled=%s\n",
                      doc["zoneCount"] | 0, (doc["enabled"] | false) ? "YES" : "NO");
    }
    else if (strcmp(type, "zones.changed") == 0 || strcmp(type, "zones.stateChanged") == 0) {
        wsClient.sendSimple("zones.get");
    }
    else if (strcmp(type, "edge_mixer.get") == 0 || strcmp(type, "edge_mixer.set") == 0) {
        // Sync EdgeMixer state
        if (!doc["mode"].isNull()) {
            if (!echoGuard.isHeld(8, now)) paramMapper.emMode = doc["mode"];
        }
        if (!doc["spread"].isNull()) {
            if (!echoGuard.isHeld(9, now)) paramMapper.emSpread = doc["spread"];
        }
        if (!doc["strength"].isNull()) {
            if (!echoGuard.isHeld(10, now)) paramMapper.emStrength = doc["strength"];
        }
        if (!doc["spatial"].isNull() && !doc["temporal"].isNull()) {
            if (!echoGuard.isHeld(11, now)) {
                uint8_t s = doc["spatial"] | 0;
                uint8_t t = doc["temporal"] | 0;
                paramMapper.stState = (s ? 1 : 0) + (t ? 2 : 0);
                ledFeedback.setSpatialTemporal(paramMapper.stState);
            }
        }
        ledFeedback.setEdgeMixerMode(paramMapper.emMode);
        ledFeedback.setParamValue(9, paramMapper.emSpread);
        ledFeedback.setParamValue(10, paramMapper.emStrength);
        // Sync display with EdgeMixer state
        display.setEdgeMixer(paramMapper.emMode, paramMapper.emSpread,
                             paramMapper.emStrength,
                             paramMapper.stState & 1, paramMapper.stState & 2);
    }
    else if (strcmp(type, "cameraMode.get") == 0 || strcmp(type, "cameraMode.set") == 0) {
        if (!doc["enabled"].isNull()) {
            paramMapper.cameraModeOn = doc["enabled"];
            ledFeedback.setCameraMode(paramMapper.cameraModeOn);
        }
    }
    else if (strcmp(type, "effects.list") == 0) {
        // Cache the ordered effect list for per-zone effect selection
        JsonArray effects = doc["effects"];
        if (!effects.isNull()) {
            static uint16_t effectIds[ParameterMapper::kMaxEffects];
            uint8_t count = 0;
            for (JsonObject eff : effects) {
                if (count >= ParameterMapper::kMaxEffects) break;
                effectIds[count++] = eff["effectId"] | (uint16_t)0;
            }
            paramMapper.setEffectList(effectIds, count);
            Serial.printf("[Effects] Cached %d effects\n", count);
        }
    }
    else if (strcmp(type, "effects.changed") == 0) {
        const char* name = doc["data"]["effectName"] | doc["effectName"] | "?";
        Serial.printf("[Effect] Changed: %s\n", name);
    }
    else {
        Serial.printf("[WS] %s\n", type);
    }
}

// ============================================================================
// Input Change Callback (from InputManager)
// ============================================================================

static void onInputChange(uint8_t inputId, int32_t delta, bool button) {
    paramMapper.onInput(inputId, delta, button);

    // Trigger live parameter overlay on the display
    if (!button) {
        // Map inputId to parameter name and page hint
        const char* paramName = nullptr;
        uint8_t pageHint = 0;
        uint16_t value = 0;

        // Page order: 0=Connection, 1=Zones, 2=Effect, 3=EdgeMixer
        if (inputId >= 16 && inputId <= 18) {
            static const char* zNames[] = {"Z1 BRI", "Z2 BRI", "Z3 BRI"};
            paramName = zNames[inputId - 16];
            value = paramMapper.zoneBrightness[inputId - 16];
            pageHint = 1;  // Zones page
        } else if (inputId == 19) {
            paramName = "MASTER";
            value = paramMapper.masterBrightness;
            pageHint = 1;  // Zones page
        } else if (inputId == 3) {
            paramName = "SPEED";
            value = paramMapper.masterSpeed;
            pageHint = 2;  // Effect page
        } else if (inputId >= 8 && inputId <= 15) {
            // EncB overlay is page-aware — labels change per active page
            uint8_t page = display.getCurrentPage();
            pageHint = page;  // Accent colour matches active page

            if (page == 1) {
                // ZONES page
                switch (inputId) {
                    case 8:  paramName = "Z1 BRI"; value = paramMapper.zoneBrightness[0]; break;
                    case 9:  paramName = "Z2 BRI"; value = paramMapper.zoneBrightness[1]; break;
                    case 10: paramName = "Z3 BRI"; value = paramMapper.zoneBrightness[2]; break;
                    case 11: paramName = "MASTER"; value = paramMapper.masterBrightness;  break;
                    case 12: paramName = "ZONES";  value = paramMapper.zoneCount;         break;
                }
            } else if (page == 2) {
                // EFFECT page
                switch (inputId) {
                    case 8:  paramName = "Z1 SPD"; value = paramMapper.zoneSpeed[0];      break;
                    case 9:  paramName = "Z2 SPD"; value = paramMapper.zoneSpeed[1];      break;
                    case 10: paramName = "Z3 SPD"; value = paramMapper.zoneSpeed[2];      break;
                    case 11: paramName = "SPEED";  value = paramMapper.masterSpeed;       break;
                    case 12: paramName = "ZONES";  value = paramMapper.zoneCount;         break;
                    case 13: paramName = "TRANS";  value = paramMapper.transitionType;    break;
                }
            } else if (page == 3) {
                // EDGEMIXER page
                switch (inputId) {
                    case 8:  paramName = "EM MODE";  value = paramMapper.emMode;          break;
                    case 9:  paramName = "SPREAD";   value = paramMapper.emSpread;        break;
                    case 10: paramName = "STRENGTH"; value = paramMapper.emStrength;      break;
                    case 14: paramName = "PRESET";   value = paramMapper.presetSlot;      break;
                }
            }
            // Page 0 (Connection): EncB inactive — no overlay
        }
        // Only trigger overlay for known parameters
        if (paramName) {
            display.triggerOverlay(paramName, value, pageHint);
        }
    }

    // Debug log
    if (button) {
        Serial.printf("[In] id=%d BTN\n", inputId);
    } else {
        Serial.printf("[In] id=%d d=%+d\n", inputId, delta);
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = false;   // Don't probe IMU — we don't use it, and
    cfg.internal_rtc = false;   // deferred Wire.begin() corrupts i2c-ng driver
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("\n[ZoneMixer] v0.4 — Phases 1-6");
    Serial.printf("[Init] Board detected: %d (AtomS3=0x%X)\n",
                  (int)M5.getBoard(), (int)m5::board_t::board_M5AtomS3);

    // Display init
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, 4);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.print("Zone Mixer v0.4");

    // I2C + input devices (Wire.begin() is called inside InputManager::begin())
    inputMgr.setCallback(onInputChange);
    if (!inputMgr.begin()) {
        Serial.println("[FATAL] No input devices found!");
        M5.Display.setTextColor(TFT_RED);
        M5.Display.println("NO INPUT DEVICES");
    }

    // Init LED feedback
    ledFeedback.begin(&inputMgr);

    // Init parameter mapper
    paramMapper.begin(&wsClient, &echoGuard, &ledFeedback);

    // Init colour-coded tab display
    display.begin();
    paramMapper.setActivePage(display.getCurrentPage());
    ledFeedback.setActivePage(display.getCurrentPage());

    // Register WS message callback
    wsClient.onMessage(onWsMessage);

    // Start WiFi
    wifiMgr.begin(net::K1_SSID, net::K1_PASSWORD);

    // Set initial connection LED state
    ledFeedback.setConnectionState(LedFeedback::ConnState::DISCONNECTED);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    M5.update();

    // AtomS3 screen touch (GPIO41 capacitive button = BtnA)
    // Single tap: cycle display page. Double tap: toggle zone system.
    if (M5.BtnA.wasPressed()) {
        Serial.println("[BTN] Pressed");
    }
    if (M5.BtnA.wasReleased()) {
        Serial.println("[BTN] Released");
    }
    if (M5.BtnA.wasDecideClickCount()) {
        uint8_t clicks = M5.BtnA.getClickCount();
        Serial.printf("[BTN] Clicks: %d\n", clicks);
        if (clicks == 1) {
            display.nextPage();
            uint8_t page = display.getCurrentPage();
            paramMapper.setActivePage(page);
            ledFeedback.setActivePage(page);
        } else if (clicks >= 2) {
            paramMapper.toggleZoneSystem();
        }
    }

    // WiFi state machine
    wifiMgr.update();

    // Start WS when WiFi connects
    if (wifiMgr.isConnected() && !wsStarted) {
        IPAddress k1ip;
        k1ip.fromString(net::K1_IP);
        wsClient.begin(k1ip, net::K1_PORT, net::K1_WS_PATH);
        wsStarted = true;
        echoGuard.resetSync();
        ledFeedback.setConnectionState(LedFeedback::ConnState::CONNECTING);
    }

    // Reset WS state if WiFi drops
    if (!wifiMgr.isConnected() && wsStarted) {
        wsStarted = false;
        ledFeedback.setConnectionState(LedFeedback::ConnState::DISCONNECTED);
    }

    // WS state machine
    if (wsStarted) {
        wsClient.update();
        // Track connection state changes for LED
        static bool wasConnected = false;
        bool isConn = wsClient.isConnected();
        if (isConn != wasConnected) {
            wasConnected = isConn;
            ledFeedback.setConnectionState(isConn
                ? LedFeedback::ConnState::CONNECTED
                : LedFeedback::ConnState::CONNECTING);
        }
    }

    // Poll inputs (100Hz target)
    inputMgr.poll();

    // Update LED feedback (10Hz dirty flush)
    ledFeedback.update(millis());

    // Update WiFi/WS state on display (only on change)
    static bool lastWifi = false, lastWs = false;
    bool nowWifi = wifiMgr.isConnected();
    bool nowWs = wsClient.isConnected();
    if (nowWifi != lastWifi || nowWs != lastWs) {
        lastWifi = nowWifi;
        lastWs = nowWs;
        display.setWifiState(nowWifi, wifiMgr.getRSSI(),
                             nowWifi ? wifiMgr.getLocalIP().toString().c_str() : "---");
        display.setWsState(nowWs);
    }

    // Sync page state if display page changed (e.g. overlay auto-return)
    static uint8_t lastSyncedPage = 0;
    uint8_t curPage = display.getCurrentPage();
    if (curPage != lastSyncedPage) {
        lastSyncedPage = curPage;
        paramMapper.setActivePage(curPage);
        ledFeedback.setActivePage(curPage);
    }

    // Update display (internally rate-limited to 5 Hz)
    display.update(millis());
}
