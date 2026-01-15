// ============================================================================
// WiFiManager Implementation - Tab5.encoder
// ============================================================================
// Non-blocking WiFi connection management with mDNS resolution.
// Ported from K1.8encoderS3 with Tab5-specific adaptations.
//
// NOTE: WiFi is currently DISABLED on Tab5 (ESP32-P4) due to SDIO pin
// configuration issues. See Config.h ENABLE_WIFI flag.
// ============================================================================

#include "WiFiManager.h"

#if ENABLE_WIFI
#include <esp_task_wdt.h>  // For watchdog reset around blocking calls
#include <ESP.h>  // For heap monitoring
#include <Preferences.h>  // For NVS storage

WiFiManager::WiFiManager()
    : _ssid(nullptr)
    , _password(nullptr)
    , _ssid2(nullptr)
    , _password2(nullptr)
    , _status(WiFiConnectionStatus::DISCONNECTED)
    , _resolvedIP(INADDR_NONE)
    , _usingPrimaryNetwork(true)
    , _primaryAttempts(0)
    , _secondaryAttempts(0)
    , _retryTimeoutStartTime(0)
    , _retryButtonEnabled(false)
    , _connectStartTime(0)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(NetworkConfig::WIFI_RECONNECT_DELAY_MS)
    , _lastMdnsAttempt(0)
    , _mdnsHostname(nullptr)
    , _mdnsRetryCount(0)
    , _mdnsStartTime(0)
    , _manualIP(INADDR_NONE)
    , _useManualIP(false)
{
    // Load manual IP from NVS
    loadManualIPFromNVS();
}

void WiFiManager::begin(const char* ssid, const char* password, 
                        const char* ssid2, const char* password2) {
    _ssid = ssid;
    _password = password;
    _ssid2 = ssid2;
    _password2 = password2;

    _usingPrimaryNetwork = true;
    _primaryAttempts = 0;
    _secondaryAttempts = 0;
    _retryTimeoutStartTime = 0;
    _retryButtonEnabled = false;

    Serial.println("[WiFi] Starting connection...");
    Serial.printf("[WiFi] Primary SSID: %s\n", _ssid ? _ssid : "none");
    if (_ssid2) {
        Serial.printf("[WiFi] Secondary SSID: %s\n", _ssid2);
    } else {
        Serial.println("[WiFi] Secondary SSID: (none)");
    }
    Serial.printf("[WiFi] Target hostname: %s:%d%s\n",
                  LIGHTWAVE_HOST, LIGHTWAVE_PORT, LIGHTWAVE_WS_PATH);

    startConnection();
}

void WiFiManager::startConnection() {
    // #region agent log
    Serial.printf("[DEBUG] startConnection entry - Heap: free=%u minFree=%u largest=%u status=%d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), (int)_status);
    // #endregion
    _status = WiFiConnectionStatus::CONNECTING;
    _connectStartTime = millis();

    // Determine which network to use
    const char* currentSSID = _usingPrimaryNetwork ? _ssid : _ssid2;
    const char* currentPassword = _usingPrimaryNetwork ? _password : _password2;

    if (!currentSSID || strlen(currentSSID) == 0) {
        Serial.println("[WiFi] No network configured");
        _status = WiFiConnectionStatus::ERROR;
        return;
    }

    // Increment attempt counter
    if (_usingPrimaryNetwork) {
        _primaryAttempts++;
    } else {
        _secondaryAttempts++;
    }

    Serial.printf("[WiFi] Attempting connection to %s (attempt %d)...\n",
                  currentSSID, _usingPrimaryNetwork ? _primaryAttempts : _secondaryAttempts);

    // Configure WiFi for station mode
    // #region agent log
    Serial.printf("[DEBUG] Before WiFi.mode(WIFI_STA) - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    WiFi.mode(WIFI_STA);
    // #region agent log
    Serial.printf("[DEBUG] After WiFi.mode(WIFI_STA) - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    delay(10);  // Small delay for mode change to take effect
    // #endregion
    WiFi.setAutoReconnect(false);  // We handle reconnection ourselves

    // Start connection
    // #region agent log
    Serial.printf("[DEBUG] Before WiFi.begin - Heap: free=%u minFree=%u largest=%u ssid=%s\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                  currentSSID ? currentSSID : "null");
    // #endregion
    esp_task_wdt_reset();  // CRITICAL: Reset WDT before potentially blocking WiFi.begin()
    WiFi.begin(currentSSID, currentPassword);
    esp_task_wdt_reset();  // CRITICAL: Reset WDT after WiFi.begin() completes
    // #region agent log
    Serial.printf("[DEBUG] After WiFi.begin - Heap: free=%u minFree=%u wifiStatus=%d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), (int)WiFi.status());
    delay(50);  // Allow WiFi.begin() to initialize buffers
    Serial.printf("[DEBUG] After 50ms delay - Heap: free=%u minFree=%u wifiStatus=%d\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), (int)WiFi.status());
    // #endregion

    Serial.println("[WiFi] Connecting...");
}

void WiFiManager::update() {
    switch (_status) {
        case WiFiConnectionStatus::DISCONNECTED:
            handleDisconnected();
            break;

        case WiFiConnectionStatus::CONNECTING:
            handleConnecting();
            break;

        case WiFiConnectionStatus::CONNECTED:
        case WiFiConnectionStatus::MDNS_RESOLVING:
        case WiFiConnectionStatus::MDNS_RESOLVED:
            handleConnected();
            break;

        case WiFiConnectionStatus::ERROR:
            handleError();
            break;
    }
}

void WiFiManager::handleDisconnected() {
    unsigned long now = millis();

    // Track when retry period started (on first disconnect)
    if (_retryTimeoutStartTime == 0) {
        _retryTimeoutStartTime = now;
        _retryButtonEnabled = false;
        Serial.println("[WiFi] Starting 2-minute retry period...");
    }

    // Check if 2 minutes have elapsed - enable retry button
    if (!_retryButtonEnabled && (now - _retryTimeoutStartTime) >= NetworkConfig::WIFI_RETRY_TIMEOUT_MS) {
        _retryButtonEnabled = true;
        Serial.println("[WiFi] 2-minute retry period elapsed, retry button enabled");
    }

    // Check if we should switch to secondary network (primary exhausted)
    if (_usingPrimaryNetwork && 
        _primaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK &&
        _ssid2 && strlen(_ssid2) > 0) {
        Serial.println("[WiFi] Primary network exhausted, switching to secondary...");
        switchToSecondaryNetwork();
        return;
    }

    // Continue retrying both networks for 2 minutes
    // After 2 minutes, retry button will be enabled but we still keep trying
    // Attempt reconnect with backoff delay
    if (now - _lastReconnectAttempt >= _reconnectDelay) {
        _lastReconnectAttempt = now;

        Serial.printf("[WiFi] Attempting reconnect (delay: %lu ms)...\n", _reconnectDelay);
        startConnection();

        // Increase backoff delay (exponential, capped)
        _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)30000);
    }
}

void WiFiManager::handleConnecting() {
    wl_status_t wifiStatus = WiFi.status();
    unsigned long elapsed = millis() - _connectStartTime;
    
    // #region agent log
    static uint32_t s_lastConnectingLog = 0;
    uint32_t now = millis();
    if (now - s_lastConnectingLog >= 10000) {  // Log every 10 seconds instead of every loop
        Serial.printf("[DEBUG] handleConnecting - WiFi.status()=%d elapsed=%lu Heap: free=%u minFree=%u\n",
                      (int)wifiStatus, elapsed,
                      ESP.getFreeHeap(), ESP.getMinFreeHeap());
        s_lastConnectingLog = now;
    }
    // #endregion

    if (wifiStatus == WL_CONNECTED) {
        // Connection successful
        _status = WiFiConnectionStatus::CONNECTED;
        _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff

        Serial.println("[WiFi] Connected!");
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());

        // Initialize mDNS responder
        // Use "tab5encoder" as mDNS hostname for this device
        if (!MDNS.begin("tab5encoder")) {
            Serial.println("[WiFi] mDNS responder failed to start");
        } else {
            Serial.println("[WiFi] mDNS responder started: tab5encoder.local");
        }

        // Reset mDNS resolution state for target host
        _lastMdnsAttempt = 0;
        _mdnsRetryCount = 0;
        _mdnsStartTime = 0;  // Reset mDNS start time

    } else if (wifiStatus == WL_CONNECT_FAILED ||
               wifiStatus == WL_NO_SSID_AVAIL) {
        // Connection failed - check if we should switch networks
        Serial.printf("[WiFi] Connection failed (status: %d)\n", wifiStatus);
        
        // If primary network failed and we have secondary, switch now
        if (_usingPrimaryNetwork && 
            _primaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK &&
            _ssid2 && strlen(_ssid2) > 0) {
            Serial.println("[WiFi] Switching to secondary network...");
            switchToSecondaryNetwork();
            return;
        }
        
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();

    } else if (elapsed >= NetworkConfig::WIFI_CONNECT_TIMEOUT_MS) {
        // Connection timeout - check if we should switch networks
        Serial.printf("[WiFi] Connection timeout after %lu ms\n", elapsed);
        WiFi.disconnect();
        
        // If primary network timed out and we have secondary, switch now
        if (_usingPrimaryNetwork && 
            _primaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK &&
            _ssid2 && strlen(_ssid2) > 0) {
            Serial.println("[WiFi] Switching to secondary network...");
            switchToSecondaryNetwork();
            return;
        }
        
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();
    }
    // else: still connecting, wait for next update()
}

void WiFiManager::handleConnected() {
    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost!");
        _status = WiFiConnectionStatus::DISCONNECTED;
        _resolvedIP = INADDR_NONE;
        _lastReconnectAttempt = millis();
        _lastMdnsAttempt = 0;  // Reset mDNS timer for next connection
        _mdnsRetryCount = 0;
        _mdnsStartTime = 0;  // Reset mDNS start time
    }
}

void WiFiManager::handleError() {
    // Error state with longer delay before retry
    unsigned long now = millis();

    if (now - _lastReconnectAttempt >= 10000) {  // 10 second delay in error state
        _lastReconnectAttempt = now;
        _status = WiFiConnectionStatus::DISCONNECTED;
        Serial.println("[WiFi] Exiting error state, attempting reconnect...");
    }
}

bool WiFiManager::resolveMDNS(const char* hostname) {
    // Must be connected to WiFi
    if (!isConnected()) {
        return false;
    }

    // Store hostname for retry logic
    _mdnsHostname = hostname;

    unsigned long now = millis();

    // If already resolved, return immediately
    if (_status == WiFiConnectionStatus::MDNS_RESOLVED && _resolvedIP != INADDR_NONE) {
        return true;
    }

    // Track start time on first attempt
    if (_mdnsStartTime == 0) {
        _mdnsStartTime = now;
    }

    // Check if timeout exceeded or max attempts reached
    bool timeoutExceeded = (now - _mdnsStartTime) >= NetworkConfig::MDNS_FALLBACK_TIMEOUT_MS;
    bool maxAttemptsReached = _mdnsRetryCount >= NetworkConfig::MDNS_MAX_ATTEMPTS;

    if (timeoutExceeded || maxAttemptsReached) {
        // Timeout exceeded - use fallback IP
        IPAddress fallbackIP = INADDR_NONE;
        const char* fallbackSource = nullptr;

        // Priority 1: Manual IP from NVS (if configured and enabled)
        if (shouldUseManualIP() && _manualIP != INADDR_NONE) {
            fallbackIP = _manualIP;
            fallbackSource = "manual IP from NVS";
        }
        // Priority 2: Gateway IP (if on secondary network)
        else if (!_usingPrimaryNetwork && WiFi.SSID() == "LightwaveOS-AP") {
            fallbackIP = WiFi.gatewayIP();
            fallbackSource = "gateway IP (secondary network)";
        }
        // Priority 3: Default fallback IP for primary network
        else if (_usingPrimaryNetwork) {
            fallbackIP.fromString(NetworkConfig::MDNS_FALLBACK_IP_PRIMARY);
            fallbackSource = "default fallback IP (primary network)";
        }

        if (fallbackIP != INADDR_NONE) {
            _resolvedIP = fallbackIP;
            _status = WiFiConnectionStatus::MDNS_RESOLVED;
            Serial.printf("[WiFi] mDNS timeout exceeded (attempt %d/%d, elapsed: %lu ms), using %s: %s\n",
                          _mdnsRetryCount, NetworkConfig::MDNS_MAX_ATTEMPTS,
                          now - _mdnsStartTime, fallbackSource, fallbackIP.toString().c_str());
            return true;
        } else {
            // No fallback available - reset counters to break infinite loop and allow fresh timeout cycle
            _mdnsStartTime = 0;
            _mdnsRetryCount = 0;
            Serial.printf("[WiFi] mDNS timeout exceeded but no fallback available, resetting retry counter\n");
            return false;  // Allow fresh timeout cycle
        }
    }

    // Apply backoff: don't query mDNS every loop tick
    unsigned long delay;
    if (_lastMdnsAttempt == 0) {
        // First attempt: wait initial delay after WiFi connects
        delay = NetworkConfig::MDNS_INITIAL_DELAY_MS;
    } else {
        // Subsequent attempts: use retry delay
        delay = NetworkConfig::MDNS_RETRY_DELAY_MS;
    }

    if (now - _lastMdnsAttempt < delay) {
        return false;  // Still in backoff period
    }

    _lastMdnsAttempt = now;
    _status = WiFiConnectionStatus::MDNS_RESOLVING;
    _mdnsRetryCount++;

    // Log mDNS attempt with context
    Serial.printf("[WiFi] mDNS: attempt %d/%d, elapsed: %lu ms, timeout: %lu ms\n",
                  _mdnsRetryCount, NetworkConfig::MDNS_MAX_ATTEMPTS,
                  now - _mdnsStartTime, NetworkConfig::MDNS_FALLBACK_TIMEOUT_MS);
    Serial.printf("[WiFi] Resolving mDNS: %s.local (attempt %d)...\n",
                  hostname, _mdnsRetryCount);
    
    // Diagnostic information
    Serial.printf("[WiFi] Network status: SSID='%s', IP=%s, Gateway=%s, Mode=%s\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  _usingPrimaryNetwork ? "PRIMARY" : "SECONDARY");

    // Query mDNS for the hostname
    esp_task_wdt_reset();  // CRITICAL: Reset WDT before potentially blocking mDNS query
    IPAddress resolvedIP = MDNS.queryHost(hostname);
    esp_task_wdt_reset();  // CRITICAL: Reset WDT after mDNS query completes

    // Enhanced diagnostic on failure
    if (resolvedIP == INADDR_NONE) {
        Serial.printf("[WiFi] mDNS queryHost('%s') returned INADDR_NONE\n", hostname);
    } else {
        Serial.printf("[WiFi] mDNS queryHost('%s') returned: %s\n", hostname, resolvedIP.toString().c_str());
    }

    // FALLBACK: If connected to LightwaveOS AP (secondary network) and mDNS fails, use Gateway IP
    // This ensures connection works even if mDNS is flaky on the SoftAP interface
    if (resolvedIP == INADDR_NONE && !_usingPrimaryNetwork && 
        WiFi.SSID() == "LightwaveOS-AP") {
        Serial.println("[WiFi] mDNS failed, but connected to LightwaveOS AP. Using Gateway IP.");
        resolvedIP = WiFi.gatewayIP();
    }

    if (resolvedIP != INADDR_NONE) {
        _resolvedIP = resolvedIP;
        _status = WiFiConnectionStatus::MDNS_RESOLVED;
        Serial.printf("[WiFi] mDNS resolved: %s.local -> %s\n",
                      hostname, resolvedIP.toString().c_str());
        return true;
    } else {
        // Resolution failed - keep status as MDNS_RESOLVING (or CONNECTED) to allow retry
        // Don't change to CONNECTED here - let main loop handle retries via resolveMDNS() calls
        // Status will be checked again on next resolveMDNS() call
        Serial.printf("[WiFi] mDNS resolution failed for %s.local (will retry)\n", hostname);
        return false;  // Return false to allow retry
    }
}

void WiFiManager::switchToSecondaryNetwork() {
    if (!_ssid2 || strlen(_ssid2) == 0) {
        Serial.println("[WiFi] No secondary network configured");
        return;
    }
    
    _usingPrimaryNetwork = false;
    _secondaryAttempts = 0;
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
    WiFi.disconnect();
    delay(100);
    startConnection();
}

void WiFiManager::triggerRetry() {
    Serial.println("[WiFi] Manual retry triggered by user");
    reconnect();
}

void WiFiManager::reconnect() {
    Serial.println("[WiFi] Forcing reconnect...");
    WiFi.disconnect();
    _status = WiFiConnectionStatus::DISCONNECTED;
    _resolvedIP = INADDR_NONE;
    _usingPrimaryNetwork = true;  // Reset to primary
    _primaryAttempts = 0;
    _secondaryAttempts = 0;
    _retryTimeoutStartTime = 0;  // Reset retry timer
    _retryButtonEnabled = false;  // Hide retry button
    _lastReconnectAttempt = millis();
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
    _lastMdnsAttempt = 0;
    _mdnsRetryCount = 0;
    _mdnsStartTime = 0;  // Reset mDNS start time
}

void WiFiManager::enterErrorState(const char* reason) {
    Serial.printf("[WiFi] Error: %s\n", reason);
    _status = WiFiConnectionStatus::ERROR;
    _lastReconnectAttempt = millis();
}

const char* WiFiManager::getStatusString() const {
    switch (_status) {
        case WiFiConnectionStatus::DISCONNECTED:   return "Disconnected";
        case WiFiConnectionStatus::CONNECTING:     return "Connecting";
        case WiFiConnectionStatus::CONNECTED:      return "Connected";
        case WiFiConnectionStatus::MDNS_RESOLVING: return "Resolving mDNS";
        case WiFiConnectionStatus::MDNS_RESOLVED:  return "Ready";
        case WiFiConnectionStatus::ERROR:          return "Error";
        default:                                   return "Unknown";
    }
}

void WiFiManager::loadManualIPFromNVS() {
    Preferences prefs;
    if (!prefs.begin(NetworkNVS::NAMESPACE, true)) {  // Read-only
        _useManualIP = false;
        _manualIP = INADDR_NONE;
        return;
    }
    
    // Read use_manual flag
    _useManualIP = prefs.getBool(NetworkNVS::KEY_USE_MANUAL_IP, false);
    
    // Read manual IP string
    String ipStr = prefs.getString(NetworkNVS::KEY_MANUAL_IP, "");
    if (ipStr.length() > 0) {
        if (!_manualIP.fromString(ipStr.c_str())) {
            Serial.printf("[WiFi] Invalid manual IP in NVS: %s\n", ipStr.c_str());
            _manualIP = INADDR_NONE;
            _useManualIP = false;
        } else {
            Serial.printf("[WiFi] Loaded manual IP from NVS: %s\n", ipStr.c_str());
        }
    } else {
        _manualIP = INADDR_NONE;
    }
    
    prefs.end();
}

bool WiFiManager::setManualIP(const IPAddress& ip) {
    if (ip == INADDR_NONE) {
        return false;
    }
    
    Preferences prefs;
    if (!prefs.begin(NetworkNVS::NAMESPACE, false)) {  // Read-write
        Serial.println("[WiFi] Failed to open NVS for manual IP");
        return false;
    }
    
    String ipStr = ip.toString();
    if (!prefs.putString(NetworkNVS::KEY_MANUAL_IP, ipStr)) {
        Serial.println("[WiFi] Failed to store manual IP in NVS");
        prefs.end();
        return false;
    }
    
    if (!prefs.putBool(NetworkNVS::KEY_USE_MANUAL_IP, true)) {
        Serial.println("[WiFi] Failed to store use_manual flag");
        prefs.end();
        return false;
    }
    
    _manualIP = ip;
    _useManualIP = true;
    prefs.end();
    
    Serial.printf("[WiFi] Manual IP stored: %s\n", ipStr.c_str());
    return true;
}

void WiFiManager::clearManualIP() {
    Preferences prefs;
    if (prefs.begin(NetworkNVS::NAMESPACE, false)) {
        prefs.remove(NetworkNVS::KEY_MANUAL_IP);
        prefs.remove(NetworkNVS::KEY_USE_MANUAL_IP);
        prefs.end();
    }
    _manualIP = INADDR_NONE;
    _useManualIP = false;
}

bool WiFiManager::isMDNSTimeoutExceeded() const {
    if (_mdnsStartTime == 0) {
        return false;  // Not started yet
    }
    unsigned long elapsed = millis() - _mdnsStartTime;
    return elapsed >= NetworkConfig::MDNS_FALLBACK_TIMEOUT_MS || 
           _mdnsRetryCount >= NetworkConfig::MDNS_MAX_ATTEMPTS;
}

IPAddress WiFiManager::getManualIP() const {
    return _manualIP;
}

bool WiFiManager::shouldUseManualIP() const {
    return _useManualIP && _manualIP != INADDR_NONE;
}

#endif // ENABLE_WIFI
