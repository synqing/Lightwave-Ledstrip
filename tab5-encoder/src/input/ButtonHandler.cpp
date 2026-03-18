// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// ButtonHandler Implementation
// ============================================================================

#include "ButtonHandler.h"
#include "../network/WebSocketClient.h"
#include "../ui/ZoneComposerUI.h"
#include <Arduino.h>

ButtonHandler::ButtonHandler() {
    _zoneModeEnabled = false;
    for (uint8_t i = 0; i < 4; i++) {
        _zoneEncoderMode[i] = SpeedPaletteMode::SPEED;
    }
}

bool ButtonHandler::handleButtonPress(uint8_t index) {
    // Unit-B buttons (8-15): all reserved for Preset System in main.cpp
    if (index >= 8) {
        return true;
    }

    // Unit-A buttons (0-7): allow default reset-to-default behaviour
    return false;
}

void ButtonHandler::toggleZoneMode() {
    _zoneModeEnabled = !_zoneModeEnabled;
    Serial.printf("[Button] Zone mode %s\n", _zoneModeEnabled ? "ENABLED" : "DISABLED");

    // Send zone mode command to LightwaveOS.
    // When enabling, send the current zone layout first so K1 has segment
    // definitions — without layout, zone.enable is a no-op on the K1 renderer.
    if (_wsClient && _wsClient->isConnected()) {
        if (_zoneModeEnabled && _zoneUI && _zoneUI->getEditingZoneCount() > 0) {
            _wsClient->sendZonesSetLayout(
                _zoneUI->getEditingSegments(),
                _zoneUI->getEditingZoneCount());
        }
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

