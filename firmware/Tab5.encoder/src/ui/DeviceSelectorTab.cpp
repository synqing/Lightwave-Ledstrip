// ============================================================================
// DeviceSelectorTab Implementation - Multi-Device Selector UI
// ============================================================================
// Unified with ZoneComposerUI / ConnectivityTab TAB5 Design System
// ============================================================================

#include "../config/Config.h"
#include "DeviceSelectorTab.h"

#if ENABLE_WIFI

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include "../hal/EspHal.h"
#include "Theme.h"
#include "../input/ButtonHandler.h"
#include "../network/WebSocketClient.h"
#include "fonts/experimental_fonts.h"

// ============================================================================
// TAB5 Design System Colors (matches ConnectivityTab / ZoneComposerUI)
// ============================================================================
static constexpr uint32_t TAB5_COLOR_BG_PAGE             = 0x0A0A0B;  // Page background
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_BASE     = 0x121214;  // Card base
static constexpr uint32_t TAB5_COLOR_BG_SURFACE_ELEVATED = 0x1A1A1C;  // Elevated cards
static constexpr uint32_t TAB5_COLOR_BORDER_BASE         = 0x2A2A2E;  // Default borders
static constexpr uint32_t TAB5_COLOR_FG_PRIMARY          = 0xFFFFFF;  // White text
static constexpr uint32_t TAB5_COLOR_FG_SECONDARY        = 0x9CA3AF;  // Gray text
static constexpr uint32_t TAB5_COLOR_BRAND_PRIMARY       = 0xFFC700;  // Brand yellow
static constexpr uint32_t TAB5_COLOR_STATUS_SUCCESS      = 0x22C55E;  // Green
static constexpr uint32_t TAB5_COLOR_STATUS_ERROR        = 0xEF4444;  // Red
static constexpr uint32_t TAB5_COLOR_STATUS_WARNING      = 0xF59E0B;  // Amber
static constexpr int TAB5_GRID_MARGIN = 20;
static constexpr int TAB5_GRID_GAP = 12;

// ============================================================================
// TAB5 Card Helper (matches ConnectivityTab make_card pattern)
// ============================================================================
static lv_obj_t* make_card(lv_obj_t* parent, bool elevated = false) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_style_bg_color(card,
        lv_color_hex(elevated ? TAB5_COLOR_BG_SURFACE_ELEVATED : TAB5_COLOR_BG_SURFACE_BASE),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

// Helper function to format IP address from DeviceInfo
static void formatDeviceIP(const DeviceInfo& dev, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", dev.ip[0], dev.ip[1], dev.ip[2], dev.ip[3]);
}

DeviceSelectorTab::DeviceSelectorTab(M5GFX& display)
    : _display(display)
{
}

DeviceSelectorTab::~DeviceSelectorTab() {
    // Cleanup handled by LVGL parent
    #if ENABLE_WIFI
    if (_ownsHttpClient && _httpClient) {
        delete _httpClient;
        _httpClient = nullptr;
    }
    #endif
}

void DeviceSelectorTab::begin(lv_obj_t* parent) {
    esp_task_wdt_reset();

    markDirty();
    _lastRenderTime = 0;

    esp_task_wdt_reset();

    // Initialize LVGL styles
    initStyles();

    esp_task_wdt_reset();

    // Create LVGL widgets if parent provided
    if (parent) {
        createInteractiveUI(parent);
        Serial.println("[DeviceSelectorTab] LVGL interactive UI created");
        Serial.flush();
    }

    Serial.println("[DeviceSelectorTab] Interactive UI initialized");

    // Update status label immediately
    updateStatusLabel();

    // DEFERRED LOADING: Set flag to load saved devices on first loop() iteration
    #if ENABLE_WIFI
    _needsInitialLoad = true;
    Serial.println("[DeviceSelectorTab] Deferred device loading to loop()");
    #endif

    esp_task_wdt_reset();
}

void DeviceSelectorTab::initStyles() {
    // Initialize normal style (TAB5 design system)
    lv_style_init(&_styleNormal);
    lv_style_set_bg_color(&_styleNormal, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE));
    lv_style_set_border_width(&_styleNormal, 2);
    lv_style_set_border_color(&_styleNormal, lv_color_hex(TAB5_COLOR_BORDER_BASE));
    lv_style_set_radius(&_styleNormal, 14);
    lv_style_set_pad_all(&_styleNormal, 10);

    // Initialize selected style (brand yellow highlight)
    lv_style_init(&_styleSelected);
    lv_style_set_bg_color(&_styleSelected, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED));
    lv_style_set_border_width(&_styleSelected, 3);
    lv_style_set_border_color(&_styleSelected, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY));
    lv_style_set_radius(&_styleSelected, 14);
    lv_style_set_pad_all(&_styleSelected, 10);

    // Initialize error style (red accent)
    lv_style_init(&_styleError);
    lv_style_set_bg_color(&_styleError, lv_color_hex(0x2A1515));
    lv_style_set_border_width(&_styleError, 2);
    lv_style_set_border_color(&_styleError, lv_color_hex(TAB5_COLOR_STATUS_ERROR));
    lv_style_set_radius(&_styleError, 14);
    lv_style_set_pad_all(&_styleError, 10);
}

void DeviceSelectorTab::createInteractiveUI(lv_obj_t* parent) {
    _screen = parent;

    // Set TAB5 page background (dark charcoal, not pure black)
    lv_obj_set_style_bg_color(_screen, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);

    // Create page title with BEBAS_BOLD_40 font - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "DEVICE SELECTOR");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_40, LV_PART_MAIN);
    // Center the title: starts at X=160 (after back button), spans remaining width
    lv_obj_set_width(title, 1280 - 160 - 20);
    lv_obj_set_pos(title, 160, 25);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    createBackButton(_screen);
    esp_task_wdt_reset();

    createStatusLabel(_screen);

    // ==========================================================================
    // LEFT: Discovered Devices card
    // ==========================================================================
    createDiscoveredDevicesCard(_screen);
    esp_task_wdt_reset();

    // ==========================================================================
    // CENTER: Action buttons (SCAN, SELECT, FORGET)
    // ==========================================================================
    createCenterButtons(_screen);
    esp_task_wdt_reset();

    // ==========================================================================
    // RIGHT: Saved Devices card
    // ==========================================================================
    createSavedDevicesCard(_screen);
    esp_task_wdt_reset();

    // ==========================================================================
    // BOTTOM: Manual IP entry bar
    // ==========================================================================
    createManualEntryBar(_screen);
    esp_task_wdt_reset();

    // ==========================================================================
    // KEYBOARD: Hidden virtual keyboard for IP input
    // ==========================================================================
    createKeyboard(_screen);
    esp_task_wdt_reset();
}

void DeviceSelectorTab::createBackButton(lv_obj_t* parent) {
    // TAB5 back button: elevated card with brand yellow border
    _backButton = lv_btn_create(parent);
    lv_obj_set_size(_backButton, 120, 44);
    lv_obj_set_pos(_backButton, TAB5_GRID_MARGIN, TAB5_GRID_MARGIN);
    lv_obj_set_style_bg_color(_backButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_backButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_backButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_backButton, 14, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_backButton);
    lv_label_set_text(label, "BACK");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(_backButton, backButtonCb, LV_EVENT_CLICKED, this);
}

void DeviceSelectorTab::createStatusLabel(lv_obj_t* parent) {
    // TAB5 status label with RAJDHANI font
    _statusLabel = lv_label_create(parent);
    lv_obj_set_pos(_statusLabel, TAB5_GRID_MARGIN, STATUS_Y);
    lv_obj_set_size(_statusLabel, 1200, 30);
    lv_label_set_text(_statusLabel, "Status: No device selected");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(_statusLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
}

// ==========================================================================
// DISCOVERED DEVICES CARD (LEFT) - Shows devices found via network scan
// ==========================================================================
void DeviceSelectorTab::createDiscoveredDevicesCard(lv_obj_t* parent) {
    _discoveredCard = make_card(parent, false);
    lv_obj_set_pos(_discoveredCard, DISCOVERED_CARD_X, DISCOVERED_CARD_Y);
    lv_obj_set_size(_discoveredCard, DISCOVERED_CARD_W, DISCOVERED_CARD_H);

    // Section title - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(_discoveredCard);
    lv_label_set_text(title, "DISCOVERED DEVICES");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_width(title, DISCOVERED_CARD_W - 20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);

    // Inner scrollable list
    _discoveredDevicesList = lv_obj_create(_discoveredCard);
    lv_obj_set_pos(_discoveredDevicesList, 0, 40);
    lv_obj_set_size(_discoveredDevicesList, DISCOVERED_CARD_W - 20, DEVICE_LIST_H);
    lv_obj_set_style_bg_color(_discoveredDevicesList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_discoveredDevicesList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_discoveredDevicesList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_discoveredDevicesList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_discoveredDevicesList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_discoveredDevicesList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_discoveredDevicesList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_discoveredDevicesList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_discoveredDevicesList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_discoveredDevicesList, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_discoveredDevicesList, LV_DIR_VER);
    lv_obj_clear_flag(_discoveredDevicesList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

// ==========================================================================
// CENTER BUTTON COLUMN - SCAN, SELECT, FORGET
// Vertically centered between cards (Y: 120 to 540, center at Y=330)
// ==========================================================================
void DeviceSelectorTab::createCenterButtons(lv_obj_t* parent) {
    // Vertical centering: cards Y=120, H=420, center at Y=330
    // 3 buttons: 50 + 10 + 50 + 10 + 50 = 170px total height
    // First button Y: 330 - (170/2) = 245
    constexpr int CENTER_BTN_START_Y = 245;

    // SCAN button: Yellow primary action
    _scanButton = lv_btn_create(parent);
    lv_obj_set_size(_scanButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_scanButton, BUTTON_COLUMN_X, CENTER_BTN_START_Y);
    lv_obj_set_style_bg_color(_scanButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_color(_scanButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scanButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_scanButton, 14, LV_PART_MAIN);

    // Pressed state
    lv_obj_set_style_bg_color(_scanButton, lv_color_hex(0xCCA000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(_scanButton, -2, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(_scanButton, -2, LV_STATE_PRESSED);

    _scanButtonLabel = lv_label_create(_scanButton);
    lv_label_set_text(_scanButtonLabel, "SCAN");
    lv_obj_set_style_text_color(_scanButtonLabel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(_scanButtonLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(_scanButtonLabel);
    lv_obj_add_event_cb(_scanButton, scanButtonCb, LV_EVENT_CLICKED, this);

    // SELECT button: Green action
    _selectButton = lv_btn_create(parent);
    lv_obj_set_size(_selectButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_selectButton, BUTTON_COLUMN_X, CENTER_BTN_START_Y + BUTTON_H + BUTTON_GAP);
    lv_obj_set_style_bg_color(_selectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_color(_selectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_width(_selectButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_selectButton, 14, LV_PART_MAIN);

    lv_obj_t* selectLabel = lv_label_create(_selectButton);
    lv_label_set_text(selectLabel, "SELECT");
    lv_obj_set_style_text_color(selectLabel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(selectLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(selectLabel);
    lv_obj_add_event_cb(_selectButton, selectButtonCb, LV_EVENT_CLICKED, this);

    // FORGET button: Red danger action
    _forgetButton = lv_btn_create(parent);
    lv_obj_set_size(_forgetButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_forgetButton, BUTTON_COLUMN_X, CENTER_BTN_START_Y + (BUTTON_H + BUTTON_GAP) * 2);
    lv_obj_set_style_bg_color(_forgetButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_forgetButton, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
    lv_obj_set_style_border_width(_forgetButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_forgetButton, 14, LV_PART_MAIN);

    lv_obj_t* forgetLabel = lv_label_create(_forgetButton);
    lv_label_set_text(forgetLabel, "FORGET");
    lv_obj_set_style_text_color(forgetLabel, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
    lv_obj_set_style_text_font(forgetLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(forgetLabel);
    lv_obj_add_event_cb(_forgetButton, forgetButtonCb, LV_EVENT_CLICKED, this);
}

// ==========================================================================
// SAVED DEVICES CARD (RIGHT) - Shows persisted devices from registry
// ==========================================================================
void DeviceSelectorTab::createSavedDevicesCard(lv_obj_t* parent) {
    _savedCard = make_card(parent, false);
    lv_obj_set_pos(_savedCard, SAVED_CARD_X, SAVED_CARD_Y);
    lv_obj_set_size(_savedCard, SAVED_CARD_W, SAVED_CARD_H);

    // Section title - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(_savedCard);
    lv_label_set_text(title, "SAVED DEVICES");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_width(title, SAVED_CARD_W - 20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);

    // Inner scrollable list
    _savedDevicesList = lv_obj_create(_savedCard);
    lv_obj_set_pos(_savedDevicesList, 0, 40);
    lv_obj_set_size(_savedDevicesList, SAVED_CARD_W - 20, DEVICE_LIST_H);
    lv_obj_set_style_bg_color(_savedDevicesList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_savedDevicesList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_savedDevicesList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_savedDevicesList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_savedDevicesList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_savedDevicesList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_savedDevicesList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_savedDevicesList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_savedDevicesList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_savedDevicesList, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_savedDevicesList, LV_DIR_VER);
    lv_obj_clear_flag(_savedDevicesList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

// ==========================================================================
// MANUAL ENTRY BAR (BOTTOM) - IP address input with ADD button
// ==========================================================================
void DeviceSelectorTab::createManualEntryBar(lv_obj_t* parent) {
    _manualEntryBar = make_card(parent, true);
    lv_obj_set_pos(_manualEntryBar, MANUAL_BAR_X, MANUAL_BAR_Y);
    lv_obj_set_size(_manualEntryBar, MANUAL_BAR_W, MANUAL_BAR_H);

    // Flex row layout
    lv_obj_set_layout(_manualEntryBar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_manualEntryBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_manualEntryBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(_manualEntryBar, 12, LV_PART_MAIN);

    // "IP ADDRESS:" label
    lv_obj_t* ipLabel = lv_label_create(_manualEntryBar);
    lv_label_set_text(ipLabel, "IP ADDRESS:");
    lv_obj_set_style_text_color(ipLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(ipLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);

    // IP input textarea
    _ipInput = lv_textarea_create(_manualEntryBar);
    lv_obj_set_size(_ipInput, 300, 40);
    lv_obj_set_style_bg_color(_ipInput, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_color(_ipInput, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_ipInput, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_ipInput, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(_ipInput, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(_ipInput, JETBRAINS_MONO_BOLD_24, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(_ipInput, "192.168.x.x");
    lv_textarea_set_one_line(_ipInput, true);
    lv_textarea_set_max_length(_ipInput, 15);  // Max IPv4 length "xxx.xxx.xxx.xxx"

    // ADD button: Yellow primary action
    _addButton = lv_btn_create(_manualEntryBar);
    lv_obj_set_size(_addButton, 120, 40);
    lv_obj_set_style_bg_color(_addButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_addButton, 14, LV_PART_MAIN);

    lv_obj_t* addLabel = lv_label_create(_addButton);
    lv_label_set_text(addLabel, "ADD");
    lv_obj_set_style_text_color(addLabel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(addLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(addLabel);
    lv_obj_add_event_cb(_addButton, addButtonCb, LV_EVENT_CLICKED, this);
}

// ==========================================================================
// KEYBOARD: Virtual keyboard for IP input on touch devices
// ==========================================================================
void DeviceSelectorTab::createKeyboard(lv_obj_t* parent) {
    _keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(_keyboard, 1200, 320);
    lv_obj_align(_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);

    // Use number mode for IP address input
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_NUMBER);

    // ===== MAIN CONTAINER STYLING =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(0x1A1A1C), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_keyboard, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_color(_keyboard, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_keyboard, 12, LV_PART_MAIN);

    // Container padding
    lv_obj_set_style_pad_top(_keyboard, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(_keyboard, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_left(_keyboard, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(_keyboard, 12, LV_PART_MAIN);

    // ===== KEY BUTTON STYLING =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(0x3A3A3C), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(_keyboard, LV_OPA_100, LV_PART_ITEMS);

    // FONT - JetBrains Mono for full ASCII coverage
    lv_obj_set_style_text_font(_keyboard, JETBRAINS_MONO_BOLD_24, LV_PART_ITEMS);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);

    // Button borders for definition
    lv_obj_set_style_border_color(_keyboard, lv_color_hex(0x5A5A5C), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(_keyboard, 8, LV_PART_ITEMS);

    // SPACING BETWEEN BUTTONS
    lv_obj_set_style_pad_row(_keyboard, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_keyboard, 6, LV_PART_MAIN);

    // ===== PRESSED STATE =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_PRESSED);

    // ===== CHECKED STATE (Shift/Caps indicator) =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(0x0D7377), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Enable popovers for key feedback
    lv_keyboard_set_popovers(_keyboard, true);

    // IP input focus -> show keyboard
    lv_obj_add_event_cb(_ipInput, [](lv_event_t* e) {
        DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_keyboard_set_textarea(tab->_keyboard, tab->_ipInput);
            lv_obj_clear_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_FOCUSED, this);

    // Hide keyboard on READY (Enter pressed)
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_READY, this);

    // Hide keyboard on CANCEL
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_CANCEL, this);
}

// ==========================================================================
// LOOP - Main rendering and state update
// ==========================================================================
void DeviceSelectorTab::loop() {
    uint32_t now = millis();

    // =========================================================================
    // DEFERRED INITIAL LOAD: Load devices on first loop() iteration
    // =========================================================================
    #if ENABLE_WIFI
    if (_needsInitialLoad) {
        _needsInitialLoad = false;

        Serial.println("[DeviceSelectorTab] ========== DEFERRED DEVICE LOAD START ==========");

        // Create HttpClient if not injected
        if (!_httpClient) {
            Serial.println("[DeviceSelectorTab] Creating HttpClient...");
            _httpClient = new HttpClient();
            _ownsHttpClient = true;
        }

        // Load saved devices from registry
        if (_deviceRegistry) {
            uint8_t count = _deviceRegistry->getDeviceCount();
            Serial.printf("[DeviceSelectorTab] Registry has %d devices\n", count);
        }

        // Start discovery
        _initialLoadAwaitingDiscovery = true;
        _httpClient->startDiscovery();

        // Also start a scan
        startScan();

        Serial.println("[DeviceSelectorTab] ========== DEFERRED DEVICE LOAD END ==========");
        forceDirty();
    }

    // Poll discovery state
    if (_initialLoadAwaitingDiscovery && _httpClient) {
        auto state = _httpClient->getDiscoveryState();
        if (state == HttpClient::DiscoveryState::SUCCESS) {
            _initialLoadAwaitingDiscovery = false;
            Serial.println("[DeviceSelectorTab] Discovery complete");

            // Add discovered device to registry
            if (_deviceRegistry) {
                IPAddress discoveredIP = _httpClient->getDiscoveredIP();
                if (discoveredIP != IPAddress(0, 0, 0, 0)) {
                    _deviceRegistry->addDiscoveredDevice(discoveredIP, DeviceInfo::Source::MDNS, "lightwaveos");
                    _deviceRegistry->fingerprintAll();
                    Serial.printf("[DeviceSelectorTab] Added discovered device: %d.%d.%d.%d\n",
                                  discoveredIP[0], discoveredIP[1], discoveredIP[2], discoveredIP[3]);
                }
            }

            forceDirty();
        } else if (state == HttpClient::DiscoveryState::FAILED) {
            _initialLoadAwaitingDiscovery = false;
            Serial.println("[DeviceSelectorTab] Discovery failed");
        }
    }
    #endif

    // Update connection status periodically
    if (now - _lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS) {
        updateConnectionStatus();
        _lastStatusUpdate = now;
    }

    // Check scan timeout
    #if ENABLE_WIFI
    if (_scanInProgress && _scanStartMs != 0 && (now - _scanStartMs) > SCAN_TIMEOUT_MS) {
        Serial.println("[DeviceSelectorTab] Scan timed out");
        _scanInProgress = false;
        _state = DeviceSelectorState::ERROR;
        _errorMessage = "Device scan timed out";
        if (_scanButtonLabel) {
            lv_label_set_text(_scanButtonLabel, "SCAN");
            lv_obj_center(_scanButtonLabel);
        }
        markDirty();
    }
    #endif

    // Render if dirty (frame-gated at 10 FPS)
    if (now - _lastRenderTime >= FRAME_INTERVAL_MS) {
        if (_pendingDirty) {
            _dirty = true;
            _pendingDirty = false;
        }

        if (_dirty) {
            updateStatusLabel();
            refreshDeviceLists();
            _dirty = false;
        }
        _lastRenderTime = now;
    }
}

// ==========================================================================
// STATUS LABEL UPDATE
// ==========================================================================
void DeviceSelectorTab::updateConnectionStatus(const char* connectedIP) {
    #if ENABLE_WIFI
    // Check if fingerprinting finished and update dirty flag
    if (_deviceRegistry && !_deviceRegistry->isFingerprintRunning()) {
        // Fingerprinting may have updated device info
    }
    updateStatusLabel();
    #endif
}

void DeviceSelectorTab::updateStatusLabel() {
    if (!_statusLabel) return;

    #if ENABLE_WIFI
    if (_state == DeviceSelectorState::SCANNING) {
        lv_label_set_text(_statusLabel, "Status: Scanning for devices...");
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_STATUS_WARNING), LV_PART_MAIN);
        return;
    }

    if (_state == DeviceSelectorState::ERROR) {
        char statusText[128];
        snprintf(statusText, sizeof(statusText), "Error: %s", _errorMessage.c_str());
        lv_label_set_text(_statusLabel, statusText);
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
        return;
    }

    if (_deviceRegistry) {
        const DeviceInfo* selected = _deviceRegistry->getSelectedDevice();
        if (selected && selected->isValid()) {
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                     selected->ip[0], selected->ip[1], selected->ip[2], selected->ip[3]);
            char statusText[128];
            snprintf(statusText, sizeof(statusText), "Selected: %s (%s)%s",
                     selected->displayName(), ipStr,
                     selected->verified ? " [Verified]" : "");
            lv_label_set_text(_statusLabel, statusText);
            lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
            return;
        }
    }

    lv_label_set_text(_statusLabel, "Status: No device selected");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    #else
    lv_label_set_text(_statusLabel, "WiFi disabled");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    #endif
}

// ==========================================================================
// DEVICE ITEM FACTORY - Creates multi-zone structured items
// Layout: [Name (180px)] [IP (140px)] [RSSI (80px)] [Status Dot (40px)]
// ==========================================================================
lv_obj_t* DeviceSelectorTab::createDeviceItem(
    lv_obj_t* parent,
    const char* name,
    const char* ip,
    int32_t rssi,
    bool isConnected,
    bool isSelected,
    bool isVerified,
    int index,
    bool isDiscoveredList
) {
    // Calculate item width based on parent container
    int parentW = isDiscoveredList ? DISCOVERED_CARD_W : SAVED_CARD_W;
    int itemWidth = parentW - 40;  // Account for padding

    // Item container (60px for touch-friendly targets)
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_size(item, itemWidth, ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_radius(item, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    // Flex layout for multi-zone structure
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // === ZONE 1: Name/Hostname (left-aligned) ===
    lv_obj_t* nameZone = lv_obj_create(item);
    lv_obj_set_size(nameZone, ITEM_NAME_W, ITEM_H - 4);
    lv_obj_set_flex_grow(nameZone, 0);
    lv_obj_set_style_bg_opa(nameZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(nameZone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(nameZone, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(nameZone, 4, LV_PART_MAIN);
    lv_obj_remove_flag(nameZone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* nameLabel = lv_label_create(nameZone);
    lv_obj_set_width(nameLabel, ITEM_NAME_W - 20);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_text(nameLabel, name);
    lv_obj_set_style_text_font(nameLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(nameLabel, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 0, 0);

    // === ZONE 2: IP Address (fixed width) ===
    lv_obj_t* ipZone = lv_obj_create(item);
    lv_obj_set_size(ipZone, ITEM_IP_W, ITEM_H - 4);
    lv_obj_set_flex_grow(ipZone, 0);
    lv_obj_set_style_bg_opa(ipZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ipZone, 0, LV_PART_MAIN);
    lv_obj_remove_flag(ipZone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ipLabel = lv_label_create(ipZone);
    lv_label_set_text(ipLabel, ip);
    lv_obj_set_style_text_font(ipLabel, JETBRAINS_MONO_REG_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(ipLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_align(ipLabel, LV_ALIGN_LEFT_MID, 0, 0);

    // === ZONE 3: RSSI (fixed width, color-coded) ===
    lv_obj_t* rssiZone = lv_obj_create(item);
    lv_obj_set_size(rssiZone, ITEM_RSSI_W, ITEM_H - 4);
    lv_obj_set_flex_grow(rssiZone, 0);
    lv_obj_set_style_bg_opa(rssiZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rssiZone, 0, LV_PART_MAIN);
    lv_obj_remove_flag(rssiZone, LV_OBJ_FLAG_SCROLLABLE);

    if (rssi != INT32_MIN) {
        char rssiText[16];
        snprintf(rssiText, sizeof(rssiText), "%ld dBm", (long)rssi);

        lv_obj_t* rssiLabel = lv_label_create(rssiZone);
        lv_label_set_text(rssiLabel, rssiText);
        lv_obj_set_style_text_font(rssiLabel, RAJDHANI_MED_24, LV_PART_MAIN);

        // Color code signal strength: green >= -60, orange >= -75, red < -75
        uint32_t rssiColor = (rssi >= -60) ? TAB5_COLOR_STATUS_SUCCESS :
                             (rssi >= -75) ? TAB5_COLOR_STATUS_WARNING :
                                             TAB5_COLOR_STATUS_ERROR;
        lv_obj_set_style_text_color(rssiLabel, lv_color_hex(rssiColor), LV_PART_MAIN);
        lv_obj_align(rssiLabel, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    // === ZONE 4: Status Dot (verified/reachable indicator) ===
    lv_obj_t* dotZone = lv_obj_create(item);
    lv_obj_set_size(dotZone, ITEM_DOT_W, ITEM_H - 4);
    lv_obj_set_flex_grow(dotZone, 0);
    lv_obj_set_style_bg_opa(dotZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotZone, 0, LV_PART_MAIN);
    lv_obj_remove_flag(dotZone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* dot = lv_obj_create(dotZone);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);

    // Dot color: green = connected/selected, cyan = verified, gray = offline
    if (isConnected) {
        lv_obj_set_style_bg_color(dot, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else if (isVerified) {
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x06B6D4), LV_PART_MAIN);  // Cyan for verified
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_color(dot, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_50, LV_PART_MAIN);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }

    // === SELECTION STYLING (yellow border + tinted background) ===
    if (isSelected) {
        lv_obj_set_style_border_color(item, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x252528), LV_PART_MAIN);
    }

    // === CONNECTED STATE (green left border accent) ===
    if (isConnected && !isSelected) {
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_color(item, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 4, LV_PART_MAIN);
    }

    // Store index for callback
    lv_obj_set_user_data(item, (void*)(intptr_t)index);

    // Add click handler
    if (isDiscoveredList) {
        lv_obj_add_event_cb(item, discoveredDeviceSelectedCb, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_add_event_cb(item, savedDeviceSelectedCb, LV_EVENT_CLICKED, this);
    }

    // Make clickable
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

    return item;
}

// ==========================================================================
// REFRESH DEVICE LISTS
// ==========================================================================
void DeviceSelectorTab::refreshDeviceLists() {
    #if ENABLE_WIFI
    if (!_deviceRegistry) return;

    int8_t selectedIndex = _deviceRegistry->getSelectedIndex();

    // ==========================================================================
    // UPDATE DISCOVERED DEVICES LIST
    // ==========================================================================
    if (_discoveredDevicesList) {
        lv_obj_clean(_discoveredDevicesList);

        uint8_t deviceCount = _deviceRegistry->getDeviceCount();

        if (deviceCount == 0) {
            // Empty state message
            lv_obj_t* emptyLabel = lv_label_create(_discoveredDevicesList);
            lv_label_set_text(emptyLabel, "No devices found\nTap SCAN to search");
            lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_text_color(emptyLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(emptyLabel, RAJDHANI_MED_24, LV_PART_MAIN);
            lv_obj_set_width(emptyLabel, DISCOVERED_CARD_W - 60);
            lv_obj_center(emptyLabel);
        } else {
            uint8_t itemCount = 0;
            for (uint8_t i = 0; i < DeviceRegistry::MAX_DEVICES; i++) {
                const DeviceInfo* dev = _deviceRegistry->getDevice(i);
                if (!dev || !dev->isValid()) continue;

                // Show all devices in the discovered list
                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                         dev->ip[0], dev->ip[1], dev->ip[2], dev->ip[3]);

                bool isSelected = (_discoveredListHasFocus && _selectedDiscoveredIndex == (int8_t)i);
                bool isConnectedDevice = (selectedIndex == (int8_t)i);

                createDeviceItem(
                    _discoveredDevicesList,
                    dev->displayName(),
                    ipStr,
                    dev->rssi,
                    isConnectedDevice,
                    isSelected,
                    dev->verified,
                    i,
                    true  // isDiscoveredList
                );

                itemCount++;

                // CRITICAL: Watchdog reset every 3 items
                if (itemCount % 3 == 0) {
                    delay(1);
                    esp_task_wdt_reset();
                }
            }
        }
        lv_obj_invalidate(_discoveredDevicesList);
    }

    // ==========================================================================
    // UPDATE SAVED DEVICES LIST
    // ==========================================================================
    if (_savedDevicesList) {
        lv_obj_clean(_savedDevicesList);

        uint8_t deviceCount = _deviceRegistry->getDeviceCount();

        if (deviceCount == 0) {
            // Empty state message
            lv_obj_t* emptyLabel = lv_label_create(_savedDevicesList);
            lv_label_set_text(emptyLabel, "No saved devices\nAdd from Discovered or Manual IP");
            lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_text_color(emptyLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(emptyLabel, RAJDHANI_MED_24, LV_PART_MAIN);
            lv_obj_set_width(emptyLabel, SAVED_CARD_W - 60);
            lv_obj_center(emptyLabel);
        } else {
            uint8_t itemCount = 0;
            for (uint8_t i = 0; i < DeviceRegistry::MAX_DEVICES; i++) {
                const DeviceInfo* dev = _deviceRegistry->getDevice(i);
                if (!dev || !dev->isValid()) continue;

                char ipStr[16];
                snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                         dev->ip[0], dev->ip[1], dev->ip[2], dev->ip[3]);

                bool isSelected = (!_discoveredListHasFocus && _selectedSavedIndex == (int8_t)i);
                bool isConnectedDevice = (selectedIndex == (int8_t)i);

                // Show hostname or friendly name in saved list
                const char* displayName = dev->friendlyName[0] != '\0' ? dev->friendlyName :
                                          dev->hostname[0] != '\0' ? dev->hostname :
                                          ipStr;

                createDeviceItem(
                    _savedDevicesList,
                    displayName,
                    ipStr,
                    dev->rssi,
                    isConnectedDevice,
                    isSelected,
                    dev->verified,
                    i,
                    false  // isDiscoveredList = false (saved)
                );

                itemCount++;

                // CRITICAL: Watchdog reset every 3 items
                if (itemCount % 3 == 0) {
                    delay(1);
                    esp_task_wdt_reset();
                }
            }
        }
        lv_obj_invalidate(_savedDevicesList);
    }
    #endif
}

void DeviceSelectorTab::updateDiscoveredDevicesList() {
    refreshDeviceLists();
}

void DeviceSelectorTab::updateSavedDevicesList() {
    refreshDeviceLists();
}

// ==========================================================================
// DEVICE OPERATIONS
// ==========================================================================
void DeviceSelectorTab::startScan() {
    #if ENABLE_WIFI
    // Visual feedback: Change button to "SCANNING..."
    if (_scanButtonLabel) {
        lv_label_set_text(_scanButtonLabel, "SCANNING...");
        lv_obj_center(_scanButtonLabel);
    }

    _state = DeviceSelectorState::SCANNING;
    _scanInProgress = true;
    _scanStartMs = millis();

    // Start HttpClient discovery if available
    if (_httpClient) {
        if (_httpClient->getDiscoveryState() != HttpClient::DiscoveryState::RUNNING) {
            _httpClient->startDiscovery();
        }
    }

    // Also fingerprint existing devices
    if (_deviceRegistry) {
        _deviceRegistry->fingerprintAll();
    }

    // Mark scan as complete after a short delay (discovery is async)
    // The actual completion is handled in loop() polling
    Serial.println("[DeviceSelectorTab] Device scan started");
    markDirty();

    // For now, mark scan as done after starting discovery
    // The loop() will pick up results from HttpClient
    _scanInProgress = false;
    _state = DeviceSelectorState::IDLE;
    if (_scanButtonLabel) {
        lv_label_set_text(_scanButtonLabel, "SCAN");
        lv_obj_center(_scanButtonLabel);
    }
    #endif
}

void DeviceSelectorTab::selectDevice() {
    #if ENABLE_WIFI
    if (!_deviceRegistry) return;

    int8_t targetIndex = -1;

    if (_discoveredListHasFocus && _selectedDiscoveredIndex >= 0) {
        targetIndex = _selectedDiscoveredIndex;
    } else if (!_discoveredListHasFocus && _selectedSavedIndex >= 0) {
        targetIndex = _selectedSavedIndex;
    }

    if (targetIndex < 0) {
        Serial.println("[DeviceSelectorTab] SELECT: No device selected");
        return;
    }

    const DeviceInfo* dev = _deviceRegistry->getDevice(static_cast<uint8_t>(targetIndex));
    if (!dev || !dev->isValid()) {
        Serial.println("[DeviceSelectorTab] SELECT: Invalid device index");
        return;
    }

    // Select in registry
    _deviceRegistry->selectDevice(targetIndex);

    // Fire callback
    if (_deviceSelectedCallback) {
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                 dev->ip[0], dev->ip[1], dev->ip[2], dev->ip[3]);
        _deviceSelectedCallback(ipStr, 80);
    }

    Serial.printf("[DeviceSelectorTab] Selected device index %d: %s\n",
                  targetIndex, dev->displayName());
    forceDirty();
    #endif
}

void DeviceSelectorTab::forgetDevice() {
    #if ENABLE_WIFI
    if (!_deviceRegistry) return;

    // Forget from the saved list
    int8_t targetIndex = -1;
    if (!_discoveredListHasFocus && _selectedSavedIndex >= 0) {
        targetIndex = _selectedSavedIndex;
    } else if (_discoveredListHasFocus && _selectedDiscoveredIndex >= 0) {
        targetIndex = _selectedDiscoveredIndex;
    }

    if (targetIndex < 0) {
        Serial.println("[DeviceSelectorTab] FORGET: No device selected");
        return;
    }

    const DeviceInfo* dev = _deviceRegistry->getDevice(static_cast<uint8_t>(targetIndex));
    if (!dev || !dev->isValid()) {
        Serial.println("[DeviceSelectorTab] FORGET: Invalid device index");
        return;
    }

    Serial.printf("[DeviceSelectorTab] Forgetting device index %d: %s\n",
                  targetIndex, dev->displayName());

    _deviceRegistry->removeDevice(static_cast<uint8_t>(targetIndex));

    // Clear selection
    _selectedDiscoveredIndex = -1;
    _selectedSavedIndex = -1;

    forceDirty();
    #endif
}

void DeviceSelectorTab::addManualDevice() {
    #if ENABLE_WIFI
    if (!_deviceRegistry || !_ipInput) return;

    const char* ipText = lv_textarea_get_text(_ipInput);
    if (!ipText || strlen(ipText) == 0) {
        Serial.println("[DeviceSelectorTab] ADD: Empty IP address");
        return;
    }

    // Parse IP address
    IPAddress ip;
    if (!ip.fromString(ipText)) {
        Serial.printf("[DeviceSelectorTab] ADD: Invalid IP address: %s\n", ipText);
        _state = DeviceSelectorState::ERROR;
        _errorMessage = "Invalid IP address format";
        markDirty();
        return;
    }

    // Validate non-zero
    if (ip == IPAddress(0, 0, 0, 0)) {
        Serial.println("[DeviceSelectorTab] ADD: IP cannot be 0.0.0.0");
        _state = DeviceSelectorState::ERROR;
        _errorMessage = "Invalid IP address";
        markDirty();
        return;
    }

    // Add to registry
    int8_t index = _deviceRegistry->addManualDevice(ip, nullptr);
    if (index >= 0) {
        Serial.printf("[DeviceSelectorTab] Added manual device at index %d: %s\n", index, ipText);

        // Start fingerprinting
        _deviceRegistry->fingerprintDevice(static_cast<uint8_t>(index));

        // Clear input
        lv_textarea_set_text(_ipInput, "");

        // Hide keyboard
        if (_keyboard) {
            lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
        }

        _state = DeviceSelectorState::IDLE;
        forceDirty();
    } else {
        Serial.println("[DeviceSelectorTab] ADD: Registry full or duplicate");
        _state = DeviceSelectorState::ERROR;
        _errorMessage = "Device registry full";
        markDirty();
    }
    #endif
}

// ==========================================================================
// ENCODER HANDLING
// ==========================================================================
void DeviceSelectorTab::handleEncoderChange(uint8_t encoderIndex, int32_t delta) {
    #if ENABLE_WIFI
    if (!_deviceRegistry) return;

    // Encoder 0: scroll through device list items
    if (encoderIndex != 0) return;

    uint8_t deviceCount = _deviceRegistry->getDeviceCount();
    if (deviceCount == 0) return;

    // Build a list of valid device indices
    int8_t validIndices[DeviceRegistry::MAX_DEVICES];
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < DeviceRegistry::MAX_DEVICES; i++) {
        const DeviceInfo* dev = _deviceRegistry->getDevice(i);
        if (dev && dev->isValid()) {
            validIndices[validCount++] = static_cast<int8_t>(i);
        }
    }

    if (validCount == 0) return;

    if (_discoveredListHasFocus) {
        // Navigate discovered list
        if (_selectedDiscoveredIndex < 0) {
            _selectedDiscoveredIndex = validIndices[0];
        } else {
            // Find current position in valid indices
            int8_t pos = -1;
            for (uint8_t i = 0; i < validCount; i++) {
                if (validIndices[i] == _selectedDiscoveredIndex) {
                    pos = static_cast<int8_t>(i);
                    break;
                }
            }

            int8_t newPos = pos + (delta > 0 ? 1 : -1);

            // Cross-list navigation: past bottom -> switch to saved list
            if (newPos >= (int8_t)validCount) {
                _discoveredListHasFocus = false;
                _selectedSavedIndex = validIndices[0];
                _selectedDiscoveredIndex = -1;
            } else if (newPos < 0) {
                // Wrap to bottom of discovered
                _selectedDiscoveredIndex = validIndices[validCount - 1];
            } else {
                _selectedDiscoveredIndex = validIndices[newPos];
            }
        }
    } else {
        // Navigate saved list
        if (_selectedSavedIndex < 0) {
            _selectedSavedIndex = validIndices[0];
        } else {
            int8_t pos = -1;
            for (uint8_t i = 0; i < validCount; i++) {
                if (validIndices[i] == _selectedSavedIndex) {
                    pos = static_cast<int8_t>(i);
                    break;
                }
            }

            int8_t newPos = pos + (delta > 0 ? 1 : -1);

            // Cross-list navigation: past top -> switch to discovered list
            if (newPos < 0) {
                _discoveredListHasFocus = true;
                _selectedDiscoveredIndex = validIndices[validCount - 1];
                _selectedSavedIndex = -1;
            } else if (newPos >= (int8_t)validCount) {
                // Wrap to top of saved
                _selectedSavedIndex = validIndices[0];
            } else {
                _selectedSavedIndex = validIndices[newPos];
            }
        }
    }

    markDirty();
    #endif
}

void DeviceSelectorTab::handleTouch(int16_t x, int16_t y) {
    // Handled by LVGL event system
}

// ==========================================================================
// STATIC EVENT CALLBACKS
// ==========================================================================
void DeviceSelectorTab::backButtonCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab && tab->_backButtonCallback) {
        tab->_backButtonCallback();
    }
}

void DeviceSelectorTab::scanButtonCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->startScan();
    }
}

void DeviceSelectorTab::selectButtonCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->selectDevice();
    }
}

void DeviceSelectorTab::forgetButtonCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->forgetDevice();
    }
}

void DeviceSelectorTab::addButtonCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->addManualDevice();
    }
}

void DeviceSelectorTab::discoveredDeviceSelectedCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        uintptr_t index = (uintptr_t)lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        tab->_selectedDiscoveredIndex = static_cast<int8_t>(index);
        tab->_discoveredListHasFocus = true;
        tab->_selectedSavedIndex = -1;  // Clear other list selection
        tab->markDirty();
    }
}

void DeviceSelectorTab::savedDeviceSelectedCb(lv_event_t* e) {
    DeviceSelectorTab* tab = static_cast<DeviceSelectorTab*>(lv_event_get_user_data(e));
    if (tab) {
        uintptr_t index = (uintptr_t)lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        tab->_selectedSavedIndex = static_cast<int8_t>(index);
        tab->_discoveredListHasFocus = false;
        tab->_selectedDiscoveredIndex = -1;  // Clear other list selection
        tab->markDirty();
    }
}

#endif // ENABLE_WIFI
