/**
 * @file DemoModeUI.cpp
 * @brief Interactive Demo Mode Screen implementation
 *
 * Phase 1: Shell implementation with navigation
 * - Creates screen with status bar and back button
 * - Placeholder lanes for scene cards, feel controls, palette picker
 */

#include "DemoModeUI.h"
#include "../network/WebSocketClient.h"
#include "../demo/DemoScenes.h"
#include <Arduino.h>

// Static instance pointer for callbacks
static DemoModeUI* s_instance = nullptr;

// =============================================================================
// Constructor / Destructor
// =============================================================================

DemoModeUI::DemoModeUI(M5GFX& display)
    : _display(display)
{
    s_instance = this;
}

DemoModeUI::~DemoModeUI() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

// =============================================================================
// Initialization
// =============================================================================

void DemoModeUI::begin(lv_obj_t* parent) {
    // Create screen if no parent provided
    if (parent == nullptr) {
        _screen = lv_obj_create(nullptr);
        lv_obj_set_style_bg_color(_screen, lv_color_hex(Theme::LVGL::COLOR_BG), 0);
    } else {
        _screen = parent;
    }

    createUI(_screen);
    _dirty = true;
}

void DemoModeUI::createUI(lv_obj_t* parent) {
    // Main container (full screen)
    lv_obj_t* mainContainer = lv_obj_create(parent);
    lv_obj_set_size(mainContainer, Theme::SCREEN_W, Theme::SCREEN_H);
    lv_obj_set_pos(mainContainer, 0, 0);
    lv_obj_set_style_bg_color(mainContainer, lv_color_hex(Theme::LVGL::COLOR_BG), 0);
    lv_obj_set_style_border_width(mainContainer, 0, 0);
    lv_obj_set_style_pad_all(mainContainer, 0, 0);
    lv_obj_clear_flag(mainContainer, LV_OBJ_FLAG_SCROLLABLE);

    // Create UI components
    createStatusBar(mainContainer);
    createLaneA(mainContainer);
    createLaneB(mainContainer);
    createLaneC(mainContainer);
}

// =============================================================================
// Status Bar (80px height)
// =============================================================================

void DemoModeUI::createStatusBar(lv_obj_t* parent) {
    _statusBar = lv_obj_create(parent);
    lv_obj_set_size(_statusBar, Theme::SCREEN_W, Theme::STATUS_BAR_H);
    lv_obj_set_pos(_statusBar, 0, 0);
    lv_obj_set_style_bg_color(_statusBar, lv_color_hex(Theme::LVGL::COLOR_HEADER_BG), 0);
    lv_obj_set_style_border_width(_statusBar, 0, 0);
    lv_obj_set_style_pad_all(_statusBar, 8, 0);
    lv_obj_clear_flag(_statusBar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(_statusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_statusBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left section: Back button + Title
    lv_obj_t* leftSection = lv_obj_create(_statusBar);
    lv_obj_set_size(leftSection, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(leftSection, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftSection, 0, 0);
    lv_obj_set_style_pad_all(leftSection, 0, 0);
    lv_obj_set_flex_flow(leftSection, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftSection, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(leftSection, 16, 0);

    // Back button
    lv_obj_t* backBtn = lv_btn_create(leftSection);
    lv_obj_set_size(backBtn, 80, 48);
    lv_obj_set_style_bg_color(backBtn, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(backBtn, 8, 0);
    lv_obj_add_event_cb(backBtn, backButtonCb, LV_EVENT_CLICKED, this);

    lv_obj_t* backLabel = lv_label_create(backBtn);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(backLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(backLabel);

    // Title
    lv_obj_t* title = lv_label_create(leftSection);
    lv_label_set_text(title, "DEMO MODE");
    lv_obj_set_style_text_color(title, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // Connection dot
    _connDot = lv_obj_create(_statusBar);
    lv_obj_set_size(_connDot, 16, 16);
    lv_obj_set_style_radius(_connDot, 8, 0);  // Circle
    lv_obj_set_style_bg_color(_connDot, lv_color_hex(0x808080), 0);  // Gray (disconnected)
    lv_obj_set_style_border_width(_connDot, 0, 0);

    // Sync quality label
    _syncLabel = lv_label_create(_statusBar);
    lv_label_set_text(_syncLabel, "Sync: --");
    lv_obj_set_style_text_color(_syncLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(_syncLabel, &lv_font_montserrat_14, 0);

    // Last applied label
    _lastAppliedLabel = lv_label_create(_statusBar);
    lv_label_set_text(_lastAppliedLabel, "Ready");
    lv_obj_set_style_text_color(_lastAppliedLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(_lastAppliedLabel, &lv_font_montserrat_16, 0);

    // FPS badge
    _fpsLabel = lv_label_create(_statusBar);
    lv_label_set_text(_fpsLabel, "-- FPS");
    lv_obj_set_style_text_color(_fpsLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(_fpsLabel, &lv_font_montserrat_14, 0);

    // Reset button
    _resetButton = lv_btn_create(_statusBar);
    lv_obj_set_size(_resetButton, 120, 48);
    lv_obj_set_style_bg_color(_resetButton, lv_color_hex(Theme::LVGL::COLOR_ACCENT), 0);
    lv_obj_set_style_radius(_resetButton, 8, 0);
    lv_obj_add_event_cb(_resetButton, resetButtonCb, LV_EVENT_CLICKED, this);

    lv_obj_t* resetLabel = lv_label_create(_resetButton);
    lv_label_set_text(resetLabel, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_color(resetLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(resetLabel);
}

// =============================================================================
// Lane A: Scene Cards (280px height)
// =============================================================================

void DemoModeUI::createLaneA(lv_obj_t* parent) {
    _laneA = lv_obj_create(parent);
    lv_obj_set_size(_laneA, Theme::SCREEN_W - 32, 280);
    lv_obj_set_pos(_laneA, 16, Theme::STATUS_BAR_H + 8);
    lv_obj_set_style_bg_color(_laneA, lv_color_hex(Theme::LVGL::COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(_laneA, 1, 0);
    lv_obj_set_style_border_color(_laneA, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(_laneA, 12, 0);
    lv_obj_set_style_pad_all(_laneA, 16, 0);
    lv_obj_set_flex_flow(_laneA, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_laneA, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_laneA, 12, 0);
    lv_obj_set_scroll_dir(_laneA, LV_DIR_HOR);

    // Create 8 scene cards
    for (uint8_t i = 0; i < demo::DEMO_SCENE_COUNT; i++) {
        _sceneCards[i] = createSceneCard(_laneA, i);
    }

    // Watch Demo button
    _watchDemoButton = lv_btn_create(_laneA);
    lv_obj_set_size(_watchDemoButton, 140, 200);
    lv_obj_set_style_bg_color(_watchDemoButton, lv_color_hex(Theme::LVGL::COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(_watchDemoButton, 12, 0);
    lv_obj_add_event_cb(_watchDemoButton, watchDemoButtonCb, LV_EVENT_CLICKED, this);

    lv_obj_t* watchContainer = lv_obj_create(_watchDemoButton);
    lv_obj_set_size(watchContainer, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(watchContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(watchContainer, 0, 0);
    lv_obj_set_flex_flow(watchContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(watchContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(watchContainer);

    lv_obj_t* playIcon = lv_label_create(watchContainer);
    lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(playIcon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(playIcon, &lv_font_montserrat_48, 0);

    lv_obj_t* watchLabel = lv_label_create(watchContainer);
    lv_label_set_text(watchLabel, "WATCH\nDEMO");
    lv_obj_set_style_text_color(watchLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(watchLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(watchLabel, LV_TEXT_ALIGN_CENTER, 0);
}

lv_obj_t* DemoModeUI::createSceneCard(lv_obj_t* parent, uint8_t index) {
    const demo::DemoScene& scene = demo::DEMO_SCENES[index];

    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_set_size(card, 140, 200);
    lv_obj_set_style_bg_color(card, lv_color_hex(Theme::LVGL::COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_add_event_cb(card, sceneCardTouchCb, LV_EVENT_CLICKED, this);

    // Store index in user data
    lv_obj_set_user_data(card, (void*)(uintptr_t)index);

    // Card content container
    lv_obj_t* content = lv_obj_create(card);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);

    // Scene name
    lv_obj_t* nameLabel = lv_label_create(content);
    lv_label_set_text(nameLabel, scene.name);
    lv_obj_set_style_text_color(nameLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(nameLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(nameLabel, 120);

    // Effect name (smaller)
    lv_obj_t* effectLabel = lv_label_create(content);
    lv_label_set_text(effectLabel, scene.effectName);
    lv_obj_set_style_text_color(effectLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(effectLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(effectLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(effectLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(effectLabel, 120);

    return card;
}

// =============================================================================
// Lane B: Feel Controls (200px height)
// =============================================================================

void DemoModeUI::createLaneB(lv_obj_t* parent) {
    _laneB = lv_obj_create(parent);
    lv_obj_set_size(_laneB, Theme::SCREEN_W - 32, 180);
    lv_obj_set_pos(_laneB, 16, Theme::STATUS_BAR_H + 8 + 280 + 8);
    lv_obj_set_style_bg_color(_laneB, lv_color_hex(Theme::LVGL::COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(_laneB, 1, 0);
    lv_obj_set_style_border_color(_laneB, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(_laneB, 12, 0);
    lv_obj_set_style_pad_all(_laneB, 16, 0);
    lv_obj_set_flex_flow(_laneB, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_laneB, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_laneB, LV_OBJ_FLAG_SCROLLABLE);

    // Create 3 feel sliders
    _energySlider = createFeelSlider(_laneB, "ENERGY", _currentEnergy);
    _flowSlider = createFeelSlider(_laneB, "FLOW", _currentFlow);
    _brightnessSlider = createFeelSlider(_laneB, "BRIGHTNESS", _currentBrightness);

    // Add specific callbacks
    lv_obj_add_event_cb(_energySlider, energySliderCb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(_flowSlider, flowSliderCb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(_brightnessSlider, brightnessSliderCb, LV_EVENT_VALUE_CHANGED, this);
}

lv_obj_t* DemoModeUI::createFeelSlider(lv_obj_t* parent, const char* label, uint8_t initialValue) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, 350, 140);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container, 8, 0);

    // Label
    lv_obj_t* titleLabel = lv_label_create(container);
    lv_label_set_text(titleLabel, label);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_18, 0);

    // Slider
    lv_obj_t* slider = lv_slider_create(container);
    lv_obj_set_size(slider, 300, 40);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, initialValue, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x3A3A50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(Theme::LVGL::COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);

    // Value label
    lv_obj_t* valueLabel = lv_label_create(container);
    char valueBuf[8];
    snprintf(valueBuf, sizeof(valueBuf), "%d%%", initialValue);
    lv_label_set_text(valueLabel, valueBuf);
    lv_obj_set_style_text_color(valueLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(valueLabel, &lv_font_montserrat_14, 0);

    // Store value label in slider user data for updates
    lv_obj_set_user_data(slider, valueLabel);

    return slider;
}

// =============================================================================
// Lane C: Palette Picker (160px height)
// =============================================================================

void DemoModeUI::createLaneC(lv_obj_t* parent) {
    _laneC = lv_obj_create(parent);
    lv_obj_set_size(_laneC, Theme::SCREEN_W - 32, 140);
    lv_obj_set_pos(_laneC, 16, Theme::STATUS_BAR_H + 8 + 280 + 8 + 180 + 8);
    lv_obj_set_style_bg_color(_laneC, lv_color_hex(Theme::LVGL::COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(_laneC, 1, 0);
    lv_obj_set_style_border_color(_laneC, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(_laneC, 12, 0);
    lv_obj_set_style_pad_all(_laneC, 12, 0);
    lv_obj_set_flex_flow(_laneC, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_laneC, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_laneC, LV_OBJ_FLAG_SCROLLABLE);

    // Create 8 palette chips
    for (uint8_t i = 0; i < demo::PALETTE_GROUP_COUNT; i++) {
        _paletteChips[i] = createPaletteChip(_laneC, i);
    }

    // Saturation slider container
    lv_obj_t* satContainer = lv_obj_create(_laneC);
    lv_obj_set_size(satContainer, 200, 100);
    lv_obj_set_style_bg_opa(satContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(satContainer, 0, 0);
    lv_obj_set_flex_flow(satContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(satContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(satContainer, 4, 0);

    lv_obj_t* satLabel = lv_label_create(satContainer);
    lv_label_set_text(satLabel, "SATURATION");
    lv_obj_set_style_text_color(satLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(satLabel, &lv_font_montserrat_14, 0);

    _saturationSlider = lv_slider_create(satContainer);
    lv_obj_set_size(_saturationSlider, 160, 30);
    lv_slider_set_range(_saturationSlider, 0, 100);
    lv_slider_set_value(_saturationSlider, _currentSaturation, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_saturationSlider, lv_color_hex(0x3A3A50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_saturationSlider, lv_color_hex(Theme::LVGL::COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_saturationSlider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(_saturationSlider, saturationSliderCb, LV_EVENT_VALUE_CHANGED, this);
}

lv_obj_t* DemoModeUI::createPaletteChip(lv_obj_t* parent, uint8_t index) {
    const demo::PaletteGroup& group = demo::PALETTE_GROUPS[index];

    lv_obj_t* chip = lv_btn_create(parent);
    lv_obj_set_size(chip, 100, 80);
    lv_obj_set_style_bg_color(chip, lv_color_hex(Theme::LVGL::COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(chip, 2, 0);
    lv_obj_set_style_border_color(chip, lv_color_hex(0x3A3A50), 0);
    lv_obj_set_style_radius(chip, 8, 0);
    lv_obj_add_event_cb(chip, paletteChipTouchCb, LV_EVENT_CLICKED, this);

    // Store index in user data
    lv_obj_set_user_data(chip, (void*)(uintptr_t)index);

    lv_obj_t* nameLabel = lv_label_create(chip);
    lv_label_set_text(nameLabel, group.name);
    lv_obj_set_style_text_color(nameLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(nameLabel);

    return chip;
}

// =============================================================================
// Main Loop
// =============================================================================

void DemoModeUI::loop() {
    uint32_t now = millis();

    // Update Watch Demo playback
    if (_watchDemoPlaying) {
        updateWatchDemo();
    }

    // Check ACK timeout
    if (_status.applyState == ApplyState::APPLYING) {
        if (now - _status.commandSent_ms > ACK_TIMEOUT_MS) {
            _status.applyState = ApplyState::TIMEOUT;
            updateStatusBar();
        }
    }

    // Rate-limited rendering
    if (_dirty && (now - _lastRenderTime >= FRAME_INTERVAL_MS)) {
        updateStatusBar();
        _dirty = false;
        _lastRenderTime = now;
    }
}

// =============================================================================
// Status Bar Updates
// =============================================================================

void DemoModeUI::updateStatusBar() {
    if (!_connDot || !_syncLabel || !_lastAppliedLabel || !_fpsLabel) return;

    // Connection dot
    uint32_t connColor = _status.connected ? 0x22DD88 : 0xFF4444;  // Green or Red
    lv_obj_set_style_bg_color(_connDot, lv_color_hex(connColor), 0);

    // Sync quality
    const char* syncText = "Sync: --";
    if (_status.connected) {
        switch (_status.syncQuality) {
            case 1:  syncText = "Sync: Good"; break;
            case 0:  syncText = "Sync: OK"; break;
            case -1: syncText = "Sync: Poor"; break;
        }
    }
    lv_label_set_text(_syncLabel, syncText);

    // Last applied (ACK-based)
    switch (_status.applyState) {
        case ApplyState::APPLYING:
            if (_status.pendingName) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Applying %s...", _status.pendingName);
                lv_label_set_text(_lastAppliedLabel, buf);
            }
            lv_obj_set_style_text_color(_lastAppliedLabel, lv_color_hex(Theme::LVGL::COLOR_WARNING), 0);
            break;

        case ApplyState::TIMEOUT:
            lv_label_set_text(_lastAppliedLabel, "Not responding");
            lv_obj_set_style_text_color(_lastAppliedLabel, lv_color_hex(Theme::LVGL::COLOR_ERROR), 0);
            break;

        case ApplyState::IDLE:
        default:
            if (_status.appliedName) {
                uint32_t elapsed = (millis() - _status.lastAcked_ms) / 1000;
                char buf[64];
                snprintf(buf, sizeof(buf), "%s applied %lus ago", _status.appliedName, elapsed);
                lv_label_set_text(_lastAppliedLabel, buf);
            } else {
                lv_label_set_text(_lastAppliedLabel, "Ready");
            }
            lv_obj_set_style_text_color(_lastAppliedLabel, lv_color_hex(Theme::LVGL::COLOR_TEXT_PRIMARY), 0);
            break;
    }

    // FPS
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%d FPS", _status.fps);
    lv_label_set_text(_fpsLabel, fpsBuf);
}

void DemoModeUI::updateConnectionState(bool connected, uint8_t fps, int8_t syncQuality) {
    _status.connected = connected;
    _status.fps = fps;
    _status.syncQuality = syncQuality;
    _dirty = true;
}

void DemoModeUI::onNodeAck(const char* sceneName) {
    _status.applyState = ApplyState::IDLE;
    _status.lastAcked_ms = millis();
    _status.appliedName = sceneName;
    _dirty = true;
}

// =============================================================================
// Scene Actions
// =============================================================================

void DemoModeUI::applyScene(uint8_t sceneIndex) {
    if (sceneIndex >= demo::DEMO_SCENE_COUNT) return;

    const demo::DemoScene& scene = demo::DEMO_SCENES[sceneIndex];

    // UI feedback immediately
    _status.applyState = ApplyState::APPLYING;
    _status.commandSent_ms = millis();
    _status.pendingName = scene.name;
    _selectedSceneIndex = sceneIndex;

    // Highlight selected card
    for (uint8_t i = 0; i < demo::DEMO_SCENE_COUNT; i++) {
        if (_sceneCards[i]) {
            uint32_t borderColor = (i == sceneIndex) ? Theme::LVGL::COLOR_ACCENT : 0x3A3A50;
            lv_obj_set_style_border_color(_sceneCards[i], lv_color_hex(borderColor), 0);
        }
    }

    // Send to node
    sendSceneBundle(scene);
    _dirty = true;
}

void DemoModeUI::resetToSignature() {
    applyScene(demo::SIGNATURE_SCENE_INDEX);
}

void DemoModeUI::toggleWatchDemo() {
    _watchDemoPlaying = !_watchDemoPlaying;

    if (_watchDemoPlaying) {
        _watchDemoStep = 0;
        _watchDemoStepStart = millis();
        applyScene(demo::WATCH_DEMO_SEQUENCE[0].sceneIndex);

        // Update button appearance
        if (_watchDemoButton) {
            lv_obj_set_style_bg_color(_watchDemoButton, lv_color_hex(Theme::LVGL::COLOR_ERROR), 0);
        }
    } else {
        // Update button appearance
        if (_watchDemoButton) {
            lv_obj_set_style_bg_color(_watchDemoButton, lv_color_hex(Theme::LVGL::COLOR_SUCCESS), 0);
        }
    }
}

void DemoModeUI::updateWatchDemo() {
    uint32_t now = millis();
    const auto& currentStep = demo::WATCH_DEMO_SEQUENCE[_watchDemoStep];

    if (now - _watchDemoStepStart >= currentStep.durationMs) {
        _watchDemoStep++;

        if (_watchDemoStep >= demo::WATCH_DEMO_STEP_COUNT) {
            // Demo complete
            _watchDemoPlaying = false;
            if (_watchDemoButton) {
                lv_obj_set_style_bg_color(_watchDemoButton, lv_color_hex(Theme::LVGL::COLOR_SUCCESS), 0);
            }
            return;
        }

        // Start next step
        _watchDemoStepStart = now;
        applyScene(demo::WATCH_DEMO_SEQUENCE[_watchDemoStep].sceneIndex);
    }
}

// =============================================================================
// Feel Controls
// =============================================================================

void DemoModeUI::setEnergy(uint8_t energy) {
    _currentEnergy = energy;

    // Map to underlying parameters
    uint8_t speed = map(energy, 0, 100, 15, 85);
    uint8_t intensity = map(energy, 0, 100, 80, 255);
    uint8_t complexity = map(energy, 0, 100, 50, 200);

    sendParameter("speed", speed);
    sendParameter("intensity", intensity);
    sendParameter("complexity", complexity);
}

void DemoModeUI::setFlow(uint8_t flow) {
    _currentFlow = flow;

    // Map to underlying parameters
    uint8_t mood = map(flow, 0, 100, 0, 200);
    uint8_t variation = map(flow, 0, 100, 20, 180);
    uint8_t fade = map(flow, 0, 100, 100, 220);

    sendParameter("hue", mood);
    sendParameter("variation", variation);
    sendParameter("fadeAmount", fade);
}

void DemoModeUI::setBrightness(uint8_t brightness) {
    _currentBrightness = brightness;

    // Direct mapping with floor at 30
    uint8_t ledBrightness = map(brightness, 0, 100, 30, 255);
    sendParameter("brightness", ledBrightness);
}

// =============================================================================
// Palette Controls
// =============================================================================

void DemoModeUI::applyPaletteGroup(uint8_t groupIndex) {
    if (groupIndex >= demo::PALETTE_GROUP_COUNT) return;

    const demo::PaletteGroup& group = demo::PALETTE_GROUPS[groupIndex];
    _selectedPaletteIndex = groupIndex;

    // Highlight selected chip
    for (uint8_t i = 0; i < demo::PALETTE_GROUP_COUNT; i++) {
        if (_paletteChips[i]) {
            uint32_t borderColor = (i == groupIndex) ? Theme::LVGL::COLOR_ACCENT : 0x3A3A50;
            lv_obj_set_style_border_color(_paletteChips[i], lv_color_hex(borderColor), 0);
        }
    }

    sendParameter("paletteId", group.paletteId);
}

void DemoModeUI::setSaturation(uint8_t saturation) {
    _currentSaturation = saturation;
    uint8_t paletteSat = map(saturation, 0, 100, 100, 255);
    sendParameter("saturation", paletteSat);
}

// =============================================================================
// WebSocket Communication
// =============================================================================

void DemoModeUI::sendParameter(const char* key, uint8_t value) {
    if (!_wsClient) return;

    // Rate limit slider updates
    uint32_t now = millis();
    if (now - _lastSliderSend < SLIDER_UPDATE_INTERVAL_MS) {
        return;  // Skip this update
    }
    _lastSliderSend = now;

    // Send via WebSocket (fire-and-forget pattern for sliders)
    // Actual implementation would use _wsClient->sendParameter(key, value)
    Serial.printf("[DemoModeUI] Send %s=%d\n", key, value);
}

void DemoModeUI::sendSceneBundle(const demo::DemoScene& scene) {
    if (!_wsClient) return;

    // Send effect and palette as bundle
    Serial.printf("[DemoModeUI] Apply scene: %s (effect=%d, palette=%d)\n",
                  scene.name, scene.effectId, scene.paletteId);

    // Actual implementation would send via WebSocket:
    // _wsClient->sendCommand("setEffect", scene.effectId);
    // _wsClient->sendCommand("setPalette", scene.paletteId);
    // Apply feel parameters
    setEnergy(scene.energy);
    setFlow(scene.flow);
    setBrightness(scene.brightness);
    setSaturation(scene.saturation);
}

// =============================================================================
// LVGL Callbacks
// =============================================================================

void DemoModeUI::sceneCardTouchCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* card = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t index = (uint8_t)(uintptr_t)lv_obj_get_user_data(card);

    if (self) {
        self->applyScene(index);
    }
}

void DemoModeUI::watchDemoButtonCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    if (self) {
        self->toggleWatchDemo();
    }
}

void DemoModeUI::resetButtonCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    if (self) {
        self->resetToSignature();
    }
}

void DemoModeUI::energySliderCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_bar_get_value(slider);  // LVGL 9.x: slider inherits from bar

    // Update value label
    lv_obj_t* valueLabel = static_cast<lv_obj_t*>(lv_obj_get_user_data(slider));
    if (valueLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%ld%%", value);
        lv_label_set_text(valueLabel, buf);
    }

    if (self) {
        self->setEnergy((uint8_t)value);
    }
}

void DemoModeUI::flowSliderCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_bar_get_value(slider);  // LVGL 9.x: slider inherits from bar

    // Update value label
    lv_obj_t* valueLabel = static_cast<lv_obj_t*>(lv_obj_get_user_data(slider));
    if (valueLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%ld%%", value);
        lv_label_set_text(valueLabel, buf);
    }

    if (self) {
        self->setFlow((uint8_t)value);
    }
}

void DemoModeUI::brightnessSliderCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_bar_get_value(slider);  // LVGL 9.x: slider inherits from bar

    // Update value label
    lv_obj_t* valueLabel = static_cast<lv_obj_t*>(lv_obj_get_user_data(slider));
    if (valueLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%ld%%", value);
        lv_label_set_text(valueLabel, buf);
    }

    if (self) {
        self->setBrightness((uint8_t)value);
    }
}

void DemoModeUI::paletteChipTouchCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* chip = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t index = (uint8_t)(uintptr_t)lv_obj_get_user_data(chip);

    if (self) {
        self->applyPaletteGroup(index);
    }
}

void DemoModeUI::saturationSliderCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int32_t value = lv_bar_get_value(slider);  // LVGL 9.x: slider inherits from bar

    if (self) {
        self->setSaturation((uint8_t)value);
    }
}

void DemoModeUI::backButtonCb(lv_event_t* e) {
    DemoModeUI* self = static_cast<DemoModeUI*>(lv_event_get_user_data(e));
    if (self && self->_backButtonCallback) {
        self->_backButtonCallback();
    }
}
