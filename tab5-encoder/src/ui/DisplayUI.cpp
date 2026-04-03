// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
// ============================================================================
// DisplayUI - Main UI controller implementation
// ============================================================================
// 4x4 grid of encoder gauges with status bar
// Clean sprite-based rendering, no bullshit animations
// ============================================================================

#include "DisplayUI.h"

#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_task_wdt.h>
#include "../presets/PresetManager.h"
#include "../hal/EspHal.h"
#include "../network/WiFiManager.h"
#include "fonts/bebas_neue_fonts.h"
#include "fonts/experimental_fonts.h"
#include "ZoneComposerUI.h"
#include "ConnectivityTab.h"
#include "ControlSurfaceUI.h"
#include "DesignTokens.h"

// Forward declarations - instances in main.cpp
extern WiFiManager g_wifiManager;

#include "../input/ButtonHandler.h"
extern ButtonHandler* g_buttonHandler;


// Forward declaration
static void formatDuration(uint32_t seconds, char* buf, size_t bufSize);

// Static member initialization
DisplayUI* DisplayUI::s_instance = nullptr;


// Display order: Effect, Palette, Speed, Mood, Fade, Complexity, Variation, Brightness
static constexpr const char* kParamNames[8] = {
    "EFFECT", "PALETTE", "SPEED", "MOOD", "FADE", "COMPLEXITY", "VARIATION", "BRIGHTNESS"};

// Mapping from encoder index (0-7) to display position (0-7)
// Encoder indices: 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=Fade, 5=Complexity, 6=Variation, 7=Brightness
// Display order: 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=Fade, 5=Complexity, 6=Variation, 7=Brightness
static constexpr uint8_t kEncoderToDisplayPos[8] = {
    0,  // Encoder 0 (Effect) -> Display 0
    1,  // Encoder 1 (Palette) -> Display 1
    2,  // Encoder 2 (Speed) -> Display 2
    3,  // Encoder 3 (Mood) -> Display 3
    4,  // Encoder 4 (Fade) -> Display 4
    5,  // Encoder 5 (Complexity) -> Display 5
    6,  // Encoder 6 (Variation) -> Display 6
    7   // Encoder 7 (Brightness) -> Display 7
};

DisplayUI::DisplayUI(M5GFX& display) : _display(display) {
    s_instance = this;
}

DisplayUI::~DisplayUI() {
    if (_zoneComposer) {
        delete _zoneComposer;
        _zoneComposer = nullptr;
    }
    if (_connectivityTab) {
        delete _connectivityTab;
        _connectivityTab = nullptr;
    }
    if (_controlSurface) {
        delete _controlSurface;
        _controlSurface = nullptr;
    }
    if (_sidebar) {
        delete _sidebar;
        _sidebar = nullptr;
    }
    if (_screen_global) lv_obj_del(_screen_global);
    if (_screen_zone) lv_obj_del(_screen_zone);
    if (_screen_connectivity) lv_obj_del(_screen_connectivity);
    if (_screen_control_surface) lv_obj_del(_screen_control_surface);
    _screen_global = nullptr;
    _screen_zone = nullptr;
    _screen_connectivity = nullptr;
    _screen_control_surface = nullptr;
}

void DisplayUI::begin() {
    if (!LVGLBridge::init()) return;

    _screen_global = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_global, lv_color_hex(DesignTokens::BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_global, 0, LV_PART_MAIN);

    _header = lv_obj_create(_screen_global);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Header creation - screen width=1280, border_width=2, margin=7px\n");
        // #endregion
    // CRITICAL FIX: Use very conservative width to prevent right-side clipping
    // Screen width = 1280px, border = 2px, need margin on both sides
    // Maximum safe width = 1280 - left_margin - right_margin
    // Using 10px margin each side: width = 1280 - 10 - 10 = 1260
    lv_obj_set_size(_header, 1260, DesignTokens::STATUSBAR_HEIGHT);
    // Position with 10px margin from left edge (more than 7px to ensure no clipping)
    lv_obj_set_pos(_header, 10, 7);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Header: pos=(%d,%d), size=(%d,%d), right_edge=%d, margin_left=10, margin_right=%d\n", 
                  // 10, 7, 1260, DesignTokens::STATUSBAR_HEIGHT, 10 + 1260, 1280 - (10 + 1260));
        // #endregion
    lv_obj_set_style_bg_color(_header, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    // White border and rounded corners matching src/src/deck_ui.cpp (2px instead of 3px)
    lv_obj_set_style_border_width(_header, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_header, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(_header, 14, LV_PART_MAIN);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Header after styling: actual_width=%d, actual_height=%d, border_width=%d\n",
                  // (int)lv_obj_get_width(_header), (int)lv_obj_get_height(_header),
                  // (int)lv_obj_get_style_border_width(_header, LV_PART_MAIN));
        // #endregion
    lv_obj_set_style_pad_left(_header, DesignTokens::GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_header, DesignTokens::GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_header, 16, LV_PART_MAIN);  // Equal top and bottom padding for proper centering
    lv_obj_set_style_pad_bottom(_header, 16, LV_PART_MAIN);  // Equal top and bottom padding for proper centering
    lv_obj_set_layout(_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_header, LV_OBJ_FLAG_SCROLLABLE);

    // Pattern container (fixed width to prevent shifting) - moved to first position
    _header_effect_container = lv_obj_create(_header);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(_header_effect_container, 220, 24);
    // Explicitly set background color to match header (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(_header_effect_container, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_header_effect_container, LV_OPA_TRANSP, LV_PART_MAIN);
    // Clip overflow to prevent any rendering outside the container bounds
    // Clear borders on ALL parts and ALL states
    lv_obj_set_style_border_width(_header_effect_container, 0, LV_PART_MAIN);
    // Clear borders on other parts too
    lv_obj_set_style_border_width(_header_effect_container, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(_header_effect_container, 0, LV_PART_INDICATOR);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(_header_effect_container, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_header_effect_container, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(_header_effect_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_effect_container, 0, LV_PART_MAIN); // No left padding - header padding provides edge spacing
    lv_obj_clear_flag(_header_effect_container, LV_OBJ_FLAG_SCROLLABLE);

    _header_effect = lv_label_create(_header_effect_container);
    lv_label_set_text(_header_effect, "");
    lv_obj_set_style_text_font(_header_effect, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_effect, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Pattern label created, checking styles before clear\n");
        // #endregion
    // Clear borders, text decoration, outline, and shadow
    lv_obj_set_style_border_width(_header_effect, 0, LV_PART_MAIN);
    lv_obj_set_style_text_decor(_header_effect, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    lv_obj_set_style_outline_width(_header_effect, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_header_effect, 0, LV_PART_MAIN);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Pattern label: AFTER clear - border=%d, text_decor=%d, outline=%d, shadow=%d\n", 
                  // (int)lv_obj_get_style_border_width(_header_effect, LV_PART_MAIN),
                  // (int)lv_obj_get_style_text_decor(_header_effect, LV_PART_MAIN),
                  // (int)lv_obj_get_style_outline_width(_header_effect, LV_PART_MAIN),
                  // (int)lv_obj_get_style_shadow_width(_header_effect, LV_PART_MAIN));
        // #endregion
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Pattern label: BEFORE setting long_mode, checking if LV_LABEL_LONG_DOT causes underline\n");
        // #endregion
    // Try LV_LABEL_LONG_SCROLL instead of LV_LABEL_LONG_DOT to see if that's causing the line
    lv_label_set_long_mode(_header_effect, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Pattern label: Changed to LV_LABEL_LONG_SCROLL_CIRCULAR, checking text_decor=%d\n", 
                  // (int)lv_obj_get_style_text_decor(_header_effect, LV_PART_MAIN));
        // #endregion
    lv_obj_set_width(_header_effect, 220);

    // Click handler: navigate to Control Surface screen
    lv_obj_add_flag(_header_effect_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_header_effect_container, [](lv_event_t* e) {
        DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
        if (ui) {
            ui->setScreen(UIScreen::CONTROL_SURFACE);
        }
    }, LV_EVENT_CLICKED, this);

    // Palette container (fixed width to prevent shifting) - moved to second position
    _header_palette_container = lv_obj_create(_header);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(_header_palette_container, 220, 24);
    // Explicitly set background color to match header (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(_header_palette_container, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_header_palette_container, LV_OPA_TRANSP, LV_PART_MAIN);
    // Clip overflow to prevent any rendering outside the container bounds
    // Clear borders on ALL parts
    lv_obj_set_style_border_width(_header_palette_container, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(_header_palette_container, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(_header_palette_container, 0, LV_PART_INDICATOR);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(_header_palette_container, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_header_palette_container, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(_header_palette_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_palette_container, 10, LV_PART_MAIN); // 10px gap after Effect container
    lv_obj_clear_flag(_header_palette_container, LV_OBJ_FLAG_SCROLLABLE);

    _header_palette = lv_label_create(_header_palette_container);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Palette label created - checking default styles: border=%d, text_decor=%d, outline_width=%d\n", 
                  // (int)lv_obj_get_style_border_width(_header_palette, LV_PART_MAIN),
                  // (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN),
                  // (int)lv_obj_get_style_outline_width(_header_palette, LV_PART_MAIN));
        // #endregion
    lv_label_set_text(_header_palette, "");
    lv_obj_set_style_text_font(_header_palette, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_palette, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    // Clear borders, text decoration, outline, and shadow
    lv_obj_set_style_border_width(_header_palette, 0, LV_PART_MAIN);
    lv_obj_set_style_text_decor(_header_palette, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    lv_obj_set_style_outline_width(_header_palette, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_header_palette, 0, LV_PART_MAIN);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Palette label: AFTER clear - border=%d, text_decor=%d, outline=%d, shadow=%d\n", 
                  // (int)lv_obj_get_style_border_width(_header_palette, LV_PART_MAIN),
                  // (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN),
                  // (int)lv_obj_get_style_outline_width(_header_palette, LV_PART_MAIN),
                  // (int)lv_obj_get_style_shadow_width(_header_palette, LV_PART_MAIN));
        // #endregion
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Palette label: BEFORE setting long_mode, checking if LV_LABEL_LONG_DOT causes underline\n");
        // #endregion
    // Try LV_LABEL_LONG_SCROLL instead of LV_LABEL_LONG_DOT to see if that's causing the line
    lv_label_set_long_mode(_header_palette, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // #region agent log (DISABLED)
    // Serial.printf("[DEBUG] Palette label: Changed to LV_LABEL_LONG_SCROLL_CIRCULAR, checking text_decor=%d\n", 
                  // (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN));
        // #endregion
    lv_obj_set_width(_header_palette, 220);

    // Title container - use absolute centering for true center position
    // This ensures the title stays centered regardless of left/right content changes
    lv_obj_t* title_container = lv_obj_create(_header);
    lv_obj_remove_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(title_container, LV_OBJ_FLAG_IGNORE_LAYOUT);  // Break out of flex, use absolute position
    lv_obj_set_style_bg_opa(title_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_container, 0, LV_PART_MAIN);
    lv_obj_set_size(title_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(title_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_align(title_container, LV_ALIGN_CENTER, 0, 0);  // TRUE CENTER (absolute)

    // LIGHTWAVEOS title inside centered container
    _header_title_main = lv_label_create(title_container);
    lv_label_set_text(_header_title_main, "LIGHTWAVE");
    lv_obj_set_style_text_font(_header_title_main, &bebas_neue_40px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_title_main, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_align(_header_title_main, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    // Ensure consistent vertical alignment
    lv_obj_set_style_pad_top(_header_title_main, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_title_main, 0, LV_PART_MAIN);

    _header_title_os = lv_label_create(title_container);
    lv_label_set_text(_header_title_os, "OS");
    lv_obj_set_style_text_font(_header_title_os, &bebas_neue_40px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_title_os, lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_align(_header_title_os, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    // Match padding to align baseline with main title
    lv_obj_set_style_pad_top(_header_title_os, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_title_os, 0, LV_PART_MAIN);

    // Spacer to push network info to the right
    lv_obj_t* right_spacer = lv_obj_create(_header);
    lv_obj_set_style_bg_opa(right_spacer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_spacer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_spacer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(right_spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(right_spacer, 1);

    // Network info: SSID (RSSI) IP - wrapped in clickable container for navigation
    _header_net_container = lv_obj_create(_header);
    lv_obj_set_size(_header_net_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // CRITICAL: fit content only
    lv_obj_set_style_bg_opa(_header_net_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_header_net_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_header_net_container, LV_OBJ_FLAG_SCROLLABLE);  // CRITICAL: disable scrolling
    lv_obj_add_flag(_header_net_container, LV_OBJ_FLAG_CLICKABLE);

    // Touch target accessibility: 44-48px minimum (add vertical padding)
    lv_obj_set_style_pad_top(_header_net_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_net_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_header_net_container, 8, LV_PART_MAIN);

    // Visual feedback on press
    lv_obj_set_style_bg_opa(_header_net_container, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(_header_net_container, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_radius(_header_net_container, 8, LV_PART_MAIN);

    // Flex layout for horizontal arrangement
    lv_obj_set_layout(_header_net_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_header_net_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_header_net_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // SSID label (inside container)
    _header_net_ssid = lv_label_create(_header_net_container);
    lv_label_set_text(_header_net_ssid, "");
    lv_obj_set_style_text_font(_header_net_ssid, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);

    // RSSI label (inside container)
    _header_net_rssi = lv_label_create(_header_net_container);
    lv_label_set_text(_header_net_rssi, "");
    lv_obj_set_style_text_font(_header_net_rssi, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_rssi, 4, LV_PART_MAIN);

    // IP label (inside container)
    _header_net_ip = lv_label_create(_header_net_container);
    lv_label_set_text(_header_net_ip, "");
    lv_obj_set_style_text_font(_header_net_ip, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_ip, 8, LV_PART_MAIN);

    // Click handler: navigate to ConnectivityTab
    lv_obj_add_event_cb(_header_net_container, [](lv_event_t* e) {
        DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
        if (ui) {
            Serial.println("[DisplayUI] Network info tapped - switching to Connectivity");
            ui->setScreen(UIScreen::CONNECTIVITY);
        }
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));

    // Create retry button (initially hidden) - moved to last position
    _header_retry_button = lv_label_create(_header);
    lv_label_set_text(_header_retry_button, "RETRY");
    lv_obj_set_style_text_font(_header_retry_button, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_retry_button, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_retry_button, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_header_retry_button, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_header_retry_button, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_retry_button, 4, LV_PART_MAIN);
    lv_obj_add_flag(_header_retry_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_header_retry_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(_header_retry_button, LV_OPA_TRANSP, LV_PART_MAIN);
    
    // Add event handler for retry button click
    lv_obj_add_event_cb(_header_retry_button, [](lv_event_t* e) {
        DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
        if (ui && ui->_retry_callback) {
            ui->_retry_callback();
        }
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));

    // --- Sidebar (GLOBAL screen only) ------------------------------------
    esp_task_wdt_reset();
    _sidebar = new SidebarWidget();
    _sidebar->setTabCallback([](SidebarTab tab) {
        if (!DisplayUI::s_instance) return;
        auto* self = DisplayUI::s_instance;
        self->_currentTab = tab;

        // Show/hide bottom-zone panels
        if (self->_fx_panel) {
            if (tab == SidebarTab::FX_PARAMS)
                lv_obj_clear_flag(self->_fx_panel, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(self->_fx_panel, LV_OBJ_FLAG_HIDDEN);
        }
        if (self->_zones_panel) {
            if (tab == SidebarTab::ZONES)
                lv_obj_clear_flag(self->_zones_panel, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(self->_zones_panel, LV_OBJ_FLAG_HIDDEN);
        }
        if (self->_presets_panel) {
            if (tab == SidebarTab::PRESETS)
                lv_obj_clear_flag(self->_presets_panel, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(self->_presets_panel, LV_OBJ_FLAG_HIDDEN);
        }

        // Request fresh data from K1 when switching to zones or presets
        if (tab == SidebarTab::ZONES && self->_wsClient) {
            self->_wsClient->requestZonesState();
        }
        if (tab == SidebarTab::PRESETS && self->_wsClient) {
            self->_wsClient->requestEffectPresetsList();
        }

        // Phase 5 — apply neon colour to Unit B bars
        self->applyTabColour(tab);
    });
    _sidebar->begin(_screen_global);
    esp_task_wdt_reset();

    static constexpr lv_coord_t kGaugeRowHeight = 125;
    static constexpr lv_coord_t kModeRowHeight = 80;
    static constexpr lv_coord_t kTopZoneHeight = 232;

    // --- Top zone (persistent, always visible) ---
    esp_task_wdt_reset();
    _top_zone = lv_obj_create(_screen_global);
    lv_obj_set_pos(_top_zone, DesignTokens::SIDEBAR_WIDTH, 73);
    lv_obj_set_size(_top_zone, 1280 - DesignTokens::SIDEBAR_WIDTH, kTopZoneHeight);
    lv_obj_set_style_bg_color(_top_zone, lv_color_hex(DesignTokens::ROW_TOP), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_top_zone, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_top_zone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_top_zone, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_top_zone, 8, LV_PART_MAIN);
    lv_obj_clear_flag(_top_zone, LV_OBJ_FLAG_SCROLLABLE);

    // --- Gauges container (child of top zone) ---
    _gauges_container = lv_obj_create(_top_zone);
    lv_obj_set_size(_gauges_container, 1210 - 2 * 10, kGaugeRowHeight);
    lv_obj_align(_gauges_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(_gauges_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_gauges_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_gauges_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_gauges_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_gauges_container, LV_LAYOUT_GRID);

    static const lv_coord_t col_dsc[9] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[2] = {kGaugeRowHeight, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_gauges_container, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(_gauges_container, DesignTokens::GRID_GAP, LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        _gauge_cards[i] = make_card(_gauges_container, false);
        lv_obj_set_grid_cell(_gauge_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        _gauge_labels[i] = lv_label_create(_gauge_cards[i]);
        lv_label_set_text(_gauge_labels[i], kParamNames[i]);
        lv_obj_set_style_text_font(_gauge_labels[i], BEBAS_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_gauge_labels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_align(_gauge_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        _gauge_values[i] = lv_label_create(_gauge_cards[i]);
        lv_label_set_text(_gauge_values[i], "");
        lv_obj_set_style_text_font(_gauge_values[i], JETBRAINS_MONO_REG_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(_gauge_values[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_gauge_values[i], LV_ALIGN_TOP_MID, 0, 30);

        _gauge_bars[i] = lv_bar_create(_gauge_cards[i]);
        lv_bar_set_range(_gauge_bars[i], 0, 255);
        lv_bar_set_value(_gauge_bars[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(_gauge_bars[i], LV_PCT(90), 10);
        lv_obj_align(_gauge_bars[i], LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_color(_gauge_bars[i], lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(_gauge_bars[i], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_INDICATOR);
    }

    // --- Separator line (must precede panel creation) ---
    _separator = lv_obj_create(_screen_global);
    lv_obj_set_pos(_separator, DesignTokens::SIDEBAR_WIDTH, 73 + kTopZoneHeight);
    lv_obj_set_size(_separator, 1280 - DesignTokens::SIDEBAR_WIDTH, 1);
    lv_obj_set_style_bg_color(_separator, lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_separator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_separator, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_separator, LV_OBJ_FLAG_SCROLLABLE);

    // --- Bottom zone (parent for tab content panels) ---
    _bottom_zone = lv_obj_create(_screen_global);
    lv_obj_set_pos(_bottom_zone, DesignTokens::SIDEBAR_WIDTH, 73 + kTopZoneHeight + 1);
    lv_obj_set_size(_bottom_zone, 1280 - DesignTokens::SIDEBAR_WIDTH,
                    720 - 73 - kTopZoneHeight - 1 - 69);
    lv_obj_set_style_bg_color(_bottom_zone, lv_color_hex(DesignTokens::ROW_BOTTOM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_bottom_zone, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_bottom_zone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_bottom_zone, 10, LV_PART_MAIN);
    lv_obj_clear_flag(_bottom_zone, LV_OBJ_FLAG_SCROLLABLE);
    esp_task_wdt_reset();

    // =================================================================
    // Phase 4 — Tab content panels (children of _bottom_zone)
    // =================================================================

    // --- Panel 1: FX PARAMS (8-column grid, encoders 8-15) ---
    _fx_panel = lv_obj_create(_bottom_zone);
    lv_obj_set_size(_fx_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_fx_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_fx_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_fx_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_fx_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_fx_panel, LV_LAYOUT_GRID);

    static const lv_coord_t fx_col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t fx_row_dsc[2] = {150, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_fx_panel, fx_col_dsc, fx_row_dsc);
    lv_obj_set_style_pad_column(_fx_panel, DesignTokens::GRID_GAP, LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        _fxa_cards[i] = make_card(_fx_panel, false);
        lv_obj_set_grid_cell(_fxa_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        char defaultLabel[8];
        snprintf(defaultLabel, sizeof(defaultLabel), "FX%d", i + 1);
        _fxa_labels[i] = lv_label_create(_fxa_cards[i]);
        lv_label_set_text(_fxa_labels[i], defaultLabel);
        lv_obj_set_style_text_font(_fxa_labels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_fxa_labels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_width(_fxa_labels[i], LV_PCT(100));
        lv_label_set_long_mode(_fxa_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(_fxa_labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(_fxa_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        _fxa_values[i] = lv_label_create(_fxa_cards[i]);
        lv_label_set_text(_fxa_values[i], "");
        lv_obj_set_style_text_font(_fxa_values[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_fxa_values[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_fxa_values[i], LV_ALIGN_TOP_MID, 0, 50);

        _fxa_bars[i] = lv_bar_create(_fxa_cards[i]);
        lv_bar_set_range(_fxa_bars[i], 0, 255);
        lv_bar_set_value(_fxa_bars[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(_fxa_bars[i], LV_PCT(90), 8);
        lv_obj_align(_fxa_bars[i], LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_style_bg_color(_fxa_bars[i], lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(_fxa_bars[i], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_fxa_bars[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(_fxa_bars[i], 8, LV_PART_INDICATOR);
    }
    // FX panel visible by default (matches initial _currentTab = FX_PARAMS)

    esp_task_wdt_reset();

    // --- Panel 2: ZONES (zone selector strip + 8-column param grid) ---
    _zones_panel = lv_obj_create(_bottom_zone);
    lv_obj_set_size(_zones_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_zones_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_zones_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_zones_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_zones_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_zones_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_zones_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_zones_panel, 8, LV_PART_MAIN);
    lv_obj_add_flag(_zones_panel, LV_OBJ_FLAG_HIDDEN);  // Hidden initially

    // Zone selector strip (3 equal buttons in a flex row)
    lv_obj_t* zone_selector = lv_obj_create(_zones_panel);
    lv_obj_set_size(zone_selector, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(zone_selector, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(zone_selector, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(zone_selector, 0, LV_PART_MAIN);
    lv_obj_clear_flag(zone_selector, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(zone_selector, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(zone_selector, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(zone_selector, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(zone_selector, DesignTokens::GRID_GAP, LV_PART_MAIN);

    // Zone Mode toggle button (first in row — enables/disables zone rendering on K1)
    _zone_mode_btn = lv_obj_create(zone_selector);
    lv_obj_set_size(_zone_mode_btn, 140, 44);
    lv_obj_set_style_bg_color(_zone_mode_btn,
                              lv_color_hex(_zonesEnabled ? 0x22CC44 : 0x882222),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_zone_mode_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_zone_mode_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_zone_mode_btn,
                                  lv_color_hex(_zonesEnabled ? 0x22CC44 : 0x882222),
                                  LV_PART_MAIN);
    lv_obj_set_style_radius(_zone_mode_btn, DesignTokens::CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_zone_mode_btn, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_zone_mode_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_zone_mode_btn, LV_OBJ_FLAG_CLICKABLE);

    _zone_mode_label = lv_label_create(_zone_mode_btn);
    lv_label_set_text(_zone_mode_label, "ZONES");
    lv_obj_set_style_text_font(_zone_mode_label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_zone_mode_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(_zone_mode_label);

    lv_obj_add_event_cb(_zone_mode_btn, [](lv_event_t* e) {
        DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
        if (!ui || !ui->_wsClient || !ui->_wsClient->isConnected()) return;

        ui->_zonesEnabled = !ui->_zonesEnabled;
        ui->updateZoneModeButton(ui->_zonesEnabled);

        if (ui->_zonesEnabled) {
            // Centre-origin layouts for 1, 2, and 3 zones
            static const zones::ZoneSegment LAYOUT_1[1] = {
                {0, 0, 79, 80, 159, 160}
            };
            static const zones::ZoneSegment LAYOUT_2[2] = {
                {0, 40, 79, 80, 119, 80},
                {1, 0, 39, 120, 159, 80}
            };
            static const zones::ZoneSegment LAYOUT_3[3] = {
                {0, 53, 79, 80, 106, 54},   // Zone 0: inner (27 LEDs/side)
                {1, 26, 52, 107, 133, 54},   // Zone 1: middle (27 LEDs/side)
                {2, 0, 25, 134, 159, 52}     // Zone 2: outer (26 LEDs/side)
            };

            const zones::ZoneSegment* layout = LAYOUT_2;
            uint8_t count = ui->_zoneCount;
            if (count == 1) layout = LAYOUT_1;
            else if (count == 3) layout = LAYOUT_3;

            // 1: Send layout
            ui->_wsClient->sendZonesSetLayout(layout, count);
            Serial.printf("[DisplayUI] zones.setLayout (%u zones)\n", count);

            // 2: Enable
            ui->_wsClient->sendZoneEnable(true);
            Serial.println("[DisplayUI] zone.enable=true");

            // 3: Assign current effect to all zones so they render
            uint16_t eid = ui->_wsClient->getCurrentEffectId();
            if (eid == 0) eid = 0x0100;
            for (uint8_t z = 0; z < count; z++) {
                ui->_wsClient->sendZoneEffect(z, eid);
                Serial.printf("[DisplayUI] zone.setEffect z=%u eid=0x%04X\n", z, eid);
            }
        } else {
            ui->_wsClient->sendZoneEnable(false);
            Serial.println("[DisplayUI] zone.enable=false");
        }
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));

    const char* zoneLabels[] = {"ZONE 1", "ZONE 2", "ZONE 3"};
    for (int i = 0; i < 3; i++) {
        _zone_selector_buttons[i] = lv_obj_create(zone_selector);
        lv_obj_set_flex_grow(_zone_selector_buttons[i], 1);
        lv_obj_set_height(_zone_selector_buttons[i], 44);
        lv_obj_set_style_bg_color(_zone_selector_buttons[i], lv_color_hex(DesignTokens::SURFACE_BASE), LV_PART_MAIN);
        lv_obj_set_style_border_width(_zone_selector_buttons[i], 2, LV_PART_MAIN);
        // First zone active by default
        lv_obj_set_style_border_color(_zone_selector_buttons[i],
            lv_color_hex(i == 0 ? DesignTokens::NEON_PURPLE : DesignTokens::BORDER_SUBTLE),
            LV_PART_MAIN);
        lv_obj_set_style_radius(_zone_selector_buttons[i], DesignTokens::CARD_RADIUS, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_zone_selector_buttons[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(_zone_selector_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_zone_selector_buttons[i], LV_OBJ_FLAG_CLICKABLE);

        _zone_selector_labels[i] = lv_label_create(_zone_selector_buttons[i]);
        lv_label_set_text(_zone_selector_labels[i], zoneLabels[i]);
        lv_obj_set_style_text_font(_zone_selector_labels[i], RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_zone_selector_labels[i],
            lv_color_hex(i == 0 ? DesignTokens::NEON_PURPLE : DesignTokens::FG_SECONDARY),
            LV_PART_MAIN);
        lv_obj_center(_zone_selector_labels[i]);
        lv_obj_clear_flag(_zone_selector_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_zone_selector_labels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // Click handler — store zone index on widget, recover this via event user_data
        lv_obj_set_user_data(_zone_selector_buttons[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_zone_selector_buttons[i], [](lv_event_t* e) {
            lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
            DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
            if (!ui) return;
            ui->_selectedZone = idx;
            // Update all three buttons
            for (int z = 0; z < 3; z++) {
                if (!ui->_zone_selector_buttons[z]) continue;
                bool active = (z == idx);
                lv_obj_set_style_border_color(ui->_zone_selector_buttons[z],
                    lv_color_hex(active ? DesignTokens::NEON_PURPLE : DesignTokens::BORDER_SUBTLE),
                    LV_PART_MAIN);
                if (ui->_zone_selector_labels[z]) {
                    lv_obj_set_style_text_color(ui->_zone_selector_labels[z],
                        lv_color_hex(active ? DesignTokens::NEON_PURPLE : DesignTokens::FG_SECONDARY),
                        LV_PART_MAIN);
                }
            }
            // Refresh zone param value labels from cached state
            const auto& zs = ui->_zoneSidebarState[idx];
            char buf[16];
            if (ui->_zone_param_values[0]) lv_label_set_text(ui->_zone_param_values[0], zs.effectName);
            if (ui->_zone_param_values[1]) { snprintf(buf, sizeof(buf), "%u", zs.speed);      lv_label_set_text(ui->_zone_param_values[1], buf); }
            if (ui->_zone_param_values[2]) lv_label_set_text(ui->_zone_param_values[2], zs.paletteName);
            if (ui->_zone_param_values[3]) { snprintf(buf, sizeof(buf), "%u", zs.blendMode);  lv_label_set_text(ui->_zone_param_values[3], buf); }
            if (ui->_zone_param_values[4]) { snprintf(buf, sizeof(buf), "%u", zs.brightness); lv_label_set_text(ui->_zone_param_values[4], buf); }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
    }

    // Zone params grid (8-column, fills remaining height)
    lv_obj_t* zone_grid = lv_obj_create(_zones_panel);
    lv_obj_set_size(zone_grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(zone_grid, 1);
    lv_obj_set_style_bg_opa(zone_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(zone_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(zone_grid, 0, LV_PART_MAIN);
    lv_obj_clear_flag(zone_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(zone_grid, LV_LAYOUT_GRID);

    static const lv_coord_t zone_col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t zone_row_dsc[2] = {125, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(zone_grid, zone_col_dsc, zone_row_dsc);
    lv_obj_set_style_pad_column(zone_grid, DesignTokens::GRID_GAP, LV_PART_MAIN);

    const char* zoneParamNames[] = {"EFFECT", "SPEED", "PALETTE", "BLEND", "BRIGHTNESS", "ZONES", "LED COUNT", ""};
    for (int i = 0; i < 8; i++) {
        _zone_param_cards[i] = make_card(zone_grid, false);
        lv_obj_set_grid_cell(_zone_param_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        // Dim empty placeholder card (index 7 only)
        if (i >= 7) {
            lv_obj_set_style_bg_opa(_zone_param_cards[i], LV_OPA_40, LV_PART_MAIN);
            lv_obj_set_style_border_color(_zone_param_cards[i],
                lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
        }

        _zone_param_labels[i] = lv_label_create(_zone_param_cards[i]);
        lv_label_set_text(_zone_param_labels[i], zoneParamNames[i]);
        lv_obj_set_style_text_font(_zone_param_labels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_zone_param_labels[i],
            lv_color_hex(i < 5 ? DesignTokens::FG_SECONDARY : (i < 7 ? DesignTokens::NEON_PURPLE : DesignTokens::FG_DIMMED)),
            LV_PART_MAIN);
        lv_obj_align(_zone_param_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        _zone_param_values[i] = lv_label_create(_zone_param_cards[i]);
        lv_label_set_text(_zone_param_values[i], "");
        lv_obj_set_style_text_font(_zone_param_values[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_zone_param_values[i],
            lv_color_hex(i < 5 ? DesignTokens::FG_PRIMARY : (i < 7 ? DesignTokens::NEON_PURPLE : DesignTokens::FG_DIMMED)),
            LV_PART_MAIN);
        lv_obj_align(_zone_param_values[i], LV_ALIGN_CENTER, 0, 8);
    }

    // Style zone count card (slot 5) — purple accent, encoder-only (no touch)
    if (_zone_param_cards[5]) {
        lv_obj_set_style_border_color(_zone_param_cards[5], lv_color_hex(DesignTokens::NEON_PURPLE), LV_PART_MAIN);
        lv_obj_clear_flag(_zone_param_cards[5], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_zone_param_cards[5], LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_clear_flag(_zone_param_cards[5], LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    // Style LED count card (slot 6) — purple accent, encoder-only (no touch)
    if (_zone_param_cards[6]) {
        lv_obj_set_style_border_color(_zone_param_cards[6], lv_color_hex(DesignTokens::NEON_PURPLE), LV_PART_MAIN);
        lv_obj_clear_flag(_zone_param_cards[6], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_zone_param_cards[6], LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_clear_flag(_zone_param_cards[6], LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    // Set initial values for zone count and preset cards
    if (_zone_param_values[5]) lv_label_set_text(_zone_param_values[5], "2");
    if (_zone_param_values[6]) lv_label_set_text(_zone_param_values[6], "80 LEDS");

    esp_task_wdt_reset();

    // --- Panel 3: PRESETS (8-column grid, preset cards) ---
    _presets_panel = lv_obj_create(_bottom_zone);
    lv_obj_set_size(_presets_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_presets_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_presets_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_presets_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_presets_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_presets_panel, LV_LAYOUT_GRID);
    lv_obj_add_flag(_presets_panel, LV_OBJ_FLAG_HIDDEN);  // Hidden initially

    static const lv_coord_t preset_col_dsc[9] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t preset_row_dsc[2] = {125, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_presets_panel, preset_col_dsc, preset_row_dsc);
    lv_obj_set_style_pad_column(_presets_panel, DesignTokens::GRID_GAP, LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        _preset_cards[i] = make_card(_presets_panel, true);
        lv_obj_set_grid_cell(_preset_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_flag(_preset_cards[i], LV_OBJ_FLAG_CLICKABLE);

        char pLabel[4];
        snprintf(pLabel, sizeof(pLabel), "P%d", i + 1);
        _preset_labels[i] = lv_label_create(_preset_cards[i]);
        lv_label_set_text(_preset_labels[i], pLabel);
        lv_obj_set_style_text_font(_preset_labels[i], &bebas_neue_24px, LV_PART_MAIN);
        lv_obj_set_style_text_color(_preset_labels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_align(_preset_labels[i], LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_clear_flag(_preset_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_preset_labels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        _preset_values[i] = lv_label_create(_preset_cards[i]);
        lv_label_set_text(_preset_values[i], "");
        lv_obj_set_style_text_font(_preset_values[i], JETBRAINS_MONO_REG_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_preset_values[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_preset_values[i], LV_ALIGN_CENTER, 0, 8);
        lv_obj_clear_flag(_preset_values[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_preset_values[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // Click-to-load handler — load preset if occupied
        lv_obj_set_user_data(_preset_cards[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_preset_cards[i], [](lv_event_t* e) {
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint8_t slot = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
            auto* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
            if (!ui || !ui->_wsClient || slot >= 8) return;
            if (ui->_presetSidebarSlots[slot].occupied) {
                ui->_wsClient->sendEffectPresetLoad(slot);
                // Brief visual feedback — highlight the loaded card
                lv_obj_set_style_border_color(target,
                    lv_color_hex(DesignTokens::PRESET_ACTIVE_BORDER), LV_PART_MAIN);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
    }

    // --- Mode buttons (6-column grid, child of top zone) ---
    _mode_container = lv_obj_create(_top_zone);
    lv_obj_set_size(_mode_container, 1210 - 2 * 10, kModeRowHeight);
    lv_obj_align(_mode_container, LV_ALIGN_TOP_MID, 0, kGaugeRowHeight + 8);
    lv_obj_set_style_bg_opa(_mode_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_mode_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_mode_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_mode_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_mode_container, LV_LAYOUT_GRID);
    static const lv_coord_t mode_col_dsc[7] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                          LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t mode_row_dsc[2] = {kModeRowHeight, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_mode_container, mode_col_dsc, mode_row_dsc);
    lv_obj_set_style_pad_column(_mode_container, DesignTokens::GRID_GAP, LV_PART_MAIN);

    const char* mode_names[] = {"GAMMA", "COLOUR", "EDGEMIXER", "SPATIAL", "EM SPREAD", "EM STRENGTH"};
    for (int i = 0; i < 6; i++) {
        _mode_buttons[i] = make_card(_mode_container, false);
        lv_obj_set_grid_cell(_mode_buttons[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_flag(_mode_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(_mode_buttons[i], LV_OBJ_FLAG_SCROLLABLE);

        _mode_labels[i] = lv_label_create(_mode_buttons[i]);
        lv_label_set_text(_mode_labels[i], mode_names[i]);
        lv_obj_set_style_text_color(_mode_labels[i], lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_font(_mode_labels[i], RAJDHANI_MED_18, LV_PART_MAIN);
        lv_obj_set_width(_mode_labels[i], LV_PCT(100));
        lv_label_set_long_mode(_mode_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(_mode_labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(_mode_labels[i], LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_clear_flag(_mode_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_mode_labels[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        _mode_values[i] = lv_label_create(_mode_buttons[i]);
        lv_label_set_text(_mode_values[i], "");
        lv_obj_set_style_text_color(_mode_values[i], lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        // All mode values use Rajdhani Bold 24 for consistency
        lv_obj_set_style_text_font(_mode_values[i], RAJDHANI_BOLD_24, LV_PART_MAIN);
        lv_obj_set_width(_mode_values[i], LV_PCT(100));
        lv_label_set_long_mode(_mode_values[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(_mode_values[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(_mode_values[i], LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_clear_flag(_mode_values[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(_mode_values[i], LV_OBJ_FLAG_EVENT_BUBBLE);

        // Event handler — all 6 buttons call _action_callback(index)
        lv_obj_set_user_data(_mode_buttons[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_mode_buttons[i], [](lv_event_t* e) {
            lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
            DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
            if (ui && ui->_action_callback) {
                ui->_action_callback(index);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
    }
    esp_task_wdt_reset();  // Reset after mode-button row (6 cards × 3 objects = 18 LVGL objects)

    // Create footer (66px height, matching header)
    _footer = lv_obj_create(_screen_global);
    // Reduce width by 2 * (border_width + padding) = 2 * (2 + 1) = 6px to prevent border clipping
    lv_obj_set_size(_footer, 1280 - 6, DesignTokens::STATUSBAR_HEIGHT);
    // Position at bottom: 720px - 66px - 3px (border/padding offset) = 651px from top
    lv_obj_set_pos(_footer, 3, 720 - DesignTokens::STATUSBAR_HEIGHT - 3);
    lv_obj_set_style_bg_color(_footer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_width(_footer, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_footer, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(_footer, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_footer, DesignTokens::GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer, DesignTokens::GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer, 16, LV_PART_MAIN);
    lv_obj_clear_flag(_footer, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling on footer
    lv_obj_set_layout(_footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left side: BPM, KEY, MIC, UPTIME - each in fixed-width containers
    // Total: 95+112+125+185 = 517px + 3×24px gaps = 589px minimum
    lv_obj_t* leftGroup = lv_obj_create(_footer);
    lv_obj_set_size(leftGroup, 589, 24);
    lv_obj_set_style_bg_opa(leftGroup, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(leftGroup, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(leftGroup, 0, LV_PART_MAIN);
    lv_obj_clear_flag(leftGroup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(leftGroup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(leftGroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftGroup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(leftGroup, 24, LV_PART_MAIN);  // 24px gap between metric groups

    // BPM: Fixed-width container with title + value
    lv_obj_t* bpmContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(bpmContainer, 95, 24);  // +20px - fits "BPM: 999" comfortably
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(bpmContainer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bpmContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bpmContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(bpmContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(bpmContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bpmContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bpmContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_bpm = lv_label_create(bpmContainer);
    lv_label_set_text(_footer_bpm, "BPM:");
    lv_obj_set_style_text_font(_footer_bpm, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_bpm, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_bpm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_bpm, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_bpm_value = lv_label_create(bpmContainer);
    lv_label_set_text(_footer_bpm_value, "");
    lv_obj_set_style_text_font(_footer_bpm_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_bpm_value, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_bpm_value, 0, LV_PART_MAIN);

    // KEY: Fixed-width container with title + value
    // Maximum key length: 5 chars (e.g., "C#dim", "A#aug") = ~70px at 24px font
    // Title "KEY:" = ~45px, so total = 115px + 2px gap = 117px → 112px is tight but OK
    lv_obj_t* keyContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(keyContainer, 112, 24);  // Fixed width for longest key (C#dim, A#aug)
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(keyContainer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(keyContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keyContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(keyContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(keyContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(keyContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(keyContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_key = lv_label_create(keyContainer);
    lv_label_set_text(_footer_key, "KEY:");
    lv_obj_set_style_text_font(_footer_key, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_key, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_key, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_key, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_key_value = lv_label_create(keyContainer);
    lv_label_set_text(_footer_key_value, "");
    lv_obj_set_style_text_font(_footer_key_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_key_value, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_key_value, 0, LV_PART_MAIN);

    // MIC: Fixed-width container with title + value
    lv_obj_t* micContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(micContainer, 125, 24);  // +25px - fits "MIC: -99.9 DB" comfortably
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(micContainer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(micContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(micContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(micContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(micContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(micContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(micContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_mic = lv_label_create(micContainer);
    lv_label_set_text(_footer_mic, "MIC:");
    lv_obj_set_style_text_font(_footer_mic, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_mic, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_mic, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_mic, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_mic_value = lv_label_create(micContainer);
    lv_label_set_text(_footer_mic_value, "");
    lv_obj_set_style_text_font(_footer_mic_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_mic_value, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_mic_value, 0, LV_PART_MAIN);

    // UPTIME: Fixed-width container with title + value
    lv_obj_t* uptimeContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(uptimeContainer, 185, 24);  // Fits "UPTIME: 999h 59m" at Rajdhani 24px
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(uptimeContainer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(uptimeContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(uptimeContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(uptimeContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(uptimeContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(uptimeContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(uptimeContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_host_uptime = lv_label_create(uptimeContainer);
    lv_label_set_text(_footer_host_uptime, "UPTIME:");
    lv_obj_set_style_text_font(_footer_host_uptime, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_host_uptime, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_host_uptime, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_host_uptime, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_uptime_value = lv_label_create(uptimeContainer);
    lv_label_set_text(_footer_uptime_value, "");
    lv_obj_set_style_text_font(_footer_uptime_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_uptime_value, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_uptime_value, 0, LV_PART_MAIN);

    esp_task_wdt_reset();  // Reset after footer left-group (BPM/KEY/MIC/UPTIME labels)

    _footer_k1_group = lv_obj_create(_footer);
    lv_obj_set_style_bg_opa(_footer_k1_group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_footer_k1_group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_k1_group, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_footer_k1_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(_footer_k1_group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_footer_k1_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_footer_k1_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_footer_k1_group, 16, LV_PART_MAIN);

    for (int i = 0; i < 2; i++) {
        _footer_k1_buttons[i] = lv_obj_create(_footer_k1_group);
        lv_obj_add_flag(_footer_k1_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(_footer_k1_buttons[i], 80, 32);
        lv_obj_set_style_bg_color(_footer_k1_buttons[i], lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
        lv_obj_set_style_border_width(_footer_k1_buttons[i], 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(_footer_k1_buttons[i], lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
        lv_obj_add_state(_footer_k1_buttons[i], LV_STATE_DISABLED);

        lv_obj_t* label = lv_label_create(_footer_k1_buttons[i]);
        lv_label_set_text(label, "K1");
        lv_obj_set_style_text_font(label, RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_obj_center(label);

        lv_obj_set_user_data(_footer_k1_buttons[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_footer_k1_buttons[i], [](lv_event_t* e) {
            lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
            DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));
            if (ui && ui->_k1SelectionCallback) {
                ui->_k1SelectionCallback(index);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
    }

    // Right side: WS and Battery (with bar) - aligned right, ensure it has space
    lv_obj_t* rightGroup = lv_obj_create(_footer);
    // Don't use LV_SIZE_CONTENT - set explicit minimum width to ensure WS and Battery fit
    // WS label ~80px + gap 24px + Battery label ~145px + bar 60px + gap 8px = ~317px minimum → 345px for safety
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(rightGroup, 345, 24);  // +25px for safety margin - prevents battery text clipping
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(rightGroup, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rightGroup, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rightGroup, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rightGroup, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rightGroup, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(rightGroup, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(rightGroup, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(rightGroup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rightGroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rightGroup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // Changed from END to START
    lv_obj_set_style_pad_column(rightGroup, 24, LV_PART_MAIN);  // Gap between WS and Battery

    _footer_ws_status = lv_label_create(rightGroup);
    lv_label_set_text(_footer_ws_status, "WS: --");
    lv_obj_set_style_text_font(_footer_ws_status, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_ws_status, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer_ws_status, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_ws_status, 0, LV_PART_MAIN);

    // Battery container: label + bar side by side
    lv_obj_t* batteryContainer = lv_obj_create(rightGroup);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(batteryContainer, LV_SIZE_CONTENT, 24);  // Height fixed to font size
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(batteryContainer, lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(batteryContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(batteryContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(batteryContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(batteryContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(batteryContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(batteryContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(batteryContainer, 8, LV_PART_MAIN);  // 8px gap between label and bar

    _footer_battery = lv_label_create(batteryContainer);
    lv_label_set_text(_footer_battery, "BATTERY: --");
    lv_obj_set_style_text_font(_footer_battery, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_battery, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer_battery, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_battery, 0, LV_PART_MAIN);

    // Battery bar - positioned right next to battery label
    _footer_battery_bar = lv_bar_create(batteryContainer);
    lv_bar_set_range(_footer_battery_bar, 0, 100);
    lv_bar_set_value(_footer_battery_bar, 0, LV_ANIM_OFF);
    lv_obj_set_size(_footer_battery_bar, 60, 8);  // 60px wide, 8px tall
    lv_obj_set_style_radius(_footer_battery_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_footer_battery_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_pad_top(_footer_battery_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_battery_bar, 0, LV_PART_MAIN);

    // Create zone composer screen
    _screen_zone = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_zone, lv_color_hex(DesignTokens::BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_zone, 0, LV_PART_MAIN);

    // Create ZoneComposerUI and initialize with zone screen as parent
    _zoneComposer = new ZoneComposerUI(_display);
    _zoneComposer->setBackButtonCallback(onZoneComposerBackButton);  // Wire Back button
    _zoneComposer->begin(_screen_zone);  // Create LVGL widgets on zone screen

    Serial.println("[DisplayUI] Zone Composer initialized");

        // Serial.printf("[DisplayUI_TRACE] Creating connectivity screen @ %lu ms\n", millis());
    esp_task_wdt_reset();

    // Create connectivity screen
    _screen_connectivity = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_connectivity, lv_color_hex(DesignTokens::BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_connectivity, 0, LV_PART_MAIN);

        // Serial.printf("[DisplayUI_TRACE] Connectivity screen created @ %lu ms\n", millis());
    esp_task_wdt_reset();

#if ENABLE_WIFI
        // Serial.printf("[DisplayUI_TRACE] Creating ConnectivityTab @ %lu ms\n", millis());
    Serial.flush();
    // Create ConnectivityTab and initialize with connectivity screen as parent
    _connectivityTab = new ConnectivityTab(_display);
        // Serial.printf("[DisplayUI_TRACE] ConnectivityTab constructed @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->setBackButtonCallback(onConnectivityTabBackButton);  // Wire Back button
        // Serial.printf("[DisplayUI_TRACE] setBackButtonCallback done @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->setWiFiManager(&g_wifiManager);
        // Serial.printf("[DisplayUI_TRACE] setWiFiManager done @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset();
        // Serial.printf("[DisplayUI_TRACE] Before ConnectivityTab::begin @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->begin(_screen_connectivity);  // Create LVGL widgets on connectivity screen
        // Serial.printf("[DisplayUI_TRACE] After ConnectivityTab::begin @ %lu ms\n", millis());
    Serial.flush();
    Serial.println("[DisplayUI] Connectivity Tab initialized");
#else
        // Serial.println("[DisplayUI_TRACE] ENABLE_WIFI not defined - skipping ConnectivityTab");
#endif

    // Create Control Surface screen
    esp_task_wdt_reset();
    _screen_control_surface = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_control_surface, lv_color_hex(0x0A0A0B), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_control_surface, 0, LV_PART_MAIN);

    _controlSurface = new ControlSurfaceUI(_display);
    _controlSurface->setBackButtonCallback(onControlSurfaceBackButton);
    _controlSurface->begin(_screen_control_surface);
    Serial.println("[DisplayUI] Control Surface initialized");

    // Serial.printf("[DisplayUI_TRACE] Before lv_scr_load @ %lu ms\n", millis());
    esp_task_wdt_reset();
    lv_scr_load(_screen_global);
    // Serial.printf("[DisplayUI_TRACE] After lv_scr_load @ %lu ms\n", millis());
    esp_task_wdt_reset();
    _currentScreen = UIScreen::GLOBAL;
    // Serial.printf("[DisplayUI_TRACE] begin() complete @ %lu ms\n", millis());
}

// =========================================================================
// Zone / Preset sidebar state binding
// =========================================================================

void DisplayUI::updateZoneModeButton(bool enabled) {
    _zonesEnabled = enabled;
    if (_zone_mode_btn) {
        lv_obj_set_style_bg_color(_zone_mode_btn,
                                  lv_color_hex(enabled ? 0x22CC44 : 0x882222),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(_zone_mode_btn,
                                      lv_color_hex(enabled ? 0x22CC44 : 0x882222),
                                      LV_PART_MAIN);
    }
}

void DisplayUI::adjustZoneCount(int32_t delta) {
    int newCount = static_cast<int>(_zoneCount) + delta;
    if (newCount < 1) newCount = 3;  // Wrap: 1→2→3→1
    if (newCount > 3) newCount = 1;
    _zoneCount = static_cast<uint8_t>(newCount);

    // Update zone count label
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", _zoneCount);
    if (_zone_param_values[5]) lv_label_set_text(_zone_param_values[5], buf);

    // Update LED count per zone label
    uint8_t ledsPerZone = 160 / _zoneCount;
    char ledBuf[16];
    snprintf(ledBuf, sizeof(ledBuf), "%u LEDS", ledsPerZone);
    if (_zone_param_values[6]) lv_label_set_text(_zone_param_values[6], ledBuf);

    Serial.printf("[DisplayUI] Zone count → %u (%u LEDs/zone)\n", _zoneCount, ledsPerZone);
}

void DisplayUI::adjustPreset(int32_t delta) {
    // Slot 6 encoder adjusts LED count — same as zone count for now
    adjustZoneCount(delta);
}

void DisplayUI::updateZoneSidebarState(uint8_t zoneId, uint16_t effectId, const char* effectName,
                                        uint8_t speed, uint8_t paletteId, const char* paletteName,
                                        uint8_t blendMode, uint8_t brightness) {
    if (zoneId >= 3) return;  // Bounds check — sidebar supports 3 zones

    ZoneSidebarParam& z = _zoneSidebarState[zoneId];
    z.effectId = effectId;
    if (effectName && effectName[0]) {
        strncpy(z.effectName, effectName, sizeof(z.effectName) - 1);
        z.effectName[sizeof(z.effectName) - 1] = '\0';
    }
    z.speed = speed;
    z.paletteId = paletteId;
    if (paletteName && paletteName[0]) {
        strncpy(z.paletteName, paletteName, sizeof(z.paletteName) - 1);
        z.paletteName[sizeof(z.paletteName) - 1] = '\0';
    }
    z.blendMode = blendMode;
    z.brightness = brightness;

    // Refresh visible labels if zones panel is showing the selected zone
    if (_currentTab == SidebarTab::ZONES && _selectedZone == zoneId) {
        char buf[16];
        if (_zone_param_values[0]) lv_label_set_text(_zone_param_values[0], z.effectName);
        if (_zone_param_values[1]) { snprintf(buf, sizeof(buf), "%u", z.speed);      lv_label_set_text(_zone_param_values[1], buf); }
        if (_zone_param_values[2]) lv_label_set_text(_zone_param_values[2], z.paletteName);
        if (_zone_param_values[3]) { snprintf(buf, sizeof(buf), "%u", z.blendMode);  lv_label_set_text(_zone_param_values[3], buf); }
        if (_zone_param_values[4]) { snprintf(buf, sizeof(buf), "%u", z.brightness); lv_label_set_text(_zone_param_values[4], buf); }
    }
}

const DisplayUI::ZoneSidebarParam& DisplayUI::getZoneSidebarState(uint8_t zone) const {
    if (zone >= 3) return _zoneSidebarState[0];
    return _zoneSidebarState[zone];
}

void DisplayUI::updatePresetSidebarSlots(const PresetSidebarSlot* slots, uint8_t count) {
    if (!slots) return;
    const uint8_t n = (count > 8) ? 8 : count;

    for (uint8_t i = 0; i < 8; i++) {
        if (i < n) {
            _presetSidebarSlots[i] = slots[i];
        } else {
            _presetSidebarSlots[i] = PresetSidebarSlot();
        }

        // Update preset card labels
        if (_preset_values[i]) {
            if (_presetSidebarSlots[i].occupied && _presetSidebarSlots[i].name[0]) {
                lv_label_set_text(_preset_values[i], _presetSidebarSlots[i].name);
            } else {
                lv_label_set_text(_preset_values[i], "");
            }
        }

        // Adjust card border: occupied = blue highlight, empty = subtle
        if (_preset_cards[i]) {
            if (_presetSidebarSlots[i].occupied) {
                lv_obj_set_style_border_color(_preset_cards[i],
                    lv_color_hex(DesignTokens::PRESET_OCCUPIED), LV_PART_MAIN);
            } else {
                lv_obj_set_style_border_color(_preset_cards[i],
                    lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
            }
        }
    }
}

// =========================================================================
// Phase 5 — Neon colour system
// =========================================================================
// Updates Unit B bar indicator colours to match the active tab's neon colour.
// Unit A gauge bars (_gauge_bars) are NEVER touched — they stay gold always.
// =========================================================================
// Static helpers: neon and dim-tint per tab
static uint32_t neonForTab(SidebarTab tab) {
    switch (tab) {
        case SidebarTab::FX_PARAMS: return DesignTokens::NEON_GOLD;
        case SidebarTab::ZONES:     return DesignTokens::NEON_PURPLE;
        case SidebarTab::PRESETS:   return DesignTokens::NEON_TEAL;
        default:                    return DesignTokens::NEON_GOLD;
    }
}
static uint32_t dimBorderForTab(SidebarTab tab) {
    switch (tab) {
        case SidebarTab::FX_PARAMS: return DesignTokens::DIM_BORDER_GOLD;
        case SidebarTab::ZONES:     return DesignTokens::DIM_BORDER_PURPLE;
        case SidebarTab::PRESETS:   return DesignTokens::DIM_BORDER_TEAL;
        default:                    return DesignTokens::DIM_BORDER_GOLD;
    }
}

void DisplayUI::applyTabColour(SidebarTab tab) {
    const uint32_t neon = neonForTab(tab);
    const uint32_t dimBorder = dimBorderForTab(tab);

    // Unit A gauge bars: always gold (never change)
    // Unit A gauge card borders: always dim-tinted gold
    for (int i = 0; i < 8; i++) {
        if (_gauge_cards[i]) {
            lv_obj_set_style_border_color(_gauge_cards[i],
                lv_color_hex(DesignTokens::DIM_BORDER_GOLD), LV_PART_MAIN);
        }
    }

    // Unit B FX param card borders: dim-tinted neon per active tab
    for (int i = 0; i < 8; i++) {
        if (_fxa_cards[i]) {
            lv_obj_set_style_border_color(_fxa_cards[i],
                lv_color_hex(dimBorder), LV_PART_MAIN);
        }
        if (_fxa_bars[i]) {
            lv_obj_set_style_bg_color(_fxa_bars[i],
                lv_color_hex(neon), LV_PART_INDICATOR);
        }
    }

    // Zone param card borders: dim-tinted neon
    for (int i = 0; i < 8; i++) {
        if (_zone_param_cards[i]) {
            lv_obj_set_style_border_color(_zone_param_cards[i],
                lv_color_hex(dimBorder), LV_PART_MAIN);
        }
    }

    // Preset card borders: dim-tinted neon (for empty slots only — occupied/active handled separately)
    for (int i = 0; i < 8; i++) {
        if (_preset_cards[i] && _preset_values[i]) {
            const char* val = lv_label_get_text(_preset_values[i]);
            if (!val || val[0] == '\0' || strcmp(val, "--") == 0) {
                lv_obj_set_style_border_color(_preset_cards[i],
                    lv_color_hex(dimBorder), LV_PART_MAIN);
            }
        }
    }

    // FX Row B bars — future-proof
    for (int i = 0; i < 8; i++) {
        if (_fxb_bars[i]) {
            lv_obj_set_style_bg_color(_fxb_bars[i],
                lv_color_hex(neon), LV_PART_INDICATOR);
        }
    }
}

void DisplayUI::loop() {
    const uint32_t now = millis();
    for (int i = 0; i < 8; i++) {
        if (_feedback_until_ms[i] != 0 && now >= _feedback_until_ms[i]) {
            _feedback_until_ms[i] = 0;
            // Preset cards removed in Phase 3 — null-guard
            if (!_preset_cards[i]) continue;
            // After feedback expires, restore the appropriate border colour:
            // - Green (0x00FF99) if this is the active preset
            // - Yellow (DesignTokens::BRAND_PRIMARY) if preset is saved (occupied) but not active
            // - Subtle (BORDER_SUBTLE) if preset is empty
            if (i == _activePresetSlot && _activePresetSlot < 8) {
                lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0x00FF99), LV_PART_MAIN);
            } else {
                if (_preset_values[i]) {
                    const char* valueText = lv_label_get_text(_preset_values[i]);
                    if (valueText && valueText[0] != '\0' && strcmp(valueText, "--") != 0) {
                        lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
                    } else {
                        lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
                    }
                } else {
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
                }
            }
        }
    }

    // Update footer metrics with throttling (1Hz for battery/uptime)
    if (now - _lastFooterUpdate >= 1000) {
        _lastFooterUpdate = now;

        // Update battery indicator
        if (_footer_battery) {
            #ifndef SIMULATOR_BUILD
            int8_t batteryPercent = EspHal::getBatteryLevel();
            
            char buf[32];
            if (batteryPercent >= 0) {
                // Format: "BATTERY: xxx%" (no CHG text)
                snprintf(buf, sizeof(buf), "BATTERY: %d%%", batteryPercent);
                
                // Color-code battery text: Green (>50%), Yellow (20-50%), Red (<20%)
                uint32_t batColor;
                if (batteryPercent > 50) {
                    batColor = 0x00FF00;  // Green
                } else if (batteryPercent > 20) {
                    batColor = DesignTokens::BRAND_PRIMARY;  // Yellow (0xFFC700)
                } else {
                    batColor = 0xFF0000;  // Red (low battery)
                }
                lv_obj_set_style_text_color(_footer_battery, lv_color_hex(batColor), LV_PART_MAIN);
                
                // Update battery status bar
                if (_footer_battery_bar) {
                    lv_bar_set_value(_footer_battery_bar, batteryPercent, LV_ANIM_OFF);
                    // Color-code bar indicator: Green (>50%), Yellow (20-50%), Red (<20%)
                    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(batColor), LV_PART_INDICATOR);
                }
            } else {
                strcpy(buf, "BATTERY: --");
                lv_obj_set_style_text_color(_footer_battery, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
                if (_footer_battery_bar) {
                    lv_bar_set_value(_footer_battery_bar, 0, LV_ANIM_OFF);
                    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_INDICATOR);
                }
            }
            lv_label_set_text(_footer_battery, buf);
            #else
            lv_label_set_text(_footer_battery, "BATTERY: --");
            if (_footer_battery_bar) {
                lv_bar_set_value(_footer_battery_bar, 0, LV_ANIM_OFF);
            }
            #endif
        }

        // Update host uptime value (title stays fixed)
        // K1 sends uptime periodically but may not send every second —
        // increment locally each tick so the display always advances.
        if (_hostUptime > 0) {
            _hostUptime++;
        }
        if (_footer_uptime_value) {
            char buf[32];
            formatDuration(_hostUptime, buf, sizeof(buf));
            lv_label_set_text(_footer_uptime_value, buf);
        }
    }

    // Call Control Surface loop (dirty-flag rendering throttle)
    if (_currentScreen == UIScreen::CONTROL_SURFACE && _controlSurface) {
        _controlSurface->loop();
    }
}

void DisplayUI::updateEncoder(uint8_t index, int32_t value, bool highlight) {
    // Map encoder index to display position
    if (index >= 8) return;
    uint8_t displayPos = kEncoderToDisplayPos[index];
    if (!_gauge_values[displayPos] || !_gauge_bars[displayPos] || !_gauge_cards[displayPos]) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    lv_label_set_text(_gauge_values[displayPos], buf);

    int32_t v = value;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    lv_bar_set_value(_gauge_bars[displayPos], v, LV_ANIM_OFF);

    // Neon border system: active card = full neon, inactive = dim-tinted neon
    lv_obj_set_style_border_color(
        _gauge_cards[displayPos],
        lv_color_hex(highlight ? DesignTokens::NEON_GOLD : DesignTokens::DIM_BORDER_GOLD),
        LV_PART_MAIN);
}

void DisplayUI::updateEffectParamSlot(uint8_t slot, int32_t value, bool highlight) {
    if (slot >= 16) return;

    lv_obj_t* card = nullptr;
    lv_obj_t* valueLabel = nullptr;
    lv_obj_t* bar = nullptr;
    const uint8_t local = static_cast<uint8_t>(slot % 8);

    if (slot < 8) {
        card = _fxa_cards[local];
        valueLabel = _fxa_values[local];
        bar = _fxa_bars[local];
    } else {
        card = _fxb_cards[local];
        valueLabel = _fxb_values[local];
        bar = _fxb_bars[local];
    }

    if (!card || !valueLabel || !bar) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    lv_label_set_text(valueLabel, buf);

    int32_t clamped = value;
    if (clamped < 0) clamped = 0;
    if (clamped > 255) clamped = 255;
    lv_bar_set_value(bar, clamped, LV_ANIM_OFF);

    // Neon border system: active card = full tab neon, inactive = dim-tinted tab neon
    const uint32_t neon = neonForTab(_currentTab);
    const uint32_t dim = dimBorderForTab(_currentTab);
    lv_obj_set_style_border_color(card,
                                  lv_color_hex(highlight ? neon : dim),
                                  LV_PART_MAIN);
}

void DisplayUI::setEffectParamSlotLabel(uint8_t slot, const char* label) {
    if (slot >= 16) return;

    lv_obj_t* labelObj = nullptr;
    const uint8_t local = static_cast<uint8_t>(slot % 8);
    if (slot < 8) {
        labelObj = _fxa_labels[local];
    } else {
        labelObj = _fxb_labels[local];
    }

    if (!labelObj) return;
    if (label && label[0]) {
        // Force two-word labels onto two lines for consistent alignment
        char buf[64];
        strncpy(buf, label, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char* space = strchr(buf, ' ');
        if (space) *space = '\n';
        lv_label_set_text(labelObj, buf);
    } else {
        lv_label_set_text(labelObj, "");
    }
}

void DisplayUI::setConnectionState(bool wifi, bool ws, bool encA, bool encB) {
    // Connection state no longer displayed in header
    (void)wifi;
    (void)ws;
    (void)encA;
    (void)encB;
}

void DisplayUI::setCurrentEffect(uint8_t id, const char* name) {
    if (!_header_effect) return;
    if (name && name[0]) {
        lv_label_set_text(_header_effect, name);
    } else {
        lv_label_set_text(_header_effect, "");
    }
    (void)id; // ID no longer displayed
}

void DisplayUI::setCurrentPalette(uint8_t id, const char* name) {
    if (!_header_palette) return;
    if (name && name[0]) {
        lv_label_set_text(_header_palette, name);
    } else {
        lv_label_set_text(_header_palette, "");
    }
    (void)id; // ID no longer displayed
}

void DisplayUI::setWiFiInfo(const char* ip, const char* ssid, int32_t rssi) {
    if (!_header_net_ip || !_header_net_ssid || !_header_net_rssi) return;
    
    if (ip && ssid && ip[0] && ssid[0]) {
        // Format: "SSID (RSSI dBm) IP" - SSID in secondary color, RSSI color-coded, IP in primary color
        lv_label_set_text(_header_net_ssid, ssid);
        lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        
        // Format RSSI with parentheses and unit (dBm)
        char rssiText[20];
        snprintf(rssiText, sizeof(rssiText), " (%ld dBm)", (long)rssi);
        lv_label_set_text(_header_net_rssi, rssiText);
        
        // Color-code RSSI: Green (>-50), Yellow (-50 to -70), Red (<-70)
        // Yellow uses the same color as "OS" (DesignTokens::BRAND_PRIMARY = 0xFFC700)
        uint32_t rssiColor;
        if (rssi > -50) {
            rssiColor = 0x00FF00;  // Green - excellent signal
        } else if (rssi > -70) {
            rssiColor = DesignTokens::BRAND_PRIMARY;  // Yellow (same as "OS") - good signal
        } else {
            rssiColor = 0xFF0000;  // Red - poor signal
        }
        lv_obj_set_style_text_color(_header_net_rssi, lv_color_hex(rssiColor), LV_PART_MAIN);
        
        // IP address comes after RSSI
        lv_label_set_text(_header_net_ip, ip);
        lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        // Update padding: 8px between RSSI and IP (was 4px between SSID and RSSI)
        lv_obj_set_style_pad_left(_header_net_ip, 8, LV_PART_MAIN);
    } else {
        lv_label_set_text(_header_net_ssid, "");
        lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(DesignTokens::FG_SECONDARY), LV_PART_MAIN);
        lv_label_set_text(_header_net_rssi, "");
        lv_label_set_text(_header_net_ip, "");
        lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(DesignTokens::FG_PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_pad_left(_header_net_ip, 8, LV_PART_MAIN);  // Keep 8px padding
    }
}

void DisplayUI::updateRetryButton(bool shouldShow) {
    if (!_header_retry_button) return;
    
    if (shouldShow) {
        lv_obj_clear_flag(_header_retry_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_header_retry_button, LV_OBJ_FLAG_HIDDEN);
    }
}

void DisplayUI::setScreen(UIScreen screen) {
    if (screen == _currentScreen) return;
    _currentScreen = screen;
    
    lv_obj_t* targetScreen = _screen_global;
    switch (screen) {
        case UIScreen::GLOBAL:
            targetScreen = _screen_global;
            break;
        case UIScreen::ZONE_COMPOSER:
            targetScreen = _screen_zone;
            break;
        case UIScreen::CONNECTIVITY:
            targetScreen = _screen_connectivity;
            break;
        case UIScreen::CONTROL_SURFACE:
            targetScreen = _screen_control_surface;
            if (_controlSurface) {
                _controlSurface->onScreenEnter();
            }
            break;
    }
    lv_scr_load(targetScreen);
}

void DisplayUI::onZoneComposerBackButton() {
    if (s_instance) {
        s_instance->setScreen(UIScreen::GLOBAL);
    }
}

void DisplayUI::onConnectivityTabBackButton() {
    if (s_instance) {
        s_instance->setScreen(UIScreen::GLOBAL);
    }
}

void DisplayUI::onControlSurfaceBackButton() {
    if (s_instance) {
        s_instance->setScreen(UIScreen::GLOBAL);
    }
}

void DisplayUI::updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness) {
    if (slot >= 8) return;
    if (!_preset_values[slot] || !_preset_cards[slot]) return;

    if (!occupied) {
        lv_label_set_text(_preset_values[slot], "");
        lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
        return;
    }

    lv_label_set_text_fmt(_preset_values[slot], "E%u P%u %u", effectId, paletteId, brightness);
    lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
}

void DisplayUI::setActivePresetSlot(uint8_t slot) {
    _activePresetSlot = slot;
    for (int i = 0; i < 8; i++) {
        if (!_preset_cards[i]) continue;
        const bool isActive = (slot < 8) && (i == (int)slot);
        if (isActive) {
            // Active preset: green border
            lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0x00FF99), LV_PART_MAIN);
        } else {
            // Not active: check if saved (yellow) or empty (subtle)
            if (_preset_values[i]) {
                const char* valueText = lv_label_get_text(_preset_values[i]);
                if (valueText && valueText[0] != '\0' && strcmp(valueText, "--") != 0) {
                    // Preset is saved but not active: yellow border
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_MAIN);
                } else {
                    // Preset is empty: subtle border
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
                }
            } else {
                // Fallback: subtle border
                lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(DesignTokens::BORDER_SUBTLE), LV_PART_MAIN);
            }
        }
    }
}

void DisplayUI::showPresetSaveFeedback(uint8_t slot) {
    if (slot >= 8 || !_preset_cards[slot]) return;
    _feedback_until_ms[slot] = millis() + 600;
    _feedback_color_hex[slot] = 0xFFE066;
    lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(_feedback_color_hex[slot]), LV_PART_MAIN);
}

void DisplayUI::showPresetRecallFeedback(uint8_t slot) {
    if (slot >= 8 || !_preset_cards[slot]) return;
    _feedback_until_ms[slot] = millis() + 600;
    _feedback_color_hex[slot] = 0x00FF99;
    lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(_feedback_color_hex[slot]), LV_PART_MAIN);
}

void DisplayUI::showPresetDeleteFeedback(uint8_t slot) {
    if (slot >= 8 || !_preset_cards[slot]) return;
    _feedback_until_ms[slot] = millis() + 600;
    _feedback_color_hex[slot] = 0xFF3355;
    lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(_feedback_color_hex[slot]), LV_PART_MAIN);
}

void DisplayUI::setColourCorrectionState(const ColorCorrectionState& state) {
    if (!_mode_buttons[0] || !_mode_values[0]) return;

    // GAMMA button (index 0)
    // Display format: "2.2" for 2.2, "2.5" for 2.5, "2.8" for 2.8, "OFF" when disabled
    char gamma_buf[16];
    if (state.gammaEnabled) {
        snprintf(gamma_buf, sizeof(gamma_buf), "%.1f", state.gammaValue);
    } else {
        strcpy(gamma_buf, "OFF");
    }
    lv_label_set_text(_mode_values[0], gamma_buf);
    lv_obj_set_style_border_color(_mode_buttons[0],
                                   lv_color_hex(state.gammaEnabled ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // COLOUR button (index 1)
    const char* colour_mode = "OFF";
    if (state.mode == 1) colour_mode = "HSV";
    else if (state.mode == 2) colour_mode = "RGB";
    else if (state.mode == 3) colour_mode = "BOTH";
    lv_label_set_text(_mode_values[1], colour_mode);
    lv_obj_set_style_border_color(_mode_buttons[1],
                                   lv_color_hex(state.mode != 0 ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

}

void DisplayUI::setEdgeMixerState(const EdgeMixerState& state) {
    if (!_mode_buttons[2] || !_mode_values[2]) return;

    // EDGEMIXER button (index 2) — mode cycle
    static const char* kModeNames[] = {"MIRROR", "ANALOGOUS", "COMPLEMENTARY", "SPLIT-COMP", "SATURATION VEIL", "TRIADIC", "TETRADIC", "STM DUAL", "STM SPECTRAL"};
    const char* modeName = (state.mode < 9) ? kModeNames[state.mode] : "???";
    lv_label_set_text(_mode_values[2], modeName);
    lv_obj_set_style_border_color(_mode_buttons[2],
                                   lv_color_hex(state.mode != 0 ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // SPATIAL button (index 3) — Spatial + Temporal combo
    // Combos: spatial | (temporal<<1): 0=OFF, 1=CENTRE GRADIENT, 2=RMS GATE, 3=CENTRE+RMS
    static const char* kComboNames[] = {"OFF", "CENTRE GRADIENT", "RMS GATE", "CENTRE+RMS"};
    uint8_t combo = state.spatial | (state.temporal << 1);
    const char* comboName = (combo < 4) ? kComboNames[combo] : "???";
    lv_label_set_text(_mode_values[3], comboName);
    lv_obj_set_style_border_color(_mode_buttons[3],
                                   lv_color_hex((state.spatial || state.temporal) ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // EM SPREAD button (index 4) — spread in degrees, displayed as named step
    if (_mode_buttons[4] && _mode_values[4]) {
        const char* spreadName;
        switch (state.spread) {
            case  0: spreadName = "None";   break;
            case 10: spreadName = "Subtle"; break;
            case 20: spreadName = "Medium"; break;
            case 30: spreadName = "Wide";   break;
            case 45: spreadName = "Max";    break;
            case 60: spreadName = "Ultra";  break;
            default: {
                static char spreadBuf[8];
                snprintf(spreadBuf, sizeof(spreadBuf), "%u", state.spread);
                spreadName = spreadBuf;
                break;
            }
        }
        lv_label_set_text(_mode_values[4], spreadName);
        lv_obj_set_style_border_color(_mode_buttons[4],
                                       lv_color_hex(state.spread != 0 ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                       LV_PART_MAIN);
    }

    // EM STRENGTH button (index 5) — mix strength, displayed as named step
    if (_mode_buttons[5] && _mode_values[5]) {
        const char* strengthName;
        switch (state.strength) {
            case   0: strengthName = "Off";    break;
            case  64: strengthName = "Light";  break;
            case 128: strengthName = "Medium"; break;
            case 255: strengthName = "Full";   break;
            default: {
                static char strengthBuf[8];
                snprintf(strengthBuf, sizeof(strengthBuf), "%u", state.strength);
                strengthName = strengthBuf;
                break;
            }
        }
        lv_label_set_text(_mode_values[5], strengthName);
        lv_obj_set_style_border_color(_mode_buttons[5],
                                       lv_color_hex(state.strength != 0 ? DesignTokens::BRAND_PRIMARY : 0xFFFFFF),
                                       LV_PART_MAIN);
    }
}

void DisplayUI::updateAudioMetrics(float bpm, const char* key, float micLevel) {
    _bpm = bpm;
    if (key && key[0]) {
        strncpy(_key, key, sizeof(_key) - 1);
        _key[sizeof(_key) - 1] = '\0';
    } else {
        _key[0] = '\0';
    }
    _micLevel = micLevel;

    // Update footer value labels only (titles stay fixed)
    if (_footer_bpm_value) {
        char buf[32];
        if (_bpm >= 0.0f) {
            snprintf(buf, sizeof(buf), "%.0f", _bpm);
        } else {
            buf[0] = '\0';
        }
        lv_label_set_text(_footer_bpm_value, buf);
    }

    if (_footer_key_value) {
        char buf[32];
        if (_key[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s", _key);
        } else {
            buf[0] = '\0';
        }
        lv_label_set_text(_footer_key_value, buf);
    }

    if (_footer_mic_value) {
        char buf[32];
        if (_micLevel > -80.0f) {
            snprintf(buf, sizeof(buf), "%.1f DB", _micLevel);
        } else {
            buf[0] = '\0';
        }
        lv_label_set_text(_footer_mic_value, buf);
    }
}

void DisplayUI::updateHostUptime(uint32_t uptimeSeconds) {
    _hostUptime = uptimeSeconds;
    // #region agent log (DISABLED)
    // Serial.printf("[DisplayUI] updateHostUptime called: %lu seconds\n", uptimeSeconds);
        // #endregion
    // Format will be updated in loop() with throttling
}

void DisplayUI::setWebSocketConnected(bool connected, uint32_t connectTime) {
    // Connection time no longer needed (removed connection duration metric)
    (void)connected;
    (void)connectTime;
    // Status will be updated via updateWebSocketStatus()
}

void DisplayUI::updateWebSocketStatus(WebSocketStatus status) {
    if (!_footer_ws_status) return;

    // IMPORTANT: Keep status text abbreviated to fit 345px rightGroup container
    // Maximum safe length: ~55px (measured with RAJDHANI_MED_24 font)
    // Longer text causes battery bar to clip off-screen
    // Current states: "OK" (~50px), "OFF" (~55px), "..." (~25px), "ERR" (~45px)
    const char* statusText = "OFF";  // Abbreviated from "DISCONNECTED"
    uint32_t statusColor = DesignTokens::FG_SECONDARY;  // Gray for disconnected

    switch (status) {
        case WebSocketStatus::CONNECTED:
            statusText = "OK";  // Abbreviated from "CONNECTED" (~50px vs ~115px)
            statusColor = 0x00FF00;  // Green
            break;
        case WebSocketStatus::CONNECTING:
            statusText = "...";  // Abbreviated from "CONNECTING" (~25px vs ~120px)
            statusColor = DesignTokens::BRAND_PRIMARY;  // Yellow
            break;
        case WebSocketStatus::DISCONNECTED:
            statusText = "OFF";  // Abbreviated from "DISCONNECTED" (~55px vs ~160px)
            statusColor = DesignTokens::FG_SECONDARY;  // Gray
            break;
        case WebSocketStatus::ERROR:
            statusText = "ERR";  // Abbreviated from "ERROR" (~45px vs ~75px)
            statusColor = 0xFF0000;  // Red
            break;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "WS: %s", statusText);
    lv_label_set_text(_footer_ws_status, buf);
    lv_obj_set_style_text_color(_footer_ws_status, lv_color_hex(statusColor), LV_PART_MAIN);
}

void DisplayUI::setK1SelectionCallback(void (*callback)(uint8_t index)) {
    _k1SelectionCallback = callback;
}

void DisplayUI::updateK1Slots(bool slot0Online, bool slot1Online, int8_t selectedIndex) {
    bool online[2] = {slot0Online, slot1Online};
    for (int i = 0; i < 2; i++) {
        if (!_footer_k1_buttons[i]) {
            continue;
        }
        if (!online[i]) {
            lv_obj_add_state(_footer_k1_buttons[i], LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(_footer_k1_buttons[i], lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
            lv_obj_set_style_border_color(_footer_k1_buttons[i], lv_color_hex(DesignTokens::BORDER_BASE), LV_PART_MAIN);
        } else {
            lv_obj_clear_state(_footer_k1_buttons[i], LV_STATE_DISABLED);
            bool selected = (selectedIndex == i);
            uint32_t borderColor = selected ? DesignTokens::BRAND_PRIMARY : DesignTokens::STATUS_SUCCESS;
            lv_obj_set_style_bg_color(_footer_k1_buttons[i], lv_color_hex(DesignTokens::SURFACE_ELEVATED), LV_PART_MAIN);
            lv_obj_set_style_border_color(_footer_k1_buttons[i], lv_color_hex(borderColor), LV_PART_MAIN);
        }
    }
}

// Helper function to format duration in HH:MM:SS or Xm Ys
static void formatDuration(uint32_t seconds, char* buf, size_t bufSize) {
    if (seconds < 60) {
        snprintf(buf, bufSize, "%lus", seconds);
    } else if (seconds < 3600) {
        uint32_t minutes = seconds / 60;
        uint32_t secs = seconds % 60;
        snprintf(buf, bufSize, "%lum %lus", minutes, secs);
    } else {
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        uint32_t secs = seconds % 60;
        snprintf(buf, bufSize, "%luh %lum %lus", hours, minutes, secs);
    }
}

#ifndef SIMULATOR_BUILD
void DisplayUI::refreshAllPresetSlots(PresetManager* pm) {
    if (!pm) return;
    for (uint8_t i = 0; i < 8; i++) {
        PresetData data;
        if (pm->getPreset(i, data)) {
            updatePresetSlot(i, true, data.effectId, data.paletteId, data.brightness);
        } else {
            updatePresetSlot(i, false, 0, 0, 0);
        }
    }
}
#endif

#else

#ifdef SIMULATOR_BUILD
    // Minimal includes for simulator
    #include "../config/Config.h"
#else
    #include "ZoneComposerUI.h"
    #include "../parameters/ParameterMap.h"
    #include "../config/Config.h"
    #include "../presets/PresetManager.h"
    #include "../network/WebSocketClient.h"  // For ColorCorrectionState
#endif

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
    #include "../hal/SimHal.h"
#else
    #include <M5Unified.h>
    #include <esp_system.h>
    #include <ESP.h>  // For heap monitoring
    #include <Arduino.h>
#endif

#include "../hal/EspHal.h"
#include <cstdio>
#include <cstring>

DisplayUI::DisplayUI(M5GFX& display)
    : _display(display)
    , _currentScreen(UIScreen::GLOBAL)
{
    _header = nullptr;
    #ifndef SIMULATOR_BUILD
    _zoneComposer = nullptr;
    #endif
    for (int i = 0; i < 16; i++) {
        _gauges[i] = nullptr;
    }
    for (int i = 0; i < 8; i++) {
        _presetSlots[i] = nullptr;
    }
}

DisplayUI::~DisplayUI() {
    if (_header) {
        delete _header;
        _header = nullptr;
    }
    #ifndef SIMULATOR_BUILD
    if (_zoneComposer) {
        delete _zoneComposer;
        _zoneComposer = nullptr;
    }
    #endif
    for (int i = 0; i < 16; i++) {
        if (_gauges[i]) {
            delete _gauges[i];
            _gauges[i] = nullptr;
        }
    }
    for (int i = 0; i < 8; i++) {
        if (_presetSlots[i]) {
            delete _presetSlots[i];
            _presetSlots[i] = nullptr;
        }
    }
    #ifndef SIMULATOR_BUILD
    if (_actionRow) {
        delete _actionRow;
        _actionRow = nullptr;
    }
    #endif
}

void DisplayUI::begin() {
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] DisplayUI::begin entry - Heap: free=%u minFree=%u largest=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    // EspHal::log("[DEBUG] Sprite memory estimate: %u gauges * %dx%d + %u slots * %dx%d + header %dx%d = ~%u KB\n",
                  // 8, Theme::CELL_W, Theme::CELL_H,
                  // 8, Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H,
                  // Theme::SCREEN_W, Theme::STATUS_BAR_H,
                  // ((8 * Theme::CELL_W * Theme::CELL_H * 2) + 
                   // (8 * Theme::PRESET_SLOT_W * Theme::PRESET_SLOT_H * 2) +
                   // (Theme::SCREEN_W * Theme::STATUS_BAR_H * 2)) / 1024);
        // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] begin_start cols=%d rows=%d cellW=%d cellH=%d\n", Theme::GRID_COLS, Theme::GRID_ROWS, Theme::CELL_W, Theme::CELL_H);
#endif

    _display.fillScreen(Theme::BG_DARK);

    // Create header
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] Before UIHeader creation - Heap: free=%u minFree=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion
    _header = new UIHeader(&_display);
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] After UIHeader creation - Heap: free=%u minFree=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] header_created ptr=%p\n", _header);
#endif

    // Create 8x1 grid of gauges (8 global parameters only)
    int yOffset = Theme::STATUS_BAR_H;

    for (int i = 0; i < 8; i++) {
        int row = i / Theme::GRID_COLS;
        int col = i % Theme::GRID_COLS;

        int x = col * Theme::CELL_W;
        int y = yOffset + row * Theme::CELL_H;

#if ENABLE_UI_DIAGNOSTICS
        EspHal::log("[DBG] creating_gauge i=%d x=%d y=%d w=%d h=%d\n", i, x, y, Theme::CELL_W, Theme::CELL_H);
#endif

        // #region agent log (DISABLED)
        // if (i == 0 || i == 7) {  // Log first and last gauge creation
            // EspHal::log("[DEBUG] Creating gauge %d - Heap before: free=%u minFree=%u\n",
                          // i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // }
                // #endregion
        _gauges[i] = new GaugeWidget(&_display, x, y, Theme::CELL_W, Theme::CELL_H, i);
        // #region agent log (DISABLED)
        // if (i == 0 || i == 7) {
            // EspHal::log("[DEBUG] Gauge %d created - Heap after: free=%u minFree=%u\n",
                          // i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // }
                // #endregion
        
        // Set initial max value from ParameterMap
        #ifdef SIMULATOR_BUILD
            uint8_t maxValue = 255;  // Default in simulator
        #else
            uint8_t maxValue = getParameterMax(i);
        #endif
        _gauges[i]->setMaxValue(maxValue);
    }
    
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] All 8 gauges created - Heap: free=%u minFree=%u largest=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
        // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] gauges_created count=8\n");
#endif
    
    // Clear remaining gauge slots (8-15) - not used in global view
    for (int i = 8; i < 16; i++) {
        _gauges[i] = nullptr;
    }

    // Create 8 preset slot widgets below gauge row
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] Before preset slots creation - Heap: free=%u minFree=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        // #endregion
    for (int i = 0; i < 8; i++) {
        int x = i * Theme::PRESET_SLOT_W;
        int y = Theme::PRESET_ROW_Y;
        EspHal::log("[DEBUG] Creating preset slot %d (P%d) at x=%d y=%d width=%d\n", 
                      i, i+1, x, y, Theme::PRESET_SLOT_W);
        _presetSlots[i] = new PresetSlotWidget(&_display, x, y, i);
        if (!_presetSlots[i]) {
            EspHal::log("[ERROR] Failed to create preset slot %d!\n", i);
        }
    }
    // #region agent log (DISABLED)
    // EspHal::log("[DEBUG] All 8 preset slots created - Heap: free=%u minFree=%u largest=%u\n",
                  // EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
        // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] preset_slots_created count=8\n");
#endif

    // Create touch action row (third row)
    #ifndef SIMULATOR_BUILD
    _actionRow = new ActionRowWidget(&_display, 0, Theme::ACTION_ROW_Y, Theme::SCREEN_W, Theme::ACTION_ROW_H);
    #endif

    // NOTE: Zone Composer UI is created later at line 642-645 with _screen_zone as parent
    // DO NOT create it here or it will create duplicate LVGL widgets!

    // Clear entire screen to black background
    _display.fillScreen(Theme::BG_DARK);
    
    // Mark all widgets as dirty to force initial render
    if (_header) _header->markDirty();
    for (int i = 0; i < 8; i++) {
        if (_gauges[i]) _gauges[i]->markDirty();
        if (_presetSlots[i]) _presetSlots[i]->markDirty();
    }
    
    // Initial render
    renderCurrentScreen();

#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] begin_complete\n");
#endif
}

void DisplayUI::setScreen(UIScreen screen) {
    if (_currentScreen != screen) {
        _currentScreen = screen;
        
        // Clear screen for ANY transition to ensure clean rendering
        _display.fillScreen(Theme::BG_DARK);
        
        if (_currentScreen == UIScreen::GLOBAL) {
            // Force all widgets to redraw
            if (_header) {
                _header->markDirty();
            }
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
                    _gauges[i]->markDirty();
                }
                if (_presetSlots[i]) {
                    _presetSlots[i]->markDirty();
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->markDirty();
            }
            #endif
        } else if (_currentScreen == UIScreen::ZONE_COMPOSER) {
            // Force Zone Composer to redraw by setting _dirty directly
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->forceDirty();  // New method to bypass pending
            }
            #endif
        }

        renderCurrentScreen();
    }
}

void DisplayUI::renderCurrentScreen() {
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] renderCurrentScreen screen=%d\n", (int)_currentScreen);
#endif
    
    switch (_currentScreen) {
        case UIScreen::GLOBAL:
            if (_header) {
#if ENABLE_UI_DIAGNOSTICS
                EspHal::log("[DBG] rendering header\n");
#endif
                _header->render();
            }
            // Only 8 global gauges exist (indices 0-7)
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
#if ENABLE_UI_DIAGNOSTICS
                    EspHal::log("[DBG] rendering gauge %d\n", i);
#endif
                    _gauges[i]->render();
                }
            }
            // Render preset slots below gauges
            for (int i = 0; i < 8; i++) {
                if (_presetSlots[i]) {
                    #ifdef SIMULATOR_BUILD
                    printf("[DEBUG] Calling render() on preset slot %d (P%d)\n", i, i+1);
                    #endif
                    _presetSlots[i]->render();
                } else {
                    #ifdef SIMULATOR_BUILD
                    printf("[ERROR] Preset slot %d (P%d) is NULL!\n", i, i+1);
                    #endif
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->render();
            }
            #endif
#if ENABLE_UI_DIAGNOSTICS
            EspHal::log("[DBG] renderCurrentScreen complete\n");
#endif
            break;
        case UIScreen::ZONE_COMPOSER:
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->forceDirty();  // Immediate redraw, bypass pending
                _zoneComposer->loop();
            }
            #endif
            break;
        case UIScreen::CONNECTIVITY:
            #ifndef SIMULATOR_BUILD
            #if ENABLE_WIFI
            if (_connectivityTab) {
                _connectivityTab->forceDirty();  // Immediate redraw, bypass pending
                _connectivityTab->loop();
            }
            #endif
            #endif
            break;
        case UIScreen::CONTROL_SURFACE:
            #ifndef SIMULATOR_BUILD
            if (_controlSurface) {
                _controlSurface->loop();
            }
            #endif
            break;
    }
}

void DisplayUI::loop() {
    uint32_t now = EspHal::millis();

    // Update header (power + connection) every 500ms
    if (now - _lastStatsUpdate >= 500) {
        _lastStatsUpdate = now;
        updateHeader();
    }

    // Clear highlight after 300ms (only for global screen)
    if (_currentScreen == UIScreen::GLOBAL && _highlightIdx < 8 && now - _highlightTime >= 300) {
        uint8_t displayPos = kEncoderToDisplayPos[_highlightIdx];
        if (_gauges[displayPos]) {
            _gauges[displayPos]->setHighlight(false);
            _gauges[displayPos]->render();
        }
        _highlightIdx = 255;
    }

    // Render current screen
    switch (_currentScreen) {
        case UIScreen::GLOBAL:
            // Render header (checks dirty flag internally)
            if (_header) {
                _header->render();
            }
            // Render gauges (each checks dirty flag internally) - only 8 global parameters
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
                    _gauges[i]->render();
                }
            }
            // Update and render preset slots
            for (int i = 0; i < 8; i++) {
                if (_presetSlots[i]) {
                    _presetSlots[i]->update();  // Handle animations
                    _presetSlots[i]->render();
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->render();
            }
            #endif
            break;
        case UIScreen::ZONE_COMPOSER:
            // Render header on Zone Composer screen too
            if (_header) {
                _header->render();
            }
            #ifndef SIMULATOR_BUILD
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->loop();
            }
            #endif
            #endif
            break;
        case UIScreen::CONNECTIVITY:
            // Render header on Connectivity screen too
            if (_header) {
                _header->render();
            }
            #ifndef SIMULATOR_BUILD
            #if ENABLE_WIFI
            if (_connectivityTab) {
                _connectivityTab->loop();
            }
            #endif
            #endif
            break;
    }
}

void DisplayUI::updateEncoder(uint8_t index, int32_t value, bool highlight) {
    // Only handle global parameters (0-7)
    if (index >= 8) return;

    // Map encoder index to display position
    uint8_t displayPos = kEncoderToDisplayPos[index];
    if (!_gauges[displayPos]) return;

    // Sync max value from ParameterMap (in case it was updated dynamically)
    #ifdef SIMULATOR_BUILD
        uint8_t maxValue = 255;  // Default in simulator
    #else
        uint8_t maxValue = getParameterMax(index);
    #endif
    _gauges[displayPos]->setMaxValue(maxValue);
    
    // Always update the gauge's stored value (cache it even when not on GLOBAL screen)
    _gauges[displayPos]->setValue(value);

    // Only perform expensive rendering/highlighting when on GLOBAL screen
    if (_currentScreen == UIScreen::GLOBAL) {
        if (!highlight) {
            _gauges[displayPos]->render();
            return;
        }

        // Clear previous highlight (map encoder index to display position)
        if (_highlightIdx < 8 && _highlightIdx != index) {
            uint8_t prevDisplayPos = kEncoderToDisplayPos[_highlightIdx];
            if (_gauges[prevDisplayPos]) {
                _gauges[prevDisplayPos]->setHighlight(false);
            }
        }

        // Update and highlight
        _gauges[displayPos]->setHighlight(true);
        _gauges[displayPos]->render();
        
        _highlightIdx = index;  // Store encoder index for highlight tracking
        _highlightTime = EspHal::millis();
    }
}

void DisplayUI::updateEffectParamSlot(uint8_t slot, int32_t value, bool highlight) {
    (void)slot;
    (void)value;
    (void)highlight;
}

void DisplayUI::setEffectParamSlotLabel(uint8_t slot, const char* label) {
    (void)slot;
    (void)label;
}


void DisplayUI::setConnectionState(bool wifi, bool ws, bool encA, bool encB) {
    if (!_header) return;
    
    DeviceConnState state;
    state.wifi = wifi;
    state.ws = ws;
    state.encA = encA;
    state.encB = encB;
    _header->setConnection(state);
}

void DisplayUI::updateStats() {
    // Legacy method - kept for compatibility but no longer used
    // Stats (heap/psram/uptime) removed from simplified header
}

void DisplayUI::updateHeader() {
    if (!_header) return;
    
    // Update power state from HAL
    int8_t batteryPercent = EspHal::getBatteryLevel();
    bool isCharging = EspHal::isCharging();
    // Get voltage in volts
    float voltage = EspHal::getBatteryVoltage();
    _header->setPower(batteryPercent, isCharging, voltage);
}

#ifndef SIMULATOR_BUILD
void DisplayUI::setColourCorrectionState(const ColorCorrectionState& state) {
    if (!_actionRow) return;

    _actionRow->setGamma(state.gammaValue, state.gammaEnabled);
    _actionRow->setColourMode(state.mode);
    _actionRow->setAutoExposure(state.autoExposureEnabled);
    _actionRow->setBrownGuardrail(state.brownGuardrailEnabled);
}
#endif

// ============================================================================
// Metadata stubs (for effect/palette names from server - not yet wired to UI)
// ============================================================================

void DisplayUI::setCurrentEffect(uint8_t id, const char* name) {
    (void)id;  // ID available if needed for future use
    if (_header_effect) {
        lv_label_set_text(_header_effect, name ? name : "");
    }
}

void DisplayUI::setCurrentPalette(uint8_t id, const char* name) {
    (void)id;  // ID available if needed for future use
    if (_header_palette) {
        lv_label_set_text(_header_palette, name ? name : "");
    }
}

void DisplayUI::setWiFiInfo(const char* ip, const char* ssid, int32_t rssi) {
    if (_header_net_ip) {
        lv_label_set_text(_header_net_ip, ip ? ip : "");
    }
    if (_header_net_ssid) {
        lv_label_set_text(_header_net_ssid, ssid ? ssid : "");
    }
    if (_header_net_rssi) {
        lv_label_set_text_fmt(_header_net_rssi, "%d dBm", rssi);
    }
}

// ============================================================================
// Preset Bank UI Methods
// ============================================================================

void DisplayUI::updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness) {
    if (slot >= 8 || !_presetSlots[slot]) return;

    _presetSlots[slot]->setOccupied(occupied);
    if (occupied) {
        _presetSlots[slot]->setPresetInfo(effectId, paletteId, brightness);
    }
}

void DisplayUI::setActivePresetSlot(uint8_t slot) {
    // Clear previous active
    if (_activePresetSlot < 8 && _presetSlots[_activePresetSlot]) {
        _presetSlots[_activePresetSlot]->setActive(false);
    }

    // Set new active
    _activePresetSlot = slot;
    if (slot < 8 && _presetSlots[slot]) {
        _presetSlots[slot]->setActive(true);
    }
}

#ifndef SIMULATOR_BUILD
void DisplayUI::refreshAllPresetSlots(PresetManager* pm) {
    if (!pm) return;

    uint8_t occupancy = pm->getOccupancyMask();

    for (uint8_t i = 0; i < 8; i++) {
        if (!_presetSlots[i]) continue;

        bool occupied = (occupancy & (1 << i)) != 0;
        _presetSlots[i]->setOccupied(occupied);

        if (occupied) {
            PresetData preset;
            if (pm->getPreset(i, preset)) {
                _presetSlots[i]->setPresetInfo(preset.effectId, preset.paletteId, preset.brightness);
            }
        }
    }
}
#endif  // !SIMULATOR_BUILD

// Network configuration screen implementation
void DisplayUI::showNetworkConfigScreen() {
#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    if (_network_config_visible) {
        return;  // Already visible
    }

    // Create modal screen
    _network_config_screen = lv_obj_create(nullptr);
    lv_obj_set_size(_network_config_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_network_config_screen, lv_color_hex(DesignTokens::BG_PAGE), 0);
    lv_obj_set_style_bg_opa(_network_config_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_network_config_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(_network_config_screen);
    lv_label_set_text(title, "Network Configuration");
    lv_obj_set_style_text_font(title, &bebas_neue_48, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(DesignTokens::FG_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // IP input field
    _network_config_ip_input = lv_textarea_create(_network_config_screen);
    lv_textarea_set_placeholder_text(_network_config_ip_input, "192.168.0.XXX");
    lv_textarea_set_max_length(_network_config_ip_input, 15);
    lv_obj_set_width(_network_config_ip_input, 400);
    lv_obj_align(_network_config_ip_input, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(_network_config_ip_input, lv_color_hex(DesignTokens::SURFACE_ELEVATED), 0);
    lv_obj_set_style_border_color(_network_config_ip_input, lv_color_hex(DesignTokens::BORDER_BASE), 0);
    lv_obj_set_style_text_color(_network_config_ip_input, lv_color_hex(DesignTokens::FG_PRIMARY), 0);

    // Toggle: Use Manual IP
    _network_config_toggle = lv_switch_create(_network_config_screen);
    lv_obj_align(_network_config_toggle, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(_network_config_toggle, lv_color_hex(DesignTokens::BRAND_PRIMARY), LV_PART_INDICATOR);

    lv_obj_t* toggle_label = lv_label_create(_network_config_screen);
    lv_label_set_text(toggle_label, "Use Manual IP");
    lv_obj_align_to(toggle_label, _network_config_toggle, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    lv_obj_set_style_text_color(toggle_label, lv_color_hex(DesignTokens::FG_SECONDARY), 0);

    // Status label
    _network_config_status_label = lv_label_create(_network_config_screen);
    lv_label_set_text(_network_config_status_label, "");
    lv_obj_align(_network_config_status_label, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_color(_network_config_status_label, lv_color_hex(DesignTokens::FG_SECONDARY), 0);

    // Save button
    lv_obj_t* save_btn = lv_obj_create(_network_config_screen);
    lv_obj_add_flag(save_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(save_btn, 150, 50);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 50, -40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(DesignTokens::BRAND_PRIMARY), 0);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    // Cancel button
    lv_obj_t* cancel_btn = lv_obj_create(_network_config_screen);
    lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(cancel_btn, 150, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -50, -40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(DesignTokens::SURFACE_ELEVATED), 0);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    // Load current manual IP if set
    if (g_wifiManager.shouldUseManualIP()) {
        IPAddress manualIP = g_wifiManager.getManualIP();
        lv_textarea_set_text(_network_config_ip_input, manualIP.toString().c_str());
        lv_obj_add_state(_network_config_toggle, LV_STATE_CHECKED);
    }

    // Button callbacks (simplified - would need proper event handling)
    // Note: Full implementation would require proper LVGL event handlers
    // This is a basic structure that can be expanded

    lv_scr_load(_network_config_screen);
    _network_config_visible = true;
#else
    // Non-LVGL implementation would go here
    Serial.println("[UI] Network config screen not implemented for non-LVGL build");
#endif
}

void DisplayUI::hideNetworkConfigScreen() {
#if defined(TAB5_ENCODER_USE_LVGL) && (TAB5_ENCODER_USE_LVGL) && !defined(SIMULATOR_BUILD)
    if (!_network_config_visible) {
        return;
    }

    if (_network_config_screen) {
        lv_obj_del(_network_config_screen);
        _network_config_screen = nullptr;
        _network_config_ip_input = nullptr;
        _network_config_toggle = nullptr;
        _network_config_status_label = nullptr;
    }

    // Return to main screen
    lv_scr_load(_screen_global);
    _network_config_visible = false;
#endif
}

bool DisplayUI::isNetworkConfigVisible() const {
    return _network_config_visible;
}

#endif
