#pragma once
// ============================================================================
// DisplayUI - Main UI controller for Tab5.encoder
// ============================================================================
// 4x4 grid layout with status bar
// Clean, readable, no bullshit animations
// ============================================================================

#include <M5GFX.h>
#include "Theme.h"
#include "widgets/GaugeWidget.h"
#include "widgets/StatusBar.h"

// Forward declaration
class ZoneComposerUI;

/**
 * UI screen types
 */
enum class UIScreen : uint8_t {
    GLOBAL = 0,        // Default: 16-parameter gauge view
    ZONE_COMPOSER = 1  // Zone composer dashboard
};

class DisplayUI {
public:
    DisplayUI(M5GFX& display);
    ~DisplayUI();

    void begin();
    void loop();

    // Data updates
    void updateEncoder(uint8_t index, int32_t value);
    void updateValue(uint8_t index, int32_t value) { updateEncoder(index, value); }  // Alias for compat
    void setConnectionState(bool wifi, bool ws, bool encA, bool encB);

    // Metadata display (for effect/palette names from server)
    void setCurrentEffect(uint8_t id, const char* name);
    void setCurrentPalette(uint8_t id, const char* name);
    void setWiFiInfo(const char* ip, const char* ssid, int32_t rssi);

    // Screen switching
    void setScreen(UIScreen screen);
    UIScreen getCurrentScreen() const { return _currentScreen; }
    
    // Get zone composer UI (for router initialization)
    ZoneComposerUI* getZoneComposerUI() { return _zoneComposer; }
    
    // Touch handling (forward to ZoneComposerUI when on that screen)
    void handleTouch(int16_t x, int16_t y);

private:
    M5GFX& _display;

    StatusBar* _statusBar;
    GaugeWidget* _gauges[16];
    ZoneComposerUI* _zoneComposer = nullptr;

    // Screen state
    UIScreen _currentScreen = UIScreen::GLOBAL;

    // State
    uint32_t _lastStatsUpdate = 0;
    uint8_t _highlightIdx = 255;
    uint32_t _highlightTime = 0;

    void updateStats();
    void renderCurrentScreen();
};
