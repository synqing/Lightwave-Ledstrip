// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// ZoneComposerUI v2 — LED-Centric Zone Composer
// ============================================================================
// Hero LED strip visualiser + 5-column param grid + 3-column zone overview.
// Centre-origin topology: LEDs 79/80 outward. Max 3 zones (1-indexed UI).
// Spec: tab5-encoder/docs/ZONE_COMPOSER_V2_SPEC.md
// ============================================================================

#include <M5GFX.h>
#include <lvgl.h>
#include <cstdint>
#include "../zones/ZoneDefinition.h"

// Forward declarations
class ButtonHandler;
class WebSocketClient;
class UIHeader;

// ============================================================================
// Zone state for display (populated from WS zones.list response)
// ============================================================================
struct ZoneState {
    uint16_t effectId = 0;
    char effectName[48] = {0};
    uint8_t speed = 25;
    uint8_t paletteId = 0;
    char paletteName[48] = {0};
    uint8_t blendMode = 0;
    char blendModeName[32] = {0};
    bool enabled = false;
    uint8_t ledStart = 0;
    uint8_t ledEnd = 0;
    uint8_t brightness = 128;
};

// ============================================================================
// Zone colour constants (aligned to iOS canonical — cyan/green/purple)
// ============================================================================
namespace ZoneColours {
    static constexpr uint32_t ZONE_1 = 0x00FFFF;  // Cyan — INNER
    static constexpr uint32_t ZONE_2 = 0x00FF99;  // Green — MIDDLE
    static constexpr uint32_t ZONE_3 = 0x9900FF;  // Purple — OUTER
    static constexpr uint32_t ALL[3] = { ZONE_1, ZONE_2, ZONE_3 };
}

// Zone role names (2-zone and 3-zone modes)
namespace ZoneRoles {
    static constexpr const char* ROLES_3[3] = { "INNER", "MIDDLE", "OUTER" };
    static constexpr const char* ROLES_2[2] = { "INNER", "OUTER" };
}

class ZoneComposerUI {
public:
    ZoneComposerUI(M5GFX& display);
    ~ZoneComposerUI();

    // Lifecycle
    void begin(lv_obj_t* parent = nullptr);
    void loop();

    // State updates (from WsMessageRouter)
    void updateZone(uint8_t zoneId, const ZoneState& state);
    void updateSegments(const zones::ZoneSegment* segments, uint8_t count);
    void updateZoneModeButton(bool enabled);

    // Accessors
    bool isZoneModeEnabled() const { return _zonesEnabled; }
    uint8_t getZoneCount() const { return _zoneCount; }
    uint8_t getSelectedZone() const { return _selectedZone; }
    const zones::ZoneSegment* getEditingSegments() const { return _editingSegments; }
    uint8_t getEditingZoneCount() const { return _editingZoneCount; }

    const ZoneState& getZoneState(uint8_t zoneId) const {
        static ZoneState empty;
        if (zoneId >= 3) return empty;
        return _zones[zoneId];
    }

    // Dependency injection
    void setButtonHandler(ButtonHandler* handler) { _buttonHandler = handler; }
    void setWebSocketClient(WebSocketClient* wsClient) { _wsClient = wsClient; }
    void setHeader(UIHeader* header) { _header = header; }

    // Back button callback
    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }

    // Encoder input (called from main.cpp onEncoderChange)
    // index: global encoder index (8-15 for Unit-B)
    // delta: rotation delta (+/-)
    void handleEncoderChange(uint8_t encoderIndex, int32_t delta);

    // Encoder button press (called from main.cpp)
    void handleEncoderClick(uint8_t encoderIndex);

    // Touch handling
    void handleTouch(int16_t x, int16_t y);

    // Dirty flag management
    void markDirty() { _pendingDirty = true; }
    void forceDirty() {
        _dirty = true;
        _pendingDirty = false;
        _lastRenderTime = 0;
    }

private:
    M5GFX& _display;
    ButtonHandler* _buttonHandler = nullptr;
    WebSocketClient* _wsClient = nullptr;
    UIHeader* _header = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;

    // ========================================================================
    // Zone State
    // ========================================================================
    ZoneState _zones[3];
    zones::ZoneSegment _segments[zones::MAX_ZONES];
    zones::ZoneSegment _editingSegments[zones::MAX_ZONES];
    uint8_t _editingZoneCount = 0;

    uint8_t _zoneCount = 3;          // 2 or 3 active zones
    uint8_t _selectedZone = 0;       // Currently selected zone (0-2)
    bool _zonesEnabled = true;       // Zone mode on/off

    // Rendering state
    bool _dirty = true;
    bool _pendingDirty = false;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS

    // ========================================================================
    // LVGL Widget References — Header
    // ========================================================================
    lv_obj_t* _backButton = nullptr;
    lv_obj_t* _titleLabel = nullptr;
    lv_obj_t* _zoneCountCard = nullptr;
    lv_obj_t* _zoneCountValue = nullptr;
    lv_obj_t* _zoneEnableBtn = nullptr;
    lv_obj_t* _zoneEnableLabel = nullptr;

    // ========================================================================
    // LVGL Widget References — Mode Row (zone selector buttons)
    // ========================================================================
    lv_obj_t* _zoneSelectorBtns[3] = {nullptr};
    lv_obj_t* _zoneSelectorLabels[3] = {nullptr};

    // ========================================================================
    // LVGL Widget References — LED Strip Visualiser
    // ========================================================================
    lv_obj_t* _stripContainer = nullptr;
    lv_obj_t* _stripBar = nullptr;
    lv_obj_t* _stripZoneLabels[3] = {nullptr};    // Labels above strip
    lv_obj_t* _zoneSegs[5] = {nullptr};           // Z1 centre, Z2L, Z2R, Z3L, Z3R
    lv_obj_t* _centreMarker = nullptr;            // Gold 79/80 divider
    lv_obj_t* _stripTickLabels[7] = {nullptr};    // LED number ticks

    // Selected zone indicator
    lv_obj_t* _selectedZoneDot = nullptr;
    lv_obj_t* _selectedZoneTitle = nullptr;

    // ========================================================================
    // LVGL Widget References — 5-Column Parameter Grid
    // ========================================================================
    lv_obj_t* _paramGrid = nullptr;
    lv_obj_t* _paramCards[5] = {nullptr};
    lv_obj_t* _paramLabels[5] = {nullptr};     // "EFFECT", "PALETTE", etc.
    lv_obj_t* _paramValues[5] = {nullptr};     // "042", "012", "50", etc.
    lv_obj_t* _paramNames[5] = {nullptr};      // "Chroma Wave", "Ocean Breeze", etc.
    lv_obj_t* _paramBars[5] = {nullptr};       // Progress bars

    // ========================================================================
    // LVGL Widget References — 3-Column Zone Overview
    // ========================================================================
    lv_obj_t* _overviewGrid = nullptr;
    lv_obj_t* _overviewCards[3] = {nullptr};
    lv_obj_t* _ovHeaders[3] = {nullptr};       // "ZONE N — ROLE"
    lv_obj_t* _ovDots[3] = {nullptr};          // Zone colour dots
    lv_obj_t* _ovRangeLabels[3] = {nullptr};   // "40-79 | 80-119"
    lv_obj_t* _ovEffectVals[3] = {nullptr};    // "042 Chroma Wave"
    lv_obj_t* _ovPaletteVals[3] = {nullptr};   // "012 Ocean Breeze"
    lv_obj_t* _ovStatsLabels[3] = {nullptr};   // "SPD 50  BRI 200  Overwrite"
    lv_obj_t* _ovLedCounts[3] = {nullptr};     // "80 LEDs"

    // ========================================================================
    // LVGL Widget References — Footer
    // ========================================================================
    lv_obj_t* _footer = nullptr;

    // ========================================================================
    // Widget Creation Methods (called once in begin())
    // ========================================================================
    void createHeader(lv_obj_t* parent);
    void createModeRow(lv_obj_t* parent);
    void createStripVisualiser(lv_obj_t* parent);
    void createSelectedZoneIndicator(lv_obj_t* parent);
    void createParamGrid(lv_obj_t* parent);
    void createOverviewGrid(lv_obj_t* parent);
    void createFooter(lv_obj_t* parent);

    // ========================================================================
    // Update Methods (called on state change)
    // ========================================================================
    void updateStripSegments();           // Recalculate strip segment positions/sizes
    void updateParamCards();              // Refresh all 5 param cards from selected zone
    void updateOverviewCards();           // Refresh all 3 overview cards
    void updateSelectedZoneIndicator();   // Update "ZONE N PARAMETERS" text
    void updateZoneSelectorButtons();     // Highlight selected, show/hide Zone 3
    void updateZoneEnableVisuals();       // ON/OFF button state
    void updateZoneCountVisuals();        // Zone count badge

    // ========================================================================
    // Interaction Handlers
    // ========================================================================
    void selectZone(uint8_t zoneIndex);       // Select zone for editing
    void adjustZoneCount(int32_t delta);      // Change zone count 2↔3
    void adjustEffect(int32_t delta);         // ENC 0
    void adjustPalette(int32_t delta);        // ENC 1
    void adjustSpeed(int32_t delta);          // ENC 2
    void adjustBrightness(int32_t delta);     // ENC 3
    void adjustBlendMode(int32_t delta);      // ENC 4
    void toggleZoneMode();                    // ENC 7 click

    // ========================================================================
    // WebSocket Command Senders
    // ========================================================================
    void sendZoneEffect(uint8_t zoneId, uint16_t effectId);
    void sendZonePalette(uint8_t zoneId, uint8_t paletteId);
    void sendZoneSpeed(uint8_t zoneId, uint8_t speed);
    void sendZoneBrightness(uint8_t zoneId, uint8_t brightness);
    void sendZoneBlendMode(uint8_t zoneId, uint8_t blendMode);
    void sendZoneEnable(bool enable);
    void sendZoneLoadPreset(uint8_t presetId);
    void sendRequestZonesState();

    // ========================================================================
    // LVGL Static Callbacks
    // ========================================================================
    static void backButtonCb(lv_event_t* e);
    static void zoneEnableCb(lv_event_t* e);
    static void zoneCountCb(lv_event_t* e);
    static void zoneSelectorCb(lv_event_t* e);
    static void stripSegmentCb(lv_event_t* e);
    static void overviewCardCb(lv_event_t* e);

    // ========================================================================
    // Helpers
    // ========================================================================
    uint32_t getZoneColour(uint8_t zoneId) const;
    const char* getZoneRole(uint8_t zoneId) const;
    int calcSegmentX(uint8_t ledIndex) const;     // LED index → pixel X in strip
    int calcSegmentWidth(uint8_t ledStart, uint8_t ledEnd) const;

    // Strip geometry constants
    static constexpr int STRIP_BAR_X = 60;
    static constexpr int STRIP_BAR_WIDTH = 1160;
    static constexpr int LEDS_PER_STRIP = 160;
};
