// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// ControlSurfaceUI - Effect Parameter Tuning + Presets + Camera Mode
// ============================================================================
// Phase 0 Control Surface implementation.
// Maps 16 encoders to effect-specific parameters for deterministic recording.
// ============================================================================

#include "ControlSurfaceUI.h"

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "fonts/bebas_neue_fonts.h"
#include "fonts/experimental_fonts.h"
#include "../network/WebSocketClient.h"

// ============================================================================
// Color Constants (match DisplayUI.cpp cyberpunk theme)
// ============================================================================

static constexpr uint32_t CS_COLOR_BG_PAGE = 0x0A0A0B;
static constexpr uint32_t CS_COLOR_BG_SURFACE_BASE = 0x121214;
static constexpr uint32_t CS_COLOR_BG_SURFACE_ELEVATED = 0x1A1A1C;
static constexpr uint32_t CS_COLOR_BORDER_BASE = 0x2A2A2E;
static constexpr uint32_t CS_COLOR_FG_PRIMARY = 0xFFFFFF;
static constexpr uint32_t CS_COLOR_FG_SECONDARY = 0x9CA3AF;
static constexpr uint32_t CS_COLOR_FG_DIMMED = 0x4B5563;
static constexpr uint32_t CS_COLOR_BRAND_PRIMARY = 0xFFC700;
static constexpr uint32_t CS_COLOR_STATUS_SUCCESS = 0x22C55E;
static constexpr uint32_t CS_COLOR_CAMERA_ACTIVE = 0xEF4444;   // Red for REC
static constexpr uint32_t CS_COLOR_CAMERA_STANDBY = 0x6B7280;  // Gray for standby
static constexpr uint32_t CS_COLOR_PRESET_OCCUPIED = 0x3B82F6;  // Blue for occupied slot

// ============================================================================
// Layout Constants
// ============================================================================

static constexpr int32_t CS_GRID_GAP = 14;
static constexpr int32_t CS_GRID_MARGIN = 24;
static constexpr int32_t CS_STATUSBAR_HEIGHT = 66;
static constexpr int32_t CS_PARAM_ROW_HEIGHT = 110;
static constexpr int32_t CS_GLOBAL_ROW_HEIGHT = 50;

// Global param names (read-only row)
static constexpr const char* kGlobalParamNames[8] = {
    "BRI", "SPD", "MOOD", "FADE", "CPLX", "VAR", "HUE", "SAT"
};

// ============================================================================
// Helper: Create a card (matches DisplayUI make_card pattern)
// ============================================================================

static lv_obj_t* cs_make_card(lv_obj_t* parent, bool elevated) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
                              lv_color_hex(elevated ? CS_COLOR_BG_SURFACE_ELEVATED
                                                    : CS_COLOR_BG_SURFACE_BASE),
                              LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ControlSurfaceUI::ControlSurfaceUI(M5GFX& display) : _display(display) {
}

ControlSurfaceUI::~ControlSurfaceUI() {
}

// ============================================================================
// begin() - Create LVGL widgets
// ============================================================================

void ControlSurfaceUI::begin(lv_obj_t* parent) {
    if (!parent) return;

    // Set page background
    lv_obj_set_style_bg_color(parent, lv_color_hex(CS_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);

    // Content starts below header area (header is shared, managed by DisplayUI)
    int32_t contentTop = CS_STATUSBAR_HEIGHT + CS_GRID_GAP + 7;
    int32_t nextRowY = contentTop;

    // Grid column template (8 equal columns)
    static lv_coord_t col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };

    // ====================================================================
    // Row A: Effect Params 0-7 (encoders 0-7)
    // ====================================================================
    createParamRow(parent, _rowA_cards, _rowA_labels, _rowA_values, _rowA_bars,
                   _rowA_container, nextRowY, "A");
    nextRowY += CS_PARAM_ROW_HEIGHT + CS_GRID_GAP;

    // ====================================================================
    // Row B: Effect Params 8-13 + Camera Mode + Preset Bank (encoders 8-15)
    // ====================================================================
    createParamRow(parent, _rowB_cards, _rowB_labels, _rowB_values, _rowB_bars,
                   _rowB_container, nextRowY, "B");
    nextRowY += CS_PARAM_ROW_HEIGHT + CS_GRID_GAP;

    // Override cards 6 and 7 in Row B for Camera Mode and Preset Bank
    updateCameraCard();
    updatePresetCard();

    // ====================================================================
    // Global Params Row (read-only)
    // ====================================================================
    createGlobalRow(parent);

    // ====================================================================
    // Back Button
    // ====================================================================
    createBackButton(parent);

    // Initialize all param cards to empty state
    rebindEncoders();

    Serial.println("[ControlSurface] UI initialized");
}

// ============================================================================
// createParamRow() - Create a row of 8 gauge cards
// ============================================================================

void ControlSurfaceUI::createParamRow(lv_obj_t* parent,
                                       lv_obj_t** cards, lv_obj_t** labels,
                                       lv_obj_t** values, lv_obj_t** bars,
                                       lv_obj_t*& container,
                                       int32_t yPos, const char* rowLabel) {
    static lv_coord_t col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t row_dsc[2] = {CS_PARAM_ROW_HEIGHT, LV_GRID_TEMPLATE_LAST};

    container = lv_obj_create(parent);
    lv_obj_set_size(container, 1280 - 2 * CS_GRID_MARGIN, CS_PARAM_ROW_HEIGHT);
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, yPos);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_set_layout(container, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(container, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(container, CS_GRID_GAP, LV_PART_MAIN);

    for (uint8_t i = 0; i < 8; ++i) {
        cards[i] = cs_make_card(container, false);
        lv_obj_set_grid_cell(cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        // Parameter name label
        labels[i] = lv_label_create(cards[i]);
        lv_label_set_text(labels[i], "--");
        lv_obj_set_style_text_font(labels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(labels[i], lv_color_hex(CS_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_width(labels[i], LV_PCT(100));
        lv_label_set_long_mode(labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(labels[i], LV_ALIGN_TOP_MID, 0, -2);

        // Value label
        values[i] = lv_label_create(cards[i]);
        lv_label_set_text(values[i], "--");
        lv_obj_set_style_text_font(values[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(values[i], lv_color_hex(CS_COLOR_FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(values[i], LV_ALIGN_TOP_MID, 0, 22);

        // Progress bar
        bars[i] = lv_bar_create(cards[i]);
        lv_bar_set_range(bars[i], 0, 1000);  // 0-1000 for float precision
        lv_bar_set_value(bars[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(bars[i], LV_PCT(90), 8);
        lv_obj_align(bars[i], LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(CS_COLOR_BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(CS_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bars[i], 6, LV_PART_MAIN);
        lv_obj_set_style_radius(bars[i], 6, LV_PART_INDICATOR);
    }
}

// ============================================================================
// createGlobalRow() - Read-only global params display
// ============================================================================

void ControlSurfaceUI::createGlobalRow(lv_obj_t* parent) {
    static lv_coord_t col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t row_dsc[2] = {CS_GLOBAL_ROW_HEIGHT, LV_GRID_TEMPLATE_LAST};

    int32_t yPos = CS_STATUSBAR_HEIGHT + CS_GRID_GAP + 7 +
                   2 * (CS_PARAM_ROW_HEIGHT + CS_GRID_GAP);

    _globalRow_container = lv_obj_create(parent);
    lv_obj_set_size(_globalRow_container, 1280 - 2 * CS_GRID_MARGIN, CS_GLOBAL_ROW_HEIGHT);
    lv_obj_align(_globalRow_container, LV_ALIGN_TOP_MID, 0, yPos);
    lv_obj_set_style_bg_opa(_globalRow_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_globalRow_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_globalRow_container, 0, LV_PART_MAIN);
    lv_obj_set_layout(_globalRow_container, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(_globalRow_container, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(_globalRow_container, CS_GRID_GAP, LV_PART_MAIN);

    for (uint8_t i = 0; i < 8; ++i) {
        lv_obj_t* cell = lv_obj_create(_globalRow_container);
        lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_set_style_bg_color(cell, lv_color_hex(CS_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, lv_color_hex(CS_COLOR_BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_radius(cell, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, 4, LV_PART_MAIN);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        _globalRow_labels[i] = lv_label_create(cell);
        lv_label_set_text(_globalRow_labels[i], kGlobalParamNames[i]);
        lv_obj_set_style_text_font(_globalRow_labels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_globalRow_labels[i], lv_color_hex(CS_COLOR_FG_DIMMED), LV_PART_MAIN);
        lv_obj_align(_globalRow_labels[i], LV_ALIGN_LEFT_MID, 0, 0);

        _globalRow_values[i] = lv_label_create(cell);
        lv_label_set_text(_globalRow_values[i], "--");
        lv_obj_set_style_text_font(_globalRow_values[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_globalRow_values[i], lv_color_hex(CS_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_align(_globalRow_values[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

// ============================================================================
// createBackButton()
// ============================================================================

void ControlSurfaceUI::createBackButton(lv_obj_t* parent) {
    _backButton = lv_btn_create(parent);
    lv_obj_set_size(_backButton, 100, 40);
    lv_obj_set_pos(_backButton, 1280 - 110, CS_STATUSBAR_HEIGHT + CS_GRID_GAP + 7 +
                   2 * (CS_PARAM_ROW_HEIGHT + CS_GRID_GAP) + CS_GLOBAL_ROW_HEIGHT + 10);
    lv_obj_set_style_bg_color(_backButton, lv_color_hex(CS_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_width(_backButton, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_backButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(_backButton, 10, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_backButton);
    lv_label_set_text(label, "BACK");
    lv_obj_set_style_text_font(label, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(CS_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(_backButton, backButtonCb, LV_EVENT_CLICKED, this);
}

// ============================================================================
// Static Callbacks
// ============================================================================

void ControlSurfaceUI::backButtonCb(lv_event_t* e) {
    auto* self = static_cast<ControlSurfaceUI*>(lv_event_get_user_data(e));
    if (self && self->_backButtonCallback) {
        self->_backButtonCallback();
    }
}

// ============================================================================
// loop() - Throttled rendering updates
// ============================================================================

void ControlSurfaceUI::loop() {
    if (!_dirty) return;

    uint32_t now = millis();
    if (now - _lastRenderTime < FRAME_INTERVAL_MS) return;
    _lastRenderTime = now;
    _dirty = false;
}

// ============================================================================
// onScreenEnter() - Called when screen becomes active
// ============================================================================

void ControlSurfaceUI::onScreenEnter() {
    if (!_wsClient) return;

    // Request current effect's parameters
    uint16_t effectId = _wsClient->getCurrentEffectId();
    if (effectId > 0) {
        _wsClient->requestEffectParameters(effectId, "cs_enter");
    }

    // Request preset list
    _wsClient->requestEffectPresetsList();

    // Request camera mode state
    _wsClient->requestCameraModeGet();
}

// ============================================================================
// handleEncoderChange() - Route encoder rotation to param/camera/preset
// ============================================================================

void ControlSurfaceUI::handleEncoderChange(uint8_t encoderIndex, int32_t delta) {
    if (encoderIndex == CS_ENCODER_PRESET_BANK) {
        // Encoder 15: cycle preset slot selection
        int8_t newSlot = (int8_t)_selectedPresetSlot + (delta > 0 ? 1 : -1);
        if (newSlot < 0) newSlot = CS_MAX_PRESET_SLOTS - 1;
        if (newSlot >= CS_MAX_PRESET_SLOTS) newSlot = 0;
        _selectedPresetSlot = (uint8_t)newSlot;
        updatePresetCard();
        _dirty = true;
        return;
    }

    if (encoderIndex == CS_ENCODER_CAMERA_MODE) {
        // Encoder 14: rotation does nothing (use button for toggle)
        return;
    }

    // Encoders 0-13: map to effect parameters
    if (encoderIndex >= CS_MAX_PARAM_ENCODERS) return;
    if (encoderIndex >= _paramCount) return;
    if (!_params[encoderIndex].valid) return;

    float newValue = applyEncoderDelta(encoderIndex, delta);
    _params[encoderIndex].currentValue = newValue;

    // Update display immediately
    updateParamCard(encoderIndex);

    // Send to K1 via WebSocket
    if (_wsClient) {
        _wsClient->sendEffectParameterChange(
            encoderIndex, _currentEffectId,
            _params[encoderIndex].name, newValue);
    }

    _dirty = true;
}

// ============================================================================
// handleEncoderButton() - Button press routing
// ============================================================================

void ControlSurfaceUI::handleEncoderButton(uint8_t encoderIndex, bool isLongPress) {
    if (encoderIndex == CS_ENCODER_CAMERA_MODE) {
        // Toggle camera mode
        _cameraModeActive = !_cameraModeActive;
        if (_wsClient) {
            _wsClient->sendCameraModeSet(_cameraModeActive);
        }
        updateCameraCard();
        _dirty = true;
        return;
    }

    if (encoderIndex == CS_ENCODER_PRESET_BANK) {
        if (isLongPress) {
            // Long press: delete preset
            if (_wsClient) {
                _wsClient->sendEffectPresetDelete(_selectedPresetSlot);
                Serial.printf("[ControlSurface] Preset %u delete requested\n", _selectedPresetSlot);
            }
        } else {
            // Short press: load or save (load if occupied, save if empty)
            if (_presetSlots[_selectedPresetSlot].occupied) {
                // Load existing preset
                if (_wsClient) {
                    _wsClient->sendEffectPresetLoad(_selectedPresetSlot);
                    Serial.printf("[ControlSurface] Preset %u load requested\n", _selectedPresetSlot);
                }
            } else {
                // Save to empty slot
                char name[32];
                snprintf(name, sizeof(name), "STUDY_%04X_S%u", _currentEffectId, _selectedPresetSlot);
                if (_wsClient) {
                    _wsClient->sendEffectPresetSave(_selectedPresetSlot, name);
                    Serial.printf("[ControlSurface] Preset %u save requested: %s\n", _selectedPresetSlot, name);
                }
            }
        }
        return;
    }

    // Param encoder buttons: reset to default
    if (encoderIndex < CS_MAX_PARAM_ENCODERS && encoderIndex < _paramCount) {
        if (_params[encoderIndex].valid) {
            _params[encoderIndex].currentValue = _params[encoderIndex].defaultValue;
            updateParamCard(encoderIndex);
            if (_wsClient) {
                _wsClient->sendEffectParameterChange(
                    encoderIndex, _currentEffectId,
                    _params[encoderIndex].name, _params[encoderIndex].defaultValue);
            }
            _dirty = true;
        }
    }
}

// ============================================================================
// applyEncoderDelta() - Compute new param value from encoder rotation
// ============================================================================

float ControlSurfaceUI::applyEncoderDelta(uint8_t paramIndex, int32_t delta) {
    const CSEffectParam& p = _params[paramIndex];

    float newValue = p.currentValue;

    switch (p.type) {
        case CSParamType::BOOL:
            // Any delta toggles
            newValue = (p.currentValue < 0.5f) ? 1.0f : 0.0f;
            break;

        case CSParamType::INT:
        case CSParamType::ENUM:
            newValue = p.currentValue + (delta > 0 ? p.step : -p.step);
            newValue = roundf(newValue);
            break;

        case CSParamType::FLOAT:
        default:
            newValue = p.currentValue + (float)delta * p.step;
            break;
    }

    // Clamp
    if (newValue < p.minValue) newValue = p.minValue;
    if (newValue > p.maxValue) newValue = p.maxValue;

    return newValue;
}

// ============================================================================
// onEffectParametersReceived() - Update from K1 response
// ============================================================================

void ControlSurfaceUI::onEffectParametersReceived(uint16_t effectId, const char* effectName,
                                                    const CSEffectParam* params, uint8_t paramCount) {
    _currentEffectId = effectId;
    if (effectName) {
        strncpy(_currentEffectName, effectName, sizeof(_currentEffectName) - 1);
        _currentEffectName[sizeof(_currentEffectName) - 1] = '\0';
    }

    _paramCount = (paramCount > CS_MAX_EFFECT_PARAMS) ? CS_MAX_EFFECT_PARAMS : paramCount;

    for (uint8_t i = 0; i < CS_MAX_EFFECT_PARAMS; ++i) {
        if (i < _paramCount) {
            _params[i] = params[i];
            _params[i].valid = true;
        } else {
            _params[i] = CSEffectParam();  // Reset unused slots
        }
    }

    rebindEncoders();
    _dirty = true;

    Serial.printf("[ControlSurface] Effect 0x%04X '%s' loaded, %u params\n",
                  effectId, _currentEffectName, _paramCount);
}

// ============================================================================
// onPresetListReceived() - Update preset slot display
// ============================================================================

void ControlSurfaceUI::onPresetListReceived(const CSPresetSlot* slots, uint8_t slotCount) {
    uint8_t count = (slotCount > CS_MAX_PRESET_SLOTS) ? CS_MAX_PRESET_SLOTS : slotCount;
    for (uint8_t i = 0; i < CS_MAX_PRESET_SLOTS; ++i) {
        if (i < count) {
            _presetSlots[i] = slots[i];
        } else {
            _presetSlots[i] = CSPresetSlot();
        }
    }
    updatePresetCard();
    _dirty = true;
}

// ============================================================================
// setCameraMode() - Update camera mode state from K1 broadcast
// ============================================================================

void ControlSurfaceUI::setCameraMode(bool active) {
    _cameraModeActive = active;
    updateCameraCard();
    _dirty = true;
}

// ============================================================================
// updateGlobalParam() - Update global param value from status broadcast
// ============================================================================

void ControlSurfaceUI::updateGlobalParam(uint8_t index, uint8_t value) {
    if (index >= 8) return;
    if (!_globalRow_values[index]) return;
    lv_label_set_text_fmt(_globalRow_values[index], "%u", value);
}

// ============================================================================
// rebindEncoders() - Update all param cards after effect change
// ============================================================================

void ControlSurfaceUI::rebindEncoders() {
    for (uint8_t i = 0; i < 8; ++i) {
        // Row A: encoders 0-7
        updateParamCard(i);
        // Row B: encoders 8-13 (skip 14=camera, 15=preset)
        if (i < 6) {
            updateParamCard(8 + i);
        }
    }
    updateCameraCard();
    updatePresetCard();
}

// ============================================================================
// updateParamCard() - Update a single param gauge card
// ============================================================================

void ControlSurfaceUI::updateParamCard(uint8_t encoderIndex) {
    lv_obj_t** cards;
    lv_obj_t** labels;
    lv_obj_t** values;
    lv_obj_t** bars;
    uint8_t cardIndex;

    if (encoderIndex < 8) {
        cards = _rowA_cards;
        labels = _rowA_labels;
        values = _rowA_values;
        bars = _rowA_bars;
        cardIndex = encoderIndex;
    } else if (encoderIndex < 14) {
        cards = _rowB_cards;
        labels = _rowB_labels;
        values = _rowB_values;
        bars = _rowB_bars;
        cardIndex = encoderIndex - 8;
    } else {
        return;  // 14 and 15 are camera/preset
    }

    if (!cards[cardIndex] || !labels[cardIndex] || !values[cardIndex] || !bars[cardIndex]) return;

    if (encoderIndex >= _paramCount || !_params[encoderIndex].valid) {
        // Unused slot: dim appearance
        lv_label_set_text(labels[cardIndex], "--");
        lv_label_set_text(values[cardIndex], "--");
        lv_bar_set_value(bars[cardIndex], 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(labels[cardIndex], lv_color_hex(CS_COLOR_FG_DIMMED), LV_PART_MAIN);
        lv_obj_set_style_text_color(values[cardIndex], lv_color_hex(CS_COLOR_FG_DIMMED), LV_PART_MAIN);
        lv_obj_set_style_border_color(cards[cardIndex], lv_color_hex(CS_COLOR_BORDER_BASE), LV_PART_MAIN);
        return;
    }

    const CSEffectParam& p = _params[encoderIndex];

    // Update label with displayName (or name as fallback)
    const char* displayLabel = p.displayName[0] ? p.displayName : p.name;
    lv_label_set_text(labels[cardIndex], displayLabel);
    lv_obj_set_style_text_color(labels[cardIndex], lv_color_hex(CS_COLOR_FG_SECONDARY), LV_PART_MAIN);

    // Format value
    char buf[24];
    formatParamValue(buf, sizeof(buf), p);
    lv_label_set_text(values[cardIndex], buf);
    lv_obj_set_style_text_color(values[cardIndex], lv_color_hex(CS_COLOR_FG_PRIMARY), LV_PART_MAIN);

    // Update bar (normalized 0-1000)
    float range = p.maxValue - p.minValue;
    int32_t barValue = 0;
    if (range > 0.0f) {
        barValue = (int32_t)(((p.currentValue - p.minValue) / range) * 1000.0f);
        if (barValue < 0) barValue = 0;
        if (barValue > 1000) barValue = 1000;
    }
    lv_bar_set_value(bars[cardIndex], barValue, LV_ANIM_OFF);

    // Active border
    lv_obj_set_style_border_color(cards[cardIndex], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
}

// ============================================================================
// formatParamValue() - Format value with unit for display
// ============================================================================

void ControlSurfaceUI::formatParamValue(char* buf, size_t bufSize, const CSEffectParam& param) {
    switch (param.type) {
        case CSParamType::BOOL:
            snprintf(buf, bufSize, "%s", param.currentValue >= 0.5f ? "ON" : "OFF");
            break;
        case CSParamType::INT:
        case CSParamType::ENUM:
            if (param.unit[0]) {
                snprintf(buf, bufSize, "%d%s", (int)param.currentValue, param.unit);
            } else {
                snprintf(buf, bufSize, "%d", (int)param.currentValue);
            }
            break;
        case CSParamType::FLOAT:
        default:
            if (param.unit[0]) {
                snprintf(buf, bufSize, "%.1f%s", param.currentValue, param.unit);
            } else {
                snprintf(buf, bufSize, "%.2f", param.currentValue);
            }
            break;
    }
}

// ============================================================================
// updateCameraCard() - Camera Mode indicator (encoder 14 = Row B slot 6)
// ============================================================================

void ControlSurfaceUI::updateCameraCard() {
    if (!_rowB_labels[6] || !_rowB_values[6] || !_rowB_cards[6] || !_rowB_bars[6]) return;

    lv_label_set_text(_rowB_labels[6], "CAMERA");
    lv_obj_set_style_text_color(_rowB_labels[6], lv_color_hex(CS_COLOR_FG_SECONDARY), LV_PART_MAIN);

    if (_cameraModeActive) {
        lv_label_set_text(_rowB_values[6], "REC");
        lv_obj_set_style_text_color(_rowB_values[6], lv_color_hex(CS_COLOR_CAMERA_ACTIVE), LV_PART_MAIN);
        lv_obj_set_style_border_color(_rowB_cards[6], lv_color_hex(CS_COLOR_CAMERA_ACTIVE), LV_PART_MAIN);
        lv_bar_set_value(_rowB_bars[6], 1000, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_rowB_bars[6], lv_color_hex(CS_COLOR_CAMERA_ACTIVE), LV_PART_INDICATOR);
    } else {
        lv_label_set_text(_rowB_values[6], "STANDBY");
        lv_obj_set_style_text_color(_rowB_values[6], lv_color_hex(CS_COLOR_CAMERA_STANDBY), LV_PART_MAIN);
        lv_obj_set_style_border_color(_rowB_cards[6], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_bar_set_value(_rowB_bars[6], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_rowB_bars[6], lv_color_hex(CS_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
    }
}

// ============================================================================
// updatePresetCard() - Preset Bank display (encoder 15 = Row B slot 7)
// ============================================================================

void ControlSurfaceUI::updatePresetCard() {
    if (!_rowB_labels[7] || !_rowB_values[7] || !_rowB_cards[7] || !_rowB_bars[7]) return;

    lv_label_set_text(_rowB_labels[7], "PRESET");
    lv_obj_set_style_text_color(_rowB_labels[7], lv_color_hex(CS_COLOR_FG_SECONDARY), LV_PART_MAIN);

    char buf[16];
    if (_presetSlots[_selectedPresetSlot].occupied) {
        snprintf(buf, sizeof(buf), "S%u [*]", _selectedPresetSlot + 1);
        lv_obj_set_style_border_color(_rowB_cards[7], lv_color_hex(CS_COLOR_PRESET_OCCUPIED), LV_PART_MAIN);
    } else {
        snprintf(buf, sizeof(buf), "S%u [_]", _selectedPresetSlot + 1);
        lv_obj_set_style_border_color(_rowB_cards[7], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }
    lv_label_set_text(_rowB_values[7], buf);
    lv_obj_set_style_text_color(_rowB_values[7], lv_color_hex(CS_COLOR_FG_PRIMARY), LV_PART_MAIN);

    // Bar shows slot position (0-7 mapped to 0-1000)
    int32_t barVal = (_selectedPresetSlot * 1000) / (CS_MAX_PRESET_SLOTS - 1);
    lv_bar_set_value(_rowB_bars[7], barVal, LV_ANIM_OFF);
}

#endif // TAB5_ENCODER_USE_LVGL && !SIMULATOR_BUILD
