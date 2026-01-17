#pragma once
// ============================================================================
// ConnectivityTab - Network Management UI Screen
// ============================================================================
// WiFi network management dashboard for Tab5.encoder.
// Allows users to:
//   - View saved networks
//   - Connect to saved networks (one-click)
//   - Scan for new networks
//   - Add new networks (with credentials input)
//   - Delete saved networks
//   - View connection status
// ============================================================================

#include <M5GFX.h>
#include <lvgl.h>
#include <cstdint>
#include "../config/Config.h"

#if ENABLE_WIFI
#include "../network/HttpClient.h"
#include "../network/WiFiManager.h"
#endif

// Forward declarations
class ButtonHandler;
class WebSocketClient;
class UIHeader;

/**
 * Connectivity screen state
 */
enum class ConnectivityState : uint8_t {
    IDLE = 0,
    SCANNING = 1,
    CONNECTING = 2,
    ERROR = 3
};

class ConnectivityTab {
public:
    ConnectivityTab(M5GFX& display);
    ~ConnectivityTab();

    void begin(lv_obj_t* parent = nullptr);
    void loop();

    /**
     * Set button handler for navigation
     * @param handler ButtonHandler instance
     */
    void setButtonHandler(ButtonHandler* handler) { _buttonHandler = handler; }
    
    /**
     * Set WebSocket client for status updates
     * @param wsClient WebSocketClient instance
     */
    void setWebSocketClient(WebSocketClient* wsClient) { _wsClient = wsClient; }

    /**
     * Set WiFi manager for connection management
     * @param wifiManager WiFiManager instance
     */
    #if ENABLE_WIFI
    void setWiFiManager(WiFiManager* wifiManager) { _wifiManager = wifiManager; }
    #endif

    /**
     * Set callback for Back button (returns to GLOBAL screen)
     * @param callback Function to call when Back button pressed
     */
    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }
    
    /**
     * Set header instance (shared across screens)
     * @param header UIHeader instance
     */
    void setHeader(UIHeader* header) { _header = header; }

    /**
     * Mark UI as dirty (needs redraw)
     */
    void markDirty() { _pendingDirty = true; }
    
    /**
     * Force immediate dirty state
     */
    void forceDirty() { 
        _dirty = true; 
        _pendingDirty = false; 
        _lastRenderTime = 0;
    }

    /**
     * Handle touch event
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     */
    void handleTouch(int16_t x, int16_t y);

    /**
     * Handle encoder change event
     * @param encoderIndex Encoder index (0-15)
     * @param delta Encoder change delta
     */
    void handleEncoderChange(uint8_t encoderIndex, int32_t delta);

    /**
     * Update connection status
     */
    void updateConnectionStatus();

private:
    M5GFX& _display;
    ButtonHandler* _buttonHandler = nullptr;
    WebSocketClient* _wsClient = nullptr;
    UIHeader* _header = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;

    #if ENABLE_WIFI
    WiFiManager* _wifiManager = nullptr;
    HttpClient* _httpClient = nullptr;  // Lazy initialization to avoid blocking constructor
    #endif

    // State management
    ConnectivityState _state = ConnectivityState::IDLE;
    String _errorMessage;
    uint32_t _lastStatusUpdate = 0;
    static constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 2000;  // 2 seconds

    #if ENABLE_WIFI
    // Network lists
    NetworkEntry _savedNetworks[10];
    uint8_t _savedNetworkCount = 0;
    ScanResult _scannedNetworks[20];
    uint8_t _scannedNetworkCount = 0;
    uint32_t _scanJobId = 0;
    bool _scanInProgress = false;
    #endif

    // Selected network (for connecting/deleting)
    int8_t _selectedNetworkIndex = -1;  // -1 = none selected
    bool _isSavedNetworkSelected = true;  // true = saved, false = scanned

    // Add network dialog state
    bool _showAddDialog = false;
    String _newNetworkSsid;
    String _newNetworkPassword;

    // LVGL widgets
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _backButton = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    lv_obj_t* _savedNetworksList = nullptr;
    lv_obj_t* _scanButton = nullptr;
    lv_obj_t* _scannedNetworksList = nullptr;
    lv_obj_t* _addNetworkButton = nullptr;
    lv_obj_t* _addDialog = nullptr;
    lv_obj_t* _ssidInput = nullptr;
    lv_obj_t* _passwordInput = nullptr;
    lv_obj_t* _connectButton = nullptr;
    lv_obj_t* _deleteButton = nullptr;
    lv_obj_t* _disconnectButton = nullptr;

    // LVGL styles
    lv_style_t _styleNormal;
    lv_style_t _styleSelected;
    lv_style_t _styleError;

    // Rendering state
    bool _dirty = true;
    bool _pendingDirty = false;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 100;  // 10 FPS for network UI

    // Layout constants (optimized for 1280x720)
    static constexpr int STATUS_Y = 80;  // Below header
    static constexpr int SAVED_LIST_Y = 120;
    static constexpr int SAVED_LIST_H = 200;
    static constexpr int SCAN_BUTTON_Y = 340;
    static constexpr int SCANNED_LIST_Y = 380;
    static constexpr int SCANNED_LIST_H = 240;
    static constexpr int ACTIONS_Y = 640;

    // Private helper methods
    void initStyles();
    void createInteractiveUI(lv_obj_t* parent);
    void createBackButton(lv_obj_t* parent);
    void createStatusLabel(lv_obj_t* parent);
    void createSavedNetworksList(lv_obj_t* parent);
    void createScanButton(lv_obj_t* parent);
    void createScannedNetworksList(lv_obj_t* parent);
    void createAddNetworkButton(lv_obj_t* parent);
    void createAddNetworkDialog(lv_obj_t* parent);
    void createActionButtons(lv_obj_t* parent);

    // Update methods
    void updateSavedNetworksList();
    void updateScannedNetworksList();
    void updateStatusLabel();
    void refreshNetworkLists();

    // Network operations
    void startScan();
    void checkScanStatus();
    void connectToSelectedNetwork();
    void addNewNetwork();
    void deleteSelectedNetwork();
    void disconnectFromNetwork();

    // LVGL event callbacks (static)
    static void backButtonCb(lv_event_t* e);
    static void scanButtonCb(lv_event_t* e);
    static void addNetworkButtonCb(lv_event_t* e);
    static void savedNetworkSelectedCb(lv_event_t* e);
    static void scannedNetworkSelectedCb(lv_event_t* e);
    static void connectButtonCb(lv_event_t* e);
    static void deleteButtonCb(lv_event_t* e);
    static void disconnectButtonCb(lv_event_t* e);
    static void addDialogSaveCb(lv_event_t* e);
    static void addDialogCancelCb(lv_event_t* e);
};
