#include "WiFiManagerV2.h"
#include <esp_event.h>

// Static member initialization
WiFiManagerV2* WiFiManagerV2::instance = nullptr;

// WiFi event handler
void WiFiEventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data) {
    WiFiManagerV2& manager = WiFiManagerV2::getInstance();
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                Serial.println("[WiFiV2] Station started");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                Serial.println("[WiFiV2] Disconnected from AP");
                manager.notifyEvent(WiFiManagerV2::EVENT_DISCONNECTED);
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                Serial.println("[WiFiV2] Connected to AP");
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                Serial.println("[WiFiV2] Scan complete");
                manager.notifyEvent(WiFiManagerV2::EVENT_SCAN_COMPLETE);
                break;
                
            case WIFI_EVENT_AP_START:
                Serial.println("[WiFiV2] AP started");
                manager.notifyEvent(WiFiManagerV2::EVENT_AP_STARTED);
                break;
                
            case WIFI_EVENT_AP_STACONNECTED:
                Serial.println("[WiFiV2] Client connected to AP");
                manager.notifyEvent(WiFiManagerV2::EVENT_AP_CLIENT_CONNECTED);
                break;
                
            case WIFI_EVENT_AP_STADISCONNECTED:
                Serial.println("[WiFiV2] Client disconnected from AP");
                manager.notifyEvent(WiFiManagerV2::EVENT_AP_CLIENT_DISCONNECTED);
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            Serial.printf("[WiFiV2] Got IP: %s\n", IPAddress(event->ip_info.ip.addr).toString().c_str());
            manager.notifyEvent(WiFiManagerV2::EVENT_CONNECTED);
        }
    }
}

bool WiFiManagerV2::begin() {
    Serial.println("\n=== WiFiManagerV2 Initialization ===");
    
    // Create synchronization primitives
    stateMutex = xSemaphoreCreateMutex();
    wifiEventGroup = xEventGroupCreate();
    commandQueue = xQueueCreate(10, sizeof(CommandMessage));
    
    if (!stateMutex || !wifiEventGroup || !commandQueue) {
        Serial.println("[WiFiV2] Failed to create sync primitives");
        return false;
    }
    
    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WiFiEventHandler,
                                                        nullptr,
                                                        &instance_any_id));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WiFiEventHandler,
                                                        nullptr,
                                                        &instance_got_ip));
    
    // Create WiFi task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiTaskV2",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &wifiTaskHandle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        Serial.println("[WiFiV2] Failed to create task");
        return false;
    }
    
    Serial.println("[WiFiV2] ✅ Task started on Core 0");
    Serial.println("[WiFiV2] ✅ Non-blocking operation enabled");
    Serial.println("[WiFiV2] ✅ Immediate AP fallback enabled");
    
    return true;
}

void WiFiManagerV2::stop() {
    if (wifiTaskHandle) {
        vTaskDelete(wifiTaskHandle);
        wifiTaskHandle = nullptr;
    }
    
    if (stateMutex) {
        vSemaphoreDelete(stateMutex);
        stateMutex = nullptr;
    }
    
    if (wifiEventGroup) {
        vEventGroupDelete(wifiEventGroup);
        wifiEventGroup = nullptr;
    }
    
    if (commandQueue) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }
}

void WiFiManagerV2::setCredentials(const String& ssid, const String& password) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    targetSSID = ssid;
    targetPassword = password;
    xSemaphoreGive(stateMutex);
}

void WiFiManagerV2::setStaticIP(const IPAddress& ip, const IPAddress& gw, 
                                const IPAddress& mask, const IPAddress& d1, 
                                const IPAddress& d2) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    useStaticIP = true;
    staticIP = ip;
    gateway = gw;
    subnet = mask;
    dns1 = d1;
    dns2 = d2;
    xSemaphoreGive(stateMutex);
}

void WiFiManagerV2::configureAP(const String& ssid, const String& password, 
                                uint8_t channel, uint8_t maxClients) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    apSSID = ssid;
    apPassword = password;
    apChannel = channel ? channel : 6;
    maxAPClients = maxClients;
    xSemaphoreGive(stateMutex);
}

void WiFiManagerV2::connect() {
    CommandMessage cmd = {CMD_CONNECT, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

void WiFiManagerV2::disconnect() {
    CommandMessage cmd = {CMD_DISCONNECT, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

void WiFiManagerV2::scan() {
    CommandMessage cmd = {CMD_SCAN, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

void WiFiManagerV2::startAP() {
    CommandMessage cmd = {CMD_START_AP, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

void WiFiManagerV2::stopAP() {
    CommandMessage cmd = {CMD_STOP_AP, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

void WiFiManagerV2::reset() {
    CommandMessage cmd = {CMD_RESET, nullptr};
    xQueueSend(commandQueue, &cmd, 0);
}

WiFiManagerV2::WiFiState WiFiManagerV2::getState() const {
    WiFiState state;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    state = currentState;
    xSemaphoreGive(stateMutex);
    return state;
}

String WiFiManagerV2::getStateString() const {
    switch (getState()) {
        case STATE_INIT: return "Initializing";
        case STATE_SCANNING: return "Scanning";
        case STATE_CONNECTING: return "Connecting";
        case STATE_CONNECTED: return "Connected";
        case STATE_CONNECTION_FAILED: return "Connection Failed";
        case STATE_AP_MODE: return "AP Mode";
        case STATE_DISCONNECTED: return "Disconnected";
        case STATE_AP_STA_MODE: return "AP+STA Mode";
        default: return "Unknown";
    }
}

uint32_t WiFiManagerV2::getUptime() const {
    if (stats.current_session_start > 0) {
        return millis() - stats.current_session_start;
    }
    return 0;
}

float WiFiManagerV2::getSuccessRate() const {
    if (stats.attempts == 0) return 0.0f;
    return (float)stats.successes / (float)stats.attempts * 100.0f;
}

void WiFiManagerV2::wifiTask(void* parameter) {
    WiFiManagerV2* manager = static_cast<WiFiManagerV2*>(parameter);
    
    Serial.println("[WiFiV2 Task] Started on Core 0");
    Serial.printf("[WiFiV2 Task] Free heap: %d bytes\n", ESP.getFreeHeap());
    
    // Initialize WiFi
    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    
    // Main task loop
    while (true) {
        // Check for commands
        CommandMessage cmd;
        if (xQueueReceive(manager->commandQueue, &cmd, 0) == pdTRUE) {
            switch (cmd.cmd) {
                case CMD_CONNECT:
                    if (manager->currentState == STATE_DISCONNECTED || 
                        manager->currentState == STATE_INIT) {
                        manager->currentState = STATE_CONNECTING;
                    }
                    break;
                    
                case CMD_DISCONNECT:
                    WiFi.disconnect(true);
                    manager->currentState = STATE_DISCONNECTED;
                    break;
                    
                case CMD_SCAN:
                    if (!manager->scanInProgress) {
                        manager->startAsyncScan();
                    }
                    break;
                    
                case CMD_START_AP:
                    manager->startAPInternal();
                    break;
                    
                case CMD_STOP_AP:
                    manager->stopAPInternal();
                    break;
                    
                case CMD_RESET:
                    manager->currentState = STATE_INIT;
                    manager->consecutiveFailures = 0;
                    manager->currentRetryDelay = MIN_RETRY_DELAY_MS;
                    break;
            }
        }
        
        // Run state machine
        manager->runStateMachine();
        
        // Update adaptive TX power if connected
        if (manager->currentState == STATE_CONNECTED) {
            uint32_t now = millis();
            if (now - manager->lastTxPowerUpdate > 5000) {
                manager->updateAdaptiveTxPower();
                manager->lastTxPowerUpdate = now;
            }
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void WiFiManagerV2::runStateMachine() {
    // Save previous state for change detection
    WiFiState oldState = previousState;
    previousState = currentState;
    
    // Handle state transitions
    switch (currentState) {
        case STATE_INIT:
            handleInit();
            break;
            
        case STATE_SCANNING:
            handleScanning();
            break;
            
        case STATE_CONNECTING:
            handleConnecting();
            break;
            
        case STATE_CONNECTED:
            handleConnected();
            break;
            
        case STATE_CONNECTION_FAILED:
            handleConnectionFailed();
            break;
            
        case STATE_AP_MODE:
            handleAPMode();
            break;
            
        case STATE_DISCONNECTED:
            handleDisconnected();
            break;
            
        case STATE_AP_STA_MODE:
            handleAPSTAMode();
            break;
    }
    
    // Notify state change
    if (currentState != oldState) {
        notifyEvent(EVENT_STATE_CHANGED, &currentState);
        Serial.printf("[WiFiV2] State: %s -> %s\n", 
                     getStateString().c_str(),
                     ((WiFiManagerV2*)this)->getStateString().c_str());
    }
}

void WiFiManagerV2::handleInit() {
    Serial.println("[WiFiV2] Initializing WiFi...");
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection ourselves
    WiFi.persistent(false);         // Don't save credentials to flash
    
    // Enable 802.11n for better performance
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    
    // Configure TX power to maximum initially
    esp_wifi_set_max_tx_power(TX_POWER_MAX);
    currentTxPower = TX_POWER_MAX;
    
    // Start initial scan
    startAsyncScan();
    currentState = STATE_SCANNING;
}

void WiFiManagerV2::handleScanning() {
    // Check if scan is complete
    int16_t n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_RUNNING) {
        // Still scanning
        return;
    }
    
    if (n == WIFI_SCAN_FAILED) {
        Serial.println("[WiFiV2] Scan failed");
        scanInProgress = false;
        
        // Try to connect anyway if we have credentials
        if (!targetSSID.isEmpty()) {
            currentState = STATE_CONNECTING;
        }
        return;
    }
    
    // Process scan results
    if (n >= 0) {
        processScanResults();
        scanInProgress = false;
        
        // Attempt connection if we have credentials
        if (!targetSSID.isEmpty()) {
            currentState = STATE_CONNECTING;
            connectionStartTime = millis();
        } else {
            // No credentials - start AP mode
            Serial.println("[WiFiV2] No credentials configured - starting AP");
            startAP();
            currentState = STATE_AP_MODE;
        }
    }
}

void WiFiManagerV2::handleConnecting() {
    static uint32_t connectAttemptStart = 0;
    
    // First connection attempt
    if (connectAttemptStart == 0) {
        connectAttemptStart = millis();
        
        Serial.printf("[WiFiV2] Connecting to %s...\n", targetSSID.c_str());
        stats.attempts++;
        
        // Attempt connection
        if (!attemptConnection()) {
            Serial.println("[WiFiV2] Connection attempt failed to start");
            currentState = STATE_CONNECTION_FAILED;
            connectAttemptStart = 0;
            return;
        }
        
        // If immediate AP fallback is enabled, start AP while connecting
        if (immediateAPFallback && !apEnabled) {
            Serial.println("[WiFiV2] Starting AP for immediate fallback");
            startAP();
            // Don't change state - we're still trying to connect
        }
    }
    
    // Check connection status
    if (WiFi.status() == WL_CONNECTED) {
        // Success!
        stats.successes++;
        stats.current_session_start = millis();
        consecutiveFailures = 0;
        currentRetryDelay = MIN_RETRY_DELAY_MS;
        
        Serial.printf("[WiFiV2] Connected! IP: %s, RSSI: %d dBm\n", 
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
        
        // Update statistics
        int32_t rssi = WiFi.RSSI();
        stats.best_rssi = max(stats.best_rssi, rssi);
        stats.worst_rssi = min(stats.worst_rssi, rssi);
        
        // Stop AP if it was started for fallback
        if (apEnabled && immediateAPFallback) {
            Serial.println("[WiFiV2] Connected to WiFi - stopping fallback AP");
            stopAP();
        }
        
        currentState = STATE_CONNECTED;
        connectAttemptStart = 0;
        
    } else if (millis() - connectAttemptStart > INITIAL_CONNECT_TIMEOUT_MS) {
        // Timeout
        Serial.println("[WiFiV2] Connection timeout");
        WiFi.disconnect(true);
        currentState = STATE_CONNECTION_FAILED;
        connectAttemptStart = 0;
        stats.failures++;
    }
}

void WiFiManagerV2::handleConnected() {
    // Monitor connection health
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFiV2] Connection lost");
        stats.total_uptime_ms += (millis() - stats.current_session_start);
        stats.current_session_start = 0;
        currentState = STATE_DISCONNECTED;
        return;
    }
    
    // Update RSSI statistics
    static uint32_t lastRSSIUpdate = 0;
    if (millis() - lastRSSIUpdate > 1000) {
        int32_t rssi = WiFi.RSSI();
        stats.average_rssi = (stats.average_rssi * 0.9f) + (rssi * 0.1f);
        lastRSSIUpdate = millis();
    }
    
    // Periodic scan while connected (to find better APs)
    if (millis() - lastScanTime > SCAN_INTERVAL_MS && !scanInProgress) {
        startAsyncScan();
    }
}

void WiFiManagerV2::handleConnectionFailed() {
    consecutiveFailures++;
    
    Serial.printf("[WiFiV2] Connection failed (%d consecutive failures)\n", 
                 consecutiveFailures);
    
    // Start or ensure AP is running
    if (apAutoFallback && !apEnabled) {
        Serial.println("[WiFiV2] Starting AP fallback");
        startAP();
        currentState = STATE_AP_STA_MODE;  // Try both AP and STA
    } else if (apEnabled) {
        // AP is already running from immediate fallback
        currentState = STATE_AP_STA_MODE;
    } else {
        currentState = STATE_DISCONNECTED;
    }
    
    // Calculate retry delay
    calculateRetryDelay();
    lastConnectionAttempt = millis();
}

void WiFiManagerV2::handleDisconnected() {
    // Wait for retry delay
    if (millis() - lastConnectionAttempt < currentRetryDelay) {
        return;
    }
    
    // Perform periodic scan
    if (millis() - lastScanTime > QUICK_SCAN_INTERVAL_MS && !scanInProgress) {
        startAsyncScan();
    }
    
    // Retry connection if we have credentials
    if (!targetSSID.isEmpty()) {
        Serial.printf("[WiFiV2] Retrying connection (delay was %dms)\n", currentRetryDelay);
        currentState = STATE_CONNECTING;
        notifyEvent(EVENT_CONNECTION_RETRY);
    }
}

void WiFiManagerV2::handleAPMode() {
    // Pure AP mode - no STA connection attempts
    static bool apStarted = false;
    
    if (!apStarted && apEnabled) {
        Serial.printf("[WiFiV2] AP Mode: SSID=%s, IP=%s\n", 
                     apSSID.c_str(), WiFi.softAPIP().toString().c_str());
        apStarted = true;
    }
    
    // Monitor AP clients
    static uint32_t lastClientCheck = 0;
    if (millis() - lastClientCheck > 5000) {
        uint8_t clients = WiFi.softAPgetStationNum();
        if (clients > 0) {
            Serial.printf("[WiFiV2] AP: %d client(s) connected\n", clients);
        }
        lastClientCheck = millis();
    }
}

void WiFiManagerV2::handleAPSTAMode() {
    // Simultaneous AP + STA mode
    handleAPMode();  // Handle AP functions
    
    // Also try to connect to WiFi
    if (!targetSSID.isEmpty() && WiFi.status() != WL_CONNECTED) {
        if (millis() - lastConnectionAttempt >= currentRetryDelay) {
            Serial.println("[WiFiV2] AP+STA: Retrying WiFi connection");
            currentState = STATE_CONNECTING;
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        // Connected while in AP mode
        Serial.println("[WiFiV2] AP+STA: WiFi connected, maintaining AP");
        currentState = STATE_CONNECTED;
        
        // Optionally stop AP after successful connection
        // (User can control this with setAPAutoFallback)
        if (!apAutoFallback) {
            stopAP();
        }
    }
}

void WiFiManagerV2::startAsyncScan() {
    if (scanInProgress) return;
    
    Serial.println("[WiFiV2] Starting async network scan");
    WiFi.scanNetworks(true, false, false, 300);  // Async, no hidden, no passive, 300ms/channel
    scanInProgress = true;
    lastScanTime = millis();
}

void WiFiManagerV2::processScanResults() {
    int16_t n = WiFi.scanComplete();
    if (n <= 0) return;
    
    Serial.printf("[WiFiV2] Found %d networks\n", n);
    
    // Clear old results
    scanResults.clear();
    
    // Process each network
    for (int i = 0; i < n; i++) {
        ScanResult result;
        result.ssid = WiFi.SSID(i);
        result.rssi = WiFi.RSSI(i);
        result.channel = WiFi.channel(i);
        uint8_t* bssid = WiFi.BSSID(i);
        if (bssid) {
            memcpy(result.bssid, bssid, 6);
        }
        result.encryption = (wifi_auth_mode_t)WiFi.encryptionType(i);
        
        // Check capabilities (simplified - would need more detailed parsing)
        result.supports_11n = true;  // Most modern APs support 11n
        result.supports_11lr = false; // LR support is rare
        
        scanResults.push_back(result);
        
        // Debug output for strong signals or target SSID
        if (result.rssi > -70 || result.ssid == targetSSID) {
            Serial.printf("  %s (Ch:%d, %d dBm)\n", 
                         result.ssid.c_str(), result.channel, result.rssi);
        }
    }
    
    // Clean up scan
    WiFi.scanDelete();
    
    // Select best channel based on scan
    preferredChannel = selectBestChannel();
}

bool WiFiManagerV2::attemptConnection() {
    // Configure static IP if requested
    if (useStaticIP) {
        if (!WiFi.config(staticIP, gateway, subnet, dns1, dns2)) {
            Serial.println("[WiFiV2] Failed to configure static IP");
            return false;
        }
    }
    
    // Find best AP for target SSID
    int bestIndex = -1;
    int32_t bestRSSI = -100;
    uint8_t bestChannel = 0;
    
    for (size_t i = 0; i < scanResults.size(); i++) {
        if (scanResults[i].ssid == targetSSID && scanResults[i].rssi > bestRSSI) {
            bestIndex = i;
            bestRSSI = scanResults[i].rssi;
            bestChannel = scanResults[i].channel;
        }
    }
    
    // Use scan results to optimize connection
    if (bestIndex >= 0) {
        Serial.printf("[WiFiV2] Found target AP on channel %d with RSSI %d dBm\n", 
                     bestChannel, bestRSSI);
        
        // Connect with specific BSSID and channel for faster connection
        WiFi.begin(targetSSID.c_str(), targetPassword.c_str(), 
                  bestChannel, scanResults[bestIndex].bssid, true);
    } else {
        // No scan results - try anyway
        WiFi.begin(targetSSID.c_str(), targetPassword.c_str());
    }
    
    return true;
}

void WiFiManagerV2::startAPInternal() {
    if (apEnabled) return;
    
    Serial.printf("[WiFiV2] Starting AP: %s\n", apSSID.c_str());
    
    // Configure AP
    if (apChannel == 0) {
        apChannel = preferredChannel ? preferredChannel : 6;
    }
    
    // Set AP mode (STA+AP if we're trying to connect)
    if (currentState == STATE_CONNECTING || WiFi.status() == WL_CONNECTED) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_AP);
    }
    
    // Start AP
    bool success = WiFi.softAP(apSSID.c_str(), apPassword.c_str(), 
                              apChannel, false, maxAPClients);
    
    if (success) {
        // Configure AP IP
        IPAddress apIP(192, 168, 4, 1);
        IPAddress apGateway(192, 168, 4, 1);
        IPAddress apSubnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIP, apGateway, apSubnet);
        
        apEnabled = true;
        Serial.printf("[WiFiV2] AP started on channel %d, IP: %s\n", 
                     apChannel, WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFiV2] Failed to start AP");
    }
}

void WiFiManagerV2::stopAPInternal() {
    if (!apEnabled) return;
    
    Serial.println("[WiFiV2] Stopping AP");
    WiFi.softAPdisconnect(true);
    
    // Switch back to STA only if connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
    }
    
    apEnabled = false;
}

void WiFiManagerV2::updateAdaptiveTxPower() {
    int32_t rssi = WiFi.RSSI();
    int8_t newTxPower = currentTxPower;
    
    // Adaptive algorithm with hysteresis
    if (rssi > -50) {
        // Excellent signal - minimum power
        newTxPower = TX_POWER_MIN;
    } else if (rssi > -60) {
        // Good signal - low-medium power
        newTxPower = TX_POWER_MIN + 8;  // 10 dBm
    } else if (rssi > -70) {
        // Fair signal - medium power
        newTxPower = TX_POWER_MED;
    } else if (rssi > -80) {
        // Weak signal - high power
        newTxPower = TX_POWER_MED + 12;  // 17 dBm
    } else {
        // Very weak signal - maximum power
        newTxPower = TX_POWER_MAX;
    }
    
    // Apply hysteresis to prevent oscillation
    int8_t diff = abs(newTxPower - currentTxPower);
    if (diff >= 8) {  // Only change if difference is >= 2 dBm
        esp_err_t err = esp_wifi_set_max_tx_power(newTxPower);
        if (err == ESP_OK) {
            currentTxPower = newTxPower;
            Serial.printf("[WiFiV2] TX Power adjusted to %d dBm (RSSI: %d dBm)\n", 
                         newTxPower / 4, rssi);
        }
    }
}

void WiFiManagerV2::calculateRetryDelay() {
    // Exponential backoff with jitter
    currentRetryDelay = currentRetryDelay * 2;
    
    // Add random jitter (±20%)
    int32_t jitter = random(-currentRetryDelay / 5, currentRetryDelay / 5);
    currentRetryDelay += jitter;
    
    // Clamp to limits
    currentRetryDelay = constrain(currentRetryDelay, MIN_RETRY_DELAY_MS, MAX_RETRY_DELAY_MS);
    
    Serial.printf("[WiFiV2] Next retry in %d ms\n", currentRetryDelay);
}

uint8_t WiFiManagerV2::selectBestChannel() {
    if (scanResults.empty()) return 6;  // Default to channel 6
    
    // Channel score array (lower is better)
    int channelScores[14] = {0};  // Channels 1-13
    
    // Analyze each network
    for (const auto& network : scanResults) {
        if (network.channel >= 1 && network.channel <= 13) {
            // Penalty based on signal strength
            int penalty = map(network.rssi, -90, -30, 1, 10);
            
            // Apply penalty to channel and adjacent channels
            channelScores[network.channel] += penalty * 3;
            if (network.channel > 1) channelScores[network.channel - 1] += penalty;
            if (network.channel < 13) channelScores[network.channel + 1] += penalty;
        }
    }
    
    // Find best channel (prefer 1, 6, 11)
    uint8_t bestChannel = 6;
    int lowestScore = channelScores[6];
    
    for (uint8_t ch : {1, 6, 11}) {
        if (channelScores[ch] < lowestScore) {
            lowestScore = channelScores[ch];
            bestChannel = ch;
        }
    }
    
    Serial.printf("[WiFiV2] Best channel: %d (score: %d)\n", bestChannel, lowestScore);
    return bestChannel;
}

void WiFiManagerV2::notifyEvent(WiFiEvent event, void* data) {
    for (const auto& callback : eventCallbacks) {
        callback(event, data);
    }
}

void WiFiManagerV2::enable80211LR(bool enable) {
    uint8_t protocol = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
    
    if (enable) {
        protocol |= WIFI_PROTOCOL_LR;
        Serial.println("[WiFiV2] Enabling 802.11 LR mode");
    }
    
    esp_wifi_set_protocol(WIFI_IF_STA, protocol);
}

void WiFiManagerV2::setTxPowerMode(uint8_t mode) {
    int8_t txPower;
    
    switch (mode) {
        case 1: txPower = TX_POWER_MIN; break;
        case 2: txPower = TX_POWER_MED; break;
        case 3: txPower = TX_POWER_MAX; break;
        default: return;  // Mode 0 = auto (handled by updateAdaptiveTxPower)
    }
    
    esp_wifi_set_max_tx_power(txPower);
    currentTxPower = txPower;
    Serial.printf("[WiFiV2] TX Power set to %d dBm\n", txPower / 4);
}

void WiFiManagerV2::optimizeForLEDCoexistence() {
    // Configure WiFi power save for minimal interference with LEDs
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    
    // Reduce WiFi task priority to ensure LED updates have priority
    // This is already handled by running on Core 0 with lower priority
    
    Serial.println("[WiFiV2] Optimized for LED coexistence");
}