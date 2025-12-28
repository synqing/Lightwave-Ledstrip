/**
 * @file WiFiManager.cpp
 * @brief Non-blocking WiFi management implementation for LightwaveOS v2
 *
 * Implements FreeRTOS-based WiFi management with:
 * - Task running on Core 0 (with WiFi stack)
 * - Event-driven state machine
 * - Automatic reconnection with exponential backoff
 * - Soft-AP fallback mode
 * - Cached network scanning
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WiFiManager.h"

#if FEATURE_WEB_SERVER

#include <esp_wifi.h>
#include "../config/network_config.h"

namespace lightwaveos {
namespace network {

using namespace lightwaveos::config;

// ============================================================================
// Static Member Initialization
// ============================================================================

WiFiManager* WiFiManager::s_instance = nullptr;

// ============================================================================
// Lifecycle
// ============================================================================

bool WiFiManager::begin() {
    Serial.println("[WiFiManager] Starting non-blocking WiFi management");

    // Create synchronization primitives
    m_wifiEventGroup = xEventGroupCreate();
    if (!m_wifiEventGroup) {
        Serial.println("[WiFiManager] ERROR: Failed to create event group");
        return false;
    }

    m_stateMutex = xSemaphoreCreateMutex();
    if (!m_stateMutex) {
        Serial.println("[WiFiManager] ERROR: Failed to create mutex");
        vEventGroupDelete(m_wifiEventGroup);
        m_wifiEventGroup = nullptr;
        return false;
    }

    // Register WiFi event handler
    WiFi.onEvent(onWiFiEvent);

    // Set WiFi mode to STA only - AP mode is fallback only
    // (Exclusive modes: STA for normal operation, AP when connection fails)
    WiFi.mode(WIFI_MODE_STA);

    // Create WiFi management task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiManager",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_wifiTaskHandle,
        TASK_CORE
    );

    if (result != pdPASS) {
        Serial.println("[WiFiManager] ERROR: Failed to create WiFi task");
        vEventGroupDelete(m_wifiEventGroup);
        vSemaphoreDelete(m_stateMutex);
        m_wifiEventGroup = nullptr;
        m_stateMutex = nullptr;
        return false;
    }

    Serial.printf("[WiFiManager] Task created on Core %d (stack: %d bytes)\n",
                  TASK_CORE, TASK_STACK_SIZE);
    return true;
}

void WiFiManager::stop() {
    Serial.println("[WiFiManager] Stopping...");

    if (m_wifiTaskHandle) {
        vTaskDelete(m_wifiTaskHandle);
        m_wifiTaskHandle = nullptr;
    }

    if (m_wifiEventGroup) {
        vEventGroupDelete(m_wifiEventGroup);
        m_wifiEventGroup = nullptr;
    }

    if (m_stateMutex) {
        vSemaphoreDelete(m_stateMutex);
        m_stateMutex = nullptr;
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    Serial.println("[WiFiManager] Stopped");
}

// ============================================================================
// FreeRTOS Task
// ============================================================================

void WiFiManager::wifiTask(void* parameter) {
    WiFiManager* manager = static_cast<WiFiManager*>(parameter);

    Serial.println("[WiFiManager] Task started");

    // Start Soft-AP immediately if enabled
    if (manager->m_apEnabled) {
        manager->startSoftAP();
    }

    // Main state machine loop
    while (true) {
        switch (manager->m_currentState) {
            case STATE_WIFI_INIT:
                manager->handleStateInit();
                break;

            case STATE_WIFI_SCANNING:
                manager->handleStateScanning();
                break;

            case STATE_WIFI_CONNECTING:
                manager->handleStateConnecting();
                break;

            case STATE_WIFI_CONNECTED:
                manager->handleStateConnected();
                break;

            case STATE_WIFI_FAILED:
                manager->handleStateFailed();
                break;

            case STATE_WIFI_AP_MODE:
                manager->handleStateAPMode();
                break;

            case STATE_WIFI_DISCONNECTED:
                manager->handleStateDisconnected();
                break;
        }

        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// State Machine Handlers
// ============================================================================

void WiFiManager::handleStateInit() {
    Serial.println("[WiFiManager] STATE: INIT");

    // Check if we have credentials
    if (m_ssid.isEmpty()) {
        Serial.println("[WiFiManager] No credentials configured, switching to AP mode");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Check for "CONFIGURE_ME" placeholder
    if (m_ssid == "CONFIGURE_ME") {
        Serial.println("[WiFiManager] WiFi not configured (CONFIGURE_ME), switching to AP mode");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Check if we have cached channel info and it's recent
    if (m_bestChannel > 0 && (millis() - m_lastScanTime < SCAN_INTERVAL_MS)) {
        Serial.printf("[WiFiManager] Using cached channel %d\n", m_bestChannel);
        setState(STATE_WIFI_CONNECTING);
    } else {
        Serial.println("[WiFiManager] Starting network scan...");
        setState(STATE_WIFI_SCANNING);
    }
}

void WiFiManager::handleStateScanning() {
    // m_scanStarted is reset in setState() on entry

    if (!m_scanStarted) {
        Serial.println("[WiFiManager] STATE: SCANNING");
        performAsyncScan();
        m_scanStarted = true;
    }

    // Wait for scan complete event
    EventBits_t bits = xEventGroupWaitBits(
        m_wifiEventGroup,
        EVENT_SCAN_COMPLETE,
        pdTRUE,   // Clear on exit
        pdFALSE,  // Wait for any bit
        pdMS_TO_TICKS(100)
    );

    if (bits & EVENT_SCAN_COMPLETE) {
        m_scanStarted = false;
        updateBestChannel();

        if (m_bestChannel > 0) {
            Serial.printf("[WiFiManager] Best channel for '%s': %d\n",
                          m_ssid.c_str(), m_bestChannel);
            setState(STATE_WIFI_CONNECTING);
        } else {
            Serial.printf("[WiFiManager] Network '%s' not found\n", m_ssid.c_str());
            setState(STATE_WIFI_FAILED);
        }
    }
}

void WiFiManager::handleStateConnecting() {
    // m_connectStarted and m_connectStartTime are reset in setState() on entry

    if (!m_connectStarted) {
        Serial.println("[WiFiManager] STATE: CONNECTING");
        m_connectStartTime = millis();
        m_connectStarted = connectToAP();

        if (!m_connectStarted) {
            Serial.println("[WiFiManager] Failed to initiate connection");
            setState(STATE_WIFI_FAILED);
            return;
        }
    }

    // Wait for connection events
    EventBits_t bits = xEventGroupWaitBits(
        m_wifiEventGroup,
        EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED,
        pdTRUE,   // Clear on exit
        pdFALSE,  // Wait for any bit
        pdMS_TO_TICKS(100)
    );

    if (bits & EVENT_GOT_IP) {
        m_connectStarted = false;
        m_successfulConnections++;
        m_lastConnectionTime = millis();
        m_reconnectDelay = RECONNECT_DELAY_MS;  // Reset backoff
        m_attemptsOnCurrentNetwork = 0;         // Reset attempt counter

        Serial.printf("[WiFiManager] Connected! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        setState(STATE_WIFI_CONNECTED);
    } else if ((bits & EVENT_CONNECTION_FAILED) ||
               (millis() - m_connectStartTime > CONNECT_TIMEOUT_MS)) {
        m_connectStarted = false;
        m_connectionAttempts++;

        Serial.printf("[WiFiManager] Connection failed (attempt %d)\n",
                      m_connectionAttempts);
        setState(STATE_WIFI_FAILED);
    }
}

void WiFiManager::handleStateConnected() {
    static uint32_t lastStatusPrint = 0;

    // Print status periodically (every 30 seconds)
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        Serial.printf("[WiFiManager] Connected to '%s', RSSI: %d dBm, Channel: %d, Uptime: %ds\n",
                      WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel(),
                      getUptimeSeconds());
    }

    // Check for disconnection event
    EventBits_t bits = xEventGroupWaitBits(
        m_wifiEventGroup,
        EVENT_DISCONNECTED,
        pdTRUE,   // Clear on exit
        pdFALSE,  // Wait for any bit
        0         // Don't block
    );

    if (bits & EVENT_DISCONNECTED) {
        Serial.println("[WiFiManager] Disconnected from AP");
        setState(STATE_WIFI_DISCONNECTED);
    }
}

void WiFiManager::handleStateFailed() {
    Serial.println("[WiFiManager] STATE: FAILED");

    m_attemptsOnCurrentNetwork++;

    Serial.printf("[WiFiManager] Connection failed (%d/%d attempts on %s)\n",
                  m_attemptsOnCurrentNetwork, NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK, m_ssid.c_str());

    // Check if we should switch to next network
    if (m_attemptsOnCurrentNetwork >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK) {
        if (hasSecondaryNetwork()) {
            switchToNextNetwork();
            m_reconnectDelay = RECONNECT_DELAY_MS;  // Reset backoff for new network
            setState(STATE_WIFI_INIT);
            return;
        }
    }

    // If AP mode is enabled and we've exhausted all networks, fall back to it
    if (m_apEnabled && m_attemptsOnCurrentNetwork >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK && !hasSecondaryNetwork()) {
        Serial.println("[WiFiManager] Falling back to AP mode for configuration");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Otherwise, wait with backoff before retrying same network
    Serial.printf("[WiFiManager] Waiting %d ms before retry (backoff)\n", m_reconnectDelay);
    vTaskDelay(pdMS_TO_TICKS(m_reconnectDelay));

    // Exponential backoff
    m_reconnectDelay = min(m_reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);

    // Try again
    setState(STATE_WIFI_INIT);
}

void WiFiManager::handleStateAPMode() {
    static uint32_t lastStatusPrint = 0;
    static uint32_t lastRetryTime = 0;

    // Print AP status periodically
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        Serial.printf("[WiFiManager] AP Mode - SSID: '%s', IP: %s, Clients: %d\n",
                      m_apSSID.c_str(),
                      WiFi.softAPIP().toString().c_str(),
                      WiFi.softAPgetStationNum());
    }

    // Periodically try to connect to WiFi if we have valid credentials
    if (!m_ssid.isEmpty() && m_ssid != "CONFIGURE_ME") {
        if (millis() - lastRetryTime > 60000) {
            lastRetryTime = millis();
            Serial.println("[WiFiManager] Retrying WiFi connection from AP mode...");
            // Switch back to STA mode before attempting connection
            WiFi.mode(WIFI_MODE_STA);
            setState(STATE_WIFI_INIT);
        }
    }
}

void WiFiManager::handleStateDisconnected() {
    Serial.println("[WiFiManager] STATE: DISCONNECTED");

    // Wait a bit before reconnecting
    vTaskDelay(pdMS_TO_TICKS(m_reconnectDelay));

    // Try to reconnect
    setState(STATE_WIFI_INIT);
}

// ============================================================================
// Helper Functions
// ============================================================================

void WiFiManager::performAsyncScan() {
    // Clear previous results
    m_cachedScanResults.clear();

    // Start async scan
    // Parameters: async=true, show_hidden=false, passive=false, max_ms_per_chan=300
    WiFi.scanNetworks(true, false, false, 300);
}

bool WiFiManager::connectToAP() {
    m_connectionAttempts++;

    Serial.printf("[WiFiManager] Connecting to '%s'", m_ssid.c_str());
    if (m_bestChannel > 0) {
        Serial.printf(" on channel %d", m_bestChannel);
    }
    Serial.println();

    // Configure static IP if requested
    if (m_useStaticIP) {
        if (!WiFi.config(m_staticIP, m_gateway, m_subnet, m_dns1, m_dns2)) {
            Serial.println("[WiFiManager] ERROR: Failed to configure static IP");
            return false;
        }
        Serial.printf("[WiFiManager] Using static IP: %s\n",
                      m_staticIP.toString().c_str());
    }

    // Set hostname before connecting
    WiFi.setHostname(config::NetworkConfig::MDNS_HOSTNAME);

    // Connect with channel hint if available
    if (m_bestChannel > 0) {
        // Get BSSID of best AP for this SSID
        uint8_t* bssid = nullptr;
        for (const auto& scan : m_cachedScanResults) {
            if (scan.ssid == m_ssid && scan.channel == m_bestChannel) {
                bssid = (uint8_t*)scan.bssid;
                break;
            }
        }
        WiFi.begin(m_ssid.c_str(), m_password.c_str(), m_bestChannel, bssid);
    } else {
        WiFi.begin(m_ssid.c_str(), m_password.c_str());
    }

    // WiFi stability settings - must be called AFTER WiFi.begin()
    WiFi.setSleep(false);           // Disable modem sleep (prevents ASSOC_LEAVE disconnects)
    WiFi.setAutoReconnect(true);    // Auto-reconnect on disconnect
    Serial.println("[WiFiManager] WiFi sleep disabled, auto-reconnect enabled");

    return true;
}

void WiFiManager::startSoftAP() {
    Serial.printf("[WiFiManager] Starting Soft-AP: '%s' (channel %d)\n",
                  m_apSSID.c_str(), m_apChannel);

    // Switch to AP-only mode (exclusive modes architecture)
    WiFi.mode(WIFI_MODE_AP);

    // Configure and start AP
    if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
        Serial.printf("[WiFiManager] AP started - IP: %s\n",
                      WiFi.softAPIP().toString().c_str());
        xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
    } else {
        Serial.println("[WiFiManager] ERROR: Failed to start Soft-AP");
    }
}

void WiFiManager::updateBestChannel() {
    m_bestChannel = 0;
    int bestRSSI = -100;

    // Get scan results
    int n = WiFi.scanComplete();
    if (n <= 0) {
        Serial.printf("[WiFiManager] Scan returned %d networks\n", n);
        return;
    }

    // Store results and find best channel for our SSID
    for (int i = 0; i < n; i++) {
        ScanResult result;
        result.ssid = WiFi.SSID(i);
        result.rssi = WiFi.RSSI(i);
        result.channel = WiFi.channel(i);
        uint8_t* bssid = WiFi.BSSID(i);
        if (bssid) {
            memcpy(result.bssid, bssid, 6);
        }
        result.encryption = WiFi.encryptionType(i);

        m_cachedScanResults.push_back(result);

        // Check if this is our target SSID with better signal
        if (result.ssid == m_ssid && result.rssi > bestRSSI) {
            bestRSSI = result.rssi;
            m_bestChannel = result.channel;
        }
    }

    m_lastScanTime = millis();
    WiFi.scanDelete();  // Clean up scan results from WiFi driver

    Serial.printf("[WiFiManager] Found %d networks\n", n);
}

void WiFiManager::setState(WiFiState newState) {
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Reset state-specific flags on entry to avoid persistence bugs
        // (previously used static variables that persisted across state exits)
        if (newState == STATE_WIFI_SCANNING) {
            m_scanStarted = false;
        } else if (newState == STATE_WIFI_CONNECTING) {
            m_connectStarted = false;
            m_connectStartTime = 0;
        }

        m_currentState = newState;
        xSemaphoreGive(m_stateMutex);
    }
}

WiFiManager::WiFiState WiFiManager::getState() const {
    WiFiState state = STATE_WIFI_INIT;
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = m_currentState;
        xSemaphoreGive(m_stateMutex);
    }
    return state;
}

String WiFiManager::getStateString() const {
    switch (getState()) {
        case STATE_WIFI_INIT:         return "INIT";
        case STATE_WIFI_SCANNING:     return "SCANNING";
        case STATE_WIFI_CONNECTING:   return "CONNECTING";
        case STATE_WIFI_CONNECTED:    return "CONNECTED";
        case STATE_WIFI_FAILED:       return "FAILED";
        case STATE_WIFI_AP_MODE:      return "AP_MODE";
        case STATE_WIFI_DISCONNECTED: return "DISCONNECTED";
        default:                      return "UNKNOWN";
    }
}

// ============================================================================
// Configuration
// ============================================================================

void WiFiManager::setCredentials(const String& ssid, const String& password) {
    m_ssid = ssid;
    m_password = password;
    // Also load secondary network from config if available
    m_ssid2 = NetworkConfig::WIFI_SSID_2_VALUE;
    m_password2 = NetworkConfig::WIFI_PASSWORD_2_VALUE;
    m_currentNetworkIndex = 0;
    m_attemptsOnCurrentNetwork = 0;

    if (hasSecondaryNetwork()) {
        Serial.printf("[WiFiManager] Configured networks: %s (primary), %s (fallback)\n",
                      ssid.c_str(), m_ssid2.c_str());
    } else {
        Serial.printf("[WiFiManager] Credentials set for '%s'\n", ssid.c_str());
    }
}

bool WiFiManager::hasSecondaryNetwork() const {
    return m_ssid2.length() > 0 && m_ssid2 != "";
}

void WiFiManager::switchToNextNetwork() {
    if (!hasSecondaryNetwork()) return;

    m_currentNetworkIndex = (m_currentNetworkIndex + 1) % 2;
    m_attemptsOnCurrentNetwork = 0;

    // Update active credentials
    if (m_currentNetworkIndex == 0) {
        m_ssid = NetworkConfig::WIFI_SSID_VALUE;
        m_password = NetworkConfig::WIFI_PASSWORD_VALUE;
    } else {
        m_ssid = m_ssid2;
        m_password = m_password2;
    }

    Serial.printf("[WiFiManager] Switching to network: %s\n", m_ssid.c_str());

    // Clear cached channel info for new network
    m_bestChannel = 0;
}

void WiFiManager::setStaticIP(const IPAddress& ip, const IPAddress& gw,
                              const IPAddress& sn, const IPAddress& d1,
                              const IPAddress& d2) {
    m_useStaticIP = true;
    m_staticIP = ip;
    m_gateway = gw;
    m_subnet = sn;
    m_dns1 = d1;
    m_dns2 = d2;
    Serial.printf("[WiFiManager] Static IP configured: %s\n", ip.toString().c_str());
}

void WiFiManager::enableSoftAP(const String& ssid, const String& password, uint8_t channel) {
    m_apEnabled = true;
    m_apSSID = ssid;
    m_apPassword = password;
    m_apChannel = channel;
    Serial.printf("[WiFiManager] Soft-AP enabled: '%s'\n", ssid.c_str());
}

// ============================================================================
// Statistics
// ============================================================================

uint32_t WiFiManager::getUptimeSeconds() const {
    if (m_lastConnectionTime > 0 && m_currentState == STATE_WIFI_CONNECTED) {
        return (millis() - m_lastConnectionTime) / 1000;
    }
    return 0;
}

// ============================================================================
// Manual Control
// ============================================================================

void WiFiManager::disconnect() {
    Serial.println("[WiFiManager] Manual disconnect requested");
    WiFi.disconnect(false);
    setState(STATE_WIFI_DISCONNECTED);
}

void WiFiManager::reconnect() {
    Serial.println("[WiFiManager] Manual reconnect requested");
    disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    setState(STATE_WIFI_INIT);
}

void WiFiManager::scanNetworks() {
    if (m_currentState != STATE_WIFI_SCANNING) {
        Serial.println("[WiFiManager] Manual scan requested");
        setState(STATE_WIFI_SCANNING);
    }
}

// ============================================================================
// WiFi Event Handler
// ============================================================================

void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    WiFiManager& manager = getInstance();

    // Handle both old (SYSTEM_EVENT_*) and new (ARDUINO_EVENT_*) event names
    // ESP32 Arduino Core 2.x uses ARDUINO_EVENT_*, older uses SYSTEM_EVENT_*
    switch (event) {
        // Scan complete
#ifdef ARDUINO_EVENT_WIFI_SCAN_DONE
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
#else
        case SYSTEM_EVENT_SCAN_DONE:
#endif
            Serial.println("[WiFiManager] Event: Scan complete");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_SCAN_COMPLETE);
            }
            break;

        // Connected to AP
#ifdef ARDUINO_EVENT_WIFI_STA_CONNECTED
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
#else
        case SYSTEM_EVENT_STA_CONNECTED:
#endif
            Serial.println("[WiFiManager] Event: Connected to AP");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_CONNECTED);
            }
            break;

        // Got IP address
#ifdef ARDUINO_EVENT_WIFI_STA_GOT_IP
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#else
        case SYSTEM_EVENT_STA_GOT_IP:
#endif
            Serial.printf("[WiFiManager] Event: Got IP - %s\n",
                          WiFi.localIP().toString().c_str());
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_GOT_IP);
            }
            break;

        // Disconnected from AP
#ifdef ARDUINO_EVENT_WIFI_STA_DISCONNECTED
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#else
        case SYSTEM_EVENT_STA_DISCONNECTED:
#endif
            Serial.println("[WiFiManager] Event: Disconnected from AP");
            if (manager.m_wifiEventGroup) {
                // Set both events so the appropriate state handler can respond:
                // - CONNECTING state waits for EVENT_CONNECTION_FAILED
                // - CONNECTED state waits for EVENT_DISCONNECTED
                // This avoids 10-second timeout delays on failed connections
                xEventGroupSetBits(manager.m_wifiEventGroup,
                    EVENT_DISCONNECTED | EVENT_CONNECTION_FAILED);
            }
            break;

        // Auth mode changed
#ifdef ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
#else
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
#endif
            Serial.println("[WiFiManager] Event: Auth mode changed");
            break;

        // AP started
#ifdef ARDUINO_EVENT_WIFI_AP_START
        case ARDUINO_EVENT_WIFI_AP_START:
#else
        case SYSTEM_EVENT_AP_START:
#endif
            Serial.println("[WiFiManager] Event: AP started");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_START);
            }
            break;

        // Station connected to our AP
#ifdef ARDUINO_EVENT_WIFI_AP_STACONNECTED
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
#else
        case SYSTEM_EVENT_AP_STACONNECTED:
#endif
            Serial.println("[WiFiManager] Event: Station connected to AP");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_STACONNECTED);
            }
            break;

        // Station disconnected from our AP
#ifdef ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
#else
        case SYSTEM_EVENT_AP_STADISCONNECTED:
#endif
            Serial.println("[WiFiManager] Event: Station disconnected from AP");
            break;

        default:
            // Ignore other events
            break;
    }
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
