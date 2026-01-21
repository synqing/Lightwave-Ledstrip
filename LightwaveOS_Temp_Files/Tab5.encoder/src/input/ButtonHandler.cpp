// ============================================================================
// ButtonHandler Implementation
// ============================================================================

#include "ButtonHandler.h"
#include "../config/AnsiColors.h"
#include <Arduino.h>

#if ENABLE_LEGACY_WIFI_CLIENT
#include "../network/WebSocketClient.h"
#endif

ButtonHandler::ButtonHandler() {
    _zoneModeEnabled = false;
    for (uint8_t i = 0; i < 4; i++) {
        _zoneEncoderMode[i] = SpeedPaletteMode::SPEED;
    }
}

bool ButtonHandler::handleButtonPress(uint8_t index) {
    // =========================================================================
    // Unit-B buttons (8-15): Reserved for Preset System
    // =========================================================================
    // These are handled by ClickDetector + PresetManager in main.cpp.
    // Return TRUE to prevent DualEncoderService from resetting the encoder,
    // but do NOT call any zone mode functions - presets only.
    if (index >= 8) {
        return true;  // Handled (no action) - prevent reset behavior
    }

    // Unit-A buttons (0-7): Allow default reset-to-default behavior
    return false;
}

void ButtonHandler::toggleZoneMode() {
    _zoneModeEnabled = !_zoneModeEnabled;
    Serial.printf(COLOR_BUTTON "[Button]" ANSI_RESET " Zone mode %s\n", _zoneModeEnabled ? "ENABLED" : "DISABLED");

    // Send zone mode command to LightwaveOS
#if ENABLE_LEGACY_WIFI_CLIENT
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneEnable(_zoneModeEnabled);
    }
#endif
    
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
    Serial.printf(COLOR_BUTTON "[Button]" ANSI_RESET " Zone %u encoder mode: %s\n", zoneId, modeName);

    // Call callback if set
    if (_speedPaletteToggleCallback) {
        _speedPaletteToggleCallback(zoneId, _zoneEncoderMode[zoneId]);
    }
}
