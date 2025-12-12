#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include "../config/network_config.h"

/**
 * WiFiManagerV2 - Enhanced Non-blocking WiFi Management
 * 
 * Key improvements over V1:
 * - Immediate AP fallback on first connection failure
 * - Adaptive TX power based on RSSI
 * - Exponential backoff for retries
 * - Event callbacks for state changes
 * - Better integration with WiFiOptimizerPro features
 * - Configurable retry policies
 * 
 * Runs entirely on Core 0 to avoid blocking LED operations
 */
class WiFiManagerV2 {
    // Friend function for event handling
    friend void WiFiEventHandler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data);
public:
    // WiFi states
    enum WiFiState {
        STATE_INIT,
        STATE_SCANNING,
        STATE_CONNECTING,
        STATE_CONNECTED,
        STATE_CONNECTION_FAILED,
        STATE_AP_MODE,
        STATE_DISCONNECTED,
        STATE_AP_STA_MODE  // Simultaneous AP + STA mode
    };
    
    // Event types for callbacks
    enum WiFiEvent {
        EVENT_STATE_CHANGED,
        EVENT_SCAN_COMPLETE,
        EVENT_CONNECTED,
        EVENT_DISCONNECTED,
        EVENT_AP_STARTED,
        EVENT_AP_CLIENT_CONNECTED,
        EVENT_AP_CLIENT_DISCONNECTED,
        EVENT_CONNECTION_RETRY
    };
    
    // Event callback signature
    using EventCallback = std::function<void(WiFiEvent event, void* data)>;
    
    // Scan result structure
    struct ScanResult {
        String ssid;
        int32_t rssi;
        uint8_t channel;
        uint8_t bssid[6];
        wifi_auth_mode_t encryption;
        bool supports_11n;
        bool supports_11lr;
    };
    
    // Connection statistics
    struct ConnectionStats {
        uint32_t attempts = 0;
        uint32_t successes = 0;
        uint32_t failures = 0;
        uint32_t total_uptime_ms = 0;
        uint32_t current_session_start = 0;
        int32_t best_rssi = -100;
        int32_t worst_rssi = 0;
        float average_rssi = 0;
    };
    
private:
    // Task configuration
    static constexpr size_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 2;  // Higher priority than V1
    static constexpr BaseType_t TASK_CORE = 0;
    
    // Timing configuration  
    static constexpr uint32_t SCAN_INTERVAL_MS = 30000;      // Scan every 30s when connected
    static constexpr uint32_t QUICK_SCAN_INTERVAL_MS = 5000; // Quick scan when disconnected
    static constexpr uint32_t INITIAL_CONNECT_TIMEOUT_MS = 8000;
    static constexpr uint32_t MIN_RETRY_DELAY_MS = 1000;
    static constexpr uint32_t MAX_RETRY_DELAY_MS = 60000;
    static constexpr uint32_t AP_FALLBACK_DELAY_MS = 5000;   // Start AP after 5s of failed connection
    
    // Adaptive TX power levels (in 0.25 dBm units)
    static constexpr int8_t TX_POWER_MIN = 8 * 4;   // 8 dBm
    static constexpr int8_t TX_POWER_MED = 14 * 4;  // 14 dBm  
    static constexpr int8_t TX_POWER_MAX = 20 * 4;  // 20 dBm
    
    // Instance variables
    WiFiState currentState = STATE_INIT;
    WiFiState previousState = STATE_INIT;
    TaskHandle_t wifiTaskHandle = nullptr;
    EventGroupHandle_t wifiEventGroup = nullptr;
    SemaphoreHandle_t stateMutex = nullptr;
    QueueHandle_t commandQueue = nullptr;
    
    // Connection configuration
    String targetSSID;
    String targetPassword;
    bool useStaticIP = false;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
    
    // AP configuration
    bool apEnabled = false;
    bool apAutoFallback = true;
    String apSSID = "LightwaveOS";
    String apPassword = "lightwave123";
    uint8_t apChannel = 6;
    uint8_t maxAPClients = 4;
    
    // Scan cache
    std::vector<ScanResult> scanResults;
    uint32_t lastScanTime = 0;
    bool scanInProgress = false;
    uint8_t preferredChannel = 0;
    
    // Connection management
    uint32_t connectionStartTime = 0;
    uint32_t lastConnectionAttempt = 0;
    uint32_t currentRetryDelay = MIN_RETRY_DELAY_MS;
    uint8_t consecutiveFailures = 0;
    bool immediateAPFallback = true;
    
    // Adaptive TX power
    int8_t currentTxPower = TX_POWER_MAX;
    uint32_t lastTxPowerUpdate = 0;
    
    // Statistics
    ConnectionStats stats;
    
    // Event callbacks
    std::vector<EventCallback> eventCallbacks;
    
    // Singleton
    static WiFiManagerV2* instance;
    WiFiManagerV2() = default;
    
    // Task functions
    static void wifiTask(void* parameter);
    void runStateMachine();
    
    // State handlers
    void handleInit();
    void handleScanning();
    void handleConnecting();
    void handleConnected();
    void handleConnectionFailed();
    void handleAPMode();
    void handleDisconnected();
    void handleAPSTAMode();
    
    // Helper functions
    void startAsyncScan();
    void processScanResults();
    bool attemptConnection();
    void startAPInternal();
    void stopAPInternal();
    void updateAdaptiveTxPower();
    void calculateRetryDelay();
    uint8_t selectBestChannel();
    void notifyEvent(WiFiEvent event, void* data = nullptr);
    
    // Command queue messages
    enum Command {
        CMD_CONNECT,
        CMD_DISCONNECT,
        CMD_SCAN,
        CMD_START_AP,
        CMD_STOP_AP,
        CMD_RESET
    };
    
    struct CommandMessage {
        Command cmd;
        void* data;
    };
    
public:
    // Singleton access
    static WiFiManagerV2& getInstance() {
        if (!instance) {
            instance = new WiFiManagerV2();
        }
        return *instance;
    }
    
    // Disable copy
    WiFiManagerV2(const WiFiManagerV2&) = delete;
    WiFiManagerV2& operator=(const WiFiManagerV2&) = delete;
    
    // Lifecycle
    bool begin();
    void stop();
    
    // Configuration
    void setCredentials(const String& ssid, const String& password);
    void setStaticIP(const IPAddress& ip, const IPAddress& gw, 
                     const IPAddress& mask, const IPAddress& dns1, 
                     const IPAddress& dns2 = IPAddress(0,0,0,0));
    void configureAP(const String& ssid, const String& password, 
                     uint8_t channel = 0, uint8_t maxClients = 4);
    void setImmediateAPFallback(bool enable) { immediateAPFallback = enable; }
    void setAPAutoFallback(bool enable) { apAutoFallback = enable; }
    
    // Control
    void connect();
    void disconnect();
    void scan();
    void startAP();
    void stopAP();
    void reset();
    
    // Status
    WiFiState getState() const;
    String getStateString() const;
    bool isConnected() const { return currentState == STATE_CONNECTED; }
    bool isScanning() const { return scanInProgress; }
    bool isAPActive() const { return apEnabled; }
    
    // Network info
    String getSSID() const { return WiFi.SSID(); }
    IPAddress getLocalIP() const { return WiFi.localIP(); }
    IPAddress getAPIP() const { return WiFi.softAPIP(); }
    int32_t getRSSI() const { return WiFi.RSSI(); }
    uint8_t getChannel() const { return WiFi.channel(); }
    int8_t getTxPower() const { return currentTxPower / 4; }  // Return in dBm
    
    // Scan results
    const std::vector<ScanResult>& getScanResults() const { return scanResults; }
    uint32_t getTimeSinceLastScan() const { return millis() - lastScanTime; }
    
    // Statistics
    const ConnectionStats& getStats() const { return stats; }
    uint32_t getUptime() const;
    float getSuccessRate() const;
    
    // Event handling
    void addEventListener(EventCallback callback) { eventCallbacks.push_back(callback); }
    void removeEventListeners() { eventCallbacks.clear(); }
    
    // Advanced features
    void enable80211LR(bool enable);
    void setTxPowerMode(uint8_t mode); // 0=Auto, 1=Min, 2=Med, 3=Max
    void optimizeForLEDCoexistence();
};