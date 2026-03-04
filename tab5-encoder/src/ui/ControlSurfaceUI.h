// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// ControlSurfaceUI - Effect Parameter Tuning + Presets + Camera Mode
// ============================================================================
// Phase 0 Control Surface for deterministic recording.
// Maps 16 encoders to effect-specific parameters, with dedicated slots
// for camera mode toggle (encoder 14) and preset bank (encoder 15).
//
// Layout (1280x720):
//   Row A (enc 0-7):  Effect params 0-7
//   Row B (enc 8-15): Effect params 8-13, Camera Mode, Preset Bank
//   Global Row:       Read-only global params (brightness, speed, etc.)
// ============================================================================

#include <M5GFX.h>
#include <lvgl.h>
#include <cstdint>

// Forward declarations
class WebSocketClient;

// Maximum effect parameters (matches firmware-v3 IEffect.h)
static constexpr uint8_t CS_MAX_EFFECT_PARAMS = 16;

// Encoder slot assignments
static constexpr uint8_t CS_ENCODER_CAMERA_MODE = 14;
static constexpr uint8_t CS_ENCODER_PRESET_BANK = 15;
static constexpr uint8_t CS_MAX_PARAM_ENCODERS = 14;  // 0-13 for params

// Preset slots
static constexpr uint8_t CS_MAX_PRESET_SLOTS = 8;

/**
 * Effect parameter type (mirrors firmware-v3 EffectParameterType)
 */
enum class CSParamType : uint8_t {
    FLOAT = 0,
    INT = 1,
    BOOL = 2,
    ENUM = 3
};

/**
 * Cached effect parameter metadata + current value
 */
struct CSEffectParam {
    char name[32] = {0};
    char displayName[32] = {0};
    char unit[8] = {0};
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float currentValue = 0.0f;
    float defaultValue = 0.0f;
    float step = 0.01f;
    CSParamType type = CSParamType::FLOAT;
    bool valid = false;  // True if populated from server response
};

/**
 * Preset slot display state
 */
struct CSPresetSlot {
    bool occupied = false;
    char name[32] = {0};
    uint16_t effectId = 0;
};

class ControlSurfaceUI {
public:
    ControlSurfaceUI(M5GFX& display);
    ~ControlSurfaceUI();

    /**
     * Create LVGL widgets on the given parent screen object
     */
    void begin(lv_obj_t* parent = nullptr);

    /**
     * Main loop - throttled rendering updates
     */
    void loop();

    // ========================================================================
    // Encoder Input (called from main.cpp routing)
    // ========================================================================

    /**
     * Handle encoder rotation
     * @param encoderIndex 0-15 physical encoder index
     * @param delta Rotation delta (positive = clockwise)
     */
    void handleEncoderChange(uint8_t encoderIndex, int32_t delta);

    /**
     * Handle encoder button press
     * @param encoderIndex 0-15 physical encoder index
     * @param isLongPress True for long press, false for short
     */
    void handleEncoderButton(uint8_t encoderIndex, bool isLongPress);

    // ========================================================================
    // WebSocket Response Handlers
    // ========================================================================

    /**
     * Called when K1 responds with effect parameter metadata
     * Populates param slots and rebinds encoder display
     */
    void onEffectParametersReceived(uint16_t effectId, const char* effectName,
                                     const CSEffectParam* params, uint8_t paramCount);

    /**
     * Called when K1 responds with preset list
     */
    void onPresetListReceived(const CSPresetSlot* slots, uint8_t slotCount);

    /**
     * Update camera mode state (from K1 broadcast)
     */
    void setCameraMode(bool active);

    /**
     * Update a global param value (from status broadcast)
     */
    void updateGlobalParam(uint8_t index, uint8_t value);

    // ========================================================================
    // Wiring
    // ========================================================================

    void setWebSocketClient(WebSocketClient* wsClient) { _wsClient = wsClient; }

    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }

    /**
     * Called when this screen becomes active - requests param metadata
     */
    void onScreenEnter();

    /**
     * Mark UI dirty for redraw
     */
    void markDirty() { _dirty = true; }
    void forceDirty() {
        _dirty = true;
        _lastRenderTime = 0;
    }

private:
    M5GFX& _display;
    WebSocketClient* _wsClient = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;

    // ========================================================================
    // Effect Parameter State
    // ========================================================================

    CSEffectParam _params[CS_MAX_EFFECT_PARAMS];
    uint8_t _paramCount = 0;
    uint16_t _currentEffectId = 0;
    char _currentEffectName[48] = {0};

    // ========================================================================
    // Camera Mode State
    // ========================================================================

    bool _cameraModeActive = false;

    // ========================================================================
    // Preset State
    // ========================================================================

    CSPresetSlot _presetSlots[CS_MAX_PRESET_SLOTS];
    uint8_t _selectedPresetSlot = 0;

    // ========================================================================
    // LVGL Widgets - Row A (encoders 0-7, params 0-7)
    // ========================================================================

    lv_obj_t* _rowA_container = nullptr;
    lv_obj_t* _rowA_cards[8] = {nullptr};
    lv_obj_t* _rowA_labels[8] = {nullptr};
    lv_obj_t* _rowA_values[8] = {nullptr};
    lv_obj_t* _rowA_bars[8] = {nullptr};

    // ========================================================================
    // LVGL Widgets - Row B (encoders 8-15, params 8-13 + camera + preset)
    // ========================================================================

    lv_obj_t* _rowB_container = nullptr;
    lv_obj_t* _rowB_cards[8] = {nullptr};
    lv_obj_t* _rowB_labels[8] = {nullptr};
    lv_obj_t* _rowB_values[8] = {nullptr};
    lv_obj_t* _rowB_bars[8] = {nullptr};

    // ========================================================================
    // LVGL Widgets - Global Params Row (read-only)
    // ========================================================================

    lv_obj_t* _globalRow_container = nullptr;
    lv_obj_t* _globalRow_labels[8] = {nullptr};
    lv_obj_t* _globalRow_values[8] = {nullptr};

    // ========================================================================
    // LVGL Widgets - Navigation
    // ========================================================================

    lv_obj_t* _backButton = nullptr;

    // ========================================================================
    // Rendering State
    // ========================================================================

    bool _dirty = true;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 33;  // ~30 FPS

    // ========================================================================
    // Private Methods
    // ========================================================================

    // UI Construction
    void createParamRow(lv_obj_t* parent, lv_obj_t** cards, lv_obj_t** labels,
                        lv_obj_t** values, lv_obj_t** bars, lv_obj_t*& container,
                        int32_t yPos, const char* rowLabel);
    void createGlobalRow(lv_obj_t* parent);
    void createBackButton(lv_obj_t* parent);

    // Param Display
    void rebindEncoders();
    void updateParamCard(uint8_t encoderIndex);
    void updateCameraCard();
    void updatePresetCard();
    void formatParamValue(char* buf, size_t bufSize, const CSEffectParam& param);

    // Encoder Mapping
    float applyEncoderDelta(uint8_t paramIndex, int32_t delta);

    // Static callbacks
    static void backButtonCb(lv_event_t* e);
};
