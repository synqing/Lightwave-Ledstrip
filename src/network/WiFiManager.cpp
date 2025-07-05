#include "WiFiManager.h"
#include <esp_wifi.h>

// Static member initialization
WiFiManager* WiFiManager::instance = nullptr;

bool WiFiManager::begin() {
    Serial.println("[WiFiManager] Starting non-blocking WiFi management");
    
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
        Serial.println("[WiFiManager] Failed to create WiFi task");
        vEventGroupDelete(wifiEventGroup);
        vSemaphoreDelete(stateMutex);
        return false;
    }
    
    Serial.printf("[WiFiManager] Task created on Core %d\n", TASK_CORE);
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
    
    Serial.println("[WiFiManager] Task started");
    
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
    Serial.println("[WiFiManager] STATE: INIT");
    
    // Check if we have credentials
    if (ssid.isEmpty()) {
        Serial.println("[WiFiManager] No credentials, switching to AP mode");
        setState(STATE_WIFI_AP_MODE);
        return;
    }
    
    // Check if we have cached channel info and it's recent
    if (bestChannel > 0 && (millis() - lastScanTime < SCAN_INTERVAL_MS)) {
        Serial.printf("[WiFiManager] Using cached channel %d\n", bestChannel);
        setState(STATE_WIFI_CONNECTING);
    } else {
        Serial.println("[WiFiManager] Starting network scan");
        setState(STATE_WIFI_SCANNING);
    }
}

void WiFiManager::handleStateScanning() {
    static bool scanStarted = false;
    
    if (!scanStarted) {
        Serial.println("[WiFiManager] STATE: SCANNING");
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
            Serial.printf("[WiFiManager] Best channel: %d\n", bestChannel);
            setState(STATE_WIFI_CONNECTING);
        } else {
            Serial.println("[WiFiManager] No suitable networks found");
            setState(STATE_WIFI_FAILED);
        }
    }
}

void WiFiManager::handleStateConnecting() {
    static uint32_t connectStartTime = 0;
    static bool connectStarted = false;
    
    if (!connectStarted) {
        Serial.println("[WiFiManager] STATE: CONNECTING");
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
        Serial.printf("[WiFiManager] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        setState(STATE_WIFI_CONNECTED);
    } else if (bits & EVENT_CONNECTION_FAILED || 
               (millis() - connectStartTime > CONNECT_TIMEOUT_MS)) {
        connectStarted = false;
        connectionAttempts++;
        Serial.println("[WiFiManager] Connection failed");
        setState(STATE_WIFI_FAILED);
    }
}

void WiFiManager::handleStateConnected() {
    static uint32_t lastStatusPrint = 0;
    
    // Print status periodically
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        Serial.printf("[WiFiManager] Connected to %s, RSSI: %d dBm, Channel: %d\n",
                      WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel());
    }
    
    // Check for disconnection event
    EventBits_t bits = xEventGroupWaitBits(
        wifiEventGroup,
        EVENT_DISCONNECTED,
        pdTRUE,  // Clear on exit
        pdFALSE, // Wait for any bit
        0        // Don't block
    );
    
    if (bits & EVENT_DISCONNECTED) {
        Serial.println("[WiFiManager] Disconnected from AP");
        setState(STATE_WIFI_DISCONNECTED);
    }
}

void WiFiManager::handleStateFailed() {
    Serial.println("[WiFiManager] STATE: FAILED");
    
    // If AP mode is enabled, we can stay in AP mode
    if (apEnabled) {
        Serial.println("[WiFiManager] Falling back to AP mode");
        setState(STATE_WIFI_AP_MODE);
        return;
    }
    
    // Otherwise, wait with backoff before retrying
    Serial.printf("[WiFiManager] Waiting %dms before retry\n", reconnectDelay);
    vTaskDelay(pdMS_TO_TICKS(reconnectDelay));
    
    // Exponential backoff
    reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);
    
    // Try again
    setState(STATE_WIFI_INIT);
}

void WiFiManager::handleStateAPMode() {
    static uint32_t lastStatusPrint = 0;
    
    // Print AP status periodically
    if (millis() - lastStatusPrint > 30000) {
        lastStatusPrint = millis();
        Serial.printf("[WiFiManager] AP Mode - SSID: %s, IP: %s, Clients: %d\n",
                      apSSID.c_str(), WiFi.softAPIP().toString().c_str(),
                      WiFi.softAPgetStationNum());
    }
    
    // Periodically try to connect to WiFi if we have credentials
    static uint32_t lastRetryTime = 0;
    if (!ssid.isEmpty() && (millis() - lastRetryTime > 60000)) {
        lastRetryTime = millis();
        Serial.println("[WiFiManager] Retrying WiFi connection from AP mode");
        setState(STATE_WIFI_INIT);
    }
}

void WiFiManager::handleStateDisconnected() {
    Serial.println("[WiFiManager] STATE: DISCONNECTED");
    
    // Wait a bit before reconnecting
    vTaskDelay(pdMS_TO_TICKS(reconnectDelay));
    
    // Try to reconnect
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
    
    Serial.printf("[WiFiManager] Connecting to %s", ssid.c_str());
    if (bestChannel > 0) {
        Serial.printf(" on channel %d", bestChannel);
    }
    Serial.println();
    
    // Configure static IP if requested
    if (useStaticIP) {
        if (!WiFi.config(staticIP, gateway, subnet, dns1, dns2)) {
            Serial.println("[WiFiManager] Failed to configure static IP");
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
    Serial.printf("[WiFiManager] Starting Soft-AP: %s\n", apSSID.c_str());
    
    // Configure AP
    if (WiFi.softAP(apSSID.c_str(), apPassword.c_str(), apChannel)) {
        Serial.printf("[WiFiManager] AP started - IP: %s\n", 
                      WiFi.softAPIP().toString().c_str());
        xEventGroupSetBits(wifiEventGroup, EVENT_AP_START);
    } else {
        Serial.println("[WiFiManager] Failed to start AP");
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
    
    Serial.printf("[WiFiManager] Found %d networks\n", n);
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

// WiFi event handler
void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    WiFiManager& manager = getInstance();
    
    switch (event) {
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("[WiFiManager] Scan complete");
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_SCAN_COMPLETE);
            break;
            
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("[WiFiManager] Connected to AP");
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_CONNECTED);
            break;
            
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.printf("[WiFiManager] Got IP: %s\n", WiFi.localIP().toString().c_str());
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_GOT_IP);
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("[WiFiManager] Disconnected from AP");
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_DISCONNECTED);
            break;
            
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("[WiFiManager] Auth mode changed");
            break;
            
        case SYSTEM_EVENT_AP_START:
            Serial.println("[WiFiManager] AP started");
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_AP_START);
            break;
            
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("[WiFiManager] Station connected to AP");
            xEventGroupSetBits(manager.wifiEventGroup, EVENT_AP_STACONNECTED);
            break;
            
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("[WiFiManager] Station disconnected from AP");
            break;
            
        default:
            break;
    }
}