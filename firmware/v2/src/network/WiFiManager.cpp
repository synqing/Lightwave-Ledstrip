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

    // Register WiFi event handler
    WiFi.onEvent(onWiFiEvent);

    // Initialize credential storage (NVS-based)
    if (!m_credStorage.begin()) {
        LW_LOGE("Failed to initialize WiFiCredentialsStorage");
        // Continue anyway - storage is optional
    } else {
        uint8_t savedCount = m_credStorage.getNetworkCount();
        LW_LOGI("Credential storage initialized - %d networks saved", savedCount);
    }
    
    // AP-first architecture: Always start in AP-only mode
    // STA mode is only enabled when user explicitly requests connection
        WiFi.mode(WIFI_MODE_AP);
        m_apEnabled = true;
    m_forceApModeRuntime = true;  // Force AP-only mode by default
    
    LW_LOGI("AP-first mode enabled");
    LW_LOGI("  AP SSID: '%s'", m_apSSID.c_str());
        
    // Start AP immediately
        if (!m_apSSID.isEmpty()) {
        LW_LOGI("Starting Soft-AP: '%s'", m_apSSID.c_str());
            if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
            m_apStarted = true;
                LW_LOGI("AP started - IP: %s", WiFi.softAPIP().toString().c_str());
                xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
            } else {
                LW_LOGE("Failed to start Soft-AP");
        }
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

    // AP-first architecture: AP starts immediately in begin() and stays running.
    // STA mode is only enabled when user explicitly requests connection via API.

    // Main state machine loop
    while (true) {
        manager->applyPendingModeChange();

        // Optional auto-revert back to AP-only after a temporary STA window
        if (!manager->m_forceApModeRuntime && manager->m_pendingRevertToApOnly && manager->m_staWindowEndMs > 0) {
            uint32_t nowMs = millis();
            if ((int32_t)(nowMs - manager->m_staWindowEndMs) >= 0) {
                LW_LOGW("STA window expired, reverting to AP-only mode");
                manager->requestAPOnly();
            }
        }

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

    // AP-first architecture: STA connection is only initiated by user request via API
    // In normal operation, stay in AP mode unless explicitly requested to connect
    if (m_forceApModeRuntime || m_ssid.isEmpty() || m_ssid == "CONFIGURE_ME") {
        // Stay in AP mode - no auto-connection
        setState(STATE_WIFI_AP_MODE);
        return;
    }
    
    // User has requested STA connection - check if we have cached channel info
    if (m_bestChannel > 0 && (millis() - m_lastScanTime < SCAN_INTERVAL_MS)) {
        LW_LOGD("Using cached channel %d", m_bestChannel);
        setState(STATE_WIFI_CONNECTING);
    } else {
        LW_LOGI("Starting network scan for '%s'...", m_ssid.c_str());
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
        updateBestChannel();

        if (m_bestChannel > 0) {
            LW_LOGI("Best channel for '%s': %d", m_ssid.c_str(), m_bestChannel);
            setState(STATE_WIFI_CONNECTING);
        } else {
            LW_LOGW("Network '%s' not found", m_ssid.c_str());
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

    // APSTA window enforcement during connection attempt
    // If window expires while still connecting, transition to STA-only
    uint32_t nowMs = millis();
    if (m_apstaWindowEndMs > 0 && WiFi.getMode() == WIFI_MODE_APSTA) {
        if ((int32_t)(nowMs - m_apstaWindowEndMs) >= 0) {
            LW_LOGI("APSTA window expired during connection - switching to STA-only");
            m_apstaWindowEndMs = 0;

            // Stop AP and switch to STA-only
            if (m_apStarted) {
                WiFi.softAPdisconnect(true);
                m_apStarted = false;
                LW_LOGI("AP stopped during connection - heap reclaimed");
            }
            WiFi.mode(WIFI_MODE_STA);
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

    // Grace period: ignore disconnect flaps immediately after connect.
    if (millis() - m_connectedStateEntryTimeMs < CONNECTED_DISCONNECT_GRACE_MS) {
        return;
    }

    // APSTA window enforcement: transition to STA-only after window expires
    uint32_t nowMs = millis();
    if (m_apstaWindowEndMs > 0 && WiFi.getMode() == WIFI_MODE_APSTA) {
        if ((int32_t)(nowMs - m_apstaWindowEndMs) >= 0) {
            LW_LOGI("APSTA window expired - transitioning to STA-only mode (reclaiming heap)");
            m_apstaWindowEndMs = 0;

            // Stop AP and switch to STA-only
            if (m_apStarted) {
                WiFi.softAPdisconnect(true);  // true = stop DHCP server
                m_apStarted = false;
                LW_LOGI("AP stopped - freed heap");
            }
            WiFi.mode(WIFI_MODE_STA);

            size_t freeHeap = ESP.getFreeHeap();
            LW_LOGI("STA-only mode active - free heap: %u bytes", (unsigned)freeHeap);
        }
    }

    // Print status periodically (every 30 seconds)
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        LW_LOGI("Connected to '%s', RSSI: %d dBm, Channel: %d, Uptime: %ds",
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
        LW_LOGW("Disconnected from AP");
        setState(STATE_WIFI_DISCONNECTED);
    }
}

void WiFiManager::handleStateFailed() {
    LW_LOGD("STATE: FAILED");

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

    // AP-first architecture: If connection failed and all attempts exhausted, return to AP mode
    if (m_apEnabled && m_attemptsOnCurrentNetwork >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK && !hasSecondaryNetwork()) {
        LW_LOGW("STA connection failed - returning to AP-only mode");
        // Disconnect STA and return to AP mode
        WiFi.disconnect(true);
        WiFi.mode(WIFI_MODE_AP);
        // Ensure AP is still running
        if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && !m_apSSID.isEmpty()) {
            if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
                m_apStarted = true;
                LW_LOGI("AP restarted after STA failure - IP: %s (Tab5 can still connect)", 
                        WiFi.softAPIP().toString().c_str());
                xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
            }
        }
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

    // Print AP status periodically
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        LW_LOGI("AP Mode - SSID: '%s', IP: %s, Clients: %d",
                m_apSSID.c_str(),
                WiFi.softAPIP().toString().c_str(),
                WiFi.softAPgetStationNum());
    }

    // AP-first architecture: No automatic retry from AP mode
    // STA connection must be explicitly requested by user via API
}

void WiFiManager::handleStateDisconnected() {
    LW_LOGD("STATE: DISCONNECTED");

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

    // Heap-safe APSTA window policy:
    // Check if we're within APSTA window AND heap is healthy
    uint32_t nowMs = millis();
    bool withinApstaWindow = (m_apstaWindowEndMs > 0) && ((int32_t)(nowMs - m_apstaWindowEndMs) < 0);
    size_t freeHeap = ESP.getFreeHeap();
    size_t largestBlock = ESP.getMaxAllocHeap();
    bool heapHealthy = (freeHeap >= MIN_HEAP_FOR_APSTA) && (largestBlock >= MIN_LARGEST_BLOCK_FOR_APSTA);

    wifi_mode_t targetMode = WIFI_STA;  // Default to STA-only (heap-safe)

    if (withinApstaWindow && heapHealthy) {
        // APSTA allowed: keep AP active during STA connection
        targetMode = WIFI_AP_STA;
        LW_LOGI("APSTA window active (remaining: %ldms) and heap healthy (free=%u, largest=%u) - keeping AP active",
                (long)(m_apstaWindowEndMs - nowMs), (unsigned)freeHeap, (unsigned)largestBlock);
    } else {
        // STA-only: reclaim heap by disabling AP
        if (!withinApstaWindow && m_apstaWindowEndMs > 0) {
            LW_LOGI("APSTA window expired - switching to STA-only to reclaim heap");
        } else if (!heapHealthy) {
            LW_LOGW("Heap pressure detected (free=%u, largest=%u) - forcing STA-only mode (AP disabled)",
                    (unsigned)freeHeap, (unsigned)largestBlock);
        } else {
            LW_LOGI("No APSTA window requested - using STA-only mode");
        }
    }

    // Apply target mode
    if (WiFi.getMode() != targetMode) {
        WiFi.mode(targetMode);

        if (targetMode == WIFI_AP_STA) {
            // Ensure AP is still running after mode change
            if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && !m_apSSID.isEmpty()) {
                if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
                    m_apStarted = true;
                    LW_LOGI("AP restarted in APSTA mode - IP: %s", WiFi.softAPIP().toString().c_str());
                    xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
                }
            }
        } else {
            // STA-only: explicitly stop AP to reclaim memory
            if (m_apStarted) {
                WiFi.softAPdisconnect(true);  // true = stop DHCP server
                m_apStarted = false;
                LW_LOGI("AP stopped (STA-only mode) - heap reclaimed");
            }
        }
    }

    if (m_bestChannel > 0) {
        LW_LOGI("Connecting to '%s' on channel %d (mode: %s)",
                m_ssid.c_str(), m_bestChannel,
                targetMode == WIFI_AP_STA ? "APSTA" : "STA-only");
    } else {
        LW_LOGI("Connecting to '%s' (mode: %s)",
                m_ssid.c_str(),
                targetMode == WIFI_AP_STA ? "APSTA" : "STA-only");
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

    // AP-first architecture: Always use exclusive AP mode
        WiFi.mode(WIFI_MODE_AP);

    // Configure and start AP
    if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
        LW_LOGI("AP started - IP: %s", WiFi.softAPIP().toString().c_str());
        xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
    } else {
        LW_LOGE("Failed to start Soft-AP");
    }
}

void WiFiManager::applyPendingModeChange() {
    if (!m_pendingModeChange) return;

    // Allow a short delay so API responses can be sent before the interface changes.
    if (m_pendingApplyAtMs > 0) {
        uint32_t nowMs = millis();
        if ((int32_t)(nowMs - m_pendingApplyAtMs) < 0) {
            return;
        }
    }

    // Consume pending request atomically under mutex to keep state coherent.
    bool forceAp = false;
    bool revertToAp = false;
    uint32_t staWindowEndMs = 0;
    uint32_t apstaWindowDuration = 0;

    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    forceAp = m_pendingForceApModeRuntime;
    revertToAp = m_pendingRevertToApOnly;
    staWindowEndMs = m_staWindowEndMs;
    apstaWindowDuration = m_apstaWindowDurationMs;
    m_pendingModeChange = false;
    m_pendingApplyAtMs = 0;

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    if (forceAp) {
        // AP-first architecture: Force AP-only mode (disconnect STA but keep AP up)
        m_forceApModeRuntime = true;
        m_pendingRevertToApOnly = false;
        m_staWindowEndMs = 0;
        m_apstaWindowEndMs = 0;  // Clear APSTA window

        LW_LOGI("Force AP-only mode - disconnecting STA, keeping AP active");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_MODE_AP);
        
        // Ensure AP is running
        if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && !m_apSSID.isEmpty()) {
            if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
                m_apStarted = true;
                LW_LOGI("AP restarted - IP: %s", WiFi.softAPIP().toString().c_str());
                xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
            }
        }

        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // STA enable request (user-initiated connection) with APSTA window policy
    m_forceApModeRuntime = false;
    m_pendingRevertToApOnly = revertToAp;
    m_staWindowEndMs = staWindowEndMs;

    // Set APSTA window end time (will be checked in connectToAP for heap-safe conditional mode)
    uint32_t nowMs = millis();
    m_apstaWindowEndMs = nowMs + apstaWindowDuration;

    LW_LOGI("STA enable requested: APSTA window active for %lums (until window expires or heap pressure detected)",
            (unsigned long)apstaWindowDuration);

    // Mode will be determined in connectToAP() based on heap health and window state
    setState(STATE_WIFI_INIT);
}

void WiFiManager::requestSTAEnable(uint32_t durationMs, bool revertToApOnly) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    m_pendingModeChange = true;
    m_pendingForceApModeRuntime = false;
    m_pendingRevertToApOnly = revertToApOnly;
    m_staWindowEndMs = (durationMs > 0 && revertToApOnly) ? (millis() + durationMs) : 0;
    m_pendingApplyAtMs = millis() + 500;

    // Calculate APSTA window duration (bounded)
    // If durationMs provided, use it for APSTA window (clamped to safe range)
    // Otherwise use default APSTA window
    if (durationMs > 0) {
        // Clamp to safe range
        if (durationMs < MIN_APSTA_WINDOW_MS) {
            m_apstaWindowDurationMs = MIN_APSTA_WINDOW_MS;
        } else if (durationMs > MAX_APSTA_WINDOW_MS) {
            m_apstaWindowDurationMs = MAX_APSTA_WINDOW_MS;
        } else {
            m_apstaWindowDurationMs = durationMs;
        }
    } else {
        m_apstaWindowDurationMs = DEFAULT_APSTA_WINDOW_MS;
    }

    LW_LOGI("STA enable requested: APSTA window=%lums, revertToApOnly=%s",
            (unsigned long)m_apstaWindowDurationMs, revertToApOnly ? "true" : "false");

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }
}

void WiFiManager::requestAPOnly() {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    m_pendingModeChange = true;
    m_pendingForceApModeRuntime = true;
    m_pendingRevertToApOnly = false;
    m_staWindowEndMs = 0;
    m_apstaWindowEndMs = 0;  // Clear APSTA window
    m_apstaWindowDurationMs = 0;
    m_pendingApplyAtMs = millis() + 200;

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
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

void WiFiManager::setState(WiFiState newState) {
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Reset connected-state bookkeeping when leaving CONNECTED.
        if (m_currentState == STATE_WIFI_CONNECTED && newState != STATE_WIFI_CONNECTED) {
            m_inConnectedState = false;
            m_sleepSettingsApplied = false;
            m_connectedStateEntryTimeMs = 0;
        }
        
        // AP-first architecture: Reset AP started flag when leaving AP_MODE state
        // AP will be restarted when returning to AP_MODE state
        if (m_currentState == STATE_WIFI_AP_MODE && newState != STATE_WIFI_AP_MODE) {
            m_apStarted = false;
        }

        // Reset state-specific flags on entry to avoid persistence bugs
        // (previously used static variables that persisted across state exits)
        if (newState == STATE_WIFI_SCANNING) {
            m_scanStarted = false;
        } else if (newState == STATE_WIFI_CONNECTING) {
            m_connectStarted = false;
            m_connectStartTime = 0;
            // Clear stale connection bits to avoid false-positive connects.
            if (m_wifiEventGroup) {
                xEventGroupClearBits(m_wifiEventGroup,
                                     EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED);
            }
        } else if (newState == STATE_WIFI_AP_MODE) {
            if (m_apEnabled && !m_apStarted) {
                startSoftAP();
                m_apStarted = true;
            }
        }
        
        // AP-first architecture: No APSTA mode - AP is managed separately from STA states
        // When transitioning from AP_MODE to STA states, STA connection attempt will switch mode to STA-only

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
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    // Use provided credentials as primary, secondary from config
    String secondarySsid = NetworkConfig::WIFI_SSID_2_VALUE;
    String secondaryPassword = NetworkConfig::WIFI_PASSWORD_2_VALUE;
    
    // Store original primary credentials for switching back
    m_ssidPrimary = ssid;
    m_passwordPrimary = password;
    
    // Set active credentials to primary
        m_ssid = ssid;
        m_password = password;
        m_ssid2 = secondarySsid;
        m_password2 = secondaryPassword;
        
        if (hasSecondaryNetwork()) {
            LW_LOGI("Configured networks: %s (primary), %s (fallback)",
                    ssid.c_str(), m_ssid2.c_str());
        } else {
            LW_LOGI("Credentials set for '%s'", ssid.c_str());
    }
    
    m_currentNetworkIndex = 0;
    m_attemptsOnCurrentNetwork = 0;

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }
}

bool WiFiManager::hasSecondaryNetwork() const {
    return m_ssid2.length() > 0 && m_ssid2 != "";
}

void WiFiManager::switchToNextNetwork() {
    if (!hasSecondaryNetwork()) return;

    m_currentNetworkIndex = (m_currentNetworkIndex + 1) % 2;
    m_attemptsOnCurrentNetwork = 0;

    // Update active credentials based on network index
    if (m_currentNetworkIndex == 0) {
        // Switch back to primary network
        m_ssid = m_ssidPrimary;
        m_password = m_passwordPrimary;
    } else {
        // Switch to secondary network
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
    
    // AP-first architecture: Disconnect STA but keep AP running
    // Switch back to AP-only mode after disconnecting
    LW_LOGI("Disconnecting STA - returning to AP-only mode");
    WiFi.disconnect(true);
    
    // Switch to AP mode to ensure AP remains active
    WiFi.mode(WIFI_MODE_AP);
    
    // Ensure AP is still running
    if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && !m_apSSID.isEmpty()) {
        if (WiFi.softAP(m_apSSID.c_str(), m_apPassword.c_str(), m_apChannel)) {
            m_apStarted = true;
            LW_LOGI("AP restarted after disconnect - IP: %s", WiFi.softAPIP().toString().c_str());
            xEventGroupSetBits(m_wifiEventGroup, EVENT_AP_START);
        }
    }
    
    // Set flag to force AP-only mode
    m_forceApModeRuntime = true;
    setState(STATE_WIFI_AP_MODE);
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
// Credential Storage (AP-first architecture)
// ============================================================================

bool WiFiManager::addNetwork(const String& ssid, const String& password) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    bool result = m_credStorage.saveNetwork(ssid, password);

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    return result;
}

bool WiFiManager::connectToSavedNetwork(const String& ssid) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    // Check if network exists in storage
    if (!m_credStorage.hasNetwork(ssid)) {
        LW_LOGW("Network not found in saved networks: %s", ssid.c_str());
        if (m_stateMutex) {
            xSemaphoreGive(m_stateMutex);
        }
        return false;
    }

    // Load credentials from storage
    WiFiCredentialsStorage::NetworkCredential cred;
    uint8_t networkCount = m_credStorage.getNetworkCount();
    bool found = false;

    for (uint8_t i = 0; i < networkCount; i++) {
        if (m_credStorage.getNetwork(i, cred)) {
            if (cred.ssid == ssid) {
                found = true;
                break;
            }
        }
    }

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    if (!found) {
        LW_LOGE("Failed to load credentials for network: %s", ssid.c_str());
        return false;
    }

    // Use connectToNetwork with loaded credentials
    return connectToNetwork(cred.ssid, cred.password);
}

bool WiFiManager::connectToNetwork(const String& ssid, const String& password) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    // Save network to storage if not already saved (auto-save on first connection)
    if (!m_credStorage.hasNetwork(ssid)) {
        m_credStorage.saveNetwork(ssid, password);
        LW_LOGI("Auto-saved network to storage: %s", ssid.c_str());
    }

    // Set active credentials
    m_ssid = ssid;
    m_password = password;
    m_attemptsOnCurrentNetwork = 0;
    m_currentNetworkIndex = 0;  // Reset network index for new connection

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    // Request STA mode and initiate connection
    requestSTAEnable(0, true);  // No timeout, revert to AP-only after disconnect

    LW_LOGI("Connection to '%s' initiated", ssid.c_str());
    return true;
}

uint8_t WiFiManager::getSavedNetworks(WiFiCredentialsStorage::NetworkCredential* networks, uint8_t maxNetworks) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    uint8_t count = m_credStorage.loadNetworks(networks, maxNetworks);

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    return count;
}

bool WiFiManager::deleteSavedNetwork(const String& ssid) {
    if (m_stateMutex) {
        xSemaphoreTake(m_stateMutex, portMAX_DELAY);
    }

    bool result = m_credStorage.deleteNetwork(ssid);

    if (m_stateMutex) {
        xSemaphoreGive(m_stateMutex);
    }

    return result;
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
            LW_LOGD("Event: Scan complete");
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
            LW_LOGI("Event: Connected to AP");
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
            {
                LW_LOGI("Event: Got IP - %s", WiFi.localIP().toString().c_str());

                // CRITICAL: Disable WiFi sleep AFTER connection is fully established
                // This prevents ASSOC_LEAVE disconnects that occur when modem enters sleep
                WiFi.setSleep(false);
                WiFi.setAutoReconnect(true);

                // Also disable at ESP-IDF level for maximum reliability
                // WIFI_PS_NONE = 0 (no power save)
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

        // Disconnected from AP
#ifdef ARDUINO_EVENT_WIFI_STA_DISCONNECTED
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
#else
        case SYSTEM_EVENT_STA_DISCONNECTED:
#endif
            LW_LOGW("Event: Disconnected from AP");
            if (manager.m_wifiEventGroup) {
                // Only set EVENT_DISCONNECTED - let timeout handle connection failures
                // This avoids race conditions where disconnect happens but WiFi auto-reconnects
                xEventGroupSetBits(manager.m_wifiEventGroup, EVENT_DISCONNECTED);
            }
            break;

        // Auth mode changed
#ifdef ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
#else
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
#endif
            LW_LOGD("Event: Auth mode changed");
            break;

        // AP started
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

        // Station connected to our AP
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

        // Station disconnected from our AP
#ifdef ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
#else
        case SYSTEM_EVENT_AP_STADISCONNECTED:
#endif
            LW_LOGD("Event: Station disconnected from AP");
            break;

        default:
            // Ignore other events
            break;
    }
}

} // namespace network
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
