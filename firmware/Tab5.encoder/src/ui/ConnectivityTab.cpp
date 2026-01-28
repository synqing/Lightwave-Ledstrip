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
#include <WiFi.h>
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

    // CRITICAL FIX: Update status label immediately to reflect actual WiFi state
    // Previously hardcoded to "Disconnected" in createStatusLabel()
    updateStatusLabel();

    // DEFERRED LOADING: Set flag to load saved networks on first loop() iteration
    // DO NOT call HTTP here - mDNS resolution blocks for 5+ seconds and triggers WDT!
    // The blocking call was: _httpClient->listNetworks() -> connectToServer() ->
    // resolveHostname() -> mDNS.queryHost("lightwaveos.local") which blocks.
    #if ENABLE_WIFI
    _needsInitialLoad = true;
    Serial.println("[ConnectivityTab] Deferred network loading to loop()");
    Serial.printf("[CT_DIAG] begin() this=%p _needsInitialLoad set to TRUE\n", (void*)this);
    #endif

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

    // Create page title with BEBAS_BOLD_40 font - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "NETWORK SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_40, LV_PART_MAIN);
    // Center the title: starts at X=160 (after back button), spans remaining width
    lv_obj_set_width(title, 1280 - 160 - 20);  // Full width minus back button area minus margin
    lv_obj_set_pos(title, 160, 25);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    Serial.printf("[CT_TRACE] before createBackButton @ %lu ms\n", millis());
    Serial.flush();
    createBackButton(_screen);
    esp_task_wdt_reset();

    Serial.printf("[CT_TRACE] before createStatusLabel @ %lu ms\n", millis());
    Serial.flush();
    createStatusLabel(_screen);

    // ==========================================================================
    // NEW LAYOUT: Available Networks (TOP) with SCAN/ADD buttons on right
    // ==========================================================================
    Serial.printf("[CT_TRACE] before createAvailableNetworksCard @ %lu ms\n", millis());
    Serial.flush();
    createAvailableNetworksCard(_screen);
    esp_task_wdt_reset();

    Serial.printf("[CT_TRACE] before createAvailableNetworksButtons @ %lu ms\n", millis());
    Serial.flush();
    createAvailableNetworksButtons(_screen);

    // ==========================================================================
    // NEW LAYOUT: Saved Networks (BELOW) with CONNECT/DISCONNECT/DELETE on right
    // ==========================================================================
    Serial.printf("[CT_TRACE] before createSavedNetworksCard @ %lu ms\n", millis());
    Serial.flush();
    createSavedNetworksCard(_screen);
    esp_task_wdt_reset();

    Serial.printf("[CT_TRACE] before createSavedNetworksButtons @ %lu ms\n", millis());
    Serial.flush();
    createSavedNetworksButtons(_screen);
    esp_task_wdt_reset();

    Serial.printf("[CT_TRACE] before createAddNetworkDialog @ %lu ms\n", millis());
    Serial.flush();
    createAddNetworkDialog(_screen);

    Serial.printf("[CT_TRACE] createInteractiveUI() exit @ %lu ms\n", millis());
    Serial.flush();
    esp_task_wdt_reset();
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

// ==========================================================================
// AVAILABLE NETWORKS CARD (TOP) - Shows scanned WiFi networks
// ==========================================================================
void ConnectivityTab::createAvailableNetworksCard(lv_obj_t* parent) {
    lv_obj_t* card = make_card(parent, false);
    lv_obj_set_pos(card, NETWORK_CARD_X, AVAILABLE_Y);
    lv_obj_set_size(card, NETWORK_CARD_W, NETWORK_CARD_H);

    // Section title - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "AVAILABLE NETWORKS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_width(title, NETWORK_CARD_W - 20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);

    // Inner scrollable list
    _scannedNetworksList = lv_obj_create(card);
    Serial.printf("[CT_DEBUG] createAvailableNetworksCard: widget=%p\n", (void*)_scannedNetworksList);
    lv_obj_set_pos(_scannedNetworksList, 0, 40);
    lv_obj_set_size(_scannedNetworksList, NETWORK_CARD_W - 20, NETWORK_LIST_H);
    lv_obj_set_style_bg_color(_scannedNetworksList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_scannedNetworksList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_scannedNetworksList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scannedNetworksList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_scannedNetworksList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_scannedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_scannedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_scannedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_scannedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_scannedNetworksList, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_scannedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_scannedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

// ==========================================================================
// AVAILABLE NETWORKS BUTTONS (RIGHT SIDE) - SCAN, ADD
// Vertically centered with Available Networks card (card center at Y=250)
// ==========================================================================
void ConnectivityTab::createAvailableNetworksButtons(lv_obj_t* parent) {
    // Vertical centering: card Y=120, H=260, center at Y=250
    // 2 buttons: 50 + 10 + 50 = 110px total height
    // First button Y: 250 - (110/2) = 195
    constexpr int AVAILABLE_BTN_START_Y = 195;

    // SCAN button: Yellow primary action
    _scanButton = lv_btn_create(parent);
    lv_obj_set_size(_scanButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_scanButton, BUTTON_COLUMN_X, AVAILABLE_BTN_START_Y);
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

    // ADD button: White border secondary action
    _addNetworkButton = lv_btn_create(parent);
    lv_obj_set_size(_addNetworkButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_addNetworkButton, BUTTON_COLUMN_X, AVAILABLE_BTN_START_Y + BUTTON_H + BUTTON_GAP);
    lv_obj_set_style_bg_color(_addNetworkButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addNetworkButton, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addNetworkButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_addNetworkButton, 14, LV_PART_MAIN);

    lv_obj_t* addLabel = lv_label_create(_addNetworkButton);
    lv_label_set_text(addLabel, "ADD");
    lv_obj_set_style_text_color(addLabel, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(addLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(addLabel);
    lv_obj_add_event_cb(_addNetworkButton, addNetworkButtonCb, LV_EVENT_CLICKED, this);
}

// ==========================================================================
// SAVED NETWORKS CARD (BOTTOM) - Shows saved WiFi credentials
// ==========================================================================
void ConnectivityTab::createSavedNetworksCard(lv_obj_t* parent) {
    lv_obj_t* card = make_card(parent, false);
    lv_obj_set_pos(card, NETWORK_CARD_X, SAVED_Y);
    lv_obj_set_size(card, NETWORK_CARD_W, NETWORK_CARD_H);

    // Section title - CENTER ALIGNED
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "SAVED NETWORKS");
    lv_obj_set_style_text_color(title, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, BEBAS_BOLD_32, LV_PART_MAIN);
    lv_obj_set_width(title, NETWORK_CARD_W - 20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(title, 0, 0);

    // Inner scrollable list
    _savedNetworksList = lv_obj_create(card);
    lv_obj_set_pos(_savedNetworksList, 0, 40);
    lv_obj_set_size(_savedNetworksList, NETWORK_CARD_W - 20, NETWORK_LIST_H);
    lv_obj_set_style_bg_color(_savedNetworksList, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_savedNetworksList, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_savedNetworksList, lv_color_hex(TAB5_COLOR_BORDER_BASE), LV_PART_MAIN);
    lv_obj_set_style_border_width(_savedNetworksList, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_savedNetworksList, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_savedNetworksList, 6, LV_PART_MAIN);
    lv_obj_set_layout(_savedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_savedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_savedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(_savedNetworksList, 4, LV_PART_MAIN);
    lv_obj_set_scroll_dir(_savedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_savedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

// ==========================================================================
// SAVED NETWORKS BUTTONS (RIGHT SIDE) - CONNECT, DISCONNECT, DELETE
// Vertically centered with Saved Networks card (card center at Y=530)
// ==========================================================================
void ConnectivityTab::createSavedNetworksButtons(lv_obj_t* parent) {
    // Vertical centering: card Y=400, H=260, center at Y=530
    // 3 buttons: 50 + 10 + 50 + 10 + 50 = 170px total height
    // First button Y: 530 - (170/2) = 445
    constexpr int SAVED_BTN_START_Y = 445;

    // CONNECT button: Green primary action
    _connectButton = lv_btn_create(parent);
    lv_obj_set_size(_connectButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_connectButton, BUTTON_COLUMN_X, SAVED_BTN_START_Y);
    lv_obj_set_style_bg_color(_connectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_color(_connectButton, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_border_width(_connectButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_connectButton, 14, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_connectButton);
    lv_label_set_text(label, "CONNECT");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_connectButton, connectButtonCb, LV_EVENT_CLICKED, this);

    // DISCONNECT button: Amber warning action
    _disconnectButton = lv_btn_create(parent);
    lv_obj_set_size(_disconnectButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_disconnectButton, BUTTON_COLUMN_X, SAVED_BTN_START_Y + BUTTON_H + BUTTON_GAP);
    lv_obj_set_style_bg_color(_disconnectButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_disconnectButton, lv_color_hex(TAB5_COLOR_STATUS_WARNING), LV_PART_MAIN);
    lv_obj_set_style_border_width(_disconnectButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_disconnectButton, 14, LV_PART_MAIN);

    label = lv_label_create(_disconnectButton);
    lv_label_set_text(label, "DISCONNECT");  // Full spelling with 180px button
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_STATUS_WARNING), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_disconnectButton, disconnectButtonCb, LV_EVENT_CLICKED, this);

    // DELETE button: Red danger action
    _deleteButton = lv_btn_create(parent);
    lv_obj_set_size(_deleteButton, BUTTON_W, BUTTON_H);
    lv_obj_set_pos(_deleteButton, BUTTON_COLUMN_X, SAVED_BTN_START_Y + (BUTTON_H + BUTTON_GAP) * 2);
    lv_obj_set_style_bg_color(_deleteButton, lv_color_hex(TAB5_COLOR_BG_SURFACE_ELEVATED), LV_PART_MAIN);
    lv_obj_set_style_border_color(_deleteButton, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
    lv_obj_set_style_border_width(_deleteButton, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(_deleteButton, 14, LV_PART_MAIN);

    label = lv_label_create(_deleteButton);
    lv_label_set_text(label, "DELETE");
    lv_obj_set_style_text_color(label, lv_color_hex(TAB5_COLOR_STATUS_ERROR), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(_deleteButton, deleteButtonCb, LV_EVENT_CLICKED, this);
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

    // =========================================================================
    // LVGL Keyboard for text input on touch devices
    // Production-quality styling for 1280x720 display
    // =========================================================================
    _keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(_keyboard, 1200, 320);  // Wider, taller for comfortable touch
    lv_obj_align(_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);

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

    // ===== KEY BUTTON STYLING (CRITICAL FOR READABILITY!) =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(0x3A3A3C), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(_keyboard, LV_OPA_100, LV_PART_ITEMS);

    // FONT - JetBrains Mono for full ASCII coverage (@, #, $, %, etc.)
    // Perfect for keyboard input - monospace, readable, full symbol support
    lv_obj_set_style_text_font(_keyboard, JETBRAINS_MONO_BOLD_24, LV_PART_ITEMS);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);

    // Button borders for definition
    lv_obj_set_style_border_color(_keyboard, lv_color_hex(0x5A5A5C), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(_keyboard, 8, LV_PART_ITEMS);

    // SPACING BETWEEN BUTTONS - Critical for accurate touch
    lv_obj_set_style_pad_row(_keyboard, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_keyboard, 6, LV_PART_MAIN);

    // ===== PRESSED STATE - Visual feedback when tapping =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_PRESSED);

    // ===== CHECKED STATE (Shift/Caps indicator) =====
    lv_obj_set_style_bg_color(_keyboard, lv_color_hex(0x0D7377), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Enable popovers for mobile-like key feedback
    lv_keyboard_set_popovers(_keyboard, true);

    // SSID input focus -> show keyboard
    lv_obj_add_event_cb(_ssidInput, [](lv_event_t* e) {
        ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_keyboard_set_textarea(tab->_keyboard, tab->_ssidInput);
            lv_obj_clear_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_FOCUSED, this);

    // Password input focus -> show keyboard
    lv_obj_add_event_cb(_passwordInput, [](lv_event_t* e) {
        ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_keyboard_set_textarea(tab->_keyboard, tab->_passwordInput);
            lv_obj_clear_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_FOCUSED, this);

    // Hide keyboard on READY (Enter pressed)
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_READY, this);

    // Hide keyboard on CANCEL
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
        if (tab && tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_CANCEL, this);
}

void ConnectivityTab::loop() {
    uint32_t now = millis();

    // =========================================================================
    // DIAGNOSTIC: Log entry and flag state to debug deferred loading issue
    // =========================================================================
    #if ENABLE_WIFI
    static uint32_t lastDiagLog = 0;
    if (now - lastDiagLog >= 2000) {  // Every 2 seconds
        Serial.printf("[CT_DIAG] loop() this=%p _needsInitialLoad=%d _savedNetworkCount=%d\n",
                      (void*)this, _needsInitialLoad, _savedNetworkCount);
        lastDiagLog = now;
    }
    #endif

    // =========================================================================
    // DEFERRED INITIAL LOAD: Load saved networks on first loop() iteration
    // This was moved from begin() because HTTP calls block for 5+ seconds
    // during mDNS resolution, triggering the watchdog timer.
    // =========================================================================
    #if ENABLE_WIFI
    if (_needsInitialLoad) {
        _needsInitialLoad = false;  // Clear flag FIRST to prevent re-entry

        Serial.println("[ConnectivityTab] ========== DEFERRED NETWORK LOAD START ==========");
        if (!_httpClient) {
            Serial.println("[ConnectivityTab] Creating HttpClient...");
            _httpClient = new HttpClient();
        }

        _initialLoadAwaitingDiscovery = true;
        _httpClient->startDiscovery();
    }

    // =========================================================================
    // FALLBACK: If deferred load didn't work, force load after 3 seconds
    // This handles the mysterious case where _needsInitialLoad doesn't trigger
    // =========================================================================
    static uint32_t firstLoopTime = 0;
    static bool fallbackAttempted = false;
    if (firstLoopTime == 0) firstLoopTime = now;

    if (!fallbackAttempted && _savedNetworkCount == 0 && (now - firstLoopTime) > 3000) {
        fallbackAttempted = true;
        Serial.println("[CT_FALLBACK] ========== FORCE LOADING AFTER 3s TIMEOUT ==========");
        Serial.printf("[CT_FALLBACK] this=%p _needsInitialLoad was: %d\n", (void*)this, _needsInitialLoad);

        if (!_httpClient) {
            Serial.println("[CT_FALLBACK] Creating HttpClient...");
            _httpClient = new HttpClient();
        }

        _initialLoadAwaitingDiscovery = true;
        if (_httpClient->getDiscoveryState() != HttpClient::DiscoveryState::RUNNING) {
            _httpClient->startDiscovery();
        }
        Serial.println("[CT_FALLBACK] ========== FORCE LOADING COMPLETE ==========");
    }
    #endif

    #if ENABLE_WIFI
    if (_initialLoadAwaitingDiscovery && _httpClient) {
        auto state = _httpClient->getDiscoveryState();
        if (state == HttpClient::DiscoveryState::SUCCESS) {
            _initialLoadAwaitingDiscovery = false;
            Serial.println("[ConnectivityTab] Discovery complete - loading saved networks...");
            int savedCount = _httpClient->listNetworks(_savedNetworks, 10);
            Serial.printf("[ConnectivityTab] listNetworks() returned: %d\n", savedCount);

            if (savedCount >= 0) {
                _savedNetworkCount = static_cast<uint8_t>(savedCount);
                Serial.printf("[ConnectivityTab] Loaded %d saved networks:\n", savedCount);
                for (int i = 0; i < savedCount; i++) {
                    Serial.printf("  [%d] SSID: %s\n", i, _savedNetworks[i].ssid.c_str());
                }
                forceDirty();
                Serial.println("[ConnectivityTab] Auto-scanning for available networks...");
                startScan();
            } else {
                Serial.println("[ConnectivityTab] FAILED to load saved networks after discovery.");
            }
            Serial.println("[ConnectivityTab] ========== DEFERRED NETWORK LOAD END ==========");
        } else if (state == HttpClient::DiscoveryState::FAILED) {
            _initialLoadAwaitingDiscovery = false;
            Serial.println("[ConnectivityTab] Discovery failed - cannot load saved networks.");
        }
    }

    #endif

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
    // Only update status label - NO HTTP calls here!
    // HTTP calls (listNetworks, scan) are triggered by user action (SCAN button)
    // This prevents blocking the main loop with network timeouts
    updateStatusLabel();
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

// ==========================================================================
// NETWORK ITEM FACTORY - Creates 3-zone structured items
// Layout: [SSID (500px)] [RSSI (100px)] [Selection Dot (40px)]
// ==========================================================================
lv_obj_t* ConnectivityTab::createNetworkItem(
    lv_obj_t* parent,
    const char* ssid,
    int rssi,
    bool isConnected,
    bool isSelected,
    int index,
    bool isSavedNetwork
) {
    // Calculate item width based on parent container
    int itemWidth = NETWORK_CARD_W - 40;  // Account for padding

    // Item container (48px for touch-friendly targets)
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_size(item, itemWidth, ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_hex(TAB5_COLOR_BG_SURFACE_BASE), LV_PART_MAIN);
    lv_obj_set_style_radius(item, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    // Flex layout for 3-zone structure
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // === ZONE 1: SSID (left-aligned, scrolling if overflow) ===
    lv_obj_t* ssidZone = lv_obj_create(item);
    lv_obj_set_size(ssidZone, ITEM_SSID_W, ITEM_H - 4);
    lv_obj_set_flex_grow(ssidZone, 0);  // CRITICAL: prevent expansion
    lv_obj_set_style_bg_opa(ssidZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ssidZone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(ssidZone, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(ssidZone, 4, LV_PART_MAIN);
    lv_obj_remove_flag(ssidZone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ssidLabel = lv_label_create(ssidZone);
    lv_obj_set_width(ssidLabel, ITEM_SSID_W - 20);
    lv_label_set_long_mode(ssidLabel, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);  // LVGL 9.x
    lv_label_set_text(ssidLabel, ssid);
    lv_obj_set_style_text_font(ssidLabel, RAJDHANI_BOLD_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(ssidLabel, lv_color_hex(TAB5_COLOR_FG_PRIMARY), LV_PART_MAIN);
    lv_obj_align(ssidLabel, LV_ALIGN_LEFT_MID, 0, 0);

    // === ZONE 2: RSSI (fixed width, right-aligned text) ===
    lv_obj_t* rssiZone = lv_obj_create(item);
    lv_obj_set_size(rssiZone, ITEM_RSSI_W, ITEM_H - 4);
    lv_obj_set_flex_grow(rssiZone, 0);  // CRITICAL: prevent expansion
    lv_obj_set_style_bg_opa(rssiZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rssiZone, 0, LV_PART_MAIN);
    lv_obj_remove_flag(rssiZone, LV_OBJ_FLAG_SCROLLABLE);

    if (rssi != -1) {  // Only show RSSI for scanned networks
        char rssiText[16];
        snprintf(rssiText, sizeof(rssiText), "%d dBm", rssi);

        lv_obj_t* rssiLabel = lv_label_create(rssiZone);
        lv_label_set_text(rssiLabel, rssiText);
        lv_obj_set_style_text_font(rssiLabel, RAJDHANI_MED_24, LV_PART_MAIN);

        // Color code signal strength
        uint32_t rssiColor = (rssi >= -50) ? TAB5_COLOR_STATUS_SUCCESS :  // Green
                             (rssi >= -70) ? TAB5_COLOR_STATUS_WARNING :  // Amber
                                             TAB5_COLOR_STATUS_ERROR;     // Red
        lv_obj_set_style_text_color(rssiLabel, lv_color_hex(rssiColor), LV_PART_MAIN);
        lv_obj_align(rssiLabel, LV_ALIGN_RIGHT_MID, -4, 0);
    }

    // === ZONE 3: Selection/Connected Indicator Dot ===
    lv_obj_t* dotZone = lv_obj_create(item);
    lv_obj_set_size(dotZone, ITEM_DOT_W, ITEM_H - 4);
    lv_obj_set_flex_grow(dotZone, 0);  // CRITICAL: prevent expansion
    lv_obj_set_style_bg_opa(dotZone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dotZone, 0, LV_PART_MAIN);
    lv_obj_remove_flag(dotZone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* dot = lv_obj_create(dotZone);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(TAB5_COLOR_STATUS_SUCCESS), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);

    // Show dot for connected OR selected networks
    if (isConnected || isSelected) {
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }

    // === SELECTION STYLING (yellow border + tinted background) ===
    if (isSelected) {
        lv_obj_set_style_border_color(item, lv_color_hex(TAB5_COLOR_BRAND_PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x252528), LV_PART_MAIN);  // Tinted
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
    if (isSavedNetwork) {
        lv_obj_add_event_cb(item, savedNetworkSelectedCb, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_add_event_cb(item, scannedNetworkSelectedCb, LV_EVENT_CLICKED, this);
    }

    // Make clickable
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

    return item;
}

void ConnectivityTab::refreshNetworkLists() {
    // Get current connected SSID for highlighting
    String connectedSSID = "";
    #if ENABLE_WIFI
    if (_wifiManager && _wifiManager->isConnected()) {
        connectedSSID = _wifiManager->getSSID();
    }
    #endif

    // ==========================================================================
    // UPDATE AVAILABLE (SCANNED) NETWORKS LIST - Uses factory function
    // ==========================================================================
    if (_scannedNetworksList) {
        lv_obj_clean(_scannedNetworksList);

        if (_scannedNetworkCount == 0) {
            // Empty state message
            lv_obj_t* emptyLabel = lv_label_create(_scannedNetworksList);
            lv_label_set_text(emptyLabel, "No networks found\nTap SCAN to search");
            lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_text_color(emptyLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(emptyLabel, RAJDHANI_MED_24, LV_PART_MAIN);
            lv_obj_set_width(emptyLabel, NETWORK_CARD_W - 60);
            lv_obj_center(emptyLabel);
        } else {
            for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
                bool isConnected = (_scannedNetworks[i].ssid == connectedSSID);
                bool isSelected = (!_isSavedNetworkSelected && _selectedNetworkIndex == i);

                createNetworkItem(
                    _scannedNetworksList,
                    _scannedNetworks[i].ssid.c_str(),
                    _scannedNetworks[i].rssi,
                    isConnected,
                    isSelected,
                    i,
                    false  // isSavedNetwork = false
                );

                // CRITICAL: Watchdog reset every 3 items
                if ((i + 1) % 3 == 0) {
                    delay(1);
                    esp_task_wdt_reset();
                }
            }
        }
        lv_obj_invalidate(_scannedNetworksList);
    }

    // ==========================================================================
    // UPDATE SAVED NETWORKS LIST - Uses factory function
    // ==========================================================================
    if (_savedNetworksList) {
        lv_obj_clean(_savedNetworksList);

        if (_savedNetworkCount == 0) {
            // Empty state message
            lv_obj_t* emptyLabel = lv_label_create(_savedNetworksList);
            lv_label_set_text(emptyLabel, "No saved networks\nAdd from Available Networks");
            lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_text_color(emptyLabel, lv_color_hex(TAB5_COLOR_FG_SECONDARY), LV_PART_MAIN);
            lv_obj_set_style_text_font(emptyLabel, RAJDHANI_MED_24, LV_PART_MAIN);
            lv_obj_set_width(emptyLabel, NETWORK_CARD_W - 60);
            lv_obj_center(emptyLabel);
        } else {
            for (uint8_t i = 0; i < _savedNetworkCount; i++) {
                bool isConnected = (_savedNetworks[i].ssid == connectedSSID);
                bool isSelected = (_isSavedNetworkSelected && _selectedNetworkIndex == i);

                createNetworkItem(
                    _savedNetworksList,
                    _savedNetworks[i].ssid.c_str(),
                    -1,  // No RSSI for saved networks
                    isConnected,
                    isSelected,
                    i,
                    true  // isSavedNetwork = true
                );

                // CRITICAL: Watchdog reset every 3 items
                if ((i + 1) % 3 == 0) {
                    delay(1);
                    esp_task_wdt_reset();
                }
            }
        }
        lv_obj_invalidate(_savedNetworksList);
    }
}

void ConnectivityTab::startScan() {
    #if ENABLE_WIFI
    // Visual feedback: Change button to "SCANNING..."
    if (_scanButtonLabel) {
        lv_label_set_text(_scanButtonLabel, "SCANNING...");
        lv_obj_center(_scanButtonLabel);
        // Force LVGL to render the change NOW before blocking HTTP call
        lv_refr_now(NULL);
    }

    _state = ConnectivityState::SCANNING;
    _scanInProgress = true;

    Serial.println("[SCAN] Local WiFi scan started");
    int found = WiFi.scanNetworks();
    if (found < 0) {
        _errorMessage = "WiFi scan failed";
        _state = ConnectivityState::ERROR;
        _scanInProgress = false;
        if (_scanButtonLabel) {
            lv_label_set_text(_scanButtonLabel, "SCAN");
            lv_obj_center(_scanButtonLabel);
        }
        markDirty();
        Serial.println("[SCAN] Local WiFi scan failed");
        return;
    }

    _scannedNetworkCount = found > 20 ? 20 : static_cast<uint8_t>(found);
    for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
        _scannedNetworks[i].ssid = WiFi.SSID(i);
        _scannedNetworks[i].rssi = WiFi.RSSI(i);
        _scannedNetworks[i].channel = WiFi.channel(i);

        auto enc = WiFi.encryptionType(i);
        bool isOpen = (enc == WIFI_AUTH_OPEN);
        _scannedNetworks[i].encrypted = !isOpen;

        switch (enc) {
            case WIFI_AUTH_OPEN:
                _scannedNetworks[i].encryptionType = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                _scannedNetworks[i].encryptionType = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                _scannedNetworks[i].encryptionType = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                _scannedNetworks[i].encryptionType = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                _scannedNetworks[i].encryptionType = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                _scannedNetworks[i].encryptionType = "WPA2-ENT";
                break;
            case WIFI_AUTH_WPA3_PSK:
                _scannedNetworks[i].encryptionType = "WPA3";
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                _scannedNetworks[i].encryptionType = "WPA2/WPA3";
                break;
            case WIFI_AUTH_WAPI_PSK:
                _scannedNetworks[i].encryptionType = "WAPI";
                break;
            default:
                _scannedNetworks[i].encryptionType = "UNKNOWN";
                break;
        }
    }

    WiFi.scanDelete();
    _scanInProgress = false;
    _state = ConnectivityState::IDLE;
    if (_scanButtonLabel) {
        lv_label_set_text(_scanButtonLabel, "SCAN");
        lv_obj_center(_scanButtonLabel);
    }
    Serial.printf("[SCAN] Local WiFi scan complete: %u networks\n", _scannedNetworkCount);
    forceDirty();
    refreshNetworkLists();
    #endif
}

void ConnectivityTab::performScanRequest() {
    #if ENABLE_WIFI
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

        // ALSO refresh saved networks list when scanning
        int savedCount = _httpClient->listNetworks(_savedNetworks, 10);
        if (savedCount >= 0) {
            _savedNetworkCount = static_cast<uint8_t>(savedCount);
            Serial.printf("[ConnectivityTab] Refreshed %d saved networks\n", savedCount);
        }

        if (_scanButtonLabel) {
            lv_label_set_text(_scanButtonLabel, "SCAN");
            lv_obj_center(_scanButtonLabel);
        }

        forceDirty();
        refreshNetworkLists();
    } else {
        _errorMessage = "Failed to scan networks";
        _state = ConnectivityState::ERROR;
        _scanInProgress = false;

        if (_scanButtonLabel) {
            lv_label_set_text(_scanButtonLabel, "SCAN");
            lv_obj_center(_scanButtonLabel);
        }
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

        // Re-fetch saved networks to show the newly added one
        int savedCount = _httpClient->listNetworks(_savedNetworks, 10);
        if (savedCount >= 0) {
            _savedNetworkCount = static_cast<uint8_t>(savedCount);
        }

        // Refresh saved networks list to show newly added network
        forceDirty();
        refreshNetworkLists();
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

        // Re-fetch saved networks to reflect the deletion
        int savedCount = _httpClient->listNetworks(_savedNetworks, 10);
        if (savedCount >= 0) {
            _savedNetworkCount = static_cast<uint8_t>(savedCount);
        }

        forceDirty();
        refreshNetworkLists();
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
    if (!tab) return;

    #if ENABLE_WIFI
    // Must have a scanned network selected
    if (tab->_isSavedNetworkSelected || tab->_selectedNetworkIndex < 0) {
        Serial.println("[ConnectivityTab] ADD: No scanned network selected");
        return;
    }
    if (tab->_selectedNetworkIndex >= tab->_scannedNetworkCount) {
        Serial.println("[ConnectivityTab] ADD: Invalid network index");
        return;
    }

    // Get the selected network
    ScanResult& network = tab->_scannedNetworks[tab->_selectedNetworkIndex];
    tab->_newNetworkSsid = network.ssid;

    if (network.encrypted) {
        // Encrypted network: show password dialog
        tab->_showAddDialog = true;
        if (tab->_ssidInput) {
            lv_textarea_set_text(tab->_ssidInput, network.ssid.c_str());
            lv_obj_add_state(tab->_ssidInput, LV_STATE_DISABLED);  // Can't edit SSID
        }
        if (tab->_passwordInput) {
            lv_textarea_set_text(tab->_passwordInput, "");
        }
        lv_obj_clear_flag(tab->_addDialog, LV_OBJ_FLAG_HIDDEN);
        Serial.printf("[ConnectivityTab] ADD: Showing password dialog for '%s'\n", network.ssid.c_str());
    } else {
        // Open network: add directly
        tab->_newNetworkPassword = "";
        tab->addNewNetwork();
        Serial.printf("[ConnectivityTab] ADD: Added open network '%s'\n", network.ssid.c_str());
    }
    #endif
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
    if (tab && tab->_passwordInput) {
        // Only get SSID from field if it's not disabled (manual entry mode)
        // When adding scanned network, _newNetworkSsid is already set
        if (tab->_ssidInput && !lv_obj_has_state(tab->_ssidInput, LV_STATE_DISABLED)) {
            tab->_newNetworkSsid = lv_textarea_get_text(tab->_ssidInput);
        }
        tab->_newNetworkPassword = lv_textarea_get_text(tab->_passwordInput);

        // Hide keyboard before processing
        if (tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        // Re-enable SSID input for next use
        if (tab->_ssidInput) {
            lv_obj_clear_state(tab->_ssidInput, LV_STATE_DISABLED);
        }
        tab->addNewNetwork();
    }
}

void ConnectivityTab::addDialogCancelCb(lv_event_t* e) {
    ConnectivityTab* tab = static_cast<ConnectivityTab*>(lv_event_get_user_data(e));
    if (tab && tab->_addDialog) {
        lv_obj_add_flag(tab->_addDialog, LV_OBJ_FLAG_HIDDEN);
        tab->_showAddDialog = false;
        // Re-enable SSID input for next use
        if (tab->_ssidInput) {
            lv_obj_clear_state(tab->_ssidInput, LV_STATE_DISABLED);
        }
        // Hide keyboard
        if (tab->_keyboard) {
            lv_obj_add_flag(tab->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
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
