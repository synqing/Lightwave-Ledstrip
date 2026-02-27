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
#include "../core/system/OtaSessionLock.h"

#define LW_LOG_TAG "WiFi"
#include "utils/Log.h"

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
    LW_LOGI("Starting non-blocking WiFi management");

    // Create synchronization primitives
    m_wifiEventGroup = xEventGroupCreate();
    if (!m_wifiEventGroup) {
        LW_LOGE("Failed to create event group");
        return false;
    }

    m_stateMutex = xSemaphoreCreateMutex();
    if (!m_stateMutex) {
        LW_LOGE("Failed to create mutex");
        vEventGroupDelete(m_wifiEventGroup);
        m_wifiEventGroup = nullptr;
        return false;
    }

    // Initialize credential storage (NVS-based)
    if (!m_credentialsStorage.begin()) {
        LW_LOGW("WiFiCredentialsStorage init failed - saved networks unavailable");
        // Non-fatal: continue without NVS storage
    }

    // Register WiFi event handler
    WiFi.onEvent(onWiFiEvent);

    // Boot into AP-only mode. STA is ONLY activated via serial `wifi connect`.
    // This prevents STA scanning/reconnection loops from destabilising the AP,
    // which is the PRIMARY connection path for Tab5 and iOS clients.
#ifdef WIFI_AP_ONLY
    LW_LOGW("WIFI_AP_ONLY enabled - starting in AP mode only");
#endif
    {
        WiFi.mode(WIFI_MODE_AP);
        wifi_auth_mode_t authMode = m_apPassword.isEmpty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_WPA2_PSK;
        const char* pw = m_apPassword.isEmpty() ? NULL : m_apPassword.c_str();
        if (WiFi.softAP(m_apSSID.c_str(), pw, m_apChannel,
                        false, 4, authMode)) {
            LW_LOGI("AP started: '%s' at %s (STA disabled — use serial 'wifi connect' to enable)",
                    m_apSSID.c_str(), WiFi.softAPIP().toString().c_str());
            xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
        } else {
            LW_LOGE("Failed to start Soft-AP!");
        }
        m_forceApOnly = true;
        setState(STATE_WIFI_AP_MODE);
    }

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
        LW_LOGE("Failed to create WiFi task");
        vEventGroupDelete(m_wifiEventGroup);
        vSemaphoreDelete(m_stateMutex);
        m_wifiEventGroup = nullptr;
        m_stateMutex = nullptr;
        return false;
    }

    LW_LOGI("Task created on Core %d (stack: %d bytes)", TASK_CORE, TASK_STACK_SIZE);
    return true;
}

void WiFiManager::stop() {
    LW_LOGI("Stopping...");

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

    LW_LOGI("Stopped");
}

// ============================================================================
// FreeRTOS Task
// ============================================================================

void WiFiManager::wifiTask(void* parameter) {
    WiFiManager* manager = static_cast<WiFiManager*>(parameter);

    LW_LOGI("Task started");

    // NOTE: In Portable Mode, AP is started immediately in begin() via AP+STA.
    // The task handles STA connection attempts in parallel.
    // AP stays up permanently regardless of STA state.

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
    LW_LOGD("STATE: INIT");

#ifdef WIFI_AP_ONLY
    LW_LOGI("WIFI_AP_ONLY: forcing AP mode");
    setState(STATE_WIFI_AP_MODE);
    return;
#endif

    // AP-only mode: do not attempt STA connection.
    // STA is only enabled via serial `wifi connect` which clears m_forceApOnly.
    if (m_forceApOnly) {
        LW_LOGI("AP-only mode active, skipping STA");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Reset credential save flag for new connection attempt
    m_credentialsSaved = false;

    // Avoid STA scan/retry loops when credentials are placeholders or empty.
    // Repeated scans churn the WiFi stack and can contribute to esp_timer ENOMEM failures.
    if (!hasAnyStaCandidates()) {
        LW_LOGW("No valid STA credentials, switching to AP mode");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // If the current primary SSID is not valid, always scan to select a real candidate
    // from the credential pool (config primary/secondary + NVS).
    if (!isValidStaSsid(m_ssid)) {
        LW_LOGI("Primary STA SSID is not valid, scanning for known networks");
        setState(STATE_WIFI_SCANNING);
        return;
    }

    // Check if we have cached channel info and it's recent
    if (m_bestChannel > 0 && (millis() - m_lastScanTime < SCAN_INTERVAL_MS)) {
        LW_LOGD("Using cached channel %d", m_bestChannel);
        setState(STATE_WIFI_CONNECTING);
    } else {
        LW_LOGI("Starting network scan...");
        setState(STATE_WIFI_SCANNING);
    }
}

void WiFiManager::handleStateScanning() {
    // m_scanStarted is reset in setState() on entry

    if (!m_scanStarted) {
        LW_LOGD("STATE: SCANNING");
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

        // Cache scan results first (updateBestChannel populates m_cachedScanResults)
        updateBestChannel();

        // Smart network selection: find best available network from all credential sources
        BestNetworkResult best = findBestAvailableNetwork();

        if (best.found) {
            // Update active credentials to selected network
            m_ssid = best.ssid;
            m_password = best.password;
            m_bestChannel = best.channel;
            m_scanAttemptsWithoutKnown = 0;
            m_noKnownNetworksLastScan = false;

            LW_LOGI("Smart selection: '%s' (RSSI: %d dBm, Channel: %d)",
                    best.ssid.c_str(), best.rssi, best.channel);
            setState(STATE_WIFI_CONNECTING);
        } else {
            LW_LOGW("No known networks found in %d scan results",
                    m_cachedScanResults.size());
            m_noKnownNetworksLastScan = true;
            m_scanAttemptsWithoutKnown++;
            setState(STATE_WIFI_FAILED);
        }
    }
}

void WiFiManager::handleStateConnecting() {
    // m_connectStarted and m_connectStartTime are reset in setState() on entry

    if (!m_connectStarted) {
        LW_LOGD("STATE: CONNECTING");
        m_connectStartTime = millis();
        m_connectStarted = connectToAP();

        if (!m_connectStarted) {
            LW_LOGE("Failed to initiate connection");
            setState(STATE_WIFI_FAILED);
            return;
        }
    }

    // Wait for connection success events only
    // Don't wait for failure events - timeout will handle that
    EventBits_t bits = xEventGroupWaitBits(
        m_wifiEventGroup,
        EVENT_CONNECTED | EVENT_GOT_IP,
        pdTRUE,   // Clear on exit
        pdFALSE,  // Wait for any bit
        pdMS_TO_TICKS(100)
    );

    // Check if we got connected
    if (bits & EVENT_GOT_IP) {
        m_connectStarted = false;
        m_successfulConnections++;
        m_lastConnectionTime = millis();
        m_reconnectDelay = RECONNECT_DELAY_MS;  // Reset backoff
        m_attemptsOnCurrentNetwork = 0;         // Reset attempt counter

        LW_LOGI("Connected! IP: %s, RSSI: %d dBm",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
        setState(STATE_WIFI_CONNECTED);
    } else if (bits & EVENT_CONNECTED) {
        // Got CONNECTED but not GOT_IP yet - wait a bit more
        // This is normal, GOT_IP usually follows CONNECTED
    } else if (millis() - m_connectStartTime > CONNECT_TIMEOUT_MS) {
        // Timeout - but check if we're actually connected before marking as failed
        // This handles the case where GOT_IP arrives after wait timeout
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            // We're actually connected! Set the bit and transition
            LW_LOGI("Connected! IP: %s (detected after timeout)", 
                    WiFi.localIP().toString().c_str());
            if (m_wifiEventGroup) {
                xEventGroupSetBits(m_wifiEventGroup, EVENT_GOT_IP);
            }
            m_connectStarted = false;
            m_successfulConnections++;
            m_lastConnectionTime = millis();
            m_reconnectDelay = RECONNECT_DELAY_MS;
            m_attemptsOnCurrentNetwork = 0;
            setState(STATE_WIFI_CONNECTED);
        } else {
            // Genuine timeout
            m_connectStarted = false;
            m_connectionAttempts++;
            LW_LOGW("Connection timeout (attempt %d)", m_connectionAttempts);
            setState(STATE_WIFI_FAILED);
        }
    }
}

void WiFiManager::handleStateConnected() {
    static uint32_t lastStatusPrint = 0;

    // One-time entry actions (must be instance state, not static).
    // WiFi can disconnect/reconnect; we must reapply settings and clear stale events.
    if (!m_inConnectedState) {
        m_inConnectedState = true;
        m_connectedStateEntryTimeMs = millis();
        m_sleepSettingsApplied = false;

        // Clear any stale disconnect events that may have accumulated during connection.
        if (m_wifiEventGroup) {
            xEventGroupClearBits(m_wifiEventGroup, EVENT_DISCONNECTED);
        }
    }

    // Apply sleep settings once on entry to connected state (defensive).
    // This handles edge cases where ESP32 WiFi stack resets settings.
    if (!m_sleepSettingsApplied) {
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true);
        esp_wifi_set_ps(WIFI_PS_NONE);  // ESP-IDF level disable
        m_sleepSettingsApplied = true;
        LW_LOGD("Applied WiFi stability settings in connected state");
    }

    // Auto-save credentials to NVS on successful connection (once per session)
    if (!m_credentialsSaved && m_ssid.length() > 0) {
        // Save network if not already in storage
        if (!m_credentialsStorage.hasNetwork(m_ssid)) {
            if (m_credentialsStorage.saveNetwork(m_ssid, m_password)) {
                LW_LOGI("Auto-saved network '%s' to NVS", m_ssid.c_str());
            }
        }
        // Always update last-connected SSID for priority boost
        m_credentialsStorage.setLastConnectedSSID(m_ssid);
        m_credentialsSaved = true;
    }

    // Grace period: ignore disconnect flaps immediately after connect.
    if (millis() - m_connectedStateEntryTimeMs < CONNECTED_DISCONNECT_GRACE_MS) {
        return;
    }

    // Print status periodically (every 30 seconds)
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        LW_LOGI("Connected to '%s', RSSI: %d dBm, Channel: %d, Uptime: %ds, IP: %s (%s.local)",
                WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel(),
                getUptimeSeconds(), WiFi.localIP().toString().c_str(),
                config::NetworkConfig::MDNS_HOSTNAME);
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
        if (m_forceApOnly) {
            LW_LOGI("STA disconnected (AP-only mode active)");
            setState(STATE_WIFI_AP_MODE);
        } else {
            LW_LOGW("Disconnected from AP");
            setState(STATE_WIFI_DISCONNECTED);
        }
    }
}

void WiFiManager::handleStateFailed() {
    LW_LOGD("STATE: FAILED");

    if (m_noKnownNetworksLastScan) {
        if (m_scanAttemptsWithoutKnown >= 2 && m_apEnabled) {
            LW_LOGW("No known networks after %d scans, switching to AP mode",
                    m_scanAttemptsWithoutKnown);
            setState(STATE_WIFI_AP_MODE);
            return;
        }

        if (m_scanAttemptsWithoutKnown < 2) {
            LW_LOGW("No known networks found, retrying scan (%d/2)",
                    m_scanAttemptsWithoutKnown);
            setState(STATE_WIFI_SCANNING);
            return;
        }
        // If AP mode is disabled, fall through to retry logic.
        m_noKnownNetworksLastScan = false;
    }

    m_attemptsOnCurrentNetwork++;

    LW_LOGW("Connection failed (%d/%d attempts on %s)",
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
        LW_LOGW("All networks exhausted - entering AP mode (AP already up in AP+STA)");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Otherwise, wait with backoff before retrying same network
    LW_LOGD("Waiting %d ms before retry (backoff)", m_reconnectDelay);
    vTaskDelay(pdMS_TO_TICKS(m_reconnectDelay));

    // Exponential backoff
    m_reconnectDelay = min(m_reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);

    // Try again
    setState(STATE_WIFI_INIT);
}

void WiFiManager::handleStateAPMode() {
    static uint32_t lastStatusPrint = 0;
    static uint32_t lastRetryTime = 0;

    // Guard: Do NOT attempt STA retry while an OTA upload is in progress.
    // Switching to STA mode tears down the AP, which disconnects the client
    // mid-upload and bricks the update. Uses the unified cross-transport
    // OtaSessionLock which is thread-safe (spinlock-guarded).
    if (core::system::OtaSessionLock::isOtaInProgress()) {
        // Still print status so operator knows why retry is suppressed
        if (millis() - lastStatusPrint > 30000) {
            lastStatusPrint = millis();
            LW_LOGI("AP Mode - OTA in progress, STA retry suppressed");
        }
        return;
    }

    // Print AP status periodically
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        LW_LOGI("AP Mode - SSID: '%s', IP: %s, Clients: %d",
                m_apSSID.c_str(),
                WiFi.softAPIP().toString().c_str(),
                WiFi.softAPgetStationNum());
    }

#ifdef WIFI_AP_ONLY
    // AP-only build: do not attempt STA retries.
    return;
#endif

    // Runtime AP-only lock: do not attempt STA retries.
    // Set by requestAPOnly(), cleared by requestSTAEnable().
    if (m_forceApOnly) {
        return;
    }

    // Periodically try STA connection without killing AP (non-destructive retry).
    // Skip if no known networks were ever found — scanning disrupts the network
    // stack (tears down UDP streamer, triggers WiFi events) and can cause
    // watchdog timeouts when combined with rapid effect changes.
    if (hasAnyStaCandidates() && m_scanAttemptsWithoutKnown < 4) {
        if (millis() - lastRetryTime > 60000) {
            lastRetryTime = millis();
            LW_LOGI("Retrying STA connection from AP mode (AP stays up)...");
            // Disconnect STA only, preserve AP. AP+STA mode set in begin().
            WiFi.disconnect(false);
            setState(STATE_WIFI_SCANNING);
        }
    }
}

void WiFiManager::handleStateDisconnected() {
    LW_LOGD("STATE: DISCONNECTED");

    // If runtime AP-only mode, go straight to AP mode (no reconnect)
    if (m_forceApOnly) {
        LW_LOGI("AP-only mode active, skipping STA reconnect");
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Wait a bit before reconnecting
    vTaskDelay(pdMS_TO_TICKS(m_reconnectDelay));

    // Try to reconnect
    setState(STATE_WIFI_INIT);
}

// ============================================================================
// Helper Functions
// ============================================================================

bool WiFiManager::isValidStaSsid(const String& ssid) {
    if (ssid.isEmpty()) return false;
    if (ssid == "CONFIGURE_ME") return false;
    if (ssid.startsWith("PORTABLE_TEST_NONE")) return false;
    if (ssid == config::NetworkConfig::AP_SSID) return false;
    return true;
}

bool WiFiManager::hasAnyStaCandidates() const {
    if (isValidStaSsid(m_ssid)) return true;
    if (isValidStaSsid(m_ssid2)) return true;

    WiFiCredentialsStorage::NetworkCredential nvsNets[WiFiCredentialsStorage::MAX_NETWORKS];
    WiFiCredentialsStorage& storage = const_cast<WiFiCredentialsStorage&>(m_credentialsStorage);
    uint8_t nvsCount = storage.loadNetworks(nvsNets, WiFiCredentialsStorage::MAX_NETWORKS);
    for (uint8_t i = 0; i < nvsCount; i++) {
        if (isValidStaSsid(nvsNets[i].ssid)) return true;
    }
    return false;
}

void WiFiManager::performAsyncScan() {
    // Clear previous results
    m_cachedScanResults.clear();

    // Start async scan
    // Parameters: async=true, show_hidden=false, passive=false, max_ms_per_chan=300
    WiFi.scanNetworks(true, false, false, 300);
}

bool WiFiManager::connectToAP() {
    m_connectionAttempts++;

    if (m_bestChannel > 0) {
        LW_LOGI("Connecting to '%s' on channel %d", m_ssid.c_str(), m_bestChannel);
    } else {
        LW_LOGI("Connecting to '%s'", m_ssid.c_str());
    }

    // Configure static IP if requested
    if (m_useStaticIP) {
        if (!WiFi.config(m_staticIP, m_gateway, m_subnet, m_dns1, m_dns2)) {
            LW_LOGE("Failed to configure static IP");
            return false;
        }
        LW_LOGI("Using static IP: %s", m_staticIP.toString().c_str());
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
    // NOTE: These may be reset during connection handshake, so we also
    // apply them in the GOT_IP event handler to ensure they persist
    WiFi.setSleep(false);           // Disable modem sleep (prevents ASSOC_LEAVE disconnects)
    WiFi.setAutoReconnect(true);    // Auto-reconnect on disconnect
    
    // Also disable at ESP-IDF level for maximum reliability
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    LW_LOGD("WiFi sleep disabled, auto-reconnect enabled");

    return true;
}

void WiFiManager::startSoftAP() {
    LW_LOGI("Starting Soft-AP: '%s' (channel %d)", m_apSSID.c_str(), m_apChannel);

    // Switch to AP+STA concurrent mode (Portable Mode architecture)
    WiFi.mode(WIFI_MODE_APSTA);

    // Configure and start AP
    wifi_auth_mode_t apAuth = m_apPassword.isEmpty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA_WPA2_PSK;
    const char* apPw = m_apPassword.isEmpty() ? NULL : m_apPassword.c_str();
    if (WiFi.softAP(m_apSSID.c_str(), apPw, m_apChannel,
                    false, 4, apAuth)) {
        LW_LOGI("AP started - IP: %s", WiFi.softAPIP().toString().c_str());
        xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
    } else {
        LW_LOGE("Failed to start Soft-AP");
    }
}

void WiFiManager::updateBestChannel() {
    m_bestChannel = 0;
    int bestRSSI = -100;


    // Get scan results
    int n = WiFi.scanComplete();
    if (n <= 0) {
        LW_LOGD("Scan returned %d networks", n);
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

    LW_LOGD("Found %d networks", n);
}

WiFiManager::BestNetworkResult WiFiManager::findBestAvailableNetwork() {
    BestNetworkResult result = {"", "", 0, -127, false};
    BestNetworkResult lastConnectedResult = {"", "", 0, -127, false};

    // Get last connected SSID for priority boost
    String lastConnected = m_credentialsStorage.getLastConnectedSSID();

    // Build credential pool: config primary, config secondary, NVS networks
    // Use a simple struct to track known networks
    struct KnownNetwork {
        String ssid;
        String password;
        bool isLastConnected;
    };

    // Estimate max networks: 2 config + 10 NVS = 12
    static constexpr uint8_t MAX_KNOWN = 12;
    KnownNetwork knownNetworks[MAX_KNOWN];
    uint8_t knownCount = 0;

    // Add config primary (if valid)
    String configPrimarySsid = NetworkConfig::WIFI_SSID_VALUE;
    String configPrimaryPass = NetworkConfig::WIFI_PASSWORD_VALUE;
    if (isValidStaSsid(configPrimarySsid)) {
        knownNetworks[knownCount++] = {configPrimarySsid, configPrimaryPass,
                                        configPrimarySsid == lastConnected};
    }

    // Add config secondary (if valid)
    String configSecondarySsid = NetworkConfig::WIFI_SSID_2_VALUE;
    String configSecondaryPass = NetworkConfig::WIFI_PASSWORD_2_VALUE;
    if (isValidStaSsid(configSecondarySsid) && knownCount < MAX_KNOWN) {
        // Check for duplicate
        bool isDupe = false;
        for (uint8_t i = 0; i < knownCount; i++) {
            if (knownNetworks[i].ssid == configSecondarySsid) {
                isDupe = true;
                break;
            }
        }
        if (!isDupe) {
            knownNetworks[knownCount++] = {configSecondarySsid, configSecondaryPass,
                                            configSecondarySsid == lastConnected};
        }
    }

    // Add NVS-saved networks (skip duplicates)
    WiFiCredentialsStorage::NetworkCredential nvsNets[WiFiCredentialsStorage::MAX_NETWORKS];
    uint8_t nvsCount = m_credentialsStorage.loadNetworks(nvsNets, WiFiCredentialsStorage::MAX_NETWORKS);

    for (uint8_t i = 0; i < nvsCount && knownCount < MAX_KNOWN; i++) {
        bool isDupe = false;
        for (uint8_t j = 0; j < knownCount; j++) {
            if (knownNetworks[j].ssid == nvsNets[i].ssid) {
                isDupe = true;
                break;
            }
        }
        if (!isDupe) {
            knownNetworks[knownCount++] = {nvsNets[i].ssid, nvsNets[i].password,
                                            nvsNets[i].ssid == lastConnected};
        }
    }

    LW_LOGD("Credential pool: %d config + %d NVS = %d known networks",
            (configPrimarySsid.length() > 0 ? 1 : 0) + (configSecondarySsid.length() > 0 ? 1 : 0),
            nvsCount, knownCount);

    // Match scan results against known networks
    for (const auto& scan : m_cachedScanResults) {
        for (uint8_t i = 0; i < knownCount; i++) {
            if (scan.ssid == knownNetworks[i].ssid) {
                // Found a match!
                // Track last-connected network separately
                if (knownNetworks[i].isLastConnected && scan.rssi > lastConnectedResult.rssi) {
                    lastConnectedResult = {scan.ssid, knownNetworks[i].password,
                                            scan.channel, scan.rssi, true};
                }
                // Track best RSSI overall
                if (scan.rssi > result.rssi) {
                    result = {scan.ssid, knownNetworks[i].password,
                              scan.channel, scan.rssi, true};
                }
                break;  // Found match for this scan result, move to next
            }
        }
    }

    // Prefer last-connected if signal is reasonable (> -75 dBm)
    // This provides "stickiness" to avoid unnecessary network switching
    if (lastConnectedResult.found && lastConnectedResult.rssi > -75) {
        LW_LOGI("Preferring last-connected '%s' (RSSI: %d dBm)",
                lastConnectedResult.ssid.c_str(), lastConnectedResult.rssi);
        return lastConnectedResult;
    }

    if (result.found) {
        LW_LOGI("Selected best network '%s' (RSSI: %d dBm, Channel: %d)",
                result.ssid.c_str(), result.rssi, result.channel);
    } else {
        LW_LOGW("No known networks found in %d scan results", m_cachedScanResults.size());
    }

    return result;
}

void WiFiManager::setState(WiFiState newState) {
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Reset connected-state bookkeeping when leaving CONNECTED.
        if (m_currentState == STATE_WIFI_CONNECTED && newState != STATE_WIFI_CONNECTED) {
            m_inConnectedState = false;
            m_sleepSettingsApplied = false;
            m_connectedStateEntryTimeMs = 0;
        }

        // Reset state-specific flags on entry to avoid persistence bugs
        // (previously used static variables that persisted across state exits)
        if (newState == STATE_WIFI_SCANNING) {
            m_scanStarted = false;
        } else if (newState == STATE_WIFI_CONNECTING) {
            m_connectStarted = false;
            m_connectStartTime = 0;

            // CRITICAL FIX: Clear stale event bits to avoid false-positive connects.
            // EventGroup bits persist across interrupted connections, which can cause
            // the "Connected! IP: 0.0.0.0" bug when bits from a previous attempt
            // are still set. This fix is documented in CLAUDE.md as LOAD-BEARING.
            if (m_wifiEventGroup) {
                xEventGroupClearBits(m_wifiEventGroup,
                    EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED);
            }
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

uint8_t WiFiManager::getSavedNetworks(WiFiCredentialsStorage::NetworkCredential* networks,
                                      uint8_t maxNetworks) {
    return m_credentialsStorage.loadNetworks(networks, maxNetworks);
}

uint8_t WiFiManager::getSavedNetworkCount() const {
    return m_credentialsStorage.getNetworkCount();
}

bool WiFiManager::saveNetwork(const String& ssid, const String& password) {
    return m_credentialsStorage.saveNetwork(ssid, password);
}

bool WiFiManager::removeNetwork(const String& ssid) {
    return m_credentialsStorage.deleteNetwork(ssid);
}

bool WiFiManager::hasSavedNetwork(const String& ssid) {
    return m_credentialsStorage.hasNetwork(ssid);
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
    m_scanAttemptsWithoutKnown = 0;
    m_noKnownNetworksLastScan = false;

    if (hasSecondaryNetwork()) {
        LW_LOGI("Configured networks: %s (primary), %s (fallback)",
                ssid.c_str(), m_ssid2.c_str());
    } else {
        LW_LOGI("Credentials set for '%s'", ssid.c_str());
    }

}

bool WiFiManager::hasSecondaryNetwork() const {
    return isValidStaSsid(m_ssid2);
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

    LW_LOGI("Switching to network: %s", m_ssid.c_str());

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
    LW_LOGI("Static IP configured: %s", ip.toString().c_str());
}

void WiFiManager::enableSoftAP(const String& ssid, const String& password, uint8_t channel) {
    m_apEnabled = true;
    m_apSSID = ssid;
    m_apPassword = password;
    m_apChannel = channel;
    LW_LOGI("Soft-AP enabled: '%s'", ssid.c_str());
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
    LW_LOGI("Manual disconnect requested");
    WiFi.disconnect(false);
    setState(STATE_WIFI_DISCONNECTED);
}

void WiFiManager::reconnect() {
    LW_LOGI("Manual reconnect requested");
    disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    setState(STATE_WIFI_INIT);
}

void WiFiManager::scanNetworks() {
    if (m_currentState != STATE_WIFI_SCANNING) {
        LW_LOGI("Manual scan requested");
        setState(STATE_WIFI_SCANNING);
    }
}

// ============================================================================
// WiFi Event Handler
// ============================================================================

void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    WiFiManager& manager = getInstance();

#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= 0x030000
    // Arduino-ESP32 3.x: use if/else (SC_EVENT_SCAN_DONE and IP_EVENT_STA_GOT_IP can share enum value)
    if (event == SC_EVENT_SCAN_DONE) {
        LW_LOGD("Event: Scan complete");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_SCAN_COMPLETE);
        }
    } else if (event == WIFI_EVENT_STA_CONNECTED) {
        LW_LOGI("Event: Connected to AP");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_CONNECTED);
        }
    } else if (event == IP_EVENT_STA_GOT_IP) {
        LW_LOGI("Event: Got IP - %s", WiFi.localIP().toString().c_str());
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true);
        esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
        if (err != ESP_OK) {
            LW_LOGW("Failed to set WiFi PS mode: %d", err);
        }
        LW_LOGD("WiFi sleep disabled after GOT_IP (prevents ASSOC_LEAVE)");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_GOT_IP);
        }
    } else if (event == WIFI_EVENT_STA_DISCONNECTED) {
        LW_LOGW("Event: Disconnected from AP");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_DISCONNECTED);
        }
    } else if (event == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
        LW_LOGD("Event: Auth mode changed");
    } else if (event == WIFI_EVENT_AP_START) {
        LW_LOGI("Event: AP started");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_START);
        }
    } else if (event == WIFI_EVENT_AP_STACONNECTED) {
        LW_LOGI("Event: Station connected to AP");
        if (manager.m_wifiEventGroup) {
            xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_STACONNECTED);
        }
    } else if (event == WIFI_EVENT_AP_STADISCONNECTED) {
        LW_LOGD("Event: Station disconnected from AP");
    }
#else
    // Arduino-ESP32 2.x (ARDUINO_EVENT_*) or older (SYSTEM_EVENT_*)
    switch (event) {
#ifdef ARDUINO_EVENT_WIFI_SCAN_DONE
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
#else
        case SYSTEM_EVENT_SCAN_DONE:
#endif
            LW_LOGD("Event: Scan complete");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_SCAN_COMPLETE);
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_STA_CONNECTED
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
#else
        case SYSTEM_EVENT_STA_CONNECTED:
#endif
            LW_LOGI("Event: Connected to AP");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_CONNECTED);
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_STA_GOT_IP
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#else
        case SYSTEM_EVENT_STA_GOT_IP:
#endif
            {
                LW_LOGI("Event: Got IP - %s", WiFi.localIP().toString().c_str());
                WiFi.setSleep(false);
                WiFi.setAutoReconnect(true);
                esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
                if (err != ESP_OK) {
                    LW_LOGW("Failed to set WiFi PS mode: %d", err);
                }
                LW_LOGD("WiFi sleep disabled after GOT_IP (prevents ASSOC_LEAVE)");
                if (manager.m_wifiEventGroup) {
                    xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_GOT_IP);
                }
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_STA_DISCONNECTED
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#else
        case SYSTEM_EVENT_STA_DISCONNECTED:
#endif
            LW_LOGW("Event: Disconnected from AP");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_DISCONNECTED);
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
#else
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
#endif
            LW_LOGD("Event: Auth mode changed");
            break;
#ifdef ARDUINO_EVENT_WIFI_AP_START
        case ARDUINO_EVENT_WIFI_AP_START:
#else
        case SYSTEM_EVENT_AP_START:
#endif
            LW_LOGI("Event: AP started");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_START);
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_AP_STACONNECTED
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
#else
        case SYSTEM_EVENT_AP_STACONNECTED:
#endif
            LW_LOGI("Event: Station connected to AP");
            if (manager.m_wifiEventGroup) {
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_AP_STACONNECTED);
            }
            break;
#ifdef ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
#else
        case SYSTEM_EVENT_AP_STADISCONNECTED:
#endif
            LW_LOGD("Event: Station disconnected from AP");
            break;
        default:
            break;
    }
#endif
}

// ============================================================================
// Runtime Mode Control (Portable Mode)
// ============================================================================

bool WiFiManager::requestSTAEnable(uint32_t timeoutMs, bool autoRevert) {
    (void)timeoutMs; (void)autoRevert;
    LW_LOGI("STA enable requested (already active in AP+STA mode)");
    m_forceApOnly = false;
    if (m_currentState == STATE_WIFI_AP_MODE || m_currentState == STATE_WIFI_FAILED) {
        WiFi.disconnect(false);
        setState(STATE_WIFI_INIT);
    }
    return true;
}

bool WiFiManager::requestAPOnly() {
    LW_LOGI("AP-only mode requested");
    m_forceApOnly = true;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    setState(STATE_WIFI_AP_MODE);
    return true;
}

bool WiFiManager::connectToNetwork(const String& ssid, const String& password) {
    LW_LOGI("Connect requested: '%s'", ssid.c_str());
    m_forceApOnly = false;  // Clear AP-only lock when explicitly connecting
    m_credentialsStorage.saveNetwork(ssid, password);
    setCredentials(ssid, password);
    if (WiFi.getMode() == WIFI_MODE_AP) {
        WiFi.mode(WIFI_MODE_APSTA);
    }
    WiFi.disconnect(false);
    setState(STATE_WIFI_INIT);
    return true;
}

bool WiFiManager::connectToSavedNetwork(const String& ssid) {
    LW_LOGI("Connect to saved network: '%s'", ssid.c_str());
    WiFiCredentialsStorage::NetworkCredential networks[8];
    uint8_t count = m_credentialsStorage.loadNetworks(networks, 8);
    for (uint8_t i = 0; i < count; i++) {
        if (networks[i].ssid == ssid) {
            return connectToNetwork(ssid, networks[i].password);
        }
    }
    LW_LOGW("Network '%s' not found in saved credentials", ssid.c_str());
    return false;
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
