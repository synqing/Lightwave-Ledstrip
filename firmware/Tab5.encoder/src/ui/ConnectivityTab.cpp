// ============================================================================
// ConnectivityTab Implementation - Network Management UI
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
    // Initialize normal style
    lv_style_init(&_styleNormal);
    lv_style_set_bg_color(&_styleNormal, lv_color_hex(Theme::BG_DARK));
    lv_style_set_border_width(&_styleNormal, 2);
    lv_style_set_border_color(&_styleNormal, lv_color_hex(0xFFFFFF));
    lv_style_set_radius(&_styleNormal, 14);
    lv_style_set_pad_all(&_styleNormal, 10);

    // Initialize selected style
    lv_style_init(&_styleSelected);
    lv_style_set_bg_color(&_styleSelected, lv_color_hex(Theme::BG_PANEL));
    lv_style_set_border_width(&_styleSelected, 3);
    lv_style_set_border_color(&_styleSelected, lv_color_hex(Theme::ACCENT));
    lv_style_set_radius(&_styleSelected, 14);
    lv_style_set_pad_all(&_styleSelected, 10);

    // Initialize error style
    lv_style_init(&_styleError);
    lv_style_set_bg_color(&_styleError, lv_color_hex(0x4A1E1E));
    lv_style_set_border_width(&_styleError, 2);
    lv_style_set_border_color(&_styleError, lv_color_hex(0xFF4444));
    lv_style_set_radius(&_styleError, 14);
    lv_style_set_pad_all(&_styleError, 10);
}

void ConnectivityTab::createInteractiveUI(lv_obj_t* parent) {
    Serial.printf("[CT_TRACE] createInteractiveUI() entry @ %lu ms\n", millis());
    Serial.flush();
    _screen = parent;

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
    _backButton = lv_btn_create(parent);
    lv_obj_set_size(_backButton, 120, 50);
    lv_obj_set_pos(_backButton, 20, 20);
    lv_obj_set_style_bg_color(_backButton, lv_color_hex(Theme::BG_PANEL), LV_PART_MAIN);
    lv_obj_set_style_border_color(_backButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_backButton, 2, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_backButton);
    lv_label_set_text(label, "BACK");
    lv_obj_center(label);

    lv_obj_add_event_cb(_backButton, backButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createStatusLabel(lv_obj_t* parent) {
    _statusLabel = lv_label_create(parent);
    lv_obj_set_pos(_statusLabel, 20, STATUS_Y);
    lv_obj_set_size(_statusLabel, 1200, 30);
    lv_label_set_text(_statusLabel, "Status: Disconnected");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(Theme::TEXT_DIM), LV_PART_MAIN);
}

void ConnectivityTab::createSavedNetworksList(lv_obj_t* parent) {
    // Create scrollable container instead of list (LVGL lists disabled)
    _savedNetworksList = lv_obj_create(parent);
    lv_obj_set_pos(_savedNetworksList, 20, SAVED_LIST_Y);
    lv_obj_set_size(_savedNetworksList, 600, SAVED_LIST_H);
    lv_obj_set_style_bg_color(_savedNetworksList, lv_color_hex(Theme::BG_PANEL), LV_PART_MAIN);
    lv_obj_set_style_border_color(_savedNetworksList, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_savedNetworksList, 2, LV_PART_MAIN);
    lv_obj_set_layout(_savedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_savedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_savedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(_savedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_savedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

void ConnectivityTab::createScanButton(lv_obj_t* parent) {
    _scanButton = lv_btn_create(parent);
    lv_obj_set_size(_scanButton, 200, 50);
    lv_obj_set_pos(_scanButton, 20, SCAN_BUTTON_Y);
    lv_obj_set_style_bg_color(_scanButton, lv_color_hex(Theme::ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_color(_scanButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scanButton, 2, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_scanButton);
    lv_label_set_text(label, "SCAN NETWORKS");
    lv_obj_center(label);

    lv_obj_add_event_cb(_scanButton, scanButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createScannedNetworksList(lv_obj_t* parent) {
    // Create scrollable container instead of list (LVGL lists disabled)
    _scannedNetworksList = lv_obj_create(parent);
    lv_obj_set_pos(_scannedNetworksList, 20, SCANNED_LIST_Y);
    lv_obj_set_size(_scannedNetworksList, 1200, SCANNED_LIST_H);
    lv_obj_set_style_bg_color(_scannedNetworksList, lv_color_hex(Theme::BG_PANEL), LV_PART_MAIN);
    lv_obj_set_style_border_color(_scannedNetworksList, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_scannedNetworksList, 2, LV_PART_MAIN);
    lv_obj_set_layout(_scannedNetworksList, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_scannedNetworksList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_scannedNetworksList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(_scannedNetworksList, LV_DIR_VER);
    lv_obj_clear_flag(_scannedNetworksList, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

void ConnectivityTab::createAddNetworkButton(lv_obj_t* parent) {
    _addNetworkButton = lv_btn_create(parent);
    lv_obj_set_size(_addNetworkButton, 200, 50);
    lv_obj_set_pos(_addNetworkButton, 240, SCAN_BUTTON_Y);
    lv_obj_set_style_bg_color(_addNetworkButton, lv_color_hex(Theme::ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addNetworkButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addNetworkButton, 2, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_addNetworkButton);
    lv_label_set_text(label, "ADD NETWORK");
    lv_obj_center(label);

    lv_obj_add_event_cb(_addNetworkButton, addNetworkButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createActionButtons(lv_obj_t* parent) {
    // Connect button
    _connectButton = lv_btn_create(parent);
    lv_obj_set_size(_connectButton, 150, 50);
    lv_obj_set_pos(_connectButton, 20, ACTIONS_Y);
    lv_obj_set_style_bg_color(_connectButton, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_border_color(_connectButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_connectButton, 2, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(_connectButton);
    lv_label_set_text(label, "CONNECT");
    lv_obj_center(label);
    lv_obj_add_event_cb(_connectButton, connectButtonCb, LV_EVENT_CLICKED, this);

    // Delete button
    _deleteButton = lv_btn_create(parent);
    lv_obj_set_size(_deleteButton, 150, 50);
    lv_obj_set_pos(_deleteButton, 190, ACTIONS_Y);
    lv_obj_set_style_bg_color(_deleteButton, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_border_color(_deleteButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_deleteButton, 2, LV_PART_MAIN);

    label = lv_label_create(_deleteButton);
    lv_label_set_text(label, "DELETE");
    lv_obj_center(label);
    lv_obj_add_event_cb(_deleteButton, deleteButtonCb, LV_EVENT_CLICKED, this);

    // Disconnect button
    _disconnectButton = lv_btn_create(parent);
    lv_obj_set_size(_disconnectButton, 150, 50);
    lv_obj_set_pos(_disconnectButton, 360, ACTIONS_Y);
    lv_obj_set_style_bg_color(_disconnectButton, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_border_color(_disconnectButton, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_disconnectButton, 2, LV_PART_MAIN);

    label = lv_label_create(_disconnectButton);
    lv_label_set_text(label, "DISCONNECT");
    lv_obj_center(label);
    lv_obj_add_event_cb(_disconnectButton, disconnectButtonCb, LV_EVENT_CLICKED, this);
}

void ConnectivityTab::createAddNetworkDialog(lv_obj_t* parent) {
    _addDialog = lv_obj_create(parent);
    lv_obj_set_size(_addDialog, 600, 400);
    lv_obj_align(_addDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_addDialog, lv_color_hex(Theme::BG_DARK), LV_PART_MAIN);
    lv_obj_set_style_border_color(_addDialog, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(_addDialog, 2, LV_PART_MAIN);
    lv_obj_add_flag(_addDialog, LV_OBJ_FLAG_HIDDEN);  // Initially hidden

    // Title
    lv_obj_t* title = lv_label_create(_addDialog);
    lv_label_set_text(title, "ADD NETWORK");
    lv_obj_set_style_text_color(title, lv_color_hex(Theme::TEXT_BRIGHT), LV_PART_MAIN);
    lv_obj_set_pos(title, 20, 20);

    // SSID input
    lv_obj_t* ssidLabel = lv_label_create(_addDialog);
    lv_label_set_text(ssidLabel, "SSID:");
    lv_obj_set_pos(ssidLabel, 20, 60);

    _ssidInput = lv_textarea_create(_addDialog);
    lv_obj_set_size(_ssidInput, 560, 50);
    lv_obj_set_pos(_ssidInput, 20, 90);
    lv_textarea_set_placeholder_text(_ssidInput, "Enter network name");

    // Password input
    lv_obj_t* passwordLabel = lv_label_create(_addDialog);
    lv_label_set_text(passwordLabel, "Password:");
    lv_obj_set_pos(passwordLabel, 20, 160);

    _passwordInput = lv_textarea_create(_addDialog);
    lv_obj_set_size(_passwordInput, 560, 50);
    lv_obj_set_pos(_passwordInput, 20, 190);
    lv_textarea_set_placeholder_text(_passwordInput, "Enter password");
    lv_textarea_set_password_mode(_passwordInput, true);

    // Save button
    lv_obj_t* saveBtn = lv_btn_create(_addDialog);
    lv_obj_set_size(saveBtn, 120, 50);
    lv_obj_set_pos(saveBtn, 300, 280);
    lv_obj_set_style_bg_color(saveBtn, lv_color_hex(Theme::ACCENT), LV_PART_MAIN);
    lv_obj_t* saveLabel = lv_label_create(saveBtn);
    lv_label_set_text(saveLabel, "SAVE");
    lv_obj_center(saveLabel);
    lv_obj_add_event_cb(saveBtn, addDialogSaveCb, LV_EVENT_CLICKED, this);

    // Cancel button
    lv_obj_t* cancelBtn = lv_btn_create(_addDialog);
    lv_obj_set_size(cancelBtn, 120, 50);
    lv_obj_set_pos(cancelBtn, 440, 280);
    lv_obj_set_style_bg_color(cancelBtn, lv_color_hex(Theme::BG_PANEL), LV_PART_MAIN);
    lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_center(cancelLabel);
    lv_obj_add_event_cb(cancelBtn, addDialogCancelCb, LV_EVENT_CLICKED, this);
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
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(0x00FF00), LV_PART_MAIN);
    } else {
        lv_label_set_text(_statusLabel, "Status: Disconnected");
        lv_obj_set_style_text_color(_statusLabel, lv_color_hex(Theme::TEXT_DIM), LV_PART_MAIN);
    }
    #else
    lv_label_set_text(_statusLabel, "WiFi disabled");
    lv_obj_set_style_text_color(_statusLabel, lv_color_hex(Theme::TEXT_DIM), LV_PART_MAIN);
    #endif
}

void ConnectivityTab::refreshNetworkLists() {
    // Update saved networks list
    if (_savedNetworksList) {
        lv_obj_clean(_savedNetworksList);
        for (uint8_t i = 0; i < _savedNetworkCount; i++) {
            lv_obj_t* btn = lv_btn_create(_savedNetworksList);
            lv_obj_set_size(btn, 580, 40);
            lv_obj_set_style_bg_color(btn, lv_color_hex(Theme::BG_DARK), LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_all(btn, 5, LV_PART_MAIN);
            
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, _savedNetworks[i].ssid.c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(Theme::TEXT_BRIGHT), LV_PART_MAIN);
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

    // Update scanned networks list
    if (_scannedNetworksList) {
        lv_obj_clean(_scannedNetworksList);
        for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
            char label[64];
            snprintf(label, sizeof(label), "%s (%d dBm)", 
                     _scannedNetworks[i].ssid.c_str(), 
                     _scannedNetworks[i].rssi);
            
            lv_obj_t* btn = lv_btn_create(_scannedNetworksList);
            lv_obj_set_size(btn, 1180, 40);
            lv_obj_set_style_bg_color(btn, lv_color_hex(Theme::BG_DARK), LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_all(btn, 5, LV_PART_MAIN);
            
            lv_obj_t* labelObj = lv_label_create(btn);
            lv_label_set_text(labelObj, label);
            lv_obj_set_style_text_color(labelObj, lv_color_hex(Theme::TEXT_BRIGHT), LV_PART_MAIN);
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
    if (_httpClient->startScan(_scanJobId)) {
        _scanInProgress = true;
        _state = ConnectivityState::SCANNING;
        Serial.printf("[ConnectivityTab] Scan started, job ID: %u\n", _scanJobId);
        markDirty();
    } else {
        _errorMessage = "Failed to start scan";
        _state = ConnectivityState::ERROR;
        markDirty();
    }
    #endif
}

void ConnectivityTab::checkScanStatus() {
    #if ENABLE_WIFI
    if (!_httpClient) return;
    ScanStatus status;
    if (_httpClient->getScanStatus(status)) {
        _scanInProgress = status.inProgress;
        if (!status.inProgress && status.networkCount > 0) {
            _scannedNetworkCount = status.networkCount > 20 ? 20 : status.networkCount;
            for (uint8_t i = 0; i < _scannedNetworkCount; i++) {
                _scannedNetworks[i] = status.networks[i];
            }
            _state = ConnectivityState::IDLE;
            markDirty();
        }
    }
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
