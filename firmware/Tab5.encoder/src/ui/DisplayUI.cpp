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

// Forward declaration - WiFiManager instance is in main.cpp
extern WiFiManager g_wifiManager;

static constexpr uint32_t TAB5_COLOR_BG_PAGE = 0x0A0A0B;
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_BASE = 0x121214;
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_ELEVATED = 0x1A1A1C;
static constexpr uint32_t TAB5_COLOR_BORDER_BASE = 0x2A2A2E;
static constexpr uint32_t TAB5_COLOR_FG_PRIMARY = 0xFFFFFF;
static constexpr uint32_t TAB5_COLOR_FG_SECONDARY = 0x9CA3AF;
static constexpr uint32_t TAB5_COLOR_BRAND_PRIMARY = 0xFFC700;

// Forward declaration
static void formatDuration(uint32_t seconds, char* buf, size_t bufSize);

// Static member initialization
DisplayUI* DisplayUI::s_instance = nullptr;

static constexpr int32_t TAB5_STATUSBAR_HEIGHT = 66;
static constexpr int32_t TAB5_GRID_GAP = 14;
static constexpr int32_t TAB5_GRID_MARGIN = 24;

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

static lv_obj_t* make_card(lv_obj_t* parent, bool elevated) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
                              lv_color_hex(elevated ? TAB5_COLOR_BG_SURFACE_ELEVATED
                                                    : TAB5_COLOR_BG_SURFACE_BASE),
                              LV_PART_MAIN);
    // White borders matching src/src/deck_ui.cpp implementation (2px instead of 3px)
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

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
    if (_screen_global) lv_obj_del(_screen_global);
    if (_screen_zone) lv_obj_del(_screen_zone);
    if (_screen_connectivity) lv_obj_del(_screen_connectivity);
    _screen_global = nullptr;
    _screen_zone = nullptr;
    _screen_connectivity = nullptr;
}

void DisplayUI::begin() {
    if (!LVGLBridge::init()) return;

    _screen_global = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_global, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_global, 0, LV_PART_MAIN);

    _header = lv_obj_create(_screen_global);
    // #region agent log
    Serial.printf("[DEBUG] Header creation - screen width=1280, border_width=2, margin=7px\n");
    // #endregion
    // CRITICAL FIX: Use very conservative width to prevent right-side clipping
    // Screen width = 1280px, border = 2px, need margin on both sides
    // Maximum safe width = 1280 - left_margin - right_margin
    // Using 10px margin each side: width = 1280 - 10 - 10 = 1260
    lv_obj_set_size(_header, 1260, TAB5_STATUSBAR_HEIGHT);
    // Position with 10px margin from left edge (more than 7px to ensure no clipping)
    lv_obj_set_pos(_header, 10, 7);
    // #region agent log
    Serial.printf("[DEBUG] Header: pos=(%d,%d), size=(%d,%d), right_edge=%d, margin_left=10, margin_right=%d\n", 
                  10, 7, 1260, TAB5_STATUSBAR_HEIGHT, 10 + 1260, 1280 - (10 + 1260));
    // #endregion
    lv_obj_set_style_bg_color(_header, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    // White border and rounded corners matching src/src/deck_ui.cpp (2px instead of 3px)
    lv_obj_set_style_border_width(_header, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_header, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(_header, 14, LV_PART_MAIN);
    // #region agent log
    Serial.printf("[DEBUG] Header after styling: actual_width=%d, actual_height=%d, border_width=%d\n",
                  (int)lv_obj_get_width(_header), (int)lv_obj_get_height(_header),
                  (int)lv_obj_get_style_border_width(_header, LV_PART_MAIN));
    // #endregion
    lv_obj_set_style_pad_left(_header, TAB5_GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_header, TAB5_GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_header, 16, LV_PART_MAIN);  // Equal top and bottom padding for proper centering
    lv_obj_set_style_pad_bottom(_header, 16, LV_PART_MAIN);  // Equal top and bottom padding for proper centering
    lv_obj_set_layout(_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Pattern container (fixed width to prevent shifting) - moved to first position
    _header_effect_container = lv_obj_create(_header);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(_header_effect_container, 220, 24);
    // Explicitly set background color to match header (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(_header_effect_container, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
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

    _header_effect = lv_label_create(_header_effect_container);
    lv_label_set_text(_header_effect, "--");
    lv_obj_set_style_text_font(_header_effect, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_effect, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    // #region agent log
    Serial.printf("[DEBUG] Pattern label created, checking styles before clear\n");
    // #endregion
    // Clear borders, text decoration, outline, and shadow
    lv_obj_set_style_border_width(_header_effect, 0, LV_PART_MAIN);
    lv_obj_set_style_text_decor(_header_effect, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    lv_obj_set_style_outline_width(_header_effect, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_header_effect, 0, LV_PART_MAIN);
    // #region agent log
    Serial.printf("[DEBUG] Pattern label: AFTER clear - border=%d, text_decor=%d, outline=%d, shadow=%d\n", 
                  (int)lv_obj_get_style_border_width(_header_effect, LV_PART_MAIN),
                  (int)lv_obj_get_style_text_decor(_header_effect, LV_PART_MAIN),
                  (int)lv_obj_get_style_outline_width(_header_effect, LV_PART_MAIN),
                  (int)lv_obj_get_style_shadow_width(_header_effect, LV_PART_MAIN));
    // #endregion
    // #region agent log
    Serial.printf("[DEBUG] Pattern label: BEFORE setting long_mode, checking if LV_LABEL_LONG_DOT causes underline\n");
    // #endregion
    // Try LV_LABEL_LONG_SCROLL instead of LV_LABEL_LONG_DOT to see if that's causing the line
    lv_label_set_long_mode(_header_effect, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // #region agent log
    Serial.printf("[DEBUG] Pattern label: Changed to LV_LABEL_LONG_SCROLL_CIRCULAR, checking text_decor=%d\n", 
                  (int)lv_obj_get_style_text_decor(_header_effect, LV_PART_MAIN));
    // #endregion
    lv_obj_set_width(_header_effect, 220);

    // Palette container (fixed width to prevent shifting) - moved to second position
    _header_palette_container = lv_obj_create(_header);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(_header_palette_container, 220, 24);
    // Explicitly set background color to match header (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(_header_palette_container, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
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

    _header_palette = lv_label_create(_header_palette_container);
    // #region agent log
    Serial.printf("[DEBUG] Palette label created - checking default styles: border=%d, text_decor=%d, outline_width=%d\n", 
                  (int)lv_obj_get_style_border_width(_header_palette, LV_PART_MAIN),
                  (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN),
                  (int)lv_obj_get_style_outline_width(_header_palette, LV_PART_MAIN));
    // #endregion
    lv_label_set_text(_header_palette, "--");
    lv_obj_set_style_text_font(_header_palette, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_palette, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    // Clear borders, text decoration, outline, and shadow
    lv_obj_set_style_border_width(_header_palette, 0, LV_PART_MAIN);
    lv_obj_set_style_text_decor(_header_palette, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    lv_obj_set_style_outline_width(_header_palette, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_header_palette, 0, LV_PART_MAIN);
    // #region agent log
    Serial.printf("[DEBUG] Palette label: AFTER clear - border=%d, text_decor=%d, outline=%d, shadow=%d\n", 
                  (int)lv_obj_get_style_border_width(_header_palette, LV_PART_MAIN),
                  (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN),
                  (int)lv_obj_get_style_outline_width(_header_palette, LV_PART_MAIN),
                  (int)lv_obj_get_style_shadow_width(_header_palette, LV_PART_MAIN));
    // #endregion
    // #region agent log
    Serial.printf("[DEBUG] Palette label: BEFORE setting long_mode, checking if LV_LABEL_LONG_DOT causes underline\n");
    // #endregion
    // Try LV_LABEL_LONG_SCROLL instead of LV_LABEL_LONG_DOT to see if that's causing the line
    lv_label_set_long_mode(_header_palette, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // #region agent log
    Serial.printf("[DEBUG] Palette label: Changed to LV_LABEL_LONG_SCROLL_CIRCULAR, checking text_decor=%d\n", 
                  (int)lv_obj_get_style_text_decor(_header_palette, LV_PART_MAIN));
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
    lv_obj_set_style_text_color(_header_title_main, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_align(_header_title_main, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    // Ensure consistent vertical alignment
    lv_obj_set_style_pad_top(_header_title_main, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_title_main, 0, LV_PART_MAIN);

    _header_title_os = lv_label_create(title_container);
    lv_label_set_text(_header_title_os, "OS");
    lv_obj_set_style_text_font(_header_title_os, &bebas_neue_40px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_title_os, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_align(_header_title_os, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    // Match padding to align baseline with main title
    lv_obj_set_style_pad_top(_header_title_os, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_header_title_os, 0, LV_PART_MAIN);

    // Spacer to push network info to the right
    lv_obj_t* right_spacer = lv_obj_create(_header);
    lv_obj_set_style_bg_opa(right_spacer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_spacer, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(right_spacer, 1);

    // Network info: SSID (RSSI) IP - create in display order
    _header_net_ssid = lv_label_create(_header);
    lv_label_set_text(_header_net_ssid, "--");
    lv_obj_set_style_text_font(_header_net_ssid, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_ssid, 18, LV_PART_MAIN);

    _header_net_rssi = lv_label_create(_header);
    lv_label_set_text(_header_net_rssi, "");
    lv_obj_set_style_text_font(_header_net_rssi, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_rssi, 4, LV_PART_MAIN);

    _header_net_ip = lv_label_create(_header);
    lv_label_set_text(_header_net_ip, "--");
    lv_obj_set_style_text_font(_header_net_ip, &bebas_neue_24px, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_pad_left(_header_net_ip, 8, LV_PART_MAIN);  // 8px gap after RSSI

    // Create retry button (initially hidden) - moved to last position
    _header_retry_button = lv_label_create(_header);
    lv_label_set_text(_header_retry_button, "RETRY");
    lv_obj_set_style_text_font(_header_retry_button, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_header_retry_button, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
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

    _gauges_container = lv_obj_create(_screen_global);
    lv_obj_set_size(_gauges_container, 1280 - 2 * TAB5_GRID_MARGIN, 125);
    lv_obj_align(_gauges_container, LV_ALIGN_TOP_MID, 0, TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN);
    lv_obj_set_style_bg_opa(_gauges_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_gauges_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_gauges_container, 0, LV_PART_MAIN);
    lv_obj_set_layout(_gauges_container, LV_LAYOUT_GRID);

    static lv_coord_t col_dsc[9] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[2] = {125, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_gauges_container, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(_gauges_container, TAB5_GRID_GAP, LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        _gauge_cards[i] = make_card(_gauges_container, false);
        lv_obj_set_grid_cell(_gauge_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        _gauge_labels[i] = lv_label_create(_gauge_cards[i]);
        lv_label_set_text(_gauge_labels[i], kParamNames[i]);
        lv_obj_set_style_text_font(_gauge_labels[i], BEBAS_BOLD_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(_gauge_labels[i], lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_align(_gauge_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        _gauge_values[i] = lv_label_create(_gauge_cards[i]);
        lv_label_set_text(_gauge_values[i], "--");
        lv_obj_set_style_text_font(_gauge_values[i], JETBRAINS_MONO_REG_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(_gauge_values[i], lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_gauge_values[i], LV_ALIGN_TOP_MID, 0, 30);  // Moved down 2px for even spacing

        _gauge_bars[i] = lv_bar_create(_gauge_cards[i]);
        lv_bar_set_range(_gauge_bars[i], 0, 255);
        lv_bar_set_value(_gauge_bars[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(_gauge_bars[i], LV_PCT(90), 10);  // 90% width, 10px height
        lv_obj_align(_gauge_bars[i], LV_ALIGN_BOTTOM_MID, 0, -10);  // Adjusted for even spacing
        lv_obj_set_style_bg_color(_gauge_bars[i], lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_color(_gauge_bars[i], lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(_gauge_bars[i], 8, LV_PART_INDICATOR);
    }

    _preset_container = lv_obj_create(_screen_global);
    lv_obj_set_size(_preset_container, 1280 - 2 * TAB5_GRID_MARGIN, 85);
    lv_obj_align(_preset_container, LV_ALIGN_TOP_MID, 0,
                 TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN + 125 + TAB5_GRID_GAP);
    lv_obj_set_style_bg_opa(_preset_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_preset_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_preset_container, 0, LV_PART_MAIN);
    lv_obj_set_layout(_preset_container, LV_LAYOUT_GRID);
    static lv_coord_t preset_row_dsc[2] = {85, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_preset_container, col_dsc, preset_row_dsc);
    lv_obj_set_style_pad_column(_preset_container, TAB5_GRID_GAP, LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        _preset_cards[i] = make_card(_preset_container, true);
        lv_obj_set_grid_cell(_preset_cards[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);

        _preset_labels[i] = lv_label_create(_preset_cards[i]);
        lv_label_set_text_fmt(_preset_labels[i], "P%d", i + 1);
        lv_obj_set_style_text_font(_preset_labels[i], &bebas_neue_24px, LV_PART_MAIN);
        lv_obj_set_style_text_color(_preset_labels[i], lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_align(_preset_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        _preset_values[i] = lv_label_create(_preset_cards[i]);
        lv_label_set_text(_preset_values[i], "--");
        lv_obj_set_style_text_color(_preset_values[i], lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
        lv_obj_align(_preset_values[i], LV_ALIGN_TOP_MID, 0, 28);  // Position below title with uniform spacing
    }

    // Create action row (third row) - 4 buttons for GAMMA, COLOUR, EXPOSURE, BROWN
    _action_container = lv_obj_create(_screen_global);
    lv_obj_set_size(_action_container, 1280 - 2 * TAB5_GRID_MARGIN, 100);
    lv_obj_align(_action_container, LV_ALIGN_TOP_MID, 0,
                 TAB5_STATUSBAR_HEIGHT + TAB5_GRID_MARGIN + 125 + TAB5_GRID_GAP + 85 + TAB5_GRID_GAP);
    lv_obj_set_style_bg_opa(_action_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_action_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_action_container, 0, LV_PART_MAIN);
    lv_obj_set_layout(_action_container, LV_LAYOUT_GRID);
    static lv_coord_t action_col_dsc[6] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t action_row_dsc[2] = {100, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(_action_container, action_col_dsc, action_row_dsc);
    lv_obj_set_style_pad_column(_action_container, TAB5_GRID_GAP, LV_PART_MAIN);

    const char* action_names[] = {"GAMMA", "COLOUR", "EXPOSURE", "BROWN", "ZONES"};
    for (int i = 0; i < 5; i++) {
        _action_buttons[i] = make_card(_action_container, false);
        lv_obj_set_grid_cell(_action_buttons[i], LV_GRID_ALIGN_STRETCH, i, 1,
                             LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_flag(_action_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(_action_buttons[i], LV_OBJ_FLAG_SCROLLABLE);

        _action_labels[i] = lv_label_create(_action_buttons[i]);
        lv_label_set_text(_action_labels[i], action_names[i]);
        lv_obj_set_style_text_color(_action_labels[i], lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_font(_action_labels[i], RAJDHANI_MED_24, LV_PART_MAIN);
        lv_obj_align(_action_labels[i], LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_clear_flag(_action_labels[i], LV_OBJ_FLAG_CLICKABLE);  // Prevent label from capturing touch
        lv_obj_add_flag(_action_labels[i], LV_OBJ_FLAG_EVENT_BUBBLE);  // Let touch events bubble to button

        _action_values[i] = lv_label_create(_action_buttons[i]);
        lv_label_set_text(_action_values[i], "--");
        lv_obj_set_style_text_color(_action_values[i], lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
        // GAMMA (index 0) uses monospace bold for numeric values; others use Rajdhani bold for text
        lv_obj_set_style_text_font(_action_values[i], (i == 0) ? JETBRAINS_MONO_BOLD_32 : RAJDHANI_BOLD_32, LV_PART_MAIN);
        lv_obj_align(_action_values[i], LV_ALIGN_TOP_MID, 0, 28);  // Position below title with uniform spacing
        lv_obj_clear_flag(_action_values[i], LV_OBJ_FLAG_CLICKABLE);  // Prevent value from capturing touch
        lv_obj_add_flag(_action_values[i], LV_OBJ_FLAG_EVENT_BUBBLE);  // Let touch events bubble to button

        // Add event handler for button clicks
        lv_obj_set_user_data(_action_buttons[i], reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(_action_buttons[i], [](lv_event_t* e) {
            lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
            DisplayUI* ui = static_cast<DisplayUI*>(lv_event_get_user_data(e));

            // Special handling for ZONES button (index 4)
            if (index == 4) {
                Serial.println("[DisplayUI] ZONES button pressed - switching to Zone Composer");
                if (ui) {
                    ui->setScreen(UIScreen::ZONE_COMPOSER);
                }
            } else if (ui && ui->_action_callback) {
                // Other buttons call the registered callback
                ui->_action_callback(index);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(this));
    }

    // Create footer (66px height, matching header)
    _footer = lv_obj_create(_screen_global);
    // Reduce width by 2 * (border_width + padding) = 2 * (2 + 1) = 6px to prevent border clipping
    lv_obj_set_size(_footer, 1280 - 6, TAB5_STATUSBAR_HEIGHT);
    // Position at bottom: 720px - 66px - 3px (border/padding offset) = 651px from top
    lv_obj_set_pos(_footer, 3, 720 - TAB5_STATUSBAR_HEIGHT - 3);
    lv_obj_set_style_bg_color(_footer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_width(_footer, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_footer, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(_footer, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_footer, TAB5_GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer, TAB5_GRID_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer, 16, LV_PART_MAIN);
    lv_obj_clear_flag(_footer, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling on footer
    lv_obj_set_layout(_footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left side: BPM, KEY, MIC, UPTIME - each in fixed-width containers
    lv_obj_t* leftGroup = lv_obj_create(_footer);
    lv_obj_set_style_bg_opa(leftGroup, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(leftGroup, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(leftGroup, 0, LV_PART_MAIN);
    lv_obj_clear_flag(leftGroup, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    // Removed flex_grow(1) - let leftGroup only take space it needs, allowing battery to be closer
    lv_obj_set_layout(leftGroup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(leftGroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftGroup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(leftGroup, 40, LV_PART_MAIN);  // Increased gap between metric groups (was 24px)

    // BPM: Fixed-width container with title + value
    lv_obj_t* bpmContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(bpmContainer, 95, 24);  // +20px - fits "BPM: 999" comfortably
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(bpmContainer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bpmContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bpmContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(bpmContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(bpmContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bpmContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bpmContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_bpm = lv_label_create(bpmContainer);
    lv_label_set_text(_footer_bpm, "BPM:");
    lv_obj_set_style_text_font(_footer_bpm, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_bpm, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_bpm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_bpm, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_bpm_value = lv_label_create(bpmContainer);
    lv_label_set_text(_footer_bpm_value, "--");
    lv_obj_set_style_text_font(_footer_bpm_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_bpm_value, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_bpm_value, 0, LV_PART_MAIN);

    // KEY: Fixed-width container with title + value
    // Maximum key length: 5 chars (e.g., "C#dim", "A#aug") = ~70px at 24px font
    // Title "KEY:" = ~45px, so total = 115px + 2px gap = 117px → 112px is tight but OK
    lv_obj_t* keyContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(keyContainer, 112, 24);  // Fixed width for longest key (C#dim, A#aug)
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(keyContainer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(keyContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(keyContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(keyContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(keyContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(keyContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(keyContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_key = lv_label_create(keyContainer);
    lv_label_set_text(_footer_key, "KEY:");
    lv_obj_set_style_text_font(_footer_key, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_key, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_key, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_key, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_key_value = lv_label_create(keyContainer);
    lv_label_set_text(_footer_key_value, "--");
    lv_obj_set_style_text_font(_footer_key_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_key_value, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_key_value, 0, LV_PART_MAIN);

    // MIC: Fixed-width container with title + value
    lv_obj_t* micContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(micContainer, 125, 24);  // +25px - fits "MIC: -99.9 DB" comfortably
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(micContainer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(micContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(micContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(micContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(micContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(micContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(micContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_mic = lv_label_create(micContainer);
    lv_label_set_text(_footer_mic, "MIC:");
    lv_obj_set_style_text_font(_footer_mic, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_mic, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_mic, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_mic, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_mic_value = lv_label_create(micContainer);
    lv_label_set_text(_footer_mic_value, "--");
    lv_obj_set_style_text_font(_footer_mic_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_mic_value, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_mic_value, 0, LV_PART_MAIN);

    // UPTIME: Fixed-width container with title + value
    lv_obj_t* uptimeContainer = lv_obj_create(leftGroup);
    lv_obj_set_size(uptimeContainer, 145, 24);  // +25px - fits "UPTIME: 999h 59m" comfortably
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(uptimeContainer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(uptimeContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(uptimeContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(uptimeContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(uptimeContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(uptimeContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(uptimeContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _footer_host_uptime = lv_label_create(uptimeContainer);
    lv_label_set_text(_footer_host_uptime, "UPTIME:");
    lv_obj_set_style_text_font(_footer_host_uptime, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_host_uptime, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_host_uptime, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_footer_host_uptime, 2, LV_PART_MAIN);  // Tiny gap: 2px
    _footer_uptime_value = lv_label_create(uptimeContainer);
    lv_label_set_text(_footer_uptime_value, "--");
    lv_obj_set_style_text_font(_footer_uptime_value, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_uptime_value, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_footer_uptime_value, 0, LV_PART_MAIN);

    // Right side: WS and Battery (with bar) - aligned right, ensure it has space
    lv_obj_t* rightGroup = lv_obj_create(_footer);
    // Don't use LV_SIZE_CONTENT - set explicit minimum width to ensure WS and Battery fit
    // WS label ~80px + gap 24px + Battery label ~145px + bar 60px + gap 8px = ~317px minimum → 345px for safety
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(rightGroup, 345, 24);  // +25px for safety margin - prevents battery text clipping
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(rightGroup, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
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
    lv_obj_set_style_text_color(_footer_ws_status, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer_ws_status, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_ws_status, 0, LV_PART_MAIN);

    // Battery container: label + bar side by side
    lv_obj_t* batteryContainer = lv_obj_create(rightGroup);
    // Set height explicitly to font size (24px) instead of line height (26px) to prevent extra space at bottom
    lv_obj_set_size(batteryContainer, LV_SIZE_CONTENT, 24);  // Height fixed to font size
    // Explicitly set background color to match footer (not just opacity) to prevent any default color rendering
    lv_obj_set_style_bg_color(batteryContainer, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(batteryContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(batteryContainer, 0, LV_PART_MAIN);
    // Clear background on all parts to ensure no rendering
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_layout(batteryContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(batteryContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(batteryContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(batteryContainer, 8, LV_PART_MAIN);  // 8px gap between label and bar

    _footer_battery = lv_label_create(batteryContainer);
    lv_label_set_text(_footer_battery, "BATTERY: --");
    lv_obj_set_style_text_font(_footer_battery, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_footer_battery, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_pad_top(_footer_battery, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_battery, 0, LV_PART_MAIN);

    // Battery bar - positioned right next to battery label
    _footer_battery_bar = lv_bar_create(batteryContainer);
    lv_bar_set_range(_footer_battery_bar, 0, 100);
    lv_bar_set_value(_footer_battery_bar, 0, LV_ANIM_OFF);
    lv_obj_set_size(_footer_battery_bar, 60, 8);  // 60px wide, 8px tall
    lv_obj_set_style_radius(_footer_battery_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(_footer_battery_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_pad_top(_footer_battery_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_footer_battery_bar, 0, LV_PART_MAIN);

    // Create zone composer screen
    _screen_zone = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_zone, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_zone, 0, LV_PART_MAIN);

    // Create ZoneComposerUI and initialize with zone screen as parent
    _zoneComposer = new ZoneComposerUI(_display);
    _zoneComposer->setBackButtonCallback(onZoneComposerBackButton);  // Wire Back button
    _zoneComposer->begin(_screen_zone);  // Create LVGL widgets on zone screen

    Serial.println("[DisplayUI] Zone Composer initialized");

    Serial.printf("[DisplayUI_TRACE] Creating connectivity screen @ %lu ms\n", millis());
    esp_task_wdt_reset();

    // Create connectivity screen
    _screen_connectivity = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen_connectivity, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_screen_connectivity, 0, LV_PART_MAIN);

    Serial.printf("[DisplayUI_TRACE] Connectivity screen created @ %lu ms\n", millis());
    esp_task_wdt_reset();

#if ENABLE_WIFI
    Serial.printf("[DisplayUI_TRACE] Creating ConnectivityTab @ %lu ms\n", millis());
    Serial.flush();
    // Create ConnectivityTab and initialize with connectivity screen as parent
    _connectivityTab = new ConnectivityTab(_display);
    Serial.printf("[DisplayUI_TRACE] ConnectivityTab constructed @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->setBackButtonCallback(onConnectivityTabBackButton);  // Wire Back button
    Serial.printf("[DisplayUI_TRACE] setBackButtonCallback done @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->setWiFiManager(&g_wifiManager);
    Serial.printf("[DisplayUI_TRACE] setWiFiManager done @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset();
    Serial.printf("[DisplayUI_TRACE] Before ConnectivityTab::begin @ %lu ms\n", millis());
    Serial.flush();
    _connectivityTab->begin(_screen_connectivity);  // Create LVGL widgets on connectivity screen
    Serial.printf("[DisplayUI_TRACE] After ConnectivityTab::begin @ %lu ms\n", millis());
    Serial.flush();
    Serial.println("[DisplayUI] Connectivity Tab initialized");
#else
    Serial.println("[DisplayUI_TRACE] ENABLE_WIFI not defined - skipping ConnectivityTab");
#endif

    Serial.printf("[DisplayUI_TRACE] Before lv_scr_load @ %lu ms\n", millis());
    esp_task_wdt_reset();
    lv_scr_load(_screen_global);
    Serial.printf("[DisplayUI_TRACE] After lv_scr_load @ %lu ms\n", millis());
    esp_task_wdt_reset();
    _currentScreen = UIScreen::GLOBAL;
    Serial.printf("[DisplayUI_TRACE] begin() complete @ %lu ms\n", millis());
}

void DisplayUI::loop() {
    const uint32_t now = millis();
    for (int i = 0; i < 8; i++) {
        if (_feedback_until_ms[i] != 0 && now >= _feedback_until_ms[i]) {
            _feedback_until_ms[i] = 0;
            // After feedback expires, restore the appropriate border color:
            // - Green (0x00FF99) if this is the active preset
            // - Yellow (TAB5_COLOR_BRAND_PRIMARY) if preset is saved (occupied) but not active
            // - White (0xFFFFFF) if preset is empty
            if (i == _activePresetSlot && _activePresetSlot < 8) {
                // Active preset: green border
                lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0x00FF99), LV_PART_MAIN);
            } else {
                // Check if preset is occupied (saved) - if so, use yellow, otherwise white
                // We can check if the preset value label is not "--" to determine if it's saved
                if (_preset_values[i]) {
                    const char* valueText = lv_label_get_text(_preset_values[i]);
                    if (valueText && strcmp(valueText, "--") != 0) {
                        // Preset is saved but not active: yellow border
                        lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
                    } else {
                        // Preset is empty: white border
                        lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                    }
                } else {
                    // Fallback: white border
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
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
                    batColor = TAB5_COLOR_BRAND_PRIMARY;  // Yellow (0xFFC700)
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
                lv_obj_set_style_text_color(_footer_battery, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
                if (_footer_battery_bar) {
                    lv_bar_set_value(_footer_battery_bar, 0, LV_ANIM_OFF);
                    lv_obj_set_style_bg_color(_footer_battery_bar, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_INDICATOR);
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
        if (_footer_uptime_value) {
            char buf[32];
            // Always format uptime - even if 0, show "0s" instead of "--"
            formatDuration(_hostUptime, buf, sizeof(buf));
            lv_label_set_text(_footer_uptime_value, buf);
        }
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

    lv_obj_set_style_border_color(
        _gauge_cards[displayPos],
        lv_color_hex(highlight ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
        LV_PART_MAIN);
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
        lv_label_set_text(_header_effect, "--");
    }
    (void)id; // ID no longer displayed
}

void DisplayUI::setCurrentPalette(uint8_t id, const char* name) {
    if (!_header_palette) return;
    if (name && name[0]) {
        lv_label_set_text(_header_palette, name);
    } else {
        lv_label_set_text(_header_palette, "--");
    }
    (void)id; // ID no longer displayed
}

void DisplayUI::setWiFiInfo(const char* ip, const char* ssid, int32_t rssi) {
    if (!_header_net_ip || !_header_net_ssid || !_header_net_rssi) return;
    
    if (ip && ssid && ip[0] && ssid[0]) {
        // Format: "SSID (RSSI dBm) IP" - SSID in secondary color, RSSI color-coded, IP in primary color
        lv_label_set_text(_header_net_ssid, ssid);
        lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        
        // Format RSSI with parentheses and unit (dBm)
        char rssiText[20];
        snprintf(rssiText, sizeof(rssiText), " (%ld dBm)", (long)rssi);
        lv_label_set_text(_header_net_rssi, rssiText);
        
        // Color-code RSSI: Green (>-50), Yellow (-50 to -70), Red (<-70)
        // Yellow uses the same color as "OS" (TAB5_COLOR_BRAND_PRIMARY = 0xFFC700)
        uint32_t rssiColor;
        if (rssi > -50) {
            rssiColor = 0x00FF00;  // Green - excellent signal
        } else if (rssi > -70) {
            rssiColor = TAB5_COLOR_BRAND_PRIMARY;  // Yellow (same as "OS") - good signal
        } else {
            rssiColor = 0xFF0000;  // Red - poor signal
        }
        lv_obj_set_style_text_color(_header_net_rssi, lv_color_hex(rssiColor), LV_PART_MAIN);
        
        // IP address comes after RSSI
        lv_label_set_text(_header_net_ip, ip);
        lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
        // Update padding: 8px between RSSI and IP (was 4px between SSID and RSSI)
        lv_obj_set_style_pad_left(_header_net_ip, 8, LV_PART_MAIN);
    } else {
        lv_label_set_text(_header_net_ssid, "--");
        lv_obj_set_style_text_color(_header_net_ssid, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_label_set_text(_header_net_rssi, "");
        lv_label_set_text(_header_net_ip, "--");
        lv_obj_set_style_text_color(_header_net_ip, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
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

void DisplayUI::updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness) {
    if (slot >= 8) return;
    if (!_preset_values[slot] || !_preset_cards[slot]) return;

    if (!occupied) {
        lv_label_set_text(_preset_values[slot], "--");
        lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        return;
    }

    lv_label_set_text_fmt(_preset_values[slot], "E%u P%u %u", effectId, paletteId, brightness);
    lv_obj_set_style_border_color(_preset_cards[slot], lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
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
            // Not active: check if saved (yellow) or empty (white)
            if (_preset_values[i]) {
                const char* valueText = lv_label_get_text(_preset_values[i]);
                if (valueText && strcmp(valueText, "--") != 0) {
                    // Preset is saved but not active: yellow border
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
                } else {
                    // Preset is empty: white border
                    lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                }
            } else {
                // Fallback: white border
                lv_obj_set_style_border_color(_preset_cards[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
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
    if (!_action_buttons[0] || !_action_values[0]) return;

    // GAMMA button (index 0)
    // Display format: "2.2" for 2.2, "2.5" for 2.5, "2.8" for 2.8, "OFF" when disabled
    char gamma_buf[16];
    if (state.gammaEnabled) {
        snprintf(gamma_buf, sizeof(gamma_buf), "%.1f", state.gammaValue);
    } else {
        strcpy(gamma_buf, "OFF");
    }
    lv_label_set_text(_action_values[0], gamma_buf);
    lv_obj_set_style_border_color(_action_buttons[0],
                                   lv_color_hex(state.gammaEnabled ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // COLOUR button (index 1)
    const char* colour_mode = "OFF";
    if (state.mode == 1) colour_mode = "HSV";
    else if (state.mode == 2) colour_mode = "RGB";
    else if (state.mode == 3) colour_mode = "BOTH";
    lv_label_set_text(_action_values[1], colour_mode);
    lv_obj_set_style_border_color(_action_buttons[1],
                                   lv_color_hex(state.mode != 0 ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // EXPOSURE button (index 2)
    lv_label_set_text(_action_values[2], state.autoExposureEnabled ? "ON" : "OFF");
    lv_obj_set_style_border_color(_action_buttons[2],
                                   lv_color_hex(state.autoExposureEnabled ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);

    // BROWN button (index 3)
    lv_label_set_text(_action_values[3], state.brownGuardrailEnabled ? "ON" : "OFF");
    lv_obj_set_style_border_color(_action_buttons[3],
                                   lv_color_hex(state.brownGuardrailEnabled ? TAB5_COLOR_BRAND_PRIMARY : 0xFFFFFF),
                                   LV_PART_MAIN);
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
        if (_bpm >= 0.0f) {  // Valid BPM (allow 0.0f)
            snprintf(buf, sizeof(buf), "%.0f", _bpm);
        } else {
            strcpy(buf, "--");
        }
        lv_label_set_text(_footer_bpm_value, buf);
    }

    if (_footer_key_value) {
        char buf[32];
        if (_key[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s", _key);
        } else {
            strcpy(buf, "--");
        }
        lv_label_set_text(_footer_key_value, buf);
    }

    if (_footer_mic_value) {
        char buf[32];
        if (_micLevel > -80.0f) {  // Valid mic level (above silence threshold)
            snprintf(buf, sizeof(buf), "%.1f DB", _micLevel);  // Added space before DB
        } else {
            strcpy(buf, "--");
        }
        lv_label_set_text(_footer_mic_value, buf);
    }
}

void DisplayUI::updateHostUptime(uint32_t uptimeSeconds) {
    _hostUptime = uptimeSeconds;
    // #region agent log
    Serial.printf("[DisplayUI] updateHostUptime called: %lu seconds\n", uptimeSeconds);
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
    uint32_t statusColor = TAB5_COLOR_FG_SECONDARY;  // Gray for disconnected

    switch (status) {
        case WebSocketStatus::CONNECTED:
            statusText = "OK";  // Abbreviated from "CONNECTED" (~50px vs ~115px)
            statusColor = 0x00FF00;  // Green
            break;
        case WebSocketStatus::CONNECTING:
            statusText = "...";  // Abbreviated from "CONNECTING" (~25px vs ~120px)
            statusColor = TAB5_COLOR_BRAND_PRIMARY;  // Yellow
            break;
        case WebSocketStatus::DISCONNECTED:
            statusText = "OFF";  // Abbreviated from "DISCONNECTED" (~55px vs ~160px)
            statusColor = TAB5_COLOR_FG_SECONDARY;  // Gray
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
    // #region agent log
    EspHal::log("[DEBUG] DisplayUI::begin entry - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    EspHal::log("[DEBUG] Sprite memory estimate: %u gauges * %dx%d + %u slots * %dx%d + header %dx%d = ~%u KB\n",
                  8, Theme::CELL_W, Theme::CELL_H,
                  8, Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H,
                  Theme::SCREEN_W, Theme::STATUS_BAR_H,
                  ((8 * Theme::CELL_W * Theme::CELL_H * 2) + 
                   (8 * Theme::PRESET_SLOT_W * Theme::PRESET_SLOT_H * 2) +
                   (Theme::SCREEN_W * Theme::STATUS_BAR_H * 2)) / 1024);
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] begin_start cols=%d rows=%d cellW=%d cellH=%d\n", Theme::GRID_COLS, Theme::GRID_ROWS, Theme::CELL_W, Theme::CELL_H);
#endif

    _display.fillScreen(Theme::BG_DARK);

    // Create header
    // #region agent log
    EspHal::log("[DEBUG] Before UIHeader creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
    // #endregion
    _header = new UIHeader(&_display);
    // #region agent log
    EspHal::log("[DEBUG] After UIHeader creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
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

        // #region agent log
        if (i == 0 || i == 7) {  // Log first and last gauge creation
            EspHal::log("[DEBUG] Creating gauge %d - Heap before: free=%u minFree=%u\n",
                          i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        }
        // #endregion
        _gauges[i] = new GaugeWidget(&_display, x, y, Theme::CELL_W, Theme::CELL_H, i);
        // #region agent log
        if (i == 0 || i == 7) {
            EspHal::log("[DEBUG] Gauge %d created - Heap after: free=%u minFree=%u\n",
                          i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        }
        // #endregion
        
        // Set initial max value from ParameterMap
        #ifdef SIMULATOR_BUILD
            uint8_t maxValue = 255;  // Default in simulator
        #else
            uint8_t maxValue = getParameterMax(i);
        #endif
        _gauges[i]->setMaxValue(maxValue);
    }
    
    // #region agent log
    EspHal::log("[DEBUG] All 8 gauges created - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] gauges_created count=8\n");
#endif
    
    // Clear remaining gauge slots (8-15) - not used in global view
    for (int i = 8; i < 16; i++) {
        _gauges[i] = nullptr;
    }

    // Create 8 preset slot widgets below gauge row
    // #region agent log
    EspHal::log("[DEBUG] Before preset slots creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
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
    // #region agent log
    EspHal::log("[DEBUG] All 8 preset slots created - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
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
        lv_label_set_text(_header_effect, name ? name : "--");
    }
}

void DisplayUI::setCurrentPalette(uint8_t id, const char* name) {
    (void)id;  // ID available if needed for future use
    if (_header_palette) {
        lv_label_set_text(_header_palette, name ? name : "--");
    }
}

void DisplayUI::setWiFiInfo(const char* ip, const char* ssid, int32_t rssi) {
    if (_header_net_ip) {
        lv_label_set_text(_header_net_ip, ip ? ip : "--");
    }
    if (_header_net_ssid) {
        lv_label_set_text(_header_net_ssid, ssid ? ssid : "--");
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
    lv_obj_set_style_bg_color(_network_config_screen, lv_color_hex(TAB5_COLOR_BG_PAGE), 0);
    lv_obj_set_style_bg_opa(_network_config_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_network_config_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(_network_config_screen);
    lv_label_set_text(title, "Network Configuration");
    lv_obj_set_style_text_font(title, &bebas_neue_48, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // IP input field
    _network_config_ip_input = lv_textarea_create(_network_config_screen);
    lv_textarea_set_placeholder_text(_network_config_ip_input, "192.168.0.XXX");
    lv_textarea_set_max_length(_network_config_ip_input, 15);
    lv_obj_set_width(_network_config_ip_input, 400);
    lv_obj_align(_network_config_ip_input, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_bg_color(_network_config_ip_input, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), 0);
    lv_obj_set_style_border_color(_network_config_ip_input, lv_color_hex(TAB5_COLOR_BORDER_BASE), 0);
    lv_obj_set_style_text_color(_network_config_ip_input, lv_color_hex(TAB5_COLOR_FG_PRIMARY), 0);

    // Toggle: Use Manual IP
    _network_config_toggle = lv_switch_create(_network_config_screen);
    lv_obj_align(_network_config_toggle, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(_network_config_toggle, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_INDICATOR);

    lv_obj_t* toggle_label = lv_label_create(_network_config_screen);
    lv_label_set_text(toggle_label, "Use Manual IP");
    lv_obj_align_to(toggle_label, _network_config_toggle, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    lv_obj_set_style_text_color(toggle_label, lv_color_hex(TAB5_COLOR_FG_SECONDARY), 0);

    // Status label
    _network_config_status_label = lv_label_create(_network_config_screen);
    lv_label_set_text(_network_config_status_label, "");
    lv_obj_align(_network_config_status_label, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_color(_network_config_status_label, lv_color_hex(TAB5_COLOR_FG_SECONDARY), 0);

    // Save button
    lv_obj_t* save_btn = lv_btn_create(_network_config_screen);
    lv_obj_set_size(save_btn, 150, 50);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 50, -40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), 0);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(_network_config_screen);
    lv_obj_set_size(cancel_btn, 150, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -50, -40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), 0);
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
