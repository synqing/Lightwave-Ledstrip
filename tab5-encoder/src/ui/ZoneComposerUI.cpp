// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// ZoneComposerUI v2 — LED-Centric Zone Composer
// ============================================================================
// Hero LED strip visualiser + 5-column param grid + 3-column zone overview.
// Centre-origin topology: LEDs 79/80 outward. Max 3 zones (1-indexed UI).
// Spec: tab5-encoder/docs/ZONE_COMPOSER_V2_SPEC.md
// ============================================================================

#include "ZoneComposerUI.h"

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "fonts/bebas_neue_fonts.h"
#include "fonts/experimental_fonts.h"
#include "../network/WebSocketClient.h"
#include "../utils/NameLookup.h"
#include "DesignTokens.h"

// ============================================================================
// Layout Constants
// ============================================================================

static constexpr int32_t CONTENT_PAD     = 20;
static constexpr int32_t CONTENT_WIDTH   = 1240;   // 1280 - 2 * 20

// Vertical budget (from spec)
static constexpr int32_t HEADER_Y        = 10;
static constexpr int32_t HEADER_H        = 50;
static constexpr int32_t MODE_ROW_Y      = 64;
static constexpr int32_t MODE_ROW_H      = 36;
static constexpr int32_t STRIP_Y         = 108;
static constexpr int32_t STRIP_H         = 130;
static constexpr int32_t INDICATOR_Y     = 244;
static constexpr int32_t INDICATOR_H     = 36;
static constexpr int32_t PARAM_GRID_Y    = 284;
static constexpr int32_t PARAM_GRID_H    = 160;
static constexpr int32_t OVERVIEW_GRID_Y = 458;
static constexpr int32_t OVERVIEW_GRID_H = 170;
static constexpr int32_t FOOTER_Y        = 638;
static constexpr int32_t FOOTER_H        = 38;

// Strip geometry
static constexpr int32_t STRIP_CONTAINER_X = 40;
static constexpr int32_t STRIP_CONTAINER_W = 1200;
static constexpr int32_t STRIP_BAR_LOCAL_X = 20;   // Within strip container
static constexpr int32_t STRIP_BAR_H       = 60;
static constexpr int32_t STRIP_LABEL_H     = 24;
static constexpr int32_t STRIP_TICK_H      = 24;

// Centre marker pixel position: LED 79.5 / 160 * 1160 = 576
static constexpr int32_t CENTRE_MARKER_X = 576;

// Encoder index offsets (global indices 8-15 → local 0-7)
static constexpr uint8_t ENC_BASE       = 8;
static constexpr uint8_t ENC_EFFECT     = 0;
static constexpr uint8_t ENC_PALETTE    = 1;
static constexpr uint8_t ENC_SPEED      = 2;
static constexpr uint8_t ENC_BRIGHTNESS = 3;
static constexpr uint8_t ENC_BLEND      = 4;
static constexpr uint8_t ENC_ZONE_SEL   = 5;
static constexpr uint8_t ENC_ZONE_COUNT = 6;
static constexpr uint8_t ENC_ZONE_MODE  = 7;

// Parameter ranges
static constexpr uint16_t MAX_EFFECT_ID   = 103;
static constexpr uint8_t  MAX_PALETTE_ID  = 74;
static constexpr uint8_t  MIN_SPEED       = 1;
static constexpr uint8_t  MAX_SPEED       = 100;
static constexpr uint8_t  MAX_BRIGHTNESS  = 255;
static constexpr uint8_t  MAX_BLEND_MODE  = 7;

// WS rate limiting
static constexpr uint32_t WS_THROTTLE_MS = 100;

// Blend mode name lookup
static const char* const kBlendModeNames[8] = {
    "Overwrite", "Add", "Subtract", "Multiply",
    "Screen", "Overlay", "Min", "Max"
};

// Parameter card labels
static const char* const kParamLabels[5] = {
    "EFFECT", "PALETTE", "SPEED", "BRIGHTNESS", "BLEND"
};

// ============================================================================
// 3-Zone Default Layout (Triple Rings)
// ============================================================================
static constexpr uint8_t kDefault3Zone[3][4] = {
    // { leftStart, leftEnd, rightStart, rightEnd }
    { 53, 79, 80, 106 },     // Zone 1 INNER: 53-106
    { 27, 52, 107, 132 },    // Zone 2 MIDDLE: 27-52 + 107-132
    {  0, 26, 133, 159 },    // Zone 3 OUTER: 0-26 + 133-159
};

// ============================================================================
// 2-Zone Default Layout (Dual Split)
// ============================================================================
static constexpr uint8_t kDefault2Zone[2][4] = {
    { 40, 79, 80, 119 },     // Zone 1 INNER: 40-119
    {  0, 39, 120, 159 },    // Zone 2 OUTER: 0-39 + 120-159
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

ZoneComposerUI::ZoneComposerUI(M5GFX& display) : _display(display) {
    // Zero-init all widget pointers (already nullptr from header defaults)
    // Initialise zone states with default 3-zone layout
    for (uint8_t i = 0; i < 3; ++i) {
        _zones[i] = ZoneState();
        _zones[i].enabled = true;
        _zones[i].speed = 25;
        _zones[i].brightness = 128;
    }

    // Initialise default segment layout (3-zone)
    for (uint8_t i = 0; i < 3; ++i) {
        _segments[i].zoneId = i;
        _segments[i].s1LeftStart  = kDefault3Zone[i][0];
        _segments[i].s1LeftEnd    = kDefault3Zone[i][1];
        _segments[i].s1RightStart = kDefault3Zone[i][2];
        _segments[i].s1RightEnd   = kDefault3Zone[i][3];
        _segments[i].totalLeds    = (_segments[i].s1LeftEnd - _segments[i].s1LeftStart + 1)
                                  + (_segments[i].s1RightEnd - _segments[i].s1RightStart + 1);
        _editingSegments[i] = _segments[i];
    }
    _editingZoneCount = 3;
}

ZoneComposerUI::~ZoneComposerUI() {
    // No heap-allocated user_data in v2 — all indices stored as uintptr_t casts
}

// ============================================================================
// begin() — Create ALL LVGL widgets (called once)
// ============================================================================

void ZoneComposerUI::begin(lv_obj_t* parent) {
    if (!parent) return;

    esp_task_wdt_reset();

    // Set page background
    lv_obj_set_style_bg_color(parent, lv_color_hex(DesignTokens::BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // --- Section 1: Header ---
    createHeader(parent);
    esp_task_wdt_reset();

    // --- Section 2: Mode Row ---
    createModeRow(parent);
    esp_task_wdt_reset();

    // --- Section 3: LED Strip Visualiser ---
    createStripVisualiser(parent);
    esp_task_wdt_reset();

    // --- Section 4: Selected Zone Indicator ---
    createSelectedZoneIndicator(parent);
    esp_task_wdt_reset();

    // --- Section 5: Parameter Grid ---
    createParamGrid(parent);
    esp_task_wdt_reset();

    // --- Section 6: Overview Grid ---
    createOverviewGrid(parent);
    esp_task_wdt_reset();

    // --- Section 7: Footer ---
    createFooter(parent);
    esp_task_wdt_reset();

    // Initial visual state
    updateStripSegments();
    updateParamCards();
    updateOverviewCards();
    updateSelectedZoneIndicator();
    updateZoneSelectorButtons();
    updateZoneEnableVisuals();
    updateZoneCountVisuals();

    esp_task_wdt_reset();

    Serial.println("[ZoneComposer] v2 UI initialised");
}

// ============================================================================
// loop() — Throttled render updates
// ============================================================================

void ZoneComposerUI::loop() {
    // Promote pending dirty to active dirty
    if (_pendingDirty) {
        _dirty = true;
        _pendingDirty = false;
    }

    if (!_dirty) return;

    uint32_t now = millis();
    if (now - _lastRenderTime < FRAME_INTERVAL_MS) return;
    _lastRenderTime = now;
    _dirty = false;

    // All visual updates are performed immediately in the action methods
    // (selectZone, adjustEffect, etc.) — loop() just manages the frame gate.
}

// ============================================================================
// createHeader() — Back button + title + zone count + zone enable
// ============================================================================

void ZoneComposerUI::createHeader(lv_obj_t* parent) {
    // Header container: flex ROW, space-between
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, CONTENT_WIDTH, HEADER_H);
    lv_obj_set_pos(header, CONTENT_PAD, HEADER_Y);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- Back Button ---
    _backButton = make_card(header, true);
    lv_obj_set_size(_backButton, 100, 44);
    lv_obj_add_flag(_backButton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_color(_backButton, lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);

    lv_obj_t* backLabel = lv_label_create(_backButton);
    lv_label_set_text(backLabel, "< BACK");
    lv_obj_set_style_text_font(backLabel, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(backLabel, lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_center(backLabel);
    lv_obj_clear_flag(backLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(backLabel, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(_backButton, backButtonCb, LV_EVENT_CLICKED, this);

    // --- Title (absolute centred over flex row) ---
    lv_obj_t* titleContainer = lv_obj_create(header);
    lv_obj_add_flag(titleContainer, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_opa(titleContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(titleContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(titleContainer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(titleContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(titleContainer, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(titleContainer, LV_ALIGN_CENTER, 0, 0);

    _titleLabel = lv_label_create(titleContainer);
    lv_label_set_text(_titleLabel, "ZONE COMPOSER");
    lv_obj_set_style_text_font(_titleLabel, BEBAS_BOLD_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(_titleLabel, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
    lv_obj_center(_titleLabel);

    // --- Right controls container ---
    lv_obj_t* rightControls = lv_obj_create(header);
    lv_obj_set_size(rightControls, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_bg_opa(rightControls, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rightControls, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rightControls, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rightControls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(rightControls, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rightControls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rightControls, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rightControls, 10, LV_PART_MAIN);

    // --- Zone Count Card ---
    _zoneCountCard = make_card(rightControls, false);
    lv_obj_set_size(_zoneCountCard, 120, 44);
    lv_obj_add_flag(_zoneCountCard, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_layout(_zoneCountCard, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_zoneCountCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_zoneCountCard, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_zoneCountCard, 6, LV_PART_MAIN);

    lv_obj_t* zcLabel = lv_label_create(_zoneCountCard);
    lv_label_set_text(zcLabel, "ZONES:");
    lv_obj_set_style_text_font(zcLabel, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(zcLabel, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_clear_flag(zcLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(zcLabel, LV_OBJ_FLAG_EVENT_BUBBLE);

    _zoneCountValue = lv_label_create(_zoneCountCard);
    lv_label_set_text(_zoneCountValue, "3");
    lv_obj_set_style_text_font(_zoneCountValue, JETBRAINS_MONO_BOLD_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(_zoneCountValue, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
    lv_obj_clear_flag(_zoneCountValue, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_zoneCountValue, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(_zoneCountCard, zoneCountCb, LV_EVENT_CLICKED, this);

    // --- Zone Enable Button ---
    _zoneEnableBtn = make_card(rightControls, false);
    lv_obj_set_size(_zoneEnableBtn, 150, 44);
    lv_obj_add_flag(_zoneEnableBtn, LV_OBJ_FLAG_CLICKABLE);

    _zoneEnableLabel = lv_label_create(_zoneEnableBtn);
    lv_label_set_text(_zoneEnableLabel, "ZONES: ON");
    lv_obj_set_style_text_font(_zoneEnableLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_zoneEnableLabel, lv_color_hex(DesignTokens::STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_center(_zoneEnableLabel);
    lv_obj_clear_flag(_zoneEnableLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_zoneEnableLabel, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Green border for ON state
    lv_obj_set_style_border_width(_zoneEnableBtn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_zoneEnableBtn, lv_color_hex(DesignTokens::STATUS_SUCCESS), LV_PART_MAIN);

    lv_obj_add_event_cb(_zoneEnableBtn, zoneEnableCb, LV_EVENT_CLICKED, this);
}

// ============================================================================
// createModeRow() — Zone selector buttons
// ============================================================================

void ZoneComposerUI::createModeRow(lv_obj_t* parent) {
    lv_obj_t* modeRow = lv_obj_create(parent);
    lv_obj_set_size(modeRow, CONTENT_WIDTH, MODE_ROW_H);
    lv_obj_set_pos(modeRow, CONTENT_PAD, MODE_ROW_Y);
    lv_obj_set_style_bg_opa(modeRow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(modeRow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modeRow, 0, LV_PART_MAIN);
    lv_obj_clear_flag(modeRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(modeRow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(modeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modeRow, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(modeRow, DesignTokens::GRID_GAP, LV_PART_MAIN);

    // Button widths: 130, 150, 140 per spec
    static constexpr int32_t kBtnWidths[3] = { 130, 150, 140 };

    for (uint8_t i = 0; i < 3; ++i) {
        _zoneSelectorBtns[i] = make_card(modeRow, false);
        lv_obj_set_size(_zoneSelectorBtns[i], kBtnWidths[i], MODE_ROW_H);
        lv_obj_add_flag(_zoneSelectorBtns[i], LV_OBJ_FLAG_CLICKABLE);

        // Extend touch area for 36px-high buttons (need 48px effective)
        lv_obj_set_ext_click_area(_zoneSelectorBtns[i], 6);

        // Store zone index as user_data on the widget
        lv_obj_set_user_data(_zoneSelectorBtns[i],
                             reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        // Label
        _zoneSelectorLabels[i] = lv_label_create(_zoneSelectorBtns[i]);
        lv_obj_set_style_text_font(_zoneSelectorLabels[i], RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_center(_zoneSelectorLabels[i]);
        lv_obj_clear_flag(_zoneSelectorLabels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_zoneSelectorLabels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_add_event_cb(_zoneSelectorBtns[i], zoneSelectorCb, LV_EVENT_CLICKED, this);
    }

    // Set initial text (3-zone mode)
    lv_label_set_text(_zoneSelectorLabels[0], "ZONE 1 \xC2\xB7 INNER");
    lv_label_set_text(_zoneSelectorLabels[1], "ZONE 2 \xC2\xB7 MIDDLE");
    lv_label_set_text(_zoneSelectorLabels[2], "ZONE 3 \xC2\xB7 OUTER");
}

// ============================================================================
// createStripVisualiser() — Hero LED strip with zone-coloured segments
// ============================================================================

void ZoneComposerUI::createStripVisualiser(lv_obj_t* parent) {
    // Outer container
    _stripContainer = lv_obj_create(parent);
    lv_obj_set_size(_stripContainer, STRIP_CONTAINER_W, STRIP_H);
    lv_obj_set_pos(_stripContainer, STRIP_CONTAINER_X, STRIP_Y);
    lv_obj_set_style_bg_color(_stripContainer, lv_color_hex(DesignTokens::BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_stripContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_stripContainer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_stripContainer, LV_OBJ_FLAG_SCROLLABLE);

    // --- Zone labels above strip ---
    lv_obj_t* labelRow = lv_obj_create(_stripContainer);
    lv_obj_set_size(labelRow, STRIP_BAR_WIDTH, STRIP_LABEL_H);
    lv_obj_set_pos(labelRow, STRIP_BAR_LOCAL_X, 0);
    lv_obj_set_style_bg_opa(labelRow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(labelRow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(labelRow, 0, LV_PART_MAIN);
    lv_obj_clear_flag(labelRow, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < 3; ++i) {
        _stripZoneLabels[i] = lv_label_create(labelRow);
        lv_obj_set_style_text_font(_stripZoneLabels[i], RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_stripZoneLabels[i],
                                     lv_color_hex(getZoneColour(i)), LV_PART_MAIN);
        // Positioned by updateStripSegments()
    }
    lv_label_set_text(_stripZoneLabels[0], "ZONE 1 \xC2\xB7 INNER");
    lv_label_set_text(_stripZoneLabels[1], "ZONE 2 \xC2\xB7 MIDDLE");
    lv_label_set_text(_stripZoneLabels[2], "ZONE 3 \xC2\xB7 OUTER");

    // --- Strip bar (dark background for segments) ---
    _stripBar = lv_obj_create(_stripContainer);
    lv_obj_set_size(_stripBar, STRIP_BAR_WIDTH, STRIP_BAR_H);
    lv_obj_set_pos(_stripBar, STRIP_BAR_LOCAL_X, STRIP_LABEL_H + 2);
    lv_obj_set_style_bg_color(_stripBar, lv_color_hex(DesignTokens::SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_stripBar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_stripBar, lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_radius(_stripBar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_stripBar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_stripBar, LV_OBJ_FLAG_SCROLLABLE);

    // --- Zone segments (5 possible: Z1 centre, Z2L, Z2R, Z3L, Z3R) ---
    // Created inside _stripBar for correct positioning
    for (uint8_t i = 0; i < 5; ++i) {
        _zoneSegs[i] = lv_obj_create(_stripBar);
        lv_obj_set_style_radius(_zoneSegs[i], 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_zoneSegs[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(_zoneSegs[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_zoneSegs[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_zoneSegs[i], LV_OBJ_FLAG_CLICKABLE);

        // Determine which zone this segment belongs to for the callback
        uint8_t zoneIdx;
        if (i == 0) zoneIdx = 0;           // Z1 centre
        else if (i <= 2) zoneIdx = 1;      // Z2 left/right
        else zoneIdx = 2;                   // Z3 left/right

        lv_obj_set_user_data(_zoneSegs[i],
                             reinterpret_cast<void*>(static_cast<uintptr_t>(zoneIdx)));
        lv_obj_add_event_cb(_zoneSegs[i], stripSegmentCb, LV_EVENT_CLICKED, this);
    }

    // Assign zone colours
    // Seg 0: Zone 1 (cyan)
    lv_obj_set_style_bg_color(_zoneSegs[0], lv_color_hex(ZoneColours::ZONE_1), LV_PART_MAIN);
    // Seg 1,2: Zone 2 (green)
    lv_obj_set_style_bg_color(_zoneSegs[1], lv_color_hex(ZoneColours::ZONE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_zoneSegs[2], lv_color_hex(ZoneColours::ZONE_2), LV_PART_MAIN);
    // Seg 3,4: Zone 3 (purple)
    lv_obj_set_style_bg_color(_zoneSegs[3], lv_color_hex(ZoneColours::ZONE_3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_zoneSegs[4], lv_color_hex(ZoneColours::ZONE_3), LV_PART_MAIN);

    // --- Centre marker (gold bar at LED 79/80) — created last for z-order ---
    _centreMarker = lv_obj_create(_stripBar);
    lv_obj_set_size(_centreMarker, 4, STRIP_BAR_H);
    lv_obj_set_pos(_centreMarker, CENTRE_MARKER_X - 2, 0);
    lv_obj_set_style_bg_color(_centreMarker, lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_centreMarker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_centreMarker, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(_centreMarker, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_centreMarker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(_centreMarker, LV_OBJ_FLAG_SCROLLABLE);

    // --- Tick row below strip ---
    lv_obj_t* tickRow = lv_obj_create(_stripContainer);
    lv_obj_set_size(tickRow, STRIP_BAR_WIDTH, STRIP_TICK_H);
    lv_obj_set_pos(tickRow, STRIP_BAR_LOCAL_X, STRIP_LABEL_H + 2 + STRIP_BAR_H + 6);
    lv_obj_set_style_bg_opa(tickRow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tickRow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tickRow, 0, LV_PART_MAIN);
    lv_obj_clear_flag(tickRow, LV_OBJ_FLAG_SCROLLABLE);

    // Fixed tick labels: "0", "79|80", "159"
    _stripTickLabels[0] = lv_label_create(tickRow);
    lv_label_set_text(_stripTickLabels[0], "0");
    lv_obj_set_style_text_font(_stripTickLabels[0], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_stripTickLabels[0], lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
    lv_obj_set_pos(_stripTickLabels[0], 0, 0);

    _stripTickLabels[1] = lv_label_create(tickRow);
    lv_label_set_text(_stripTickLabels[1], "79|80");
    lv_obj_set_style_text_font(_stripTickLabels[1], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_stripTickLabels[1], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_pos(_stripTickLabels[1], CENTRE_MARKER_X - 30, 0);

    _stripTickLabels[2] = lv_label_create(tickRow);
    lv_label_set_text(_stripTickLabels[2], "159");
    lv_obj_set_style_text_font(_stripTickLabels[2], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_stripTickLabels[2], lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
    lv_obj_align(_stripTickLabels[2], LV_ALIGN_TOP_RIGHT, 0, 0);

    // Dynamic boundary ticks (remaining slots) — positioned by updateStripSegments()
    for (uint8_t i = 3; i < 7; ++i) {
        _stripTickLabels[i] = lv_label_create(tickRow);
        lv_label_set_text(_stripTickLabels[i], "");
        lv_obj_set_style_text_font(_stripTickLabels[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_stripTickLabels[i], lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
        lv_obj_set_pos(_stripTickLabels[i], 0, 0);
    }
}

// ============================================================================
// createSelectedZoneIndicator() — Dot + "ZONE N PARAMETERS"
// ============================================================================

void ZoneComposerUI::createSelectedZoneIndicator(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, CONTENT_WIDTH, INDICATOR_H);
    lv_obj_set_pos(row, CONTENT_PAD, INDICATOR_Y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

    // Zone colour dot
    _selectedZoneDot = lv_obj_create(row);
    lv_obj_set_size(_selectedZoneDot, 8, 8);
    lv_obj_set_style_radius(_selectedZoneDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_selectedZoneDot, lv_color_hex(getZoneColour(0)), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_selectedZoneDot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_selectedZoneDot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_selectedZoneDot, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    _selectedZoneTitle = lv_label_create(row);
    lv_label_set_text(_selectedZoneTitle, "ZONE 1 PARAMETERS");
    lv_obj_set_style_text_font(_selectedZoneTitle, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(_selectedZoneTitle, lv_color_hex(getZoneColour(0)), LV_PART_MAIN);
}

// ============================================================================
// createParamGrid() — 5-column parameter cards
// ============================================================================

void ZoneComposerUI::createParamGrid(lv_obj_t* parent) {
    // Grid descriptor arrays MUST be static (LVGL stores pointer)
    static lv_coord_t col_dsc[6] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t row_dsc[2] = { 150, LV_GRID_TEMPLATE_LAST };

    _paramGrid = lv_obj_create(parent);
    lv_obj_set_size(_paramGrid, CONTENT_WIDTH, PARAM_GRID_H);
    lv_obj_set_pos(_paramGrid, CONTENT_PAD, PARAM_GRID_Y);
    lv_obj_set_style_bg_opa(_paramGrid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_paramGrid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_paramGrid, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_paramGrid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_paramGrid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(_paramGrid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(_paramGrid, DesignTokens::GRID_GAP, LV_PART_MAIN);

    for (uint8_t i = 0; i < 5; ++i) {
        _paramCards[i] = make_card(_paramGrid, false);
        lv_obj_set_grid_cell(_paramCards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        // Parameter name label (top)
        _paramLabels[i] = lv_label_create(_paramCards[i]);
        lv_label_set_text(_paramLabels[i], kParamLabels[i]);
        lv_obj_set_style_text_font(_paramLabels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_paramLabels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_align(_paramLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(_paramLabels[i], LV_PCT(100));
        lv_obj_align(_paramLabels[i], LV_ALIGN_TOP_MID, 0, 0);

        // Numeric value
        _paramValues[i] = lv_label_create(_paramCards[i]);
        lv_label_set_text(_paramValues[i], "---");
        lv_obj_set_style_text_font(_paramValues[i], JETBRAINS_MONO_REG_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(_paramValues[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_paramValues[i], LV_ALIGN_TOP_MID, 0, 28);

        // Name sub-label (for effect/palette/blend names)
        _paramNames[i] = lv_label_create(_paramCards[i]);
        lv_label_set_text(_paramNames[i], "");
        lv_obj_set_style_text_font(_paramNames[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_paramNames[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_align(_paramNames[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(_paramNames[i], 210);
        lv_label_set_long_mode(_paramNames[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(_paramNames[i], LV_ALIGN_TOP_MID, 0, 66);

        // Progress bar at bottom
        _paramBars[i] = lv_bar_create(_paramCards[i]);
        lv_obj_set_size(_paramBars[i], LV_PCT(90), 8);
        lv_obj_align(_paramBars[i], LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_style_bg_color(_paramBars[i], lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_radius(_paramBars[i], 4, LV_PART_MAIN);
        lv_obj_set_style_radius(_paramBars[i], 4, LV_PART_INDICATOR);
    }

    // Set bar ranges per parameter
    lv_bar_set_range(_paramBars[0], 0, MAX_EFFECT_ID);     // Effect
    lv_bar_set_range(_paramBars[1], 0, MAX_PALETTE_ID);    // Palette
    lv_bar_set_range(_paramBars[2], MIN_SPEED, MAX_SPEED); // Speed
    lv_bar_set_range(_paramBars[3], 0, MAX_BRIGHTNESS);    // Brightness
    lv_bar_set_range(_paramBars[4], 0, MAX_BLEND_MODE);    // Blend
}

// ============================================================================
// createOverviewGrid() — 3-column zone summary cards
// ============================================================================

void ZoneComposerUI::createOverviewGrid(lv_obj_t* parent) {
    static lv_coord_t ov_col[4] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t ov_row[2] = { 160, LV_GRID_TEMPLATE_LAST };

    _overviewGrid = lv_obj_create(parent);
    lv_obj_set_size(_overviewGrid, CONTENT_WIDTH, OVERVIEW_GRID_H);
    lv_obj_set_pos(_overviewGrid, CONTENT_PAD, OVERVIEW_GRID_Y);
    lv_obj_set_style_bg_opa(_overviewGrid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_overviewGrid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_overviewGrid, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_overviewGrid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_overviewGrid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(_overviewGrid, ov_col, ov_row);
    lv_obj_set_style_pad_column(_overviewGrid, DesignTokens::GRID_GAP, LV_PART_MAIN);

    for (uint8_t i = 0; i < 3; ++i) {
        uint32_t zColour = getZoneColour(i);

        _overviewCards[i] = make_card(_overviewGrid, false);
        lv_obj_set_grid_cell(_overviewCards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_flag(_overviewCards[i], LV_OBJ_FLAG_CLICKABLE);

        // Store zone index for tap-to-select
        lv_obj_set_user_data(_overviewCards[i],
                             reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_overviewCards[i], overviewCardCb, LV_EVENT_CLICKED, this);

        // --- Header row: dot + "ZONE N — ROLE" ---
        _ovHeaders[i] = lv_obj_create(_overviewCards[i]);
        lv_obj_set_size(_ovHeaders[i], LV_PCT(100), 24);
        lv_obj_align(_ovHeaders[i], LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_opa(_ovHeaders[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(_ovHeaders[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_ovHeaders[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_ovHeaders[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(_ovHeaders[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovHeaders[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_layout(_ovHeaders[i], LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(_ovHeaders[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(_ovHeaders[i], LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Left side of header: dot + zone title
        lv_obj_t* headerLeft = lv_obj_create(_ovHeaders[i]);
        lv_obj_set_size(headerLeft, LV_SIZE_CONTENT, 24);
        lv_obj_set_style_bg_opa(headerLeft, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(headerLeft, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(headerLeft, 0, LV_PART_MAIN);
        lv_obj_clear_flag(headerLeft, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(headerLeft, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(headerLeft, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_layout(headerLeft, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(headerLeft, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(headerLeft, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(headerLeft, 6, LV_PART_MAIN);

        // Dot
        _ovDots[i] = lv_obj_create(headerLeft);
        lv_obj_set_size(_ovDots[i], 8, 8);
        lv_obj_set_style_radius(_ovDots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_ovDots[i], lv_color_hex(zColour), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_ovDots[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(_ovDots[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_ovDots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(_ovDots[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovDots[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // Zone title in header
        lv_obj_t* hdrTitle = lv_label_create(headerLeft);
        char hdrBuf[32];
        snprintf(hdrBuf, sizeof(hdrBuf), "ZONE %d \xE2\x80\x94 %s", i + 1, getZoneRole(i));
        lv_label_set_text(hdrTitle, hdrBuf);
        lv_obj_set_style_text_font(hdrTitle, RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(hdrTitle, lv_color_hex(zColour), LV_PART_MAIN);
        lv_obj_clear_flag(hdrTitle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(hdrTitle, LV_OBJ_FLAG_EVENT_BUBBLE);

        // LED range label (right side of header)
        _ovRangeLabels[i] = lv_label_create(_ovHeaders[i]);
        lv_label_set_text(_ovRangeLabels[i], "");
        lv_obj_set_style_text_font(_ovRangeLabels[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ovRangeLabels[i], lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
        lv_obj_clear_flag(_ovRangeLabels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovRangeLabels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // --- Effect row ---
        lv_obj_t* fxRow = lv_obj_create(_overviewCards[i]);
        lv_obj_set_size(fxRow, LV_PCT(100), 24);
        lv_obj_set_pos(fxRow, 0, 30);
        lv_obj_set_style_bg_opa(fxRow, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(fxRow, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(fxRow, 0, LV_PART_MAIN);
        lv_obj_clear_flag(fxRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(fxRow, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(fxRow, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_layout(fxRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(fxRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(fxRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* fxLabel = lv_label_create(fxRow);
        lv_label_set_text(fxLabel, "FX");
        lv_obj_set_style_text_font(fxLabel, RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(fxLabel, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_clear_flag(fxLabel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(fxLabel, LV_OBJ_FLAG_EVENT_BUBBLE);

        _ovEffectVals[i] = lv_label_create(fxRow);
        lv_label_set_text(_ovEffectVals[i], "---");
        lv_obj_set_style_text_font(_ovEffectVals[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ovEffectVals[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_set_width(_ovEffectVals[i], 280);
        lv_obj_set_style_text_align(_ovEffectVals[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_long_mode(_ovEffectVals[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_clear_flag(_ovEffectVals[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovEffectVals[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // --- Palette row ---
        lv_obj_t* palRow = lv_obj_create(_overviewCards[i]);
        lv_obj_set_size(palRow, LV_PCT(100), 24);
        lv_obj_set_pos(palRow, 0, 56);
        lv_obj_set_style_bg_opa(palRow, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(palRow, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(palRow, 0, LV_PART_MAIN);
        lv_obj_clear_flag(palRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(palRow, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(palRow, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_layout(palRow, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(palRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(palRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* palLabel = lv_label_create(palRow);
        lv_label_set_text(palLabel, "PAL");
        lv_obj_set_style_text_font(palLabel, RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(palLabel, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_clear_flag(palLabel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(palLabel, LV_OBJ_FLAG_EVENT_BUBBLE);

        _ovPaletteVals[i] = lv_label_create(palRow);
        lv_label_set_text(_ovPaletteVals[i], "---");
        lv_obj_set_style_text_font(_ovPaletteVals[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ovPaletteVals[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_set_width(_ovPaletteVals[i], 280);
        lv_obj_set_style_text_align(_ovPaletteVals[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_label_set_long_mode(_ovPaletteVals[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_clear_flag(_ovPaletteVals[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovPaletteVals[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // --- Stats row: SPD / BRI / blend name ---
        _ovStatsLabels[i] = lv_label_create(_overviewCards[i]);
        lv_label_set_text(_ovStatsLabels[i], "SPD --  BRI --  ---");
        lv_obj_set_style_text_font(_ovStatsLabels[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ovStatsLabels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_pos(_ovStatsLabels[i], 0, 82);
        lv_obj_set_width(_ovStatsLabels[i], LV_PCT(100));
        lv_label_set_long_mode(_ovStatsLabels[i], LV_LABEL_LONG_DOT);
        lv_obj_clear_flag(_ovStatsLabels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovStatsLabels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // --- LED count (bottom right) ---
        _ovLedCounts[i] = lv_label_create(_overviewCards[i]);
        lv_label_set_text(_ovLedCounts[i], "-- LEDs");
        lv_obj_set_style_text_font(_ovLedCounts[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ovLedCounts[i], lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
        lv_obj_align(_ovLedCounts[i], LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_clear_flag(_ovLedCounts[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_ovLedCounts[i], LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

// ============================================================================
// createFooter() — Encoder hint labels
// ============================================================================

void ZoneComposerUI::createFooter(lv_obj_t* parent) {
    _footer = make_card(parent, true);
    lv_obj_set_size(_footer, CONTENT_WIDTH, FOOTER_H);
    lv_obj_set_pos(_footer, CONTENT_PAD, FOOTER_Y);
    lv_obj_set_style_border_width(_footer, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(_footer, lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
    lv_obj_set_layout(_footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_footer, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(_footer, 16, LV_PART_MAIN);

    const char* hints[3] = {
        "ENC 0-4: PARAMS",
        "ENC 5: ZONE SEL",
        "ENC 6: COUNT  ENC 7: ON/OFF"
    };

    for (uint8_t i = 0; i < 3; ++i) {
        lv_obj_t* lbl = lv_label_create(_footer);
        lv_label_set_text(lbl, hints[i]);
        lv_obj_set_style_text_font(lbl, RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(DesignTokens::FG_DIMMED), LV_PART_MAIN);
    }
}

// ============================================================================
// Update Methods — Called on state change (NEVER create/destroy widgets)
// ============================================================================

void ZoneComposerUI::updateStripSegments() {
    if (!_stripBar) return;

    const auto& segs = _segments;

    // Zone 1 (single centre block): seg[0]
    {
        int x0 = calcSegmentX(segs[0].s1LeftStart);
        int x1 = calcSegmentX(segs[0].s1RightEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
        lv_obj_set_pos(_zoneSegs[0], x0, 0);
        lv_obj_set_size(_zoneSegs[0], x1 - x0, STRIP_BAR_H);
    }

    // Zone 2 left half: seg[1]
    {
        int x0 = calcSegmentX(segs[1].s1LeftStart);
        int x1 = calcSegmentX(segs[1].s1LeftEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
        lv_obj_set_pos(_zoneSegs[1], x0, 0);
        lv_obj_set_size(_zoneSegs[1], x1 - x0, STRIP_BAR_H);
    }

    // Zone 2 right half: seg[2]
    {
        int x0 = calcSegmentX(segs[1].s1RightStart);
        int x1 = calcSegmentX(segs[1].s1RightEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
        lv_obj_set_pos(_zoneSegs[2], x0, 0);
        lv_obj_set_size(_zoneSegs[2], x1 - x0, STRIP_BAR_H);
    }

    if (_zoneCount == 3) {
        // Zone 3 left half: seg[3]
        {
            int x0 = calcSegmentX(segs[2].s1LeftStart);
            int x1 = calcSegmentX(segs[2].s1LeftEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
            lv_obj_set_pos(_zoneSegs[3], x0, 0);
            lv_obj_set_size(_zoneSegs[3], x1 - x0, STRIP_BAR_H);
        }
        // Zone 3 right half: seg[4]
        {
            int x0 = calcSegmentX(segs[2].s1RightStart);
            int x1 = calcSegmentX(segs[2].s1RightEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
            lv_obj_set_pos(_zoneSegs[4], x0, 0);
            lv_obj_set_size(_zoneSegs[4], x1 - x0, STRIP_BAR_H);
        }
        lv_obj_clear_flag(_zoneSegs[3], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_zoneSegs[4], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_zoneSegs[3], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_zoneSegs[4], LV_OBJ_FLAG_HIDDEN);
    }

    // Update segment opacity/border based on selection
    for (uint8_t i = 0; i < 5; ++i) {
        uint8_t zoneIdx;
        if (i == 0) zoneIdx = 0;
        else if (i <= 2) zoneIdx = 1;
        else zoneIdx = 2;

        if (zoneIdx == _selectedZone) {
            lv_obj_set_style_bg_opa(_zoneSegs[i], LV_OPA_60, LV_PART_MAIN);
            lv_obj_set_style_border_width(_zoneSegs[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_zoneSegs[i],
                                          lv_color_hex(getZoneColour(zoneIdx)), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(_zoneSegs[i], LV_OPA_30, LV_PART_MAIN);
            lv_obj_set_style_border_width(_zoneSegs[i], 0, LV_PART_MAIN);
        }
    }

    // Update zone labels above strip (centred over each zone's span)
    for (uint8_t z = 0; z < 3; ++z) {
        if (!_stripZoneLabels[z]) continue;

        if (z >= _zoneCount) {
            lv_obj_add_flag(_stripZoneLabels[z], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(_stripZoneLabels[z], LV_OBJ_FLAG_HIDDEN);

        // Calculate centre of this zone's total span
        int leftX = calcSegmentX(segs[z].s1LeftStart);
        int rightX = calcSegmentX(segs[z].s1RightEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
        int centreX = (leftX + rightX) / 2;
        lv_obj_set_pos(_stripZoneLabels[z], centreX - 60, 0);

        // Update role text
        char buf[32];
        snprintf(buf, sizeof(buf), "ZONE %d \xC2\xB7 %s", z + 1, getZoneRole(z));
        lv_label_set_text(_stripZoneLabels[z], buf);
    }

    // Update dynamic boundary tick labels
    if (_zoneCount == 3) {
        // Show boundary ticks: zone1 start, zone2 start
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", segs[1].s1LeftStart);
        lv_label_set_text(_stripTickLabels[3], buf);
        lv_obj_set_pos(_stripTickLabels[3], calcSegmentX(segs[1].s1LeftStart), 0);
        lv_obj_clear_flag(_stripTickLabels[3], LV_OBJ_FLAG_HIDDEN);

        snprintf(buf, sizeof(buf), "%d", segs[0].s1LeftStart);
        lv_label_set_text(_stripTickLabels[4], buf);
        lv_obj_set_pos(_stripTickLabels[4], calcSegmentX(segs[0].s1LeftStart), 0);
        lv_obj_clear_flag(_stripTickLabels[4], LV_OBJ_FLAG_HIDDEN);

        snprintf(buf, sizeof(buf), "%d", segs[0].s1RightEnd);
        lv_label_set_text(_stripTickLabels[5], buf);
        lv_obj_set_pos(_stripTickLabels[5], calcSegmentX(segs[0].s1RightEnd), 0);
        lv_obj_clear_flag(_stripTickLabels[5], LV_OBJ_FLAG_HIDDEN);

        snprintf(buf, sizeof(buf), "%d", segs[1].s1RightEnd);
        lv_label_set_text(_stripTickLabels[6], buf);
        lv_obj_set_pos(_stripTickLabels[6], calcSegmentX(segs[1].s1RightEnd), 0);
        lv_obj_clear_flag(_stripTickLabels[6], LV_OBJ_FLAG_HIDDEN);
    } else {
        // 2-zone: show inner zone boundaries
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", segs[0].s1LeftStart);
        lv_label_set_text(_stripTickLabels[3], buf);
        lv_obj_set_pos(_stripTickLabels[3], calcSegmentX(segs[0].s1LeftStart), 0);
        lv_obj_clear_flag(_stripTickLabels[3], LV_OBJ_FLAG_HIDDEN);

        snprintf(buf, sizeof(buf), "%d", segs[0].s1RightEnd);
        lv_label_set_text(_stripTickLabels[4], buf);
        lv_obj_set_pos(_stripTickLabels[4], calcSegmentX(segs[0].s1RightEnd), 0);
        lv_obj_clear_flag(_stripTickLabels[4], LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(_stripTickLabels[5], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_stripTickLabels[6], LV_OBJ_FLAG_HIDDEN);
    }
}

void ZoneComposerUI::updateParamCards() {
    if (!_paramValues[0]) return;

    const ZoneState& z = _zones[_selectedZone];
    uint32_t zColour = getZoneColour(_selectedZone);

    // Card 0: EFFECT
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%03d", z.effectId);
        lv_label_set_text(_paramValues[0], buf);

        const char* name = z.effectName[0] ? z.effectName : lookupEffectName(z.effectId);
        lv_label_set_text(_paramNames[0], name ? name : "---");
        lv_bar_set_value(_paramBars[0], z.effectId, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_paramBars[0], lv_color_hex(zColour), LV_PART_INDICATOR);
    }

    // Card 1: PALETTE
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%03d", z.paletteId);
        lv_label_set_text(_paramValues[1], buf);

        const char* name = z.paletteName[0] ? z.paletteName : lookupPaletteName(z.paletteId);
        lv_label_set_text(_paramNames[1], name ? name : "---");
        lv_bar_set_value(_paramBars[1], z.paletteId, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_paramBars[1], lv_color_hex(zColour), LV_PART_INDICATOR);
    }

    // Card 2: SPEED
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", z.speed);
        lv_label_set_text(_paramValues[2], buf);
        lv_label_set_text(_paramNames[2], "");  // No name sub-label for speed
        lv_bar_set_value(_paramBars[2], z.speed, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_paramBars[2], lv_color_hex(zColour), LV_PART_INDICATOR);
    }

    // Card 3: BRIGHTNESS
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", z.brightness);
        lv_label_set_text(_paramValues[3], buf);
        lv_label_set_text(_paramNames[3], "");  // No name sub-label for brightness
        lv_bar_set_value(_paramBars[3], z.brightness, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_paramBars[3], lv_color_hex(zColour), LV_PART_INDICATOR);
    }

    // Card 4: BLEND MODE
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", z.blendMode);
        lv_label_set_text(_paramValues[4], buf);

        const char* name = z.blendModeName[0] ? z.blendModeName
                         : (z.blendMode < 8 ? kBlendModeNames[z.blendMode] : "---");
        lv_label_set_text(_paramNames[4], name);
        lv_bar_set_value(_paramBars[4], z.blendMode, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_paramBars[4], lv_color_hex(zColour), LV_PART_INDICATOR);
    }
}

void ZoneComposerUI::updateOverviewCards() {
    for (uint8_t i = 0; i < 3; ++i) {
        if (!_overviewCards[i]) continue;

        // Show/hide Zone 3
        if (i == 2 && _zoneCount < 3) {
            lv_obj_add_flag(_overviewCards[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        if (i < _zoneCount) {
            lv_obj_clear_flag(_overviewCards[i], LV_OBJ_FLAG_HIDDEN);
        }

        const ZoneState& z = _zones[i];
        uint32_t zColour = getZoneColour(i);

        // Selected state: border highlight
        if (i == _selectedZone) {
            lv_obj_set_style_border_width(_overviewCards[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_overviewCards[i], lv_color_hex(zColour), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(_overviewCards[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(_overviewCards[i],
                                          lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
        }

        // LED range
        {
            const auto& seg = _segments[i];
            char buf[32];
            snprintf(buf, sizeof(buf), "%d-%d | %d-%d",
                     seg.s1LeftStart, seg.s1LeftEnd,
                     seg.s1RightStart, seg.s1RightEnd);
            lv_label_set_text(_ovRangeLabels[i], buf);
        }

        // Effect
        {
            const char* name = z.effectName[0] ? z.effectName : lookupEffectName(z.effectId);
            char buf[64];
            snprintf(buf, sizeof(buf), "%03d %s", z.effectId, name ? name : "---");
            lv_label_set_text(_ovEffectVals[i], buf);
        }

        // Palette
        {
            const char* name = z.paletteName[0] ? z.paletteName : lookupPaletteName(z.paletteId);
            char buf[64];
            snprintf(buf, sizeof(buf), "%03d %s", z.paletteId, name ? name : "---");
            lv_label_set_text(_ovPaletteVals[i], buf);
        }

        // Stats row: SPD / BRI / blend name
        {
            const char* blendName = z.blendModeName[0] ? z.blendModeName
                                  : (z.blendMode < 8 ? kBlendModeNames[z.blendMode] : "---");
            char buf[64];
            snprintf(buf, sizeof(buf), "SPD %d  BRI %d  %s",
                     z.speed, z.brightness, blendName);
            lv_label_set_text(_ovStatsLabels[i], buf);
        }

        // LED count
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d LEDs", _segments[i].totalLeds);
            lv_label_set_text(_ovLedCounts[i], buf);
        }
    }
}

void ZoneComposerUI::updateSelectedZoneIndicator() {
    if (!_selectedZoneDot || !_selectedZoneTitle) return;

    uint32_t zColour = getZoneColour(_selectedZone);

    lv_obj_set_style_bg_color(_selectedZoneDot, lv_color_hex(zColour), LV_PART_MAIN);

    char buf[32];
    snprintf(buf, sizeof(buf), "ZONE %d PARAMETERS", _selectedZone + 1);
    lv_label_set_text(_selectedZoneTitle, buf);
    lv_obj_set_style_text_color(_selectedZoneTitle, lv_color_hex(zColour), LV_PART_MAIN);
}

void ZoneComposerUI::updateZoneSelectorButtons() {
    for (uint8_t i = 0; i < 3; ++i) {
        if (!_zoneSelectorBtns[i]) continue;

        // Show/hide Zone 3
        if (i == 2) {
            if (_zoneCount < 3) {
                lv_obj_add_flag(_zoneSelectorBtns[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_clear_flag(_zoneSelectorBtns[i], LV_OBJ_FLAG_HIDDEN);
        }

        uint32_t zColour = getZoneColour(i);

        // Update label text
        char buf[32];
        snprintf(buf, sizeof(buf), "ZONE %d \xC2\xB7 %s", i + 1, getZoneRole(i));
        lv_label_set_text(_zoneSelectorLabels[i], buf);
        lv_obj_set_style_text_color(_zoneSelectorLabels[i],
                                     lv_color_hex(zColour), LV_PART_MAIN);

        // Selected: thicker zone-colour border; unselected: subtle border
        if (i == _selectedZone) {
            lv_obj_set_style_border_width(_zoneSelectorBtns[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(_zoneSelectorBtns[i],
                                          lv_color_hex(zColour), LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(_zoneSelectorBtns[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(_zoneSelectorBtns[i],
                                          lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
        }
    }
}

void ZoneComposerUI::updateZoneEnableVisuals() {
    if (!_zoneEnableBtn || !_zoneEnableLabel) return;

    if (_zonesEnabled) {
        lv_label_set_text(_zoneEnableLabel, "ZONES: ON");
        lv_obj_set_style_text_color(_zoneEnableLabel,
                                     lv_color_hex(DesignTokens::STATUS_SUCCESS), LV_PART_MAIN);
        lv_obj_set_style_border_color(_zoneEnableBtn,
                                       lv_color_hex(DesignTokens::STATUS_SUCCESS), LV_PART_MAIN);
    } else {
        lv_label_set_text(_zoneEnableLabel, "ZONES: OFF");
        lv_obj_set_style_text_color(_zoneEnableLabel,
                                     lv_color_hex(DesignTokens::STATUS_ERROR), LV_PART_MAIN);
        lv_obj_set_style_border_color(_zoneEnableBtn,
                                       lv_color_hex(DesignTokens::STATUS_ERROR), LV_PART_MAIN);
    }

    // Dim content when zones disabled
    lv_opa_t contentOpa = _zonesEnabled ? LV_OPA_COVER : LV_OPA_30;
    if (_stripContainer) lv_obj_set_style_opa(_stripContainer, contentOpa, LV_PART_MAIN);
    if (_paramGrid) lv_obj_set_style_opa(_paramGrid, contentOpa, LV_PART_MAIN);
    if (_overviewGrid) lv_obj_set_style_opa(_overviewGrid, contentOpa, LV_PART_MAIN);
}

void ZoneComposerUI::updateZoneCountVisuals() {
    if (!_zoneCountValue) return;

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", _zoneCount);
    lv_label_set_text(_zoneCountValue, buf);
}

// ============================================================================
// State Updates (from WsMessageRouter)
// ============================================================================

void ZoneComposerUI::updateZone(uint8_t zoneId, const ZoneState& state) {
    if (zoneId >= 3) return;
    _zones[zoneId] = state;

    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::updateSegments(const zones::ZoneSegment* segments, uint8_t count) {
    if (!segments) return;
    uint8_t n = (count > zones::MAX_ZONES) ? zones::MAX_ZONES : count;
    for (uint8_t i = 0; i < n; ++i) {
        _segments[i] = segments[i];
        _editingSegments[i] = segments[i];
    }
    _editingZoneCount = n;

    // Derive zone count from received segments (2 or 3 active)
    _zoneCount = n;
    if (_zoneCount < 2) _zoneCount = 2;
    if (_zoneCount > 3) _zoneCount = 3;

    // If selected zone is now out of range, clamp
    if (_selectedZone >= _zoneCount) {
        _selectedZone = 0;
    }

    updateStripSegments();
    updateZoneSelectorButtons();
    updateZoneCountVisuals();
    updateOverviewCards();
    updateParamCards();
    updateSelectedZoneIndicator();
    markDirty();
}

void ZoneComposerUI::updateZoneModeButton(bool enabled) {
    _zonesEnabled = enabled;
    updateZoneEnableVisuals();
    markDirty();
}

// ============================================================================
// Interaction Handlers
// ============================================================================

void ZoneComposerUI::selectZone(uint8_t zoneIndex) {
    if (zoneIndex >= _zoneCount) return;
    if (zoneIndex == _selectedZone) return;

    _selectedZone = zoneIndex;

    updateStripSegments();
    updateParamCards();
    updateOverviewCards();
    updateSelectedZoneIndicator();
    updateZoneSelectorButtons();
    markDirty();

    Serial.printf("[ZoneComposer] Selected Zone %d\n", _selectedZone + 1);
}

void ZoneComposerUI::adjustZoneCount(int32_t delta) {
    uint8_t oldCount = _zoneCount;
    if (delta > 0 && _zoneCount < 3) {
        _zoneCount = 3;
    } else if (delta < 0 && _zoneCount > 2) {
        _zoneCount = 2;
    } else {
        // Toggle between 2 and 3
        _zoneCount = (_zoneCount == 2) ? 3 : 2;
    }

    if (_zoneCount == oldCount) return;

    // Load appropriate default layout
    if (_zoneCount == 3) {
        for (uint8_t i = 0; i < 3; ++i) {
            _segments[i].zoneId = i;
            _segments[i].s1LeftStart  = kDefault3Zone[i][0];
            _segments[i].s1LeftEnd    = kDefault3Zone[i][1];
            _segments[i].s1RightStart = kDefault3Zone[i][2];
            _segments[i].s1RightEnd   = kDefault3Zone[i][3];
            _segments[i].totalLeds = (_segments[i].s1LeftEnd - _segments[i].s1LeftStart + 1)
                                   + (_segments[i].s1RightEnd - _segments[i].s1RightStart + 1);
            _editingSegments[i] = _segments[i];
        }
    } else {
        for (uint8_t i = 0; i < 2; ++i) {
            _segments[i].zoneId = i;
            _segments[i].s1LeftStart  = kDefault2Zone[i][0];
            _segments[i].s1LeftEnd    = kDefault2Zone[i][1];
            _segments[i].s1RightStart = kDefault2Zone[i][2];
            _segments[i].s1RightEnd   = kDefault2Zone[i][3];
            _segments[i].totalLeds = (_segments[i].s1LeftEnd - _segments[i].s1LeftStart + 1)
                                   + (_segments[i].s1RightEnd - _segments[i].s1RightStart + 1);
            _editingSegments[i] = _segments[i];
        }
    }
    _editingZoneCount = _zoneCount;

    // Auto-select Zone 1 if Zone 3 was selected and count drops to 2
    if (_selectedZone >= _zoneCount) {
        _selectedZone = 0;
    }

    // Send zone preset load to K1 (presetId = count - 1 per spec)
    sendZoneLoadPreset(_zoneCount - 1);

    updateStripSegments();
    updateZoneSelectorButtons();
    updateZoneCountVisuals();
    updateOverviewCards();
    updateParamCards();
    updateSelectedZoneIndicator();
    markDirty();

    Serial.printf("[ZoneComposer] Zone count changed to %d\n", _zoneCount);
}

void ZoneComposerUI::adjustEffect(int32_t delta) {
    ZoneState& z = _zones[_selectedZone];
    int32_t newVal = static_cast<int32_t>(z.effectId) + (delta > 0 ? 1 : -1);

    // Wrapping behaviour
    if (newVal < 0) newVal = MAX_EFFECT_ID;
    if (newVal > MAX_EFFECT_ID) newVal = 0;

    z.effectId = static_cast<uint16_t>(newVal);

    // Look up name
    const char* name = lookupEffectName(z.effectId);
    if (name) {
        strncpy(z.effectName, name, sizeof(z.effectName) - 1);
        z.effectName[sizeof(z.effectName) - 1] = '\0';
    }

    sendZoneEffect(_selectedZone, z.effectId);
    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::adjustPalette(int32_t delta) {
    ZoneState& z = _zones[_selectedZone];
    int32_t newVal = static_cast<int32_t>(z.paletteId) + (delta > 0 ? 1 : -1);

    // Wrapping behaviour
    if (newVal < 0) newVal = MAX_PALETTE_ID;
    if (newVal > MAX_PALETTE_ID) newVal = 0;

    z.paletteId = static_cast<uint8_t>(newVal);

    // Look up name
    const char* name = lookupPaletteName(z.paletteId);
    if (name) {
        strncpy(z.paletteName, name, sizeof(z.paletteName) - 1);
        z.paletteName[sizeof(z.paletteName) - 1] = '\0';
    }

    sendZonePalette(_selectedZone, z.paletteId);
    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::adjustSpeed(int32_t delta) {
    ZoneState& z = _zones[_selectedZone];
    int32_t newVal = static_cast<int32_t>(z.speed) + (delta > 0 ? 1 : -1);

    // Clamping behaviour
    if (newVal < MIN_SPEED) newVal = MIN_SPEED;
    if (newVal > MAX_SPEED) newVal = MAX_SPEED;

    z.speed = static_cast<uint8_t>(newVal);

    sendZoneSpeed(_selectedZone, z.speed);
    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::adjustBrightness(int32_t delta) {
    ZoneState& z = _zones[_selectedZone];
    int32_t newVal = static_cast<int32_t>(z.brightness) + (delta > 0 ? 1 : -1);

    // Clamping behaviour
    if (newVal < 0) newVal = 0;
    if (newVal > MAX_BRIGHTNESS) newVal = MAX_BRIGHTNESS;

    z.brightness = static_cast<uint8_t>(newVal);

    sendZoneBrightness(_selectedZone, z.brightness);
    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::adjustBlendMode(int32_t delta) {
    ZoneState& z = _zones[_selectedZone];
    int32_t newVal = static_cast<int32_t>(z.blendMode) + (delta > 0 ? 1 : -1);

    // Wrapping behaviour
    if (newVal < 0) newVal = MAX_BLEND_MODE;
    if (newVal > MAX_BLEND_MODE) newVal = 0;

    z.blendMode = static_cast<uint8_t>(newVal);

    // Set blend name from lookup
    if (z.blendMode < 8) {
        strncpy(z.blendModeName, kBlendModeNames[z.blendMode], sizeof(z.blendModeName) - 1);
        z.blendModeName[sizeof(z.blendModeName) - 1] = '\0';
    }

    sendZoneBlendMode(_selectedZone, z.blendMode);
    updateParamCards();
    updateOverviewCards();
    markDirty();
}

void ZoneComposerUI::toggleZoneMode() {
    _zonesEnabled = !_zonesEnabled;
    sendZoneEnable(_zonesEnabled);
    updateZoneEnableVisuals();
    markDirty();

    Serial.printf("[ZoneComposer] Zones %s\n", _zonesEnabled ? "enabled" : "disabled");
}

// ============================================================================
// handleEncoderChange() — Global index 8-15 → local 0-7
// ============================================================================

void ZoneComposerUI::handleEncoderChange(uint8_t encoderIndex, int32_t delta) {
    // Translate global encoder index to local (Unit B: 8-15 → 0-7)
    if (encoderIndex < ENC_BASE || encoderIndex > ENC_BASE + 7) return;
    uint8_t local = encoderIndex - ENC_BASE;

    switch (local) {
        case ENC_EFFECT:
            adjustEffect(delta);
            break;
        case ENC_PALETTE:
            adjustPalette(delta);
            break;
        case ENC_SPEED:
            adjustSpeed(delta);
            break;
        case ENC_BRIGHTNESS:
            adjustBrightness(delta);
            break;
        case ENC_BLEND:
            adjustBlendMode(delta);
            break;
        case ENC_ZONE_SEL: {
            // Cycle zone selection: 0→1→2→0 (or 0→1→0 in 2-zone mode)
            uint8_t next = (_selectedZone + (delta > 0 ? 1 : (_zoneCount - 1))) % _zoneCount;
            selectZone(next);
            break;
        }
        case ENC_ZONE_COUNT:
            adjustZoneCount(delta);
            break;
        case ENC_ZONE_MODE:
            // Rotation on ENC 7 is reserved — no action
            break;
    }
}

// ============================================================================
// handleEncoderClick() — Button press handling
// ============================================================================

void ZoneComposerUI::handleEncoderClick(uint8_t encoderIndex) {
    if (encoderIndex < ENC_BASE || encoderIndex > ENC_BASE + 7) return;
    uint8_t local = encoderIndex - ENC_BASE;

    switch (local) {
        case ENC_ZONE_MODE:
            toggleZoneMode();
            break;
        default:
            // No click action defined for other encoders in v2
            break;
    }
}

// ============================================================================
// handleTouch() — Touch input routing (for non-LVGL touch paths)
// ============================================================================

void ZoneComposerUI::handleTouch(int16_t x, int16_t y) {
    // LVGL handles touch events via callbacks registered on clickable widgets.
    // This method exists for any fallback touch routing from main.cpp.
    // Currently a no-op — all interactions are handled by LVGL event system.
    (void)x;
    (void)y;
}

// ============================================================================
// WebSocket Command Senders (rate-limited)
// ============================================================================

static uint32_t s_lastWsSend[8] = {0};  // Per-encoder rate limit timestamps

static bool wsCanSend(uint8_t paramIndex) {
    if (paramIndex >= 8) return false;
    uint32_t now = millis();
    if (now - s_lastWsSend[paramIndex] < WS_THROTTLE_MS) return false;
    s_lastWsSend[paramIndex] = now;
    return true;
}

void ZoneComposerUI::sendZoneEffect(uint8_t zoneId, uint16_t effectId) {
    if (!wsCanSend(ENC_EFFECT)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneEffect(zoneId, effectId);
    }
}

void ZoneComposerUI::sendZonePalette(uint8_t zoneId, uint8_t paletteId) {
    if (!wsCanSend(ENC_PALETTE)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZonePalette(zoneId, paletteId);
    }
}

void ZoneComposerUI::sendZoneSpeed(uint8_t zoneId, uint8_t speed) {
    if (!wsCanSend(ENC_SPEED)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneSpeed(zoneId, speed);
    }
}

void ZoneComposerUI::sendZoneBrightness(uint8_t zoneId, uint8_t brightness) {
    if (!wsCanSend(ENC_BRIGHTNESS)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneBrightness(zoneId, brightness);
    }
}

void ZoneComposerUI::sendZoneBlendMode(uint8_t zoneId, uint8_t blendMode) {
    if (!wsCanSend(ENC_BLEND)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneBlend(zoneId, blendMode);
    }
}

void ZoneComposerUI::sendZoneEnable(bool enable) {
    if (!wsCanSend(ENC_ZONE_MODE)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneEnable(enable);
    }
}

void ZoneComposerUI::sendZoneLoadPreset(uint8_t presetId) {
    if (!wsCanSend(ENC_ZONE_COUNT)) return;
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->sendZoneLoadPreset(presetId);
    }
}

void ZoneComposerUI::sendRequestZonesState() {
    if (_wsClient && _wsClient->isConnected()) {
        _wsClient->requestZonesState();
    }
}

// ============================================================================
// LVGL Static Callbacks
// ============================================================================

void ZoneComposerUI::backButtonCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (self && self->_backButtonCallback) {
        self->_backButtonCallback();
    }
}

void ZoneComposerUI::zoneEnableCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (self) {
        self->toggleZoneMode();
    }
}

void ZoneComposerUI::zoneCountCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (self) {
        // Touch tap cycles: toggle between 2 and 3
        self->adjustZoneCount(0);
    }
}

void ZoneComposerUI::zoneSelectorCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t idx = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    self->selectZone(idx);
}

void ZoneComposerUI::stripSegmentCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t zoneIdx = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    self->selectZone(zoneIdx);
}

void ZoneComposerUI::overviewCardCb(lv_event_t* e) {
    auto* self = static_cast<ZoneComposerUI*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint8_t zoneIdx = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    self->selectZone(zoneIdx);
}

// ============================================================================
// Helpers
// ============================================================================

uint32_t ZoneComposerUI::getZoneColour(uint8_t zoneId) const {
    if (zoneId < 3) return ZoneColours::ALL[zoneId];
    return DesignTokens::FG_DIMMED;
}

const char* ZoneComposerUI::getZoneRole(uint8_t zoneId) const {
    if (_zoneCount == 3) {
        return (zoneId < 3) ? ZoneRoles::ROLES_3[zoneId] : "---";
    }
    // 2-zone mode
    return (zoneId < 2) ? ZoneRoles::ROLES_2[zoneId] : "---";
}

int ZoneComposerUI::calcSegmentX(uint8_t ledIndex) const {
    // LED index → pixel X position within the strip bar
    // Strip bar is STRIP_BAR_WIDTH (1160) pixels for LEDS_PER_STRIP (160) LEDs
    return static_cast<int>((static_cast<int32_t>(ledIndex) * STRIP_BAR_WIDTH) / LEDS_PER_STRIP);
}

int ZoneComposerUI::calcSegmentWidth(uint8_t ledStart, uint8_t ledEnd) const {
    // Width in pixels for a span of LEDs (inclusive range)
    int x0 = calcSegmentX(ledStart);
    int x1 = calcSegmentX(ledEnd) + (STRIP_BAR_WIDTH / LEDS_PER_STRIP);
    return x1 - x0;
}

#endif // TAB5_ENCODER_USE_LVGL && !SIMULATOR_BUILD
