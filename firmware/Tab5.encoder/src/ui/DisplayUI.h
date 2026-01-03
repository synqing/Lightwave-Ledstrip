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
#include "widgets/UIHeader.h"
#include "widgets/PresetSlotWidget.h"

// Forward declarations
class ZoneComposerUI;
class PresetManager;

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
    
    // Get header instance (for ZoneComposerUI)
    UIHeader* getHeader() { return _header; }

    // Preset bank UI
    PresetSlotWidget* getPresetSlot(uint8_t slot) { return (slot < 8) ? _presetSlots[slot] : nullptr; }
    void updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness);
    void setActivePresetSlot(uint8_t slot);
    void refreshAllPresetSlots(PresetManager* pm);

private:
    M5GFX& _display;

    UIHeader* _header;
    GaugeWidget* _gauges[16];
    PresetSlotWidget* _presetSlots[8];
    ZoneComposerUI* _zoneComposer = nullptr;
    uint8_t _activePresetSlot = 0xFF;  // 0xFF = none active

    // Screen state
    UIScreen _currentScreen = UIScreen::GLOBAL;

    // State
    uint32_t _lastStatsUpdate = 0;
    uint8_t _highlightIdx = 255;
    uint32_t _highlightTime = 0;

    void updateStats();
    void updateHeader();
    void renderCurrentScreen();
};
