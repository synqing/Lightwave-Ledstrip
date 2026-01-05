#pragma once
// ============================================================================
// DisplayUI - Main UI controller for Tab5.encoder
// ============================================================================
// 4x4 grid layout with status bar
// Clean, readable, no bullshit animations
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
#else
    #include <M5GFX.h>
#endif
#include "Theme.h"
#include "widgets/GaugeWidget.h"
#include "widgets/UIHeader.h"
#include "widgets/PresetSlotWidget.h"
#ifndef SIMULATOR_BUILD
#include "widgets/ActionRowWidget.h"
#include "../network/WebSocketClient.h"
#endif

// Forward declarations
#ifndef SIMULATOR_BUILD
class ZoneComposerUI;
class PresetManager;
#endif

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
    void updateEncoder(uint8_t index, int32_t value, bool highlight);
    void updateValue(uint8_t index, int32_t value, bool highlight = true) { updateEncoder(index, value, highlight); }  // Alias for compat
    void setConnectionState(bool wifi, bool ws, bool encA, bool encB);

    // Metadata display (for effect/palette names from server)
    void setCurrentEffect(uint8_t id, const char* name);
    void setCurrentPalette(uint8_t id, const char* name);
    void setWiFiInfo(const char* ip, const char* ssid, int32_t rssi);

    // Screen switching
    void setScreen(UIScreen screen);
    UIScreen getCurrentScreen() const { return _currentScreen; }
    
    // Get zone composer UI (for router initialization)
    #ifndef SIMULATOR_BUILD
    ZoneComposerUI* getZoneComposerUI() { return _zoneComposer; }
    #endif
    
    // Get header instance (for ZoneComposerUI)
    UIHeader* getHeader() { return _header; }

    // Preset bank UI
    PresetSlotWidget* getPresetSlot(uint8_t slot) { return (slot < 8) ? _presetSlots[slot] : nullptr; }
    void updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness);
    void setActivePresetSlot(uint8_t slot);
    #ifndef SIMULATOR_BUILD
    void refreshAllPresetSlots(PresetManager* pm);
    #endif

    // Colour correction state (touch action row)
    #ifndef SIMULATOR_BUILD
    void setColourCorrectionState(const ColorCorrectionState& state);
    #endif

private:
    M5GFX& _display;

    UIHeader* _header;
    GaugeWidget* _gauges[16];
    PresetSlotWidget* _presetSlots[8];
    #ifndef SIMULATOR_BUILD
    ActionRowWidget* _actionRow = nullptr;
    #endif
    #ifndef SIMULATOR_BUILD
    ZoneComposerUI* _zoneComposer = nullptr;
    #endif
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
