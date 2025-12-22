#include "WiFiManager.h"

#if FEATURE_WEB_SERVER

#include <esp_wifi.h>

// Static member initialization
WiFiManager* WiFiManager::instance = nullptr;

bool WiFiManager::begin() {
    // Create synchronization primitives
    wifiEventGroup = xEventGroupCreate();
    if (!wifiEventGroup) {
        Serial.println("[WiFiManager] Failed to create event group");
        return false;
    }
    
    stateMutex = xSemaphoreCreateMutex();
    if (!stateMutex) {
        Serial.println("[WiFiManager] Failed to create mutex");
        vEventGroupDelete(wifiEventGroup);
        return false;
    }
    
    // Register WiFi event handler
    WiFi.onEvent(onWiFiEvent);
    
    // Set WiFi mode to STA+AP for maximum flexibility
    WiFi.mode(WIFI_MODE_APSTA);
    
    // Create WiFi management task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiManager",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &wifiTaskHandle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        Serial.println("[WiFi] Task creation failed");
        vEventGroupDelete(wifiEventGroup);
        vSemaphoreDelete(stateMutex);
        return false;
    }

    return true;
}

void WiFiManager::stop() {
    if (wifiTaskHandle) {
        vTaskDelete(wifiTaskHandle);
        wifiTaskHandle = nullptr;
    }
    
    if (wifiEventGroup) {
        vEventGroupDelete(wifiEventGroup);
        wifiEventGroup = nullptr;
    }
    
    if (stateMutex) {
        vSemaphoreDelete(stateMutex);
        stateMutex = nullptr;
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void WiFiManager::wifiTask(void* parameter) {
    WiFiManager* manager = static_cast<WiFiManager*>(parameter);

    // Start Soft-AP immediately if enabled
    if (manager->apEnabled) {
        manager->startSoftAP();
    }
    
    while (true) {
        // Handle current state
        switch (manager->currentState) {
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

void WiFiManager::handleStateInit() {
    // Check if we have credentials
    if (ssid.isEmpty()) {
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Check if we have cached channel info and it's recent
    if (bestChannel > 0 && (millis() - lastScanTime < SCAN_INTERVAL_MS)) {
        setState(STATE_WIFI_CONNECTING);
    } else {
        setState(STATE_WIFI_SCANNING);
    }
}

void WiFiManager::handleStateScanning() {
    static bool scanStarted = false;

    if (!scanStarted) {
        performAsyncScan();
        scanStarted = true;
    }

    // Wait for scan complete event
    EventBits_t bits = xEventGroupWaitBits(
        wifiEventGroup,
        EVENT_SCAN_COMPLETE,
        pdTRUE,  // Clear on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(100)
    );

    if (bits & EVENT_SCAN_COMPLETE) {
        scanStarted = false;
        updateBestChannel();

        if (bestChannel > 0) {
            setState(STATE_WIFI_CONNECTING);
        } else {
            setState(STATE_WIFI_FAILED);
        }
    }
}

void WiFiManager::handleStateConnecting() {
    static uint32_t connectStartTime = 0;
    static bool connectStarted = false;

    if (!connectStarted) {
        connectStartTime = millis();
        connectStarted = connectToAP();

        if (!connectStarted) {
            setState(STATE_WIFI_FAILED);
            return;
        }
    }

    // Wait for connection events
    EventBits_t bits = xEventGroupWaitBits(
        wifiEventGroup,
        EVENT_CONNECTED | EVENT_GOT_IP | EVENT_CONNECTION_FAILED,
        pdTRUE,  // Clear on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(100)
    );

    if (bits & EVENT_GOT_IP) {
        connectStarted = false;
        successfulConnections++;
        lastConnectionTime = millis();
        reconnectDelay = RECONNECT_DELAY_MS; // Reset backoff
        attemptsOnCurrentNetwork = 0;         // Reset attempt counter
        Serial.printf("[WiFi] Connected to %s: %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
        setState(STATE_WIFI_CONNECTED);
    } else if (bits & EVENT_CONNECTION_FAILED ||
               (millis() - connectStartTime > CONNECT_TIMEOUT_MS)) {
        connectStarted = false;
        connectionAttempts++;
        setState(STATE_WIFI_FAILED);
    }
}

void WiFiManager::handleStateConnected() {
    // Check for disconnection event (no periodic status spam)
    EventBits_t bits = xEventGroupWaitBits(
        wifiEventGroup,
        EVENT_DISCONNECTED,
        pdTRUE,  // Clear on exit
        pdFALSE, // Wait for any bit
        0        // Don't block
    );

    if (bits & EVENT_DISCONNECTED) {
        Serial.println("[WiFi] Disconnected");
        setState(STATE_WIFI_DISCONNECTED);
    }
}

void WiFiManager::handleStateFailed() {
    attemptsOnCurrentNetwork++;

    Serial.printf("[WiFi] Connection failed (%d/%d attempts on %s)\n",
                  attemptsOnCurrentNetwork, NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK, ssid.c_str());

    // Check if we should switch to next network
    if (attemptsOnCurrentNetwork >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK) {
        if (hasSecondaryNetwork()) {
            switchToNextNetwork();
            reconnectDelay = RECONNECT_DELAY_MS;  // Reset backoff for new network
            setState(STATE_WIFI_INIT);
            return;
        }
    }

    // If AP mode is enabled and we've exhausted all networks, fall back to it
    if (apEnabled && attemptsOnCurrentNetwork >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK && !hasSecondaryNetwork()) {
        setState(STATE_WIFI_AP_MODE);
        return;
    }

    // Otherwise, wait with backoff before retrying same network
    vTaskDelay(pdMS_TO_TICKS(reconnectDelay));

    // Exponential backoff
    reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);

    // Try again
    setState(STATE_WIFI_INIT);
}

void WiFiManager::handleStateAPMode() {
    // Stay in AP mode until device restart (no automatic retry)
    static uint32_t lastStatusPrint = 0;
    static bool initialPrint = true;

    // Print status once on entry, then every 60 seconds (reduced spam)
    if (initialPrint || (millis() - lastStatusPrint > 60000)) {
        lastStatusPrint = millis();
        initialPrint = false;
        Serial.printf("[WiFi] AP: %s @ %s\n",
                      apSSID.c_str(), WiFi.softAPIP().toString().c_str());
    }
}

void WiFiManager::handleStateDisconnected() {
    // Wait before reconnecting
    vTaskDelay(pdMS_TO_TICKS(reconnectDelay));
    setState(STATE_WIFI_INIT);
}

void WiFiManager::performAsyncScan() {
    // Clear previous results
    cachedScanResults.clear();
    
    // Start async scan
    WiFi.scanNetworks(true, false, false, 300); // async, show_hidden=false, passive=false, max_ms=300
}

bool WiFiManager::connectToAP() {
    connectionAttempts++;

    Serial.printf("[WiFi] Connecting to %s...\n", ssid.c_str());

    // Configure static IP if requested
    if (useStaticIP) {
        if (!WiFi.config(staticIP, gateway, subnet, dns1, dns2)) {
            return false;
        }
    }
    
    // Set hostname before connecting
    WiFi.setHostname(NetworkConfig::MDNS_HOSTNAME);
    
    // Connect with channel hint if available
    if (bestChannel > 0) {
        // Get BSSID of best AP for this SSID
        uint8_t* bssid = nullptr;
        for (const auto& scan : cachedScanResults) {
            if (scan.ssid == ssid && scan.channel == bestChannel) {
                bssid = (uint8_t*)scan.bssid;
                break;
            }
        }
        WiFi.begin(ssid.c_str(), password.c_str(), bestChannel, bssid);
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    
    return true;
}

void WiFiManager::startSoftAP() {
    // Configure AP
    if (WiFi.softAP(apSSID.c_str(), apPassword.c_str(), apChannel)) {
        Serial.printf("[WiFi] AP: %s @ %s\n", apSSID.c_str(),
                      WiFi.softAPIP().toString().c_str());
        xEventGroupSetBits(wifiEventGroup, EVENT_AP_START);
    }
}

void WiFiManager::updateBestChannel() {
    bestChannel = 0;
    int bestRSSI = -100;
    
    // Get scan results
    int n = WiFi.scanComplete();
    if (n <= 0) return;
    
    // Store results and find best channel
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
        
        cachedScanResults.push_back(result);
        
        // Check if this is our target SSID
        if (result.ssid == ssid && result.rssi > bestRSSI) {
            bestRSSI = result.rssi;
            bestChannel = result.channel;
        }
    }
    
    lastScanTime = millis();
    WiFi.scanDelete(); // Clean up scan results
}

void WiFiManager::setState(WiFiState newState) {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentState = newState;
        xSemaphoreGive(stateMutex);
    }
}

WiFiManager::WiFiState WiFiManager::getState() const {
    WiFiState state = STATE_WIFI_INIT;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = currentState;
        xSemaphoreGive(stateMutex);
    }
    return state;
}

String WiFiManager::getStateString() const {
    switch (getState()) {
        case STATE_WIFI_INIT: return "INIT";
        case STATE_WIFI_SCANNING: return "SCANNING";
        case STATE_WIFI_CONNECTING: return "CONNECTING";
        case STATE_WIFI_CONNECTED: return "CONNECTED";
        case STATE_WIFI_FAILED: return "FAILED";
        case STATE_WIFI_AP_MODE: return "AP_MODE";
        case STATE_WIFI_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

void WiFiManager::setCredentials(const String& newSSID, const String& newPassword) {
    ssid = newSSID;
    password = newPassword;
    // Also load secondary network from config if available
    ssid2 = NetworkConfig::WIFI_SSID_2_VALUE;
    password2 = NetworkConfig::WIFI_PASSWORD_2_VALUE;
    currentNetworkIndex = 0;
    attemptsOnCurrentNetwork = 0;

    if (hasSecondaryNetwork()) {
        Serial.printf("[WiFi] Configured networks: %s (primary), %s (fallback)\n",
                      ssid.c_str(), ssid2.c_str());
    }
}

bool WiFiManager::hasSecondaryNetwork() const {
    return ssid2.length() > 0 && ssid2 != "";
}

void WiFiManager::switchToNextNetwork() {
    if (!hasSecondaryNetwork()) return;

    currentNetworkIndex = (currentNetworkIndex + 1) % 2;
    attemptsOnCurrentNetwork = 0;

    // Update active credentials
    if (currentNetworkIndex == 0) {
        ssid = NetworkConfig::WIFI_SSID_VALUE;
        password = NetworkConfig::WIFI_PASSWORD_VALUE;
    } else {
        ssid = ssid2;
        password = password2;
    }

    Serial.printf("[WiFi] Switching to network: %s\n", ssid.c_str());

    // Clear cached channel info for new network
    bestChannel = 0;
}

void WiFiManager::setStaticIP(const IPAddress& ip, const IPAddress& gw, 
                             const IPAddress& sn, const IPAddress& d1, 
                             const IPAddress& d2) {
    useStaticIP = true;
    staticIP = ip;
    gateway = gw;
    subnet = sn;
    dns1 = d1;
    dns2 = d2;
}

void WiFiManager::enableSoftAP(const String& apName, const String& apPass, uint8_t channel) {
    apEnabled = true;
    apSSID = apName;
    apPassword = apPass;
    apChannel = channel;
}

uint32_t WiFiManager::getUptimeSeconds() const {
    if (lastConnectionTime > 0 && currentState == STATE_WIFI_CONNECTED) {
        return (millis() - lastConnectionTime) / 1000;
    }
    return 0;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(false);
    setState(STATE_WIFI_DISCONNECTED);
}

void WiFiManager::reconnect() {
    disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    setState(STATE_WIFI_INIT);
}

void WiFiManager::scanNetworks() {
    if (currentState != STATE_WIFI_SCANNING) {
        setState(STATE_WIFI_SCANNING);
    }
}

// WiFi event handler - minimal logging
void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    WiFiManager& manager = getInstance();

    switch (event) {
        case SYSTEM_EVENT_SCAN_DONE:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_SCAN_COMPLETE);
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_CONNECTED);
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_GOT_IP);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_DISCONNECTED);
            break;

        case SYSTEM_EVENT_AP_START:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_AP_START);
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_AP_STACONNECTED);
            break;

        default:
            break;
    }
}
#endif // FEATURE_WEB_SERVER
