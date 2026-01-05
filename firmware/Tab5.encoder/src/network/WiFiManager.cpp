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
#include <ESP.h>  // For heap monitoring

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
    , _apFallbackStartTime(0)
    , _connectStartTime(0)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(NetworkConfig::WIFI_RECONNECT_DELAY_MS)
    , _lastMdnsAttempt(0)
    , _mdnsHostname(nullptr)
    , _mdnsRetryCount(0)
{
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
    _apFallbackStartTime = 0;

    Serial.println("[WiFi] Starting connection...");
    Serial.printf("[WiFi] Primary SSID: %s\n", _ssid ? _ssid : "none");
    if (_ssid2) {
        Serial.printf("[WiFi] Secondary SSID: %s\n", _ssid2);
    }

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
        Serial.println("[WiFi] No network configured, starting AP mode...");
        startAPMode();
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
    WiFi.begin(currentSSID, currentPassword);
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

    // Check if we should fall back to AP mode (both networks exhausted)
    if (_apFallbackStartTime > 0 && now >= _apFallbackStartTime) {
        Serial.println("[WiFi] Both networks failed, starting AP mode fallback...");
        startAPMode();
        return;
    }

    // Check if we should switch to secondary network (primary exhausted)
    if (_usingPrimaryNetwork && 
        _primaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK &&
        _ssid2 && strlen(_ssid2) > 0) {
        Serial.println("[WiFi] Primary network exhausted, switching to secondary...");
        switchToSecondaryNetwork();
        return;
    }

    // Check if we should start AP fallback timer (both networks tried and failed)
    if (!_usingPrimaryNetwork && 
        _secondaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK) {
        if (_apFallbackStartTime == 0) {
            _apFallbackStartTime = now + NetworkConfig::AP_FALLBACK_DELAY_MS;
            Serial.printf("[WiFi] Both networks exhausted, AP fallback in %lu ms...\n",
                          NetworkConfig::AP_FALLBACK_DELAY_MS);
        }
    }
    
    // Also check if primary failed and no secondary available - go straight to AP
    if (_usingPrimaryNetwork && 
        _primaryAttempts >= NetworkConfig::WIFI_ATTEMPTS_PER_NETWORK &&
        (!_ssid2 || strlen(_ssid2) == 0)) {
        if (_apFallbackStartTime == 0) {
            _apFallbackStartTime = now + NetworkConfig::AP_FALLBACK_DELAY_MS;
            Serial.printf("[WiFi] Primary network failed, no secondary available. AP fallback in %lu ms...\n",
                          NetworkConfig::AP_FALLBACK_DELAY_MS);
        }
    }

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
    // #region agent log
    Serial.printf("[DEBUG] handleConnecting entry - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    wl_status_t wifiStatus = WiFi.status();
    // #region agent log
    Serial.printf("[DEBUG] WiFi.status()=%d elapsed=%lu Heap: free=%u minFree=%u\n",
                  (int)wifiStatus, millis() - _connectStartTime,
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    unsigned long elapsed = millis() - _connectStartTime;

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

    Serial.printf("[WiFi] Resolving mDNS: %s.local (attempt %d)...\n",
                  hostname, _mdnsRetryCount);

    // Query mDNS for the hostname
    IPAddress resolvedIP = MDNS.queryHost(hostname);

    // FALLBACK: If connected to LightwaveOS AP (secondary network) and mDNS fails, use Gateway IP
    // This ensures connection works even if mDNS is flaky on the SoftAP interface
    if (resolvedIP == INADDR_NONE && !_usingPrimaryNetwork && 
        WiFi.SSID() == "LightwaveOS") {
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
        // Resolution failed, stay in CONNECTED state for retry
        _status = WiFiConnectionStatus::CONNECTED;
        Serial.printf("[WiFi] mDNS resolution failed for %s.local\n", hostname);
        return false;
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

void WiFiManager::startAPMode() {
    Serial.println("[WiFi] Starting Access Point mode...");
    Serial.printf("[WiFi] AP SSID: %s (open, no password)\n", NetworkConfig::AP_SSID_VALUE);
    
    WiFi.disconnect();
    delay(100);
    
    WiFi.mode(WIFI_AP);
    // Use empty string or NULL for open AP (no password) - matches v2 firmware
    const char* apPassword = (NetworkConfig::AP_PASSWORD_VALUE && 
                              strlen(NetworkConfig::AP_PASSWORD_VALUE) > 0) 
                             ? NetworkConfig::AP_PASSWORD_VALUE : nullptr;
    WiFi.softAP(NetworkConfig::AP_SSID_VALUE, apPassword);
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WiFi] AP started! IP: %s\n", apIP.toString().c_str());
    
    // Initialize mDNS responder for AP mode
    if (!MDNS.begin("tab5encoder")) {
        Serial.println("[WiFi] mDNS responder failed to start");
    } else {
        Serial.println("[WiFi] mDNS responder started: tab5encoder.local");
    }
    
    _status = WiFiConnectionStatus::CONNECTED;  // AP mode counts as "connected"
    _resolvedIP = INADDR_NONE;  // No mDNS resolution in AP mode
}

void WiFiManager::reconnect() {
    Serial.println("[WiFi] Forcing reconnect...");
    WiFi.disconnect();
    _status = WiFiConnectionStatus::DISCONNECTED;
    _resolvedIP = INADDR_NONE;
    _usingPrimaryNetwork = true;  // Reset to primary
    _primaryAttempts = 0;
    _secondaryAttempts = 0;
    _apFallbackStartTime = 0;
    _lastReconnectAttempt = millis();
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
    _lastMdnsAttempt = 0;
    _mdnsRetryCount = 0;
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

#endif // ENABLE_WIFI
