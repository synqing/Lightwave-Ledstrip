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
#include <Wire.h>
#include "config.h"
#include "network/WiFiManager.h"
#include "network/WebSocketClient.h"
#include "input/InputManager.h"
#include "parameters/ParameterMapper.h"
#include "sync/EchoGuard.h"
#include "led/LedFeedback.h"

// ============================================================================
// Globals
// ============================================================================

static ZMWiFiManager wifiMgr;
static ZMWebSocketClient wsClient;
static InputManager inputMgr;
static ParameterMapper paramMapper;
static EchoGuard echoGuard;
static LedFeedback ledFeedback;
static bool wsStarted = false;

// ============================================================================
// WebSocket Message Handler
// ============================================================================

static void onWsMessage(JsonDocument& doc) {
    const char* type = doc["type"] | "";
    uint32_t now = millis();

    if (strcmp(type, "status") == 0) {
        paramMapper.applyStatus(doc, now);
        // Log summary
        uint8_t bri = doc["brightness"] | 0;
        const char* name = doc["effectName"] | "?";
        Serial.printf("[Status] %s bri=%d spd=%d\n", name, bri, doc["speed"] | 0);
    }
    else if (strcmp(type, "zones.list") == 0 || strcmp(type, "zones") == 0) {
        paramMapper.applyZoneState(doc, now);
        Serial.printf("[Zones] count=%d enabled=%s\n",
                      doc["zoneCount"] | 0, (doc["enabled"] | false) ? "YES" : "NO");
    }
    else if (strcmp(type, "zones.changed") == 0 || strcmp(type, "zones.stateChanged") == 0) {
        wsClient.sendSimple("zones.get");
    }
    else if (strcmp(type, "edge_mixer.get") == 0 || strcmp(type, "edge_mixer.set") == 0) {
        // Sync EdgeMixer state
        if (doc.containsKey("mode")) {
            if (!echoGuard.isHeld(8, now)) paramMapper.emMode = doc["mode"];
        }
        if (doc.containsKey("spread")) {
            if (!echoGuard.isHeld(9, now)) paramMapper.emSpread = doc["spread"];
        }
        if (doc.containsKey("strength")) {
            if (!echoGuard.isHeld(10, now)) paramMapper.emStrength = doc["strength"];
        }
        if (doc.containsKey("spatial") && doc.containsKey("temporal")) {
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
    }
    else if (strcmp(type, "cameraMode.get") == 0 || strcmp(type, "cameraMode.set") == 0) {
        if (doc.containsKey("enabled")) {
            paramMapper.cameraModeOn = doc["enabled"];
            ledFeedback.setCameraMode(paramMapper.cameraModeOn);
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
    // Route through ParameterMapper
    paramMapper.onInput(inputId, delta, button);

    // Debug log
    if (button) {
        Serial.printf("[In] id=%d BTN\n", inputId);
    } else {
        Serial.printf("[In] id=%d d=%+d\n", inputId, delta);
    }
}

// ============================================================================
// Display — Connection Status on AtomS3 0.85" Screen
// ============================================================================

static void updateDisplay() {
    M5.Display.setTextSize(1);

    // WiFi (line 1)
    M5.Display.setCursor(4, 4);
    if (wifiMgr.isConnected()) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.printf("WiFi: %s    ", wifiMgr.getLocalIP().toString().c_str());
    } else {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.print("WiFi: ---             ");
    }

    // WS (line 2)
    M5.Display.setCursor(4, 20);
    if (wsClient.isConnected()) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.print("WS: connected     ");
    } else if (wsStarted) {
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.print("WS: connecting... ");
    } else {
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.print("WS: waiting       ");
    }

    // Info (line 3)
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 40);
    M5.Display.printf("Bri:%3d Spd:%3d Z:%d ",
                      paramMapper.masterBrightness, paramMapper.masterSpeed,
                      paramMapper.zoneCount);

    // Title
    M5.Display.setCursor(4, 56);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.print("Zone Mixer v0.4");
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("\n[ZoneMixer] v0.4 — Phases 1-6");

    // Display init
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, 4);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.print("Zone Mixer v0.4");

    // I2C init
    Wire.begin(hw::I2C_SDA, hw::I2C_SCL);
    Wire.setClock(hw::I2C_FREQ);

    // Init input devices
    inputMgr.setCallback(onInputChange);
    inputMgr.begin();

    // Init LED feedback
    ledFeedback.begin(&inputMgr);

    // Init parameter mapper
    paramMapper.begin(&wsClient, &echoGuard, &ledFeedback);

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

    // Update display (2Hz)
    static uint32_t lastDisplay = 0;
    if (millis() - lastDisplay > 500) {
        lastDisplay = millis();
        updateDisplay();
    }
}
