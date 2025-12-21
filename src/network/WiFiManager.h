#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include "../config/network_config.h"

/**
 * WiFiManager - Non-blocking WiFi management with FreeRTOS task
 * 
 * Features:
 * - Runs on Core 0 to avoid blocking main loop
 * - Event-driven architecture  
 * - Parallel Soft-AP fallback
 * - Cached channel scanning
 * - Automatic reconnection with backoff
 */
class WiFiManager {
public:
    // WiFi states
    enum WiFiState {
        STATE_WIFI_INIT,
        STATE_WIFI_SCANNING,
        STATE_WIFI_CONNECTING,
        STATE_WIFI_CONNECTED,
        STATE_WIFI_FAILED,
        STATE_WIFI_AP_MODE,
        STATE_WIFI_DISCONNECTED
    };
    
    // WiFi events (bit flags for event group)
    static constexpr uint32_t EVENT_SCAN_COMPLETE = BIT0;
    static constexpr uint32_t EVENT_CONNECTED = BIT1;
    static constexpr uint32_t EVENT_DISCONNECTED = BIT2;
    static constexpr uint32_t EVENT_GOT_IP = BIT3;
    static constexpr uint32_t EVENT_CONNECTION_FAILED = BIT4;
    static constexpr uint32_t EVENT_AP_START = BIT5;
    static constexpr uint32_t EVENT_AP_STACONNECTED = BIT6;
    
private:
    // Task configuration
    static constexpr size_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 1;
    static constexpr BaseType_t TASK_CORE = 0;  // Run on Core 0
    
    // Timing configuration
    static constexpr uint32_t SCAN_INTERVAL_MS = 60000;     // Re-scan every minute
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;   // 10s connection timeout
    static constexpr uint32_t RECONNECT_DELAY_MS = 5000;    // 5s between reconnect attempts
    static constexpr uint32_t MAX_RECONNECT_DELAY_MS = 60000; // Max 1 minute backoff
    
    // Instance variables
    WiFiState currentState = STATE_WIFI_INIT;
    TaskHandle_t wifiTaskHandle = nullptr;
    EventGroupHandle_t wifiEventGroup = nullptr;
    SemaphoreHandle_t stateMutex = nullptr;
    
    // Connection parameters
    String ssid;
    String password;
    String ssid2;
    String password2;
    uint8_t currentNetworkIndex = 0;     // 0 = primary, 1 = secondary
    uint8_t attemptsOnCurrentNetwork = 0;
    bool useStaticIP = false;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
    
    // Cached scan results
    struct ScanResult {
        String ssid;
        int32_t rssi;
        uint8_t channel;
        uint8_t bssid[6];
        wifi_auth_mode_t encryption;
    };
    std::vector<ScanResult> cachedScanResults;
    uint32_t lastScanTime = 0;
    uint8_t bestChannel = 0;
    
    // Connection statistics
    uint32_t connectionAttempts = 0;
    uint32_t successfulConnections = 0;
    uint32_t lastConnectionTime = 0;
    uint32_t reconnectDelay = RECONNECT_DELAY_MS;
    
    // Soft-AP configuration
    bool apEnabled = false;
    String apSSID = "LightwaveOS-AP";
    String apPassword = "lightwave123";
    uint8_t apChannel = 1;
    
    // Singleton instance
    static WiFiManager* instance;
    
    // Private constructor for singleton
    WiFiManager() = default;
    
    // FreeRTOS task function
    static void wifiTask(void* parameter);
    
    // State machine handlers
    void handleStateInit();
    void handleStateScanning();
    void handleStateConnecting();
    void handleStateConnected();
    void handleStateFailed();
    void handleStateAPMode();
    void handleStateDisconnected();
    
    // Helper functions
    void performAsyncScan();
    bool connectToAP();
    void startSoftAP();
    void updateBestChannel();
    bool isChannelCongested(uint8_t channel);
    void setState(WiFiState newState);
    void switchToNextNetwork();
    bool hasSecondaryNetwork() const;
    
    // Event handlers
    static void onWiFiEvent(WiFiEvent_t event);
    
public:
    // Singleton access
    static WiFiManager& getInstance() {
        if (!instance) {
            instance = new WiFiManager();
        }
        return *instance;
    }
    
    // Delete copy constructor and assignment operator
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    
    // Public API
    bool begin();
    void stop();
    
    // Configuration
    void setCredentials(const String& ssid, const String& password);
    void setStaticIP(const IPAddress& ip, const IPAddress& gateway, 
                     const IPAddress& subnet, const IPAddress& dns1, 
                     const IPAddress& dns2);
    void enableSoftAP(const String& ssid, const String& password, uint8_t channel = 1);
    
    // Status
    WiFiState getState() const;
    bool isConnected() const { return currentState == STATE_WIFI_CONNECTED; }
    bool isAPMode() const { return currentState == STATE_WIFI_AP_MODE; }
    String getStateString() const;
    
    // Network info
    IPAddress getLocalIP() const { return WiFi.localIP(); }
    IPAddress getAPIP() const { return WiFi.softAPIP(); }
    String getSSID() const { return WiFi.SSID(); }
    int32_t getRSSI() const { return WiFi.RSSI(); }
    uint8_t getChannel() const { return WiFi.channel(); }
    
    // Scan results
    const std::vector<ScanResult>& getScanResults() const { return cachedScanResults; }
    uint32_t getLastScanTime() const { return lastScanTime; }
    
    // Statistics
    uint32_t getConnectionAttempts() const { return connectionAttempts; }
    uint32_t getSuccessfulConnections() const { return successfulConnections; }
    uint32_t getUptimeSeconds() const;
    
    // Manual control
    void disconnect();
    void reconnect();
    void scanNetworks();
};