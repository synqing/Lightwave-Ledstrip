// ============================================================================
// ConnectivityTab Implementation - Network Management UI
// ============================================================================
// Unified with ZoneComposerUI TAB5 Design System
// ============================================================================

#include "../config/Config.h"
#include "ConnectivityTab.h"

#if ENABLE_WIFI

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_task_wdt.h>
#include "../hal/EspHal.h"
#include "../network/WiFiManager.h"
#include "Theme.h"
#include "../input/ButtonHandler.h"
#include "../network/WebSocketClient.h"
#include "fonts/experimental_fonts.h"

// ============================================================================
// TAB5 Design System Colors (matches ZoneComposerUI)
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
// TAB5 Card Helper (matches ZoneComposerUI make_zone_card pattern)
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

// Helper function to format IP address
static void formatIPv4(const IPAddress& ip, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

ConnectivityTab::ConnectivityTab(M5GFX& display)
    : _display(display)
{
}

ConnectivityTab::~ConnectivityTab() {
    // Cleanup handled by LVGL parent
    #if ENABLE_WIFI
    if (_httpClient) {
        delete _httpClient;
        _httpClient = nullptr;
    }
    #endif
}

void ConnectivityTab::begin(lv_obj_t* parent) {
    Serial.printf("[CT_TRACE] begin() entry @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset(); // Reset WDT at start of begin()

    markDirty();
    _lastRenderTime = 0;

    Serial.printf("[CT_TRACE] before initStyles @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset(); // Reset WDT before initStyles()

    // Initialize LVGL styles
    initStyles();
    Serial.printf("[CT_TRACE] after initStyles @ %lu ms\n", millis());
    Serial.flush();

    Serial.printf("[CT_TRACE] before createInteractiveUI @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset(); // Reset WDT before createInteractiveUI()

    // Create LVGL widgets if parent provided
    if (parent) {
        createInteractiveUI(parent);
        Serial.println("[ConnectivityTab] LVGL interactive UI created");
        Serial.flush();
    }

    Serial.println("[ConnectivityTab] Interactive UI initialized");
    Serial.printf("[CT_TRACE] begin() exit @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset(); // Reset WDT at end of begin()
}

void ConnectivityTab::initStyles() {
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
    lv_style_set_bg_color(&_styleError, lv_color_hex(0x2A1515));  // Dark red tint
    lv_style_set_border_width(&_styleError, 2);
    lv_style_set_border_color(&_styleError, lv_color_hex(TAB5_COLOR_STATUS_ERROR));
    lv_style_set_radius(&_styleError, 14);
    lv_style_set_pad_all(&_styleError, 10);
}

void ConnectivityTab::createInteractiveUI(lv_obj_t* parent) {
    Serial.printf("[CT_TRACE] createInteractiveUI() entry @ %lu ms\n", millis());
    Serial.flush();
    _screen = parent;

    // Set TAB5 page background (dark charcoal, not pure black)
    lv_obj_set_style_bg_color(_screen, lv_color_hex(TAB5_COLOR_BG_PAGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);

    // Create page title with BEBAS_BOLD_40 font
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "NETWORK SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_40, LV_PART_MAIN);
    lv_obj_set_pos(title, 160, 25);  // Right of back button

    Serial.printf("[CT_TRACE] before createBackButton @ %lu ms\n", millis());
    Serial.flush();
    // Create back button
    createBackButton(_screen);
    esp_task_wdt_reset(); // Reset after first widget group

    Serial.printf("[CT_TRACE] before createStatusLabel @ %lu ms\n", millis());
    Serial.flush();
    // Create status label
    createStatusLabel(_screen);

    Serial.printf("[CT_TRACE] before createSavedNetworksList @ %lu ms\n", millis());
    Serial.flush();
    // Create saved networks list
    createSavedNetworksList(_screen);
    esp_task_wdt_reset(); // Reset after list container creation

    Serial.printf("[CT_TRACE] before createScanButton @ %lu ms\n", millis());
    Serial.flush();
    // Create scan button
    createScanButton(_screen);

    Serial.printf("[CT_TRACE] before createScannedNetworksList @ %lu ms\n", millis());
    Serial.flush();
    // Create scanned networks list
    createScannedNetworksList(_screen);
    esp_task_wdt_reset(); // Reset after second list container

    Serial.printf("[CT_TRACE] before createAddNetworkButton @ %lu ms\n", millis());
    Serial.flush();
    // Create add network button
    createAddNetworkButton(_screen);

    Serial.printf("[CT_TRACE] before createActionButtons @ %lu ms\n", millis());
    Serial.flush();
    // Create action buttons (connect, delete, disconnect)
    createActionButtons(_screen);
    esp_task_wdt_reset(); // Reset after action buttons

    Serial.printf("[CT_TRACE] before createAddNetworkDialog @ %lu ms\n", millis());
    Serial.flush();
    // Create add network dialog (initially hidden)
    createAddNetworkDialog(_screen);

    Serial.printf("[CT_TRACE] createInteractiveUI() exit @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset(); // Reset at end of UI creation
}

void ConnectivityTab::createBackButton(lv_obj_t* parent) {
    // TAB5 back button: elevated card with brand yellow border
    _backButton = lv_btn_create(parent);
    lv_obj_set_size(_backButton, 120, 44);  // TAB5 standard back button height
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

void ConnectivityTab::createStatusLabel(lv_obj_t* parent) {
    // TAB5 status label with RAJDHANI font
    _statusLabel = lv_label_create(parent);
    lv_obj_set_pos(_statusLabel, TAB5_GRID_MARGIN, STATUS_Y);
    lv_obj_set_size(_statusLabel, 1200, 30);
    lv_label_set_text(_statusLabel, "Status: Disconnected");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(_statusLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
}

void ConnectivityTab::createSavedNetworksList(lv_obj_t* parent) {
    // TAB5 card container for saved networks (RIGHT side)
    lv_obj_t* savedCard = make_card(parent, false);
    lv_obj_set_pos(savedCard, 640, SAVED_LIST_Y);
    lv_obj_set_size(savedCard, 600, SAVED_LIST_H + 40);  // Extra height for title

    // Section title with BEBAS_BOLD_32
    lv_obj_t* title = lv_label_create(savedCard);
    lv_label_set_text(title, "SAVED NETWORKS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_pos(title, 5, 0);

    // Inner scrollable list with elevated background
    _savedNetworksList = lv_obj_create(savedCard);
    lv_obj_set_pos(_savedNetworksList, 0, 35);
    lv_obj_set_size(_savedNetworksList, 580, SAVED_LIST_H - 10);
    lv_obj_set_style_bg_color(_savedNetworksList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_savedNetworksList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_savedNetworksList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_savedNetworksList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_savedNetworksList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_savedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_savedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_savedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_savedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_savedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_savedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_savedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

void ConnectivityTab::createScanButton(lv_obj_t* parent) {
    // TAB5 primary action button: yellow bg, black text
    // Position below network cards (card ends at SAVED_LIST_Y + SAVED_LIST_H + 40)
    int buttonRowY = SAVED_LIST_Y + SAVED_LIST_H + 40 + 20;  // 120 + 200 + 40 + 20 = 380
    _scanButton = lv_btn_create(parent);
    lv_obj_set_size(_scanButton, 180, 50);
    lv_obj_set_pos(_scanButton, TAB5_GRID_MARGIN, buttonRowY);
    lv_obj_set_style_bg_color(_scanButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_color(_scanButton, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scanButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_scanButton, 14, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_scanButton);
    lv_label_set_text(label, "SCAN");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);  // Black on yellow
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(_scanButton, scanButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createScannedNetworksList(lv_obj_t* parent) {
    // TAB5 card container for scanned networks (LEFT side - Available Networks)
    lv_obj_t* scannedCard = make_card(parent, false);
    lv_obj_set_pos(scannedCard, TAB5_GRID_MARGIN, SAVED_LIST_Y);
    lv_obj_set_size(scannedCard, 620, SAVED_LIST_H + 40);

    // Section title with BEBAS_BOLD_32
    lv_obj_t* title = lv_label_create(scannedCard);
    lv_label_set_text(title, "AVAILABLE NETWORKS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_pos(title, 5, 0);

    // Inner scrollable list with elevated background
    _scannedNetworksList = lv_obj_create(scannedCard);
    lv_obj_set_pos(_scannedNetworksList, 0, 35);
    lv_obj_set_size(_scannedNetworksList, 600, SAVED_LIST_H - 10);
    lv_obj_set_style_bg_color(_scannedNetworksList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_scannedNetworksList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_scannedNetworksList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scannedNetworksList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_scannedNetworksList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_scannedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_scannedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_scannedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_scannedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_scannedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_scannedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_scannedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

void ConnectivityTab::createAddNetworkButton(lv_obj_t* parent) {
    // TAB5 secondary button: elevated bg, white border/text
    // Position below network cards (same row as SCAN button)
    int buttonRowY = SAVED_LIST_Y + SAVED_LIST_H + 40 + 20;  // 120 + 200 + 40 + 20 = 380
    _addNetworkButton = lv_btn_create(parent);
    lv_obj_set_size(_addNetworkButton, 180, 50);
    lv_obj_set_pos(_addNetworkButton, TAB5_GRID_MARGIN + 180 + TAB5_GRID_GAP, buttonRowY);
    lv_obj_set_style_bg_color(_addNetworkButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addNetworkButton, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addNetworkButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_addNetworkButton, 14, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_addNetworkButton);
    lv_label_set_text(label, "ADD");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(_addNetworkButton, addNetworkButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createActionButtons(lv_obj_t* parent) {
    // TAB5 Action buttons row - below SCAN/ADD button row
    // SCAN/ADD row at Y=380, height 50, so action row at Y=380+50+20=450
    int actionY = SAVED_LIST_Y + SAVED_LIST_H + 40 + 20 + 50 + 20;  // 450
    int btnSpacing = 160 + TAB5_GRID_GAP;

    // Connect button: Green bg, black text (success action)
    _connectButton = lv_btn_create(parent);
    lv_obj_set_size(_connectButton, 160, 50);
    lv_obj_set_pos(_connectButton, TAB5_GRID_MARGIN, actionY);
    lv_obj_set_style_bg_color(_connectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_color(_connectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_width(_connectButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_connectButton, 14, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_connectButton);
    lv_label_set_text(label, "CONNECT");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);  // Black on green
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_connectButton, connectButtonCb, LV_EVENT_CLICKED, this);

    // Delete button: Elevated bg, red border/text (danger action)
    _deleteButton = lv_btn_create(parent);
    lv_obj_set_size(_deleteButton, 160, 50);
    lv_obj_set_pos(_deleteButton, TAB5_GRID_MARGIN + btnSpacing, actionY);
    lv_obj_set_style_bg_color(_deleteButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_deleteButton, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
    lv_obj_set_style_border_width(_deleteButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_deleteButton, 14, LV_PART_MAIN);

    label = lv_label_create(_deleteButton);
    lv_label_set_text(label, "DELETE");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);  // Red text
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_deleteButton, deleteButtonCb, LV_EVENT_CLICKED, this);

    // Disconnect button: Elevated bg, amber border/text (warning action)
    _disconnectButton = lv_btn_create(parent);
    lv_obj_set_size(_disconnectButton, 180, 50);
    lv_obj_set_pos(_disconnectButton, TAB5_GRID_MARGIN + btnSpacing * 2, actionY);
    lv_obj_set_style_bg_color(_disconnectButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_disconnectButton, lv_color_hex(TAB5_COLOR_STATUS_WARNING), LV_PART_MAIN);
    lv_obj_set_style_border_width(_disconnectButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_disconnectButton, 14, LV_PART_MAIN);

    label = lv_label_create(_disconnectButton);
    lv_label_set_text(label, "DISCONNECT");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_STATUS_WARNING), LV_PART_MAIN);  // Amber text
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_disconnectButton, disconnectButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createAddNetworkDialog(lv_obj_t* parent) {
    // TAB5 modal dialog with brand yellow border
    _addDialog = lv_obj_create(parent);
    lv_obj_set_size(_addDialog, 600, 380);
    lv_obj_align(_addDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_addDialog, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_addDialog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_addDialog, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addDialog, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(_addDialog, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_addDialog, 20, LV_PART_MAIN);
    lv_obj_add_flag(_addDialog, LV_OBJ_FLAG_HIDDEN);  // Initially hidden
    lv_obj_clear_flag(_addDialog, LV_OBJ_FLAG_SCROLLABLE);

    // Title with BEBAS_BOLD_32
    lv_obj_t* title = lv_label_create(_addDialog);
    lv_label_set_text(title, "ADD NETWORK");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);

    // SSID label
    lv_obj_t* ssidLabel = lv_label_create(_addDialog);
    lv_label_set_text(ssidLabel, "SSID");
    lv_obj_set_style_text_color(ssidLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(ssidLabel, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_pos(ssidLabel, 0, 50);

    // SSID input with TAB5 styling
    _ssidInput = lv_textarea_create(_addDialog);
    lv_obj_set_size(_ssidInput, 560, 50);
    lv_obj_set_pos(_ssidInput, 0, 80);
    lv_obj_set_style_bg_color(_ssidInput, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_ssidInput, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_ssidInput, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_ssidInput, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(_ssidInput, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(_ssidInput, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(_ssidInput, "Enter network name");

    // Password label
    lv_obj_t* passwordLabel = lv_label_create(_addDialog);
    lv_label_set_text(passwordLabel, "PASSWORD");
    lv_obj_set_style_text_color(passwordLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(passwordLabel, RAJDHANI_MED_24, LV_PART_MAIN);
    lv_obj_set_pos(passwordLabel, 0, 145);

    // Password input with TAB5 styling
    _passwordInput = lv_textarea_create(_addDialog);
    lv_obj_set_size(_passwordInput, 560, 50);
    lv_obj_set_pos(_passwordInput, 0, 175);
    lv_obj_set_style_bg_color(_passwordInput, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_passwordInput, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_passwordInput, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_passwordInput, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(_passwordInput, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(_passwordInput, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(_passwordInput, "Enter password");
    lv_textarea_set_password_mode(_passwordInput, true);

    // Cancel button: Gray border/text
    lv_obj_t* cancelBtn = lv_btn_create(_addDialog);
    lv_obj_set_size(cancelBtn, 140, 50);
    lv_obj_set_pos(cancelBtn, 280, 260);
    lv_obj_set_style_bg_color(cancelBtn, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(cancelBtn, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(cancelBtn, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(cancelBtn, 14, LV_PART_MAIN);
    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_set_style_text_color(cancelLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(cancelLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(cancelLabel);
    lv_obj_add_event_cb(cancelBtn, addDialogCancelCb, LV_EVENT_CLICKED, this);

    // Save button: Yellow bg, black text (primary action)
    lv_obj_t* saveBtn = lv_btn_create(_addDialog);
    lv_obj_set_size(saveBtn, 140, 50);
    lv_obj_set_pos(saveBtn, 430, 260);
    lv_obj_set_style_bg_color(saveBtn, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_color(saveBtn, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(saveBtn, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(saveBtn, 14, LV_PART_MAIN);
    lv_obj_t* saveLabel = lv_label_create(saveBtn);
    lv_label_set_text(saveLabel, "SAVE");
    lv_obj_set_style_text_color(saveLabel, lv_color_hex(0x000000), LV_PART_MAIN);  // Black on yellow
    lv_obj_set_style_text_font(saveLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(saveLabel);
    lv_obj_add_event_cb(saveBtn, addDialogSaveCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::loop() {
    uint32_t now = millis();
    
    // Update connection status periodically
    if (now - _lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS) {
        updateConnectionStatus();
        _lastStatusUpdate = now;
    }

    // Check scan status if scan is in progress
    if (_scanInProgress) {
        checkScanStatus();
    }

    // Render if dirty
    if (now - _lastRenderTime >= FRAME_INTERVAL_MS) {
        if (_pendingDirty) {
            _dirty = true;
            _pendingDirty = false;
        }
        
        if (_dirty) {
            updateStatusLabel();
            refreshNetworkLists();
            _dirty = false;
        }
        _lastRenderTime = now;
    }
}

void ConnectivityTab::updateConnectionStatus() {
    #if ENABLE_WIFI
    if (_wifiManager) {
        // Lazy initialization of HttpClient to avoid blocking constructor during UI init
        if (!_httpClient) {
            _httpClient = new HttpClient();
        }

        // Resolve server IP if needed
        if (_httpClient->getServerIP() == INADDR_NONE) {
            _httpClient->resolveHostname();
        }

        // Refresh saved networks list
        _savedNetworkCount = _httpClient->listNetworks(_savedNetworks, 10);
        markDirty();
    }
    #endif
}

void ConnectivityTab::updateStatusLabel() {
    if (!_statusLabel) return;

    #if ENABLE_WIFI
    if (_wifiManager && _wifiManager->isConnected()) {
        char ipStr[16];
        formatIPv4(_wifiManager->getLocalIP(), ipStr);
        char statusText[64];
        snprintf(statusText, sizeof(statusText), "Connected: %s (%s)",
                 _wifiManager->getSSID().c_str(), ipStr);
        lv_label_set_text(_statusLabel, statusText);
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    } else {
        lv_label_set_text(_statusLabel, "Status: Disconnected");
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    }
    #else
    lv_label_set_text(_statusLabel, "WiFi disabled");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
    #endif
}

void ConnectivityTab::refreshNetworkLists() {
    // Update saved networks list with TAB5 styling
    if (_savedNetworksList) {
        lv_obj_clean(_savedNetworksList);
        for (uint8_t i = 0; i < _savedNetworkCount; i++) {
            lv_obj_t* btn = lv_btn_create(_savedNetworksList);
            lv_obj_set_size(btn, 560, 44);
            lv_obj_set_style_bg_color(btn, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);

            // Highlight selected network
            if (_isSavedNetworkSelected && _selectedNetworkIndex == i) {
                lv_obj_set_style_border_color(btn, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
            }

            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, _savedNetworks[i].ssid.c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
            lv_obj_center(label);

            lv_obj_add_event_cb(btn, savedNetworkSelectedCb, LV_EVENT_CLICKED, this);
            lv_obj_set_user_data(btn, (void*)(uintptr_t)i);

            // Yield every 3 networks to prevent watchdog timeout
            if ((i + 1) % 3 == 0) {
                delay(1);
                esp_task_wdt_reset();
            }
        }
    }

    // Update scanned networks list with TAB5 styling and signal strength colors
    if (_scannedNetworksList) {
        lv_obj_clean(_scannedNetworksList);
        for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
            char labelText[64];
            snprintf(labelText, sizeof(labelText), "%s (%d dBm)",
                     _scannedNetworks[i].ssid.c_str(),
                     _scannedNetworks[i].rssi);

            lv_obj_t* btn = lv_btn_create(_scannedNetworksList);
            lv_obj_set_size(btn, 580, 44);
            lv_obj_set_style_bg_color(btn, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);

            // Signal strength color coding on border
            int rssi = _scannedNetworks[i].rssi;
            uint32_t signalColor;
            if (rssi >= -50) {
                signalColor = TAB5_COLOR_STATUS_SUCCESS;  // Strong: Green
            } else if (rssi >= -70) {
                signalColor = TAB5_COLOR_STATUS_WARNING;  // Medium: Amber
            } else {
                signalColor = TAB5_COLOR_STATUS_ERROR;    // Weak: Red
            }
            lv_obj_set_style_border_color(btn, lv_color_hex(signalColor), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);

            // Highlight selected network
            if (!_isSavedNetworkSelected && _selectedNetworkIndex == i) {
                lv_obj_set_style_border_color(btn, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
                lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN);
            }

            lv_obj_t* labelObj = lv_label_create(btn);
            lv_label_set_text(labelObj, labelText);
            lv_obj_set_style_text_color(labelObj, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(labelObj, RAJDHANI_BOLD_24, LV_PART_MAIN);
            lv_obj_center(labelObj);

            lv_obj_add_event_cb(btn, scannedNetworkSelectedCb, LV_EVENT_CLICKED, this);
            lv_obj_set_user_data(btn, (void*)(uintptr_t)i);

            // Yield every 5 scanned networks to prevent watchdog timeout
            if ((i + 1) % 5 == 0) {
                delay(1);
                esp_task_wdt_reset();
            }
        }
    }
}

void ConnectivityTab::startScan() {
    #if ENABLE_WIFI
    if (!_httpClient) {
        _httpClient = new HttpClient();
    }

    _state = ConnectivityState::SCANNING;
    markDirty();

    ScanStatus status;
    if (_httpClient->startScan(status)) {
        // v2 returns synchronous results - no polling needed
        _scannedNetworkCount = status.networkCount > 20 ? 20 : status.networkCount;
        for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
            _scannedNetworks[i] = status.networks[i];
        }
        _scanInProgress = false;
        _state = ConnectivityState::IDLE;
        Serial.printf("[ConnectivityTab] Scan complete, found %d networks\n", _scannedNetworkCount);
        markDirty();
    } else {
        _errorMessage = "Failed to scan networks";
        _state = ConnectivityState::ERROR;
        _scanInProgress = false;
        markDirty();
    }
    #endif
}

void ConnectivityTab::checkScanStatus() {
    // No longer needed - scan is now synchronous in startScan()
    // This function is kept for interface compatibility but is a no-op
    #if ENABLE_WIFI
    // Scan results are retrieved directly in startScan() - no polling required
    #endif
}

void ConnectivityTab::connectToSelectedNetwork() {
    #if ENABLE_WIFI
    if (_selectedNetworkIndex < 0) return;
    if (!_httpClient) {
        _httpClient = new HttpClient();
    }

    String ssid;
    String password;

    if (_isSavedNetworkSelected) {
        if (_selectedNetworkIndex >= _savedNetworkCount) return;
        ssid = _savedNetworks[_selectedNetworkIndex].ssid;
        password = _savedNetworks[_selectedNetworkIndex].password;
    } else {
        if (_selectedNetworkIndex >= _scannedNetworkCount) return;
        ssid = _scannedNetworks[_selectedNetworkIndex].ssid;
        password = "";  // Will prompt for password if needed
    }

    if (_httpClient->connectToNetwork(ssid, password)) {
        _state = ConnectivityState::CONNECTING;
        Serial.printf("[ConnectivityTab] Connecting to %s\n", ssid.c_str());
        markDirty();
    } else {
        _errorMessage = "Failed to initiate connection";
        _state = ConnectivityState::ERROR;
        markDirty();
    }
    #endif
}

void ConnectivityTab::addNewNetwork() {
    #if ENABLE_WIFI
    if (_newNetworkSsid.isEmpty()) return;
    if (!_httpClient) {
        _httpClient = new HttpClient();
    }

    if (_httpClient->addNetwork(_newNetworkSsid, _newNetworkPassword)) {
        Serial.printf("[ConnectivityTab] Network %s added\n", _newNetworkSsid.c_str());
        _newNetworkSsid = "";
        _newNetworkPassword = "";
        lv_obj_add_flag(_addDialog, LV_OBJ_FLAG_HIDDEN);
        _showAddDialog = false;
        updateConnectionStatus();  // Refresh list
    } else {
        _errorMessage = "Failed to add network";
        _state = ConnectivityState::ERROR;
        markDirty();
    }
    #endif
}

void ConnectivityTab::deleteSelectedNetwork() {
    #if ENABLE_WIFI
    if (_selectedNetworkIndex < 0 || !_isSavedNetworkSelected) return;
    if (_selectedNetworkIndex >= _savedNetworkCount) return;
    if (!_httpClient) {
        _httpClient = new HttpClient();
    }

    String ssid = _savedNetworks[_selectedNetworkIndex].ssid;
    if (_httpClient->deleteNetwork(ssid)) {
        Serial.printf("[ConnectivityTab] Network %s deleted\n", ssid.c_str());
        _selectedNetworkIndex = -1;
        updateConnectionStatus();  // Refresh list
    } else {
        _errorMessage = "Failed to delete network";
        _state = ConnectivityState::ERROR;
        markDirty();
    }
    #endif
}

void ConnectivityTab::disconnectFromNetwork() {
    #if ENABLE_WIFI
    if (!_httpClient) {
        _httpClient = new HttpClient();
    }
    if (_httpClient->disconnectFromNetwork()) {
        Serial.println("[ConnectivityTab] Disconnected from network");
        markDirty();
    } else {
        _errorMessage = "Failed to disconnect";
        _state = ConnectivityState::ERROR;
        markDirty();
    }
    #endif
}

// Static event callbacks
void ConnectivityTab::backButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab && tab->_backButtonCallback) {
        tab->_backButtonCallback();
    }
}

void ConnectivityTab::scanButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->startScan();
    }
}

void ConnectivityTab::addNetworkButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->_showAddDialog = true;
        lv_obj_clear_flag(tab->_addDialog, LV_OBJ_FLAG_HIDDEN);
    }
}

void ConnectivityTab::savedNetworkSelectedCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        uintptr_t index = (uintptr_t)lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        tab->_selectedNetworkIndex = static_cast<int8_t>(index);
        tab->_isSavedNetworkSelected = true;
        tab->markDirty();
    }
}

void ConnectivityTab::scannedNetworkSelectedCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        uintptr_t index = (uintptr_t)lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        tab->_selectedNetworkIndex = static_cast<int8_t>(index);
        tab->_isSavedNetworkSelected = false;
        tab->markDirty();
    }
}

void ConnectivityTab::connectButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->connectToSelectedNetwork();
    }
}

void ConnectivityTab::deleteButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->deleteSelectedNetwork();
    }
}

void ConnectivityTab::disconnectButtonCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab) {
        tab->disconnectFromNetwork();
    }
}

void ConnectivityTab::addDialogSaveCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab && tab->_ssidInput && tab->_passwordInput) {
        tab->_newNetworkSsid = lv_textarea_get_text(tab->_ssidInput);
        tab->_newNetworkPassword = lv_textarea_get_text(tab->_passwordInput);
        tab->addNewNetwork();
    }
}

void ConnectivityTab::addDialogCancelCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab && tab->_addDialog) {
        lv_obj_add_flag(tab->_addDialog, LV_OBJ_FLAG_HIDDEN);
        tab->_showAddDialog = false;
    }
}

// Stub implementations for missing methods
void ConnectivityTab::handleTouch(int16_t x, int16_t y) {
    // Handled by LVGL event system
}

void ConnectivityTab::handleEncoderChange(uint8_t encoderIndex, int32_t delta) {
    // Not used in connectivity tab
}

void ConnectivityTab::updateSavedNetworksList() {
    refreshNetworkLists();
}

void ConnectivityTab::updateScannedNetworksList() {
    refreshNetworkLists();
}

#endif // ENABLE_WIFI
