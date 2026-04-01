/**
 * Zone Mixer — K1 Zone Composer Physical Controller
 *
 * Hardware: AtomS3 (ESP32-S3) → PaHub v2.1 (PCA9548A) → 4x Unit-Scroll + 2x M5Rotate8
 * Architecture: DJ-mixer paradigm for 3-zone LED art control
 *
 * PaHub Channel Map (from hardware scan):
 *   CH0: 8Encoder (0x42)  — Zone selection bank A
 *   CH1: 8Encoder (0x41)  — Zone selection bank B / Mix & config
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

// ============================================================================
// Globals
// ============================================================================

static ZMWiFiManager wifiMgr;
static ZMWebSocketClient wsClient;
static InputManager inputMgr;
static bool wsStarted = false;

// ============================================================================
// WebSocket Message Handler
// ============================================================================

static void onWsMessage(JsonDocument& doc) {
    const char* type = doc["type"] | "";

    if (strcmp(type, "status") == 0) {
        // Status broadcast from K1 (every 5s or on change)
        uint8_t brightness = doc["brightness"] | 0;
        uint8_t speed = doc["speed"] | 0;
        const char* effectName = doc["effectName"] | "?";
        uint16_t effectId = doc["effectId"] | 0;
        Serial.printf("[Status] effect=%s(0x%04X) bri=%d spd=%d\n",
                      effectName, effectId, brightness, speed);
    }
    else if (strcmp(type, "zones.list") == 0) {
        // Zone state response
        bool enabled = doc["enabled"] | false;
        uint8_t count = doc["zoneCount"] | 0;
        Serial.printf("[Zones] enabled=%s count=%d\n",
                      enabled ? "YES" : "NO", count);
    }
    else if (strcmp(type, "zones.changed") == 0) {
        // Zone config changed — request fresh state
        Serial.println("[Zones] Changed — requesting update");
        wsClient.sendSimple("zones.get");
    }
    else if (strcmp(type, "effects.changed") == 0) {
        const char* name = doc["data"]["effectName"] | doc["effectName"] | "?";
        Serial.printf("[Effect] Changed to: %s\n", name);
    }
    else {
        // Log unhandled message types during development
        Serial.printf("[WS] Unhandled: %s\n", type);
    }
}

// ============================================================================
// Input Change Callback
// ============================================================================

static void onInputChange(uint8_t inputId, int32_t delta, bool button) {
    const char* name = "?";
    if (inputId < 8)        name = "EncA";
    else if (inputId < 16)  name = "EncB";
    else if (inputId == 16) name = "Z1-Scroll";
    else if (inputId == 17) name = "Z2-Scroll";
    else if (inputId == 18) name = "Z3-Scroll";
    else if (inputId == 19) name = "Master";

    if (button) {
        Serial.printf("[Input] %s id=%d BUTTON\n", name, inputId);
    } else {
        Serial.printf("[Input] %s id=%d delta=%+d\n", name, inputId, delta);
    }
}

// ============================================================================
// Display — Connection Status on AtomS3 0.85" Screen
// ============================================================================

static void updateDisplay() {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    // WiFi status (top line)
    M5.Display.setCursor(4, 4);
    if (wifiMgr.isConnected()) {
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.printf("WiFi: %s    ", wifiMgr.getLocalIP().toString().c_str());
    } else {
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.print("WiFi: connecting...   ");
    }

    // WebSocket status (second line)
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

    // Title
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 44);
    M5.Display.print("Zone Mixer v0.2");
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("\n[ZoneMixer] Phase 2 — WiFi + WebSocket");

    // Display init
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, 4);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.print("Zone Mixer v0.2");

    // I2C init
    Wire.begin(hw::I2C_SDA, hw::I2C_SCL);
    Wire.setClock(hw::I2C_FREQ);
    Serial.printf("[I2C] SDA=%d SCL=%d freq=%lu\n", hw::I2C_SDA, hw::I2C_SCL, hw::I2C_FREQ);

    // Init input devices via PaHub
    inputMgr.setCallback(onInputChange);
    inputMgr.begin();

    // Register WS message callback
    wsClient.onMessage(onWsMessage);

    // Start WiFi connection to K1
    wifiMgr.begin(net::K1_SSID, net::K1_PASSWORD);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    M5.update();

    // WiFi state machine (non-blocking)
    wifiMgr.update();

    // Start WebSocket when WiFi connects (once)
    if (wifiMgr.isConnected() && !wsStarted) {
        IPAddress k1ip;
        k1ip.fromString(net::K1_IP);
        wsClient.begin(k1ip, net::K1_PORT, net::K1_WS_PATH);
        wsStarted = true;
    }

    // Reset WS state if WiFi drops
    if (!wifiMgr.isConnected() && wsStarted) {
        wsStarted = false;
    }

    // WebSocket state machine (non-blocking)
    if (wsStarted) {
        wsClient.update();
    }

    // Poll all input devices through PaHub (100Hz target)
    inputMgr.poll();

    // Update display every 500ms
    static uint32_t lastDisplay = 0;
    if (millis() - lastDisplay > 500) {
        lastDisplay = millis();
        updateDisplay();
    }
}
