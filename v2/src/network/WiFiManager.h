/**
 * @file WiFiManager.h
 * @brief Non-blocking WiFi management with FreeRTOS task for LightwaveOS v2
 *
 * Features:
 * - Runs on Core 0 to avoid blocking LED rendering on Core 1
 * - Event-driven state machine architecture
 * - Parallel Soft-AP fallback when WiFi fails
 * - Cached channel scanning for faster reconnection
 * - Automatic reconnection with exponential backoff (5s -> 60s max)
 * - Connection statistics tracking
 *
 * Architecture:
 * - WiFiManager task runs on Core 0 with WiFi stack
 * - Main loop/RendererActor runs on Core 1
 * - State changes are thread-safe via mutex
 *
 * Usage:
 * @code
 * // In setup()
 * WIFI_MANAGER.setCredentials(ssid, password);
 * WIFI_MANAGER.enableSoftAP("LightwaveOS-Setup", "lightwave123");
 * if (!WIFI_MANAGER.begin()) {
 *     Serial.println("WiFiManager failed!");
 * }
 *
 * // Wait for connection or AP mode
 * while (!WIFI_MANAGER.isConnected() && !WIFI_MANAGER.isAPMode()) {
 *     delay(100);
 * }
 * @endcode
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include "../config/network_config.h"

namespace lightwaveos {
namespace network {

/**
 * @brief Non-blocking WiFi management with FreeRTOS task
 *
 * Singleton pattern ensures only one WiFiManager exists.
 * All operations are thread-safe for multi-core access.
 */
class WiFiManager {
public:
    // ========================================================================
    // WiFi States
    // ========================================================================

    /**
     * @brief WiFi connection state machine states
     */
    enum WiFiState {
        STATE_WIFI_INIT,         ///< Initial state, checking credentials
        STATE_WIFI_SCANNING,     ///< Scanning for networks
        STATE_WIFI_CONNECTING,   ///< Attempting to connect
        STATE_WIFI_CONNECTED,    ///< Successfully connected to AP
        STATE_WIFI_FAILED,       ///< Connection failed, will retry
        STATE_WIFI_AP_MODE,      ///< Running in Soft-AP mode
        STATE_WIFI_DISCONNECTED  ///< Disconnected, will reconnect
    };

    // ========================================================================
    // Event Flags (for FreeRTOS event group)
    // ========================================================================

    static constexpr uint32_t EVENT_SCAN_COMPLETE     = BIT0;
    static constexpr uint32_t EVENT_CONNECTED         = BIT1;
    static constexpr uint32_t EVENT_DISCONNECTED      = BIT2;
    static constexpr uint32_t EVENT_GOT_IP            = BIT3;
    static constexpr uint32_t EVENT_CONNECTION_FAILED = BIT4;
    static constexpr uint32_t EVENT_AP_START          = BIT5;
    static constexpr uint32_t EVENT_AP_STACONNECTED   = BIT6;

    // ========================================================================
    // Singleton Access
    // ========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the WiFiManager instance
     */
    static WiFiManager& getInstance() {
        if (!s_instance) {
            s_instance = new WiFiManager();
        }
        return *s_instance;
    }

    // Delete copy constructor and assignment operator
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Start WiFi management task
     *
     * Creates FreeRTOS task on Core 0 for non-blocking WiFi management.
     * Call this before starting WebServer.
     *
     * @return true if task created successfully
     */
    bool begin();

    /**
     * @brief Stop WiFi management task
     *
     * Deletes the FreeRTOS task and disconnects WiFi.
     */
    void stop();

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set WiFi credentials (primary network)
     * @param ssid Network SSID
     * @param password Network password
     * 
     * Also loads secondary network from NetworkConfig if available.
     */
    void setCredentials(const String& ssid, const String& password);

    /**
     * @brief Configure static IP (optional)
     * @param ip Static IP address
     * @param gateway Gateway address
     * @param subnet Subnet mask
     * @param dns1 Primary DNS
     * @param dns2 Secondary DNS
     */
    void setStaticIP(const IPAddress& ip, const IPAddress& gateway,
                     const IPAddress& subnet, const IPAddress& dns1,
                     const IPAddress& dns2);

    /**
     * @brief Enable Soft-AP fallback mode
     * @param ssid AP SSID
     * @param password AP password (min 8 chars)
     * @param channel WiFi channel (1-13)
     */
    void enableSoftAP(const String& ssid, const String& password, uint8_t channel = 1);

    // ========================================================================
    // Status
    // ========================================================================

    /**
     * @brief Get current WiFi state
     * @return Current WiFiState
     */
    WiFiState getState() const;

    /**
     * @brief Check if connected to WiFi AP
     * @return true if connected
     */
    bool isConnected() const { return m_currentState == STATE_WIFI_CONNECTED; }

    /**
     * @brief Check if running in AP mode
     * @return true if AP mode active
     */
    bool isAPMode() const { return m_currentState == STATE_WIFI_AP_MODE; }

    /**
     * @brief Get human-readable state string
     * @return State name as string
     */
    String getStateString() const;

    // ========================================================================
    // Network Info
    // ========================================================================

    /**
     * @brief Get local IP address (STA mode)
     * @return IP address
     */
    IPAddress getLocalIP() const { return WiFi.localIP(); }

    /**
     * @brief Get AP IP address (AP mode)
     * @return AP IP address (usually 192.168.4.1)
     */
    IPAddress getAPIP() const { return WiFi.softAPIP(); }

    /**
     * @brief Get connected SSID
     * @return SSID string
     */
    String getSSID() const { return WiFi.SSID(); }

    /**
     * @brief Get current RSSI
     * @return Signal strength in dBm
     */
    int32_t getRSSI() const { return WiFi.RSSI(); }

    /**
     * @brief Get current WiFi channel
     * @return Channel number (1-13)
     */
    uint8_t getChannel() const { return WiFi.channel(); }

    // ========================================================================
    // Scan Results
    // ========================================================================

    /**
     * @brief Network scan result
     */
    struct ScanResult {
        String ssid;                    ///< Network SSID
        int32_t rssi;                   ///< Signal strength (dBm)
        uint8_t channel;                ///< WiFi channel
        uint8_t bssid[6];               ///< MAC address
        wifi_auth_mode_t encryption;    ///< Encryption type
    };

    /**
     * @brief Get cached scan results
     * @return Vector of scan results
     */
    const std::vector<ScanResult>& getScanResults() const { return m_cachedScanResults; }

    /**
     * @brief Get timestamp of last scan
     * @return millis() value of last scan
     */
    uint32_t getLastScanTime() const { return m_lastScanTime; }

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Get total connection attempts
     * @return Number of connection attempts
     */
    uint32_t getConnectionAttempts() const { return m_connectionAttempts; }

    /**
     * @brief Get successful connection count
     * @return Number of successful connections
     */
    uint32_t getSuccessfulConnections() const { return m_successfulConnections; }

    /**
     * @brief Get uptime since last connection
     * @return Uptime in seconds
     */
    uint32_t getUptimeSeconds() const;

    // ========================================================================
    // Manual Control
    // ========================================================================

    /**
     * @brief Manually disconnect from WiFi
     */
    void disconnect();

    /**
     * @brief Force reconnection attempt
     */
    void reconnect();

    /**
     * @brief Trigger network scan
     */
    void scanNetworks();

private:
    // ========================================================================
    // Private Constructor (Singleton)
    // ========================================================================

    WiFiManager() = default;

    // ========================================================================
    // Task Configuration
    // ========================================================================

    static constexpr size_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 1;
    static constexpr BaseType_t TASK_CORE = 0;  // Run on Core 0 with WiFi stack

    // ========================================================================
    // Timing Configuration
    // ========================================================================

    static constexpr uint32_t SCAN_INTERVAL_MS = 60000;       // Re-scan every minute
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;     // 10s connection timeout
    static constexpr uint32_t RECONNECT_DELAY_MS = 5000;      // 5s between reconnect attempts
    static constexpr uint32_t MAX_RECONNECT_DELAY_MS = 60000; // Max 1 minute backoff

    // ========================================================================
    // FreeRTOS Task
    // ========================================================================

    static void wifiTask(void* parameter);

    // ========================================================================
    // State Machine Handlers
    // ========================================================================

    void handleStateInit();
    void handleStateScanning();
    void handleStateConnecting();
    void handleStateConnected();
    void handleStateFailed();
    void handleStateAPMode();
    void handleStateDisconnected();

    // ========================================================================
    // Helper Functions
    // ========================================================================

    void performAsyncScan();
    bool connectToAP();
    void startSoftAP();
    void updateBestChannel();
    bool isChannelCongested(uint8_t channel);
    void setState(WiFiState newState);
    void switchToNextNetwork();
    bool hasSecondaryNetwork() const;

    // ========================================================================
    // Event Handler
    // ========================================================================

    static void onWiFiEvent(WiFiEvent_t event);

    // ========================================================================
    // Singleton Instance
    // ========================================================================

    static WiFiManager* s_instance;

    // ========================================================================
    // FreeRTOS Handles
    // ========================================================================

    TaskHandle_t m_wifiTaskHandle = nullptr;
    EventGroupHandle_t m_wifiEventGroup = nullptr;
    SemaphoreHandle_t m_stateMutex = nullptr;

    // ========================================================================
    // State
    // ========================================================================

    WiFiState m_currentState = STATE_WIFI_INIT;

    // State handler flags (reset on state entry to avoid persistence bugs)
    bool m_scanStarted = false;           ///< Reset when entering SCANNING
    bool m_connectStarted = false;        ///< Reset when entering CONNECTING
    uint32_t m_connectStartTime = 0;      ///< Reset when entering CONNECTING

    // ========================================================================
    // Connection Parameters
    // ========================================================================

    String m_ssid;
    String m_password;
    String m_ssid2;
    String m_password2;
    uint8_t m_currentNetworkIndex = 0;     // 0 = primary, 1 = secondary
    uint8_t m_attemptsOnCurrentNetwork = 0;
    bool m_useStaticIP = false;
    IPAddress m_staticIP;
    IPAddress m_gateway;
    IPAddress m_subnet;
    IPAddress m_dns1;
    IPAddress m_dns2;

    // ========================================================================
    // Scan Cache
    // ========================================================================

    std::vector<ScanResult> m_cachedScanResults;
    uint32_t m_lastScanTime = 0;
    uint8_t m_bestChannel = 0;

    // ========================================================================
    // Statistics
    // ========================================================================

    uint32_t m_connectionAttempts = 0;
    uint32_t m_successfulConnections = 0;
    uint32_t m_lastConnectionTime = 0;
    uint32_t m_reconnectDelay = RECONNECT_DELAY_MS;

    // ========================================================================
    // Soft-AP Configuration
    // ========================================================================

    bool m_apEnabled = false;
    String m_apSSID = "LightwaveOS-AP";
    String m_apPassword = "lightwave123";
    uint8_t m_apChannel = 1;
};

// ============================================================================
// Convenience Macro
// ============================================================================

/**
 * @brief Convenience macro to access WiFiManager singleton
 */
#define WIFI_MANAGER lightwaveos::network::WiFiManager::getInstance()

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
