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
    // Enc0 button (Unit B, index 8): Toggle zone mode
    if (index == 8) {
        toggleZoneMode();
        return true;  // Handled - don't reset encoder
    }

    // Enc1/3/5/7 buttons (Unit B, indices 9, 11, 13, 15): Toggle Speed/Palette mode
    // These correspond to zones 0, 1, 2, 3
    if (index == 9 || index == 11 || index == 13 || index == 15) {
        uint8_t zoneId = (index - 9) / 2;  // Maps 9->0, 11->1, 13->2, 15->3
        toggleSpeedPaletteMode(zoneId);
        return true;  // Handled - don't reset encoder
    }

    // All other buttons: allow default reset behavior
    return false;
}

void ButtonHandler::toggleZoneMode() {
    bool wasBefore = _zoneModeEnabled;
    _zoneModeEnabled = !_zoneModeEnabled;
    
    // #region agent log
    Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"tab5-fix2\",\"hypothesisId\":\"H2\",\"location\":\"ButtonHandler.cpp:toggleZoneMode\",\"message\":\"zone.toggle\",\"data\":{\"before\":%s,\"after\":%s,\"hasCallback\":%s},\"timestamp\":%lu}\n",
        wasBefore ? "true" : "false",
        _zoneModeEnabled ? "true" : "false",
        _zoneModeToggleCallback ? "true" : "false",
        static_cast<unsigned long>(millis()));
    // #endregion
    
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

