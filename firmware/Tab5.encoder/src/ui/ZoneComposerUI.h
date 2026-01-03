#pragma once
// ============================================================================
// ZoneComposerUI - Zone Composer Dashboard Screen
// ============================================================================
// Visual mixer for 4 zones, inspired by LightwaveOS Dashboard V2
// Shows per-zone effect, speed/palette, and LED ranges
// ============================================================================

#include <M5GFX.h>
#include <cstdint>
#include "Theme.h"
#include "../zones/ZoneDefinition.h"

    // Forward declarations
    class ButtonHandler;
    class WebSocketClient;
    class UIHeader;

/**
 * Zone state for display
 */
struct ZoneState {
    uint8_t effectId = 0;
    const char* effectName = "Unknown";
    uint8_t speed = 25;
    uint8_t paletteId = 0;
    const char* paletteName = "Unknown";
    uint8_t blendMode = 0;
    const char* blendModeName = "OVERWRITE";
    bool enabled = false;
    uint8_t ledStart = 0;
    uint8_t ledEnd = 0;
};

class ZoneComposerUI {
public:
    ZoneComposerUI(M5GFX& display);
    ~ZoneComposerUI();

    void begin();
    void loop();

    /**
     * Update zone state
     * @param zoneId Zone ID (0-3)
     * @param state Zone state
     */
    void updateZone(uint8_t zoneId, const ZoneState& state);

    /**
     * Update zone segments (layout)
     * @param segments Array of zone segments
     * @param count Number of segments
     */
    void updateSegments(const zones::ZoneSegment* segments, uint8_t count);

    /**
     * Set button handler for checking speed/palette mode
     * @param handler ButtonHandler instance
     */
    void setButtonHandler(ButtonHandler* handler) { _buttonHandler = handler; }
    
    /**
     * Set WebSocket client for sending commands
     * @param wsClient WebSocketClient instance
     */
    void setWebSocketClient(WebSocketClient* wsClient) { _wsClient = wsClient; }
    
    /**
     * Set header instance (shared across screens)
     * @param header UIHeader instance
     */
    void setHeader(UIHeader* header) { _header = header; }

    /**
     * Mark UI as dirty (needs redraw)
     */
    void markDirty() { _pendingDirty = true; }
    
    /**
     * Handle touch event (called from DisplayUI)
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     */
    void handleTouch(int16_t x, int16_t y);

private:
    M5GFX& _display;
    ButtonHandler* _buttonHandler = nullptr;
    WebSocketClient* _wsClient = nullptr;
    UIHeader* _header = nullptr;

    // Zone states
    ZoneState _zones[4];

    // Zone segments (layout)
    zones::ZoneSegment _segments[zones::MAX_ZONES];
    uint8_t _zoneCount = 0;
    bool _zonesEnabled = false;

    // Editor state
    uint8_t _selectedZone = 255;  // No zone selected
    uint8_t _zoneCountSelect = 3;  // Default 3 zones
    int8_t _presetSelect = -1;  // -1 = custom, 0-4 = preset IDs
    
    // Editing segments (working copy, not yet applied)
    zones::ZoneSegment _editingSegments[zones::MAX_ZONES];
    uint8_t _editingZoneCount = 0;

    // Dropdown state
    enum class DropdownType : uint8_t {
        NONE = 0,
        EFFECT,
        PALETTE,
        BLEND
    };
    DropdownType _openDropdown = DropdownType::NONE;
    uint8_t _openDropdownZone = 255;
    int16_t _dropdownScroll = 0;
    int16_t _dropdownX = 0;
    int16_t _dropdownY = 0;
    int16_t _dropdownW = 0;
    int16_t _dropdownH = 0;

    // Rendering state
    bool _dirty = true;
    bool _pendingDirty = false;  // Accumulates dirty requests
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS (was 16ms/60FPS)
    
    // Layout constants (optimized for 1280x720)
    // Note: All Y positions are relative to header (STATUS_BAR_H = 80)
    static constexpr int LED_STRIP_Y = 60 + Theme::STATUS_BAR_H;  // Was 60, now below header
    static constexpr int LED_STRIP_H = 80;
    static constexpr int ZONE_LIST_Y = 180 + Theme::STATUS_BAR_H;  // Was 180, now below header
    static constexpr int CONTROLS_Y = 520 + Theme::STATUS_BAR_H;   // Was 520, now below header

    // Zone colors (matching dashboard template)
    static constexpr uint32_t ZONE_COLORS[4] = {
        0x6EE7F3,  // Zone 0: Cyan
        0x22DD88,  // Zone 1: Green
        0xFFB84D,  // Zone 2: Orange
        0x9D4EDD   // Zone 3: Purple
    };

    // Touch handling (private helpers)
    int8_t hitTestLedStrip(int16_t x, int16_t y);
    int8_t hitTestDropdown(int16_t x, int16_t y);
    uint8_t getZoneForLed(uint8_t ledIndex, bool isRight);
    
    // Dropdown handling
    void openDropdown(DropdownType type, uint8_t zoneId, int16_t x, int16_t y, int16_t w, int16_t h);
    void closeDropdown();
    void drawDropdown();
    void drawDropdownList(DropdownType type, int16_t x, int16_t y, int16_t w, int16_t h);
    void handleDropdownSelection(DropdownType type, uint8_t zoneId, int itemIndex);

    void render();
    void drawLedStripVisualiser(int x, int y, int w, int h);
    void drawZoneList(int x, int y, int w, int h);
    void drawZoneRow(uint8_t zoneId, int x, int y, int w, int h);
    void drawEditorControls(int x, int y, int w, int h);
    void drawStepperControls(int x, int y, int w, int h);
    uint32_t getZoneColor(uint8_t zoneId) const;
    
    // Zone layout generation and editing
    void generateZoneSegments(uint8_t zoneCount);
    void loadPreset(int8_t presetId);
    void adjustZoneBoundary(uint8_t zoneId, bool increase);
    bool validateLayout(const zones::ZoneSegment* segments, uint8_t count) const;
    void validatePresets();  // Boot-time preset validation
    
    // Convert RGB888 to RGB565
    static uint16_t rgb888To565(uint32_t rgb888);
};

