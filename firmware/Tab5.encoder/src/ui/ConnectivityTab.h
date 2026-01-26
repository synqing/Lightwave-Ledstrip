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
    bool _scanAwaitingDiscovery = false;
    bool _initialLoadAwaitingDiscovery = false;

    // DEFERRED LOADING: Prevents watchdog crash from blocking HTTP in begin()
    // HTTP calls (mDNS resolution) can block for 5+ seconds, triggering WDT
    // Set in begin(), processed in loop() with proper watchdog resets
    bool _needsInitialLoad = false;
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
    lv_obj_t* _scanButtonLabel = nullptr;  // For changing text to "SCANNING..."
    lv_obj_t* _scannedNetworksList = nullptr;
    lv_obj_t* _addNetworkButton = nullptr;
    lv_obj_t* _addDialog = nullptr;
    lv_obj_t* _ssidInput = nullptr;
    lv_obj_t* _passwordInput = nullptr;
    lv_obj_t* _connectButton = nullptr;
    lv_obj_t* _deleteButton = nullptr;
    lv_obj_t* _disconnectButton = nullptr;
    lv_obj_t* _keyboard = nullptr;  // Virtual keyboard for text input

    // LVGL styles
    lv_style_t _styleNormal;
    lv_style_t _styleSelected;
    lv_style_t _styleError;

    // Rendering state
    bool _dirty = true;
    bool _pendingDirty = false;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 100;  // 10 FPS for network UI

    // ==========================================================================
    // Layout constants (optimized for 1280x720) - REDESIGNED
    // ==========================================================================
    // Layout: Available Networks (top) -> Saved Networks (bottom)
    // Buttons on right side of their respective cards
    // ==========================================================================
    static constexpr int STATUS_Y = 80;  // Below header

    // Card dimensions (REDUCED by 1/3 for better proportions)
    static constexpr int NETWORK_CARD_X = 20;
    static constexpr int NETWORK_CARD_W = 533;   // Was 800, reduced by 1/3
    static constexpr int NETWORK_CARD_H = 260;
    static constexpr int NETWORK_LIST_H = 210;   // Inner list height

    // Button column (positioned after card with 20px gap)
    static constexpr int BUTTON_COLUMN_X = 573;  // 20 + 533 + 20 gap
    static constexpr int BUTTON_W = 180;         // Sized for "DISCONNECT"
    static constexpr int BUTTON_H = 50;
    static constexpr int BUTTON_GAP = 10;

    // Available Networks section (TOP)
    static constexpr int AVAILABLE_Y = 120;

    // Saved Networks section (BELOW Available)
    static constexpr int SAVED_Y = 400;

    // Network item internal layout (3-zone structure for narrower card)
    static constexpr int ITEM_H = 48;       // Item height (touch-friendly)
    static constexpr int ITEM_SSID_W = 310; // SSID zone width (reduced for 533px card)
    static constexpr int ITEM_RSSI_W = 100; // RSSI zone width
    static constexpr int ITEM_DOT_W = 40;   // Selection dot zone width

    // Private helper methods
    void initStyles();
    void createInteractiveUI(lv_obj_t* parent);
    void createBackButton(lv_obj_t* parent);
    void createStatusLabel(lv_obj_t* parent);
    void createAvailableNetworksCard(lv_obj_t* parent);  // Renamed: scanned -> available
    void createAvailableNetworksButtons(lv_obj_t* parent);  // SCAN, ADD buttons
    void createSavedNetworksCard(lv_obj_t* parent);
    void createSavedNetworksButtons(lv_obj_t* parent);  // CONNECT, DISCONNECT, DELETE
    void createAddNetworkDialog(lv_obj_t* parent);
    void performScanRequest();

    /**
     * Factory function for creating network list items with 3-zone layout
     * @param parent Parent list container
     * @param ssid Network SSID
     * @param rssi Signal strength (-1 for saved networks without RSSI)
     * @param isConnected True if this is the currently connected network
     * @param isSelected True if this network is selected by user
     * @param index Index in the network array
     * @param isSavedNetwork True if this is a saved network, false if scanned
     */
    lv_obj_t* createNetworkItem(
        lv_obj_t* parent,
        const char* ssid,
        int rssi,
        bool isConnected,
        bool isSelected,
        int index,
        bool isSavedNetwork
    );

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
