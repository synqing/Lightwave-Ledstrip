#pragma once
// ============================================================================
// DeviceSelectorTab - Multi-Device Selector UI Screen
// ============================================================================
// Device management dashboard for Tab5.encoder.
// Allows users to:
//   - Scan for LightwaveOS devices on the local network
//   - View discovered and saved devices
//   - Select a device for control
//   - Manually add devices by IP address
//   - Remove (forget) saved devices
//   - View device status (verified, reachable, RSSI)
// ============================================================================

#include <M5GFX.h>
#include <lvgl.h>
#include <cstdint>
#include "../config/Config.h"

#if ENABLE_WIFI
#include "../network/HttpClient.h"
#include "../network/DeviceRegistry.h"
#endif

// Forward declarations
class ButtonHandler;
class WebSocketClient;
class UIHeader;

/**
 * Device selector screen state
 */
enum class DeviceSelectorState : uint8_t {
    IDLE = 0,
    SCANNING = 1,
    CONNECTING = 2,
    ERROR = 3
};

class DeviceSelectorTab {
public:
    DeviceSelectorTab(M5GFX& display);
    ~DeviceSelectorTab();

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
     * Set device registry for device management
     * @param registry DeviceRegistry instance
     */
    #if ENABLE_WIFI
    void setDeviceRegistry(DeviceRegistry* registry) { _deviceRegistry = registry; }
    #endif

    /**
     * Set HTTP client for network discovery
     * @param client HttpClient instance
     */
    #if ENABLE_WIFI
    void setHttpClient(HttpClient* client) { _httpClient = client; }
    #endif

    /**
     * Set callback for Back button (returns to GLOBAL screen)
     * @param callback Function to call when Back button pressed
     */
    typedef void (*BackButtonCallback)();
    void setBackButtonCallback(BackButtonCallback callback) { _backButtonCallback = callback; }

    /**
     * Set callback for device selection
     * @param callback Function to call when a device is selected
     */
    typedef void (*DeviceSelectedCallback)(const char* ip, uint16_t port);
    void setDeviceSelectedCallback(DeviceSelectedCallback callback) { _deviceSelectedCallback = callback; }

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
     * Update connection status display
     * @param connectedIP IP address of connected device (nullptr to auto-detect)
     */
    void updateConnectionStatus(const char* connectedIP = nullptr);

private:
    M5GFX& _display;
    ButtonHandler* _buttonHandler = nullptr;
    WebSocketClient* _wsClient = nullptr;
    UIHeader* _header = nullptr;
    BackButtonCallback _backButtonCallback = nullptr;
    DeviceSelectedCallback _deviceSelectedCallback = nullptr;

    #if ENABLE_WIFI
    DeviceRegistry* _deviceRegistry = nullptr;
    HttpClient* _httpClient = nullptr;
    bool _ownsHttpClient = false;  // True if we allocated _httpClient
    #endif

    // State management
    DeviceSelectorState _state = DeviceSelectorState::IDLE;
    String _errorMessage;
    uint32_t _lastStatusUpdate = 0;
    static constexpr uint32_t STATUS_UPDATE_INTERVAL_MS = 2000;  // 2 seconds

    #if ENABLE_WIFI
    // Scan state
    uint32_t _scanStartMs = 0;
    bool _scanInProgress = false;

    // DEFERRED LOADING: Prevents watchdog crash from blocking HTTP in begin()
    bool _needsInitialLoad = false;
    bool _initialLoadAwaitingDiscovery = false;
    #endif

    // Selection state
    int8_t _selectedDiscoveredIndex = -1;  // -1 = none selected
    int8_t _selectedSavedIndex = -1;       // -1 = none selected
    bool _discoveredListHasFocus = true;   // Which list has encoder focus

    // LVGL widgets
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _backButton = nullptr;
    lv_obj_t* _statusLabel = nullptr;

    // Left card: Discovered Devices
    lv_obj_t* _discoveredCard = nullptr;
    lv_obj_t* _discoveredDevicesList = nullptr;

    // Center button column
    lv_obj_t* _scanButton = nullptr;
    lv_obj_t* _scanButtonLabel = nullptr;
    lv_obj_t* _selectButton = nullptr;
    lv_obj_t* _forgetButton = nullptr;

    // Right card: Saved Devices
    lv_obj_t* _savedCard = nullptr;
    lv_obj_t* _savedDevicesList = nullptr;

    // Bottom manual entry bar
    lv_obj_t* _manualEntryBar = nullptr;
    lv_obj_t* _ipInput = nullptr;
    lv_obj_t* _addButton = nullptr;
    lv_obj_t* _keyboard = nullptr;

    // LVGL styles
    lv_style_t _styleNormal;
    lv_style_t _styleSelected;
    lv_style_t _styleError;

    // Rendering state
    bool _dirty = true;
    bool _pendingDirty = false;
    uint32_t _lastRenderTime = 0;
    static constexpr uint32_t FRAME_INTERVAL_MS = 100;   // 10 FPS for device UI
    static constexpr uint32_t SCAN_TIMEOUT_MS = 15000;   // Avoid stuck scan

    // ==========================================================================
    // Layout constants (optimized for 1280x720)
    // ==========================================================================
    // Layout: Discovered Devices (left) | Buttons (center) | Saved Devices (right)
    //         Manual IP Entry (bottom bar)
    // ==========================================================================
    static constexpr int STATUS_Y = 80;  // Below header

    // Left card: Discovered Devices
    static constexpr int DISCOVERED_CARD_X = 20;
    static constexpr int DISCOVERED_CARD_Y = 120;
    static constexpr int DISCOVERED_CARD_W = 480;
    static constexpr int DISCOVERED_CARD_H = 420;
    static constexpr int DEVICE_LIST_H = 370;  // Inner list height

    // Center button column
    static constexpr int BUTTON_COLUMN_X = 520;
    static constexpr int BUTTON_W = 180;
    static constexpr int BUTTON_H = 50;
    static constexpr int BUTTON_GAP = 10;

    // Right card: Saved Devices
    static constexpr int SAVED_CARD_X = 720;
    static constexpr int SAVED_CARD_Y = 120;
    static constexpr int SAVED_CARD_W = 520;
    static constexpr int SAVED_CARD_H = 420;

    // Bottom manual entry bar
    static constexpr int MANUAL_BAR_X = 20;
    static constexpr int MANUAL_BAR_Y = 560;
    static constexpr int MANUAL_BAR_W = 1220;
    static constexpr int MANUAL_BAR_H = 60;

    // Device item internal layout
    static constexpr int ITEM_H = 60;        // Item height (touch-friendly)
    static constexpr int ITEM_NAME_W = 180;   // Name/hostname zone width
    static constexpr int ITEM_IP_W = 140;     // IP address zone width
    static constexpr int ITEM_RSSI_W = 80;    // RSSI zone width
    static constexpr int ITEM_DOT_W = 40;     // Status dot zone width

    // Private helper methods
    void initStyles();
    void createInteractiveUI(lv_obj_t* parent);
    void createBackButton(lv_obj_t* parent);
    void createStatusLabel(lv_obj_t* parent);
    void createDiscoveredDevicesCard(lv_obj_t* parent);
    void createCenterButtons(lv_obj_t* parent);
    void createSavedDevicesCard(lv_obj_t* parent);
    void createManualEntryBar(lv_obj_t* parent);
    void createKeyboard(lv_obj_t* parent);

    /**
     * Factory function for creating device list items with multi-zone layout
     * @param parent Parent list container
     * @param name Display name (hostname or friendly name)
     * @param ip IP address string
     * @param rssi Signal strength (INT32_MIN = unknown)
     * @param isConnected True if this is the currently selected device
     * @param isSelected True if this device is highlighted by user
     * @param isVerified True if confirmed as LightwaveOS device
     * @param index Index in the device array
     * @param isDiscoveredList True if in discovered list, false if saved list
     */
    lv_obj_t* createDeviceItem(
        lv_obj_t* parent,
        const char* name,
        const char* ip,
        int32_t rssi,
        bool isConnected,
        bool isSelected,
        bool isVerified,
        int index,
        bool isDiscoveredList
    );

    // Update methods
    void updateDiscoveredDevicesList();
    void updateSavedDevicesList();
    void updateStatusLabel();
    void refreshDeviceLists();

    // Device operations
    void startScan();
    void selectDevice();
    void forgetDevice();
    void addManualDevice();

    // LVGL event callbacks (static)
    static void backButtonCb(lv_event_t* e);
    static void scanButtonCb(lv_event_t* e);
    static void selectButtonCb(lv_event_t* e);
    static void forgetButtonCb(lv_event_t* e);
    static void addButtonCb(lv_event_t* e);
    static void discoveredDeviceSelectedCb(lv_event_t* e);
    static void savedDeviceSelectedCb(lv_event_t* e);
};
