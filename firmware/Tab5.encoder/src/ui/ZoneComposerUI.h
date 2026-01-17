#pragma once
// ============================================================================
// ZoneComposerUI - Zone Composer Dashboard Screen
// ============================================================================
// Visual mixer for 4 zones, inspired by LightwaveOS Dashboard V2
// Shows per-zone effect, speed/palette, and LED ranges
// ============================================================================

#include <M5GFX.h>
#include <lvgl.h>
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

/**
 * Zone parameter modes (Effect, Palette, Speed, Brightness)
 */
enum class ZoneParameterMode : uint8_t {
    EFFECT = 0,
    PALETTE = 1,
    SPEED = 2,
    BRIGHTNESS = 3,
    COUNT = 4  // For array bounds
};

/**
 * Selection type (which UI element is selected)
 */
enum class SelectionType : uint8_t {
    NONE = 0,
    ZONE_PARAMETER = 1,  // One of 4 zone rows (effect/palette/speed/brightness)
    ZONE_COUNT = 2,      // Zone count selector
    PRESET = 3           // Preset selector
};

/**
 * Current selection state
 */
struct ZoneSelection {
    SelectionType type = SelectionType::NONE;
    uint8_t zoneIndex = 0;  // 0-3 for zone parameters
    ZoneParameterMode mode = ZoneParameterMode::EFFECT;  // Which parameter is active

    bool operator==(const ZoneSelection& other) const {
        return type == other.type &&
               zoneIndex == other.zoneIndex &&
               mode == other.mode;
    }

    bool operator!=(const ZoneSelection& other) const {
        return !(*this == other);
    }
};

class ZoneComposerUI {
public:
    ZoneComposerUI(M5GFX& display);
    ~ZoneComposerUI();

    void begin(lv_obj_t* parent = nullptr);
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
     * Check if zone mode is enabled
     * @return true if zones are active
     */
    bool isZoneModeEnabled() const { return _zonesEnabled; }

    /**
     * Get number of active zones
     * @return Zone count (1-4)
     */
    uint8_t getZoneCount() const { return _zoneCount; }

    /**
     * Get zone state
     * @param zoneId Zone ID (0-3)
     * @return Const reference to ZoneState
     */
    const ZoneState& getZoneState(uint8_t zoneId) const {
        static ZoneState empty;
        if (zoneId >= 4) return empty;
        return _zones[zoneId];
    }

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
     * Set callback for Back button (returns to GLOBAL screen)
     * @param callback Function to call when Back button pressed
     */
    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }
    
    /**
     * Set header instance (shared across screens)
     * @param header UIHeader instance
     */
    void setHeader(UIHeader* header) { _header = header; }

    /**
     * Mark UI as dirty (needs redraw) - queued for next frame
     */
    void markDirty() { _pendingDirty = true; }
    
    /**
     * Force immediate dirty state (bypasses pending mechanism)
     * Use this for screen transitions that require immediate redraw
     * Also resets frame timer to ensure immediate render
     */
    void forceDirty() { 
        _dirty = true; 
        _pendingDirty = false; 
        _lastRenderTime = 0;  // Reset frame timer to force immediate render
    }
    
    /**
     * Handle touch event (called from DisplayUI)
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     */
    void handleTouch(int16_t x, int16_t y);

    // ========================================================================
    // Phase 1: State Management Methods
    // ========================================================================

    /**
     * Handle encoder change event (called from DisplayUI)
     * @param encoderIndex Encoder index (0-15)
     * @param delta Encoder change delta
     */
    void handleEncoderChange(uint8_t encoderIndex, int32_t delta);

    /**
     * Select a zone parameter for editing
     * @param zoneIndex Zone index (0-3)
     * @param mode Parameter mode
     */
    void selectParameter(uint8_t zoneIndex, ZoneParameterMode mode);

    /**
     * Select zone count row
     */
    void selectZoneCount();

    /**
     * Select preset row
     */
    void selectPreset();

    /**
     * Set active parameter mode (Effect, Palette, Speed, Brightness)
     * @param mode New parameter mode
     */
    void setActiveMode(ZoneParameterMode mode);

    /**
     * Clear current selection (deselect all)
     */
    void clearSelection();

    /**
     * Get current selection
     * @return Current selection state
     */
    const ZoneSelection& getCurrentSelection() const { return _currentSelection; }

    /**
     * Get active parameter mode
     * @return Active mode
     */
    ZoneParameterMode getActiveMode() const { return _activeMode; }

    // Zone State Accessors moved to public section

private:
    M5GFX& _display;
    ButtonHandler* _buttonHandler = nullptr;
    WebSocketClient* _wsClient = nullptr;
    UIHeader* _header = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;

    // ========================================================================
    // Phase 2: LVGL Widget Metadata
    // ========================================================================

    /**
     * Parameter metadata stored in LVGL user_data
     */
    struct ParameterMetadata {
        uint8_t zoneIndex;
        ZoneParameterMode mode;
    };

    // ========================================================================
    // State Management (Phase 1)
    // ========================================================================

    // Current selection state
    ZoneSelection _currentSelection;
    ZoneParameterMode _activeMode = ZoneParameterMode::EFFECT;

    // Zone parameter values (cached local state)
    uint8_t _zoneEffects[4] = {0, 0, 0, 0};
    uint8_t _zonePalettes[4] = {0, 0, 0, 0};
    uint8_t _zoneSpeeds[4] = {25, 25, 25, 25};
    uint8_t _zoneBrightness[4] = {128, 128, 128, 128};

    // Preset state
    uint8_t _currentPresetIndex = 0;
    const char* _presetName = "Unified";

    // ========================================================================
    // LVGL Widget References (Phase 1)
    // ========================================================================

    // Zone parameter widgets (one per zone, per mode)
    lv_obj_t* _zoneParamContainers[4] = {nullptr};
    lv_obj_t* _zoneEffectLabels[4] = {nullptr};
    lv_obj_t* _zonePaletteLabels[4] = {nullptr};
    lv_obj_t* _zoneSpeedLabels[4] = {nullptr};
    lv_obj_t* _zoneBrightnessLabels[4] = {nullptr};

    // Mode selector buttons
    lv_obj_t* _modeButtons[4] = {nullptr};  // Effect, Palette, Speed, Brightness
    lv_obj_t* _backButton = nullptr;
    lv_obj_t* _zoneEnableButton = nullptr;  // Zone enable toggle button
    lv_obj_t* _zoneEnableLabel = nullptr;   // "ZONES: ON/OFF" label

    // Zone count and preset rows
    lv_obj_t* _zoneCountRow = nullptr;
    lv_obj_t* _zoneCountValueLabel = nullptr;
    lv_obj_t* _presetRow = nullptr;
    lv_obj_t* _presetValueLabel = nullptr;

    // LED strip visualization (existing M5GFX rendering)
    lv_obj_t* _ledStripCanvas = nullptr;

    // ========================================================================
    // LVGL Styles (Phase 1)
    // ========================================================================

    lv_style_t _styleSelected;      // Blue accent border + bg tint
    lv_style_t _styleHighlighted;   // Lighter border for hover/focus
    lv_style_t _styleNormal;        // Default style

    // ========================================================================
    // Legacy State (M5GFX rendering)
    // ========================================================================

    // Zone states
    ZoneState _zones[4];

    // Zone segments (layout)
    zones::ZoneSegment _segments[zones::MAX_ZONES];
    uint8_t _zoneCount = 0;
    bool _zonesEnabled = false;

    // Editing segments (working copy for visualization)
    zones::ZoneSegment _editingSegments[zones::MAX_ZONES];
    uint8_t _editingZoneCount = 0;

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

    // ========================================================================
    // Phase 1: Private Helper Methods
    // ========================================================================

    /**
     * Initialize LVGL styles for selection feedback
     */
    void initStyles();

    // ========================================================================
    // Phase 2: Widget Creation Methods
    // ========================================================================

    /**
     * Create interactive LVGL UI (called once during initialization)
     * @param parent Parent LVGL object
     */
    void createInteractiveUI(lv_obj_t* parent);

    /**
     * Create zone count selection row
     * @param parent Parent LVGL object
     * @return Zone count row object
     */
    lv_obj_t* createZoneCountRow(lv_obj_t* parent);

    /**
     * Create preset selection row
     * @param parent Parent LVGL object
     * @return Preset row object
     */
    lv_obj_t* createPresetRow(lv_obj_t* parent);

    /**
     * Create LED strip visualization bar (4 colored segments)
     * @param parent Parent LVGL object
     */
    void createLedStripBar(lv_obj_t* parent);

    /**
     * Create zone cards grid (4 HORIZONTAL columns)
     * @param parent Parent LVGL object
     */
    void createZoneCardsGrid(lv_obj_t* parent);

    /**
     * Create individual zone card
     * @param parent Parent LVGL object
     * @param zoneIndex Zone index (0-3)
     * @return Zone card object
     */
    lv_obj_t* createZoneCard(lv_obj_t* parent, uint8_t zoneIndex);

    /**
     * Create parameter section (EFFECT or PALETTE)
     * @param parent Parent LVGL object
     * @param zoneIndex Zone index (0-3)
     * @param label Section label ("EFFECT" or "PALETTE")
     * @param value Current value text
     */
    void createParameterSection(lv_obj_t* parent, uint8_t zoneIndex,
                                const char* label, const char* value);

    /**
     * Create SPD/BRI circular buttons row
     * @param parent Parent LVGL object
     * @param zoneIndex Zone index (0-3)
     */
    void createSpeedBrightnessButtons(lv_obj_t* parent, uint8_t zoneIndex);

    /**
     * Create circular button with ring outline
     * @param parent Parent LVGL object
     * @param text Button text ("SPD" or "BRI")
     * @param ringColor Ring border color
     * @param zoneIndex Zone index (0-3)
     */
    void createCircularButton(lv_obj_t* parent, const char* text,
                              uint32_t ringColor, uint8_t zoneIndex);

    /**
     * Create zone parameter grid (4 rows × 4 parameters) - LEGACY
     * @param parent Parent LVGL object
     */
    void createZoneParameterGrid(lv_obj_t* parent);

    /**
     * Create single zone parameter row - LEGACY
     * @param parent Parent LVGL object
     * @param zoneIndex Zone index (0-3)
     * @return Zone parameter row object
     */
    lv_obj_t* createZoneParamRow(lv_obj_t* parent, uint8_t zoneIndex);

    /**
     * Create clickable parameter widget (Effect/Palette/Speed/Brightness) - LEGACY
     * @param parent Parent LVGL object
     * @param label Parameter label text
     * @param value Initial value text
     * @param zoneIndex Zone index (0-3)
     * @param mode Parameter mode
     * @return Parameter widget container
     */
    lv_obj_t* createClickableParameter(lv_obj_t* parent, const char* label,
                                        const char* value, uint8_t zoneIndex,
                                        ZoneParameterMode mode);

    /**
     * Create 5-button mode selector row
     * @param parent Parent LVGL object
     */
    void createModeSelector(lv_obj_t* parent);

    /**
     * Create back button
     * @param parent Parent LVGL object
     * @return Back button object
     */
    lv_obj_t* createBackButton(lv_obj_t* parent);

    // ========================================================================
    // Phase 2: LVGL Event Callbacks (static)
    // ========================================================================

    /**
     * Zone parameter touch callback
     * @param e LVGL event
     */
    static void parameterTouchCb(lv_event_t* e);

    /**
     * Zone count touch callback
     * @param e LVGL event
     */
    static void zoneCountTouchCb(lv_event_t* e);

    /**
     * Preset touch callback
     * @param e LVGL event
     */
    static void presetTouchCb(lv_event_t* e);

    /**
     * Mode button press callback
     * @param e LVGL event
     */
    static void modeButtonCb(lv_event_t* e);

    /**
     * Back button press callback
     * @param e LVGL event
     */
    static void backButtonCb(lv_event_t* e);

    /**
     * Zone enable toggle button callback
     * @param e LVGL event
     */
    static void zoneEnableButtonCb(lv_event_t* e);

    /**
     * Apply selection highlighting to current selection
     */
    void applySelectionHighlight();

    /**
     * Get parameter widget for zone and mode
     * @param zoneIndex Zone index (0-3)
     * @param mode Parameter mode
     * @return Widget pointer or nullptr
     */
    lv_obj_t* getParameterWidget(uint8_t zoneIndex, ZoneParameterMode mode);

    /**
     * Adjust zone parameter based on active mode
     * @param zoneIndex Zone index (0-3)
     * @param delta Encoder delta
     */
    void adjustZoneParameter(uint8_t zoneIndex, int32_t delta);

    /**
     * Adjust zone count
     * @param delta Encoder delta
     */
    void adjustZoneCount(int32_t delta);

    /**
     * Adjust preset selection
     * @param delta Encoder delta
     */
    void adjustPreset(int32_t delta);

    /**
     * Update effect label for zone
     * @param zoneIndex Zone index (0-3)
     */
    void updateEffectLabel(uint8_t zoneIndex);

    /**
     * Update palette label for zone
     * @param zoneIndex Zone index (0-3)
     */
    void updatePaletteLabel(uint8_t zoneIndex);

    /**
     * Update speed label for zone
     * @param zoneIndex Zone index (0-3)
     */
    void updateSpeedLabel(uint8_t zoneIndex);

    /**
     * Update brightness label for zone
     * @param zoneIndex Zone index (0-3)
     */
    void updateBrightnessLabel(uint8_t zoneIndex);

    /**
     * Update zone count label
     */
    void updateZoneCountLabel();

    /**
     * Update preset label
     */
    void updatePresetLabel();

    // ========================================================================
    // Legacy M5GFX Rendering Methods
    // ========================================================================

    void render();
    void drawLedStripVisualiser(int x, int y, int w, int h);
    void drawZoneList(int x, int y, int w, int h);
    void drawZoneRow(uint8_t zoneId, int x, int y, int w, int h);
    void drawZoneInfo(int x, int y, int w, int h);
    uint32_t getZoneColor(uint8_t zoneId) const;
    
    // Zone layout generation (for visualization)
    void generateZoneSegments(uint8_t zoneCount);
    void loadPreset(int8_t presetId);
    bool validateLayout(const zones::ZoneSegment* segments, uint8_t count) const;
    void validatePresets();  // Boot-time preset validation
    
    // Convert RGB888 to RGB565
    static uint16_t rgb888To565(uint32_t rgb888);
};
