// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// ButtonHandler Implementation
// ============================================================================

#include "ButtonHandler.h"
#include "../network/WebSocketClient.h"
#include <Arduino.h>

ButtonHandler::ButtonHandler() {
    _zoneModeEnabled = false;
    for (uint8_t i = 0; i < 4; i++) {
        _zoneEncoderMode[i] = SpeedPaletteMode::SPEED;
    }
}

bool ButtonHandler::handleButtonPress(uint8_t index) {
    // =========================================================================
    // Unit-B button dispatch (indices 8-15)
    // =========================================================================
    // Index 8  (Enc0): Zone Mode global toggle
    // Index 9  (Enc1): SpeedPalette toggle for zone 0
    // Index 11 (Enc3): SpeedPalette toggle for zone 1
    // Index 13 (Enc5): SpeedPalette toggle for zone 2
    // Index 15 (Enc7): SpeedPalette toggle for zone 3
    // Indices 10, 12, 14 (Enc2/4/6): Reserved for Preset System — no toggle action
    if (index == 8) {
        toggleZoneMode();
        return true;
    }
    if (index == 9 || index == 11 || index == 13 || index == 15) {
        const uint8_t zoneId = (index - 9) / 2;
        toggleSpeedPaletteMode(zoneId);
        return true;
    }
    if (index >= 8) {
        // Remaining Unit-B buttons (10, 12, 14): handled by PresetManager in main.cpp
        return true;
    }

    // Unit-A buttons (0-7): allow default reset-to-default behaviour
    return false;
}

void ButtonHandler::toggleZoneMode() {
    _zoneModeEnabled = !_zoneModeEnabled;
    Serial.printf("[Button] Zone mode %s\n", _zoneModeEnabled ? "ENABLED" : "DISABLED");

    // Send zone mode command to LightwaveOS
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneEnable(_zoneModeEnabled);
    }
    
    // Call callback if set
    if (_zoneModeToggleCallback) {
        _zoneModeToggleCallback(_zoneModeEnabled);
    }
}

void ButtonHandler::toggleSpeedPaletteMode(uint8_t zoneId) {
    if (zoneId >= 4) return;

    // Toggle between SPEED and PALETTE
    _zoneEncoderMode[zoneId] = (_zoneEncoderMode[zoneId] == SpeedPaletteMode::SPEED)
        ? SpeedPaletteMode::PALETTE
        : SpeedPaletteMode::SPEED;

    const char* modeName = (_zoneEncoderMode[zoneId] == SpeedPaletteMode::SPEED) ? "SPEED" : "PALETTE";
    Serial.printf("[Button] Zone %u encoder mode: %s\n", zoneId, modeName);

    // Call callback if set
    if (_speedPaletteToggleCallback) {
        _speedPaletteToggleCallback(zoneId, _zoneEncoderMode[zoneId]);
    }
}

