#pragma once
// ============================================================================
// ButtonHandler - Zone Mode and Speed/Palette Toggle Handler
// ============================================================================
// Handles special button behaviors for Tab5.encoder:
// - Enc0 button (Unit B, index 8): Toggle zone mode ON/OFF
// - Enc1/3/5/7 buttons (Unit B, indices 9, 11, 13, 15): Toggle Speed/Palette mode
//
// For other buttons, returns false to allow default reset behavior.
// ============================================================================

#include <Arduino.h>
#include <functional>

// Forward declarations
class WebSocketClient;

/**
 * Speed/Palette mode for zone encoders
 */
enum class SpeedPaletteMode : uint8_t {
    SPEED = 0,    // Default: encoder controls zone speed
    PALETTE = 1   // Toggled: encoder controls zone palette
};

/**
 * Button event handler class
 */
class ButtonHandler {
public:
    ButtonHandler();

    /**
     * Process button press for a given encoder index
     * @param index Encoder index (0-15)
     * @return true if button was handled (no default reset), false to allow default reset
     */
    bool handleButtonPress(uint8_t index);

    /**
     * Get current zone mode state
     * @return true if zone mode is enabled
     */
    bool isZoneModeEnabled() const { return _zoneModeEnabled; }

    /**
     * Get speed/palette mode for a zone encoder
     * @param zoneId Zone ID (0-3)
     * @return Current mode (SPEED or PALETTE)
     */
    SpeedPaletteMode getZoneEncoderMode(uint8_t zoneId) const {
        if (zoneId >= 4) return SpeedPaletteMode::SPEED;
        return _zoneEncoderMode[zoneId];
    }

    /**
     * Set callback for zone mode toggle
     * @param callback Function called when zone mode toggles (bool enabled)
     */
    void onZoneModeToggle(std::function<void(bool)> callback) {
        _zoneModeToggleCallback = callback;
    }

    /**
     * Set callback for speed/palette mode toggle
     * @param callback Function called when mode toggles (uint8_t zoneId, SpeedPaletteMode mode)
     */
    void onSpeedPaletteToggle(std::function<void(uint8_t, SpeedPaletteMode)> callback) {
        _speedPaletteToggleCallback = callback;
    }

    /**
     * Set WebSocket client for sending zone mode commands
     * @param wsClient WebSocket client instance
     */
    void setWebSocketClient(WebSocketClient* wsClient) {
        _wsClient = wsClient;
    }

private:
    bool _zoneModeEnabled = false;
    SpeedPaletteMode _zoneEncoderMode[4] = {
        SpeedPaletteMode::SPEED,
        SpeedPaletteMode::SPEED,
        SpeedPaletteMode::SPEED,
        SpeedPaletteMode::SPEED
    };

    std::function<void(bool)> _zoneModeToggleCallback;
    std::function<void(uint8_t, SpeedPaletteMode)> _speedPaletteToggleCallback;
    WebSocketClient* _wsClient = nullptr;

    /**
     * Toggle zone mode ON/OFF
     */
    void toggleZoneMode();

    /**
     * Toggle speed/palette mode for a zone
     * @param zoneId Zone ID (0-3)
     */
    void toggleSpeedPaletteMode(uint8_t zoneId);
};

