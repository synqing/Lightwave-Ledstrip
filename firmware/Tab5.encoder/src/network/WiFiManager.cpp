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

#include <cstring>

#ifndef TAB5_AGENT_TRACE
#define TAB5_AGENT_TRACE 0
#endif

#if TAB5_AGENT_TRACE
#define TAB5_AGENT_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define TAB5_AGENT_PRINTF(...) ((void)0)
#endif

static void formatIPv4(const IPAddress& ip, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

WiFiManager::WiFiManager()
    : _ssid(nullptr)
    , _password(nullptr)
    , _fallbackSsid(nullptr)
    , _fallbackPassword(nullptr)
    , _hasFallback(false)
    , _useFallback(false)
    , _status(WiFiConnectionStatus::DISCONNECTED)
    , _resolvedIP(INADDR_NONE)
    , _connectStartTime(0)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(NetworkConfig::WIFI_RECONNECT_DELAY_MS)
    , _lastMdnsAttempt(0)
    , _mdnsHostname(nullptr)
    , _mdnsRetryCount(0)
{
}

void WiFiManager::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    _fallbackSsid = nullptr;
    _fallbackPassword = nullptr;
    _hasFallback = false;
    _useFallback = false;
    _preferred = PreferredNetwork::AUTO;
    _resolvedIP = INADDR_NONE;
    _lastReconnectAttempt = 0;
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;

    Serial.println("[WiFi] Starting connection...");
    Serial.printf("[WiFi] SSID: %s\n", activeSsid());

    startScan(false);
}

void WiFiManager::beginWithFallback(const char* ssid,
                                   const char* password,
                                   const char* fallbackSsid,
                                   const char* fallbackPassword) {
    _ssid = ssid;
    _password = password;
    _fallbackSsid = fallbackSsid;
    _fallbackPassword = fallbackPassword;
    _useFallback = false;
    _preferred = PreferredNetwork::AUTO;
    _resolvedIP = INADDR_NONE;
    _lastReconnectAttempt = 0;
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;

    _hasFallback = (_fallbackSsid && _fallbackSsid[0] != '\0');

    Serial.println("[WiFi] Starting connection...");
    Serial.printf("[WiFi] SSID: %s\n", activeSsid());
    if (_hasFallback) {
        Serial.printf("[WiFi] Fallback SSID: %s\n", _fallbackSsid);
    }

    startScan(false);
}

void WiFiManager::startConnection() {
    _status = WiFiConnectionStatus::CONNECTING;
    _connectStartTime = millis();

    // Stop any AP-only fallback before connecting as STA.
    WiFi.softAPdisconnect(true);

    // Configure WiFi for station mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection ourselves

    // Start connection
    WiFi.begin(activeSsid(), activePassword());

    Serial.printf("[WiFi] Connecting to '%s'...\n", activeSsid());
}

void WiFiManager::update() {
    switch (_status) {
        case WiFiConnectionStatus::DISCONNECTED:
            handleDisconnected();
            break;

        case WiFiConnectionStatus::SCANNING:
            handleScanning();
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

        case WiFiConnectionStatus::AP_ONLY:
            handleApOnly();
            break;
    }
}

void WiFiManager::handleDisconnected() {
    unsigned long now = millis();

    // Scan-first strategy: only attempt a connection when a known SSID is visible.
    if (now - _lastReconnectAttempt < _reconnectDelay) {
        return;
    }

    _lastReconnectAttempt = now;
    startScan(false);
}

void WiFiManager::startScan(bool keepAp) {
    const unsigned long now = millis();

    // Avoid starting a new scan too frequently (scan can be expensive on the radio/CPU).
    const unsigned long minInterval =
        keepAp ? NetworkConfig::WIFI_AP_ONLY_RESCAN_MS : NetworkConfig::WIFI_SCAN_INTERVAL_MS;
    if (_lastScanAttempt != 0 && (now - _lastScanAttempt) < minInterval) {
        return;
    }
    _lastScanAttempt = now;

    // If no credentials are configured, skip scanning and enter AP-only.
    const bool hasPrimary = (_ssid && _ssid[0] != '\0');
    const bool hasFallback = (_hasFallback && _fallbackSsid && _fallbackSsid[0] != '\0');
    if (!hasPrimary && !hasFallback) {
        enterApOnlyMode();
        return;
    }

    // Ensure we can scan. If we're keeping AP active, scan in AP+STA mode.
    _scanKeepAp = keepAp;
    WiFi.scanDelete();
    if (keepAp) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.softAPdisconnect(true);
    }

    _scanStartTime = now;
    _status = WiFiConnectionStatus::SCANNING;

    const int rc = WiFi.scanNetworks(true /* async */, true /* show hidden */);
    (void)rc;
    Serial.printf("[WiFi] Scanning for networks (keepAp=%d)...\n", keepAp ? 1 : 0);
}

void WiFiManager::handleScanning() {
    const unsigned long now = millis();

    const int scanResult = WiFi.scanComplete();
    if (scanResult == -1) {  // -1 = scan running
        if (_scanStartTime != 0 && (now - _scanStartTime) > NetworkConfig::WIFI_SCAN_TIMEOUT_MS) {
            Serial.println("[WiFi] Scan timeout, retrying...");
            WiFi.scanDelete();
            if (_scanKeepAp) {
                WiFi.mode(WIFI_AP);
                _status = WiFiConnectionStatus::AP_ONLY;
            } else {
                _status = WiFiConnectionStatus::DISCONNECTED;
            }
        }
        return;
    }

    if (scanResult < 0) {
        // -2 means scan was never started (or was deleted); treat as disconnected.
        if (_scanKeepAp) {
            WiFi.mode(WIFI_AP);
            _status = WiFiConnectionStatus::AP_ONLY;
        } else {
            _status = WiFiConnectionStatus::DISCONNECTED;
        }
        return;
    }

    bool primaryFound = false;
    bool fallbackFound = false;

    for (int i = 0; i < scanResult; i++) {
        const String ssid = WiFi.SSID(i);

        if (_ssid && _ssid[0] != '\0' && ssid == _ssid) {
            primaryFound = true;
        }
        if (_hasFallback && _fallbackSsid && _fallbackSsid[0] != '\0' && ssid == _fallbackSsid) {
            fallbackFound = true;
        }
    }

    WiFi.scanDelete();

    if (!primaryFound && !fallbackFound) {
        enterApOnlyMode();
        return;
    }

    // Choose the best available known network.
    bool useFallback = false;
    switch (_preferred) {
        case PreferredNetwork::PRIMARY:
            useFallback = !primaryFound && fallbackFound;
            break;
        case PreferredNetwork::FALLBACK:
            useFallback = fallbackFound;
            break;
        case PreferredNetwork::AUTO:
        default:
            // Deterministic policy: prefer primary if visible; fall back only when primary is absent.
            useFallback = !primaryFound && fallbackFound;
            break;
    }

    _useFallback = useFallback;
    Serial.printf("[WiFi] Selected SSID: %s\n", activeSsid());

    startConnection();
}

void WiFiManager::enterApOnlyMode() {
    // No known SSIDs visible: stop reconnect storms and keep running.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    bool ok = false;
    if (TAB5_FALLBACK_AP_PASSWORD[0] != '\0') {
        // ESP32 requires >=8 chars for WPA2; otherwise it fails.
        if (strlen(TAB5_FALLBACK_AP_PASSWORD) >= 8) {
            ok = WiFi.softAP(TAB5_FALLBACK_AP_SSID, TAB5_FALLBACK_AP_PASSWORD);
        } else {
            Serial.println("[WiFi] AP password too short (<8), starting open AP");
            ok = WiFi.softAP(TAB5_FALLBACK_AP_SSID);
        }
    } else {
        ok = WiFi.softAP(TAB5_FALLBACK_AP_SSID);
    }

    _status = WiFiConnectionStatus::AP_ONLY;
    _resolvedIP = INADDR_NONE;
    _lastReconnectAttempt = millis();

    char apIp[16];
    formatIPv4(WiFi.softAPIP(), apIp);
    Serial.printf("[WiFi] No known networks in range; AP-only (%s) started=%d ip=%s\n",
                  TAB5_FALLBACK_AP_SSID, ok ? 1 : 0, apIp);
}

void WiFiManager::handleApOnly() {
    // Periodically rescan while keeping AP active.
    startScan(true);
}

void WiFiManager::handleConnecting() {
    wl_status_t wifiStatus = WiFi.status();
    unsigned long elapsed = millis() - _connectStartTime;

    if (wifiStatus == WL_CONNECTED) {
        // Connection successful
        _status = WiFiConnectionStatus::CONNECTED;
        _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff

        Serial.println("[WiFi] Connected!");
        {
            char ipStr[16];
            formatIPv4(WiFi.localIP(), ipStr);
            Serial.printf("[WiFi] IP: %s\n", ipStr);
        }
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

    } else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL) {
        // Connection failed
        Serial.printf("[WiFi] Connection failed (status: %d) ssid=%s\n", wifiStatus, activeSsid());
        WiFi.disconnect(true);
        _resolvedIP = INADDR_NONE;

        // Back off and re-scan before retrying.
        _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)30000);
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();

    } else if (elapsed >= NetworkConfig::WIFI_CONNECT_TIMEOUT_MS) {
        // Connection timeout
        Serial.printf("[WiFi] Connection timeout after %lu ms (ssid=%s)\n", elapsed, activeSsid());
        WiFi.disconnect(true);
        _resolvedIP = INADDR_NONE;

        _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)30000);
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();
    }
    // else: still connecting, wait for next update()
}

void WiFiManager::handleConnected() {
    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost!");
        // Re-evaluate the best available SSID on reconnect (scan-first).
        if (_preferred != PreferredNetwork::FALLBACK) {
            _useFallback = false;
        }
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
    // #region agent log
    TAB5_AGENT_PRINTF("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"MDNS1\",\"location\":\"WiFiManager.cpp:resolveMDNS\",\"message\":\"mdns.queryHost.before\",\"data\":{\"hostname\":\"%s\",\"attempt\":%d},\"timestamp\":%lu}\n",
                      hostname, _mdnsRetryCount, (unsigned long)millis());
    // #endregion
    IPAddress resolvedIP = MDNS.queryHost(hostname);
    // #region agent log
    {
        char resolvedStr[16];
        formatIPv4(resolvedIP, resolvedStr);
        TAB5_AGENT_PRINTF("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"MDNS1\",\"location\":\"WiFiManager.cpp:resolveMDNS\",\"message\":\"mdns.queryHost.after\",\"data\":{\"hostname\":\"%s\",\"resolvedIP\":\"%s\",\"isValid\":%d},\"timestamp\":%lu}\n",
                          hostname, resolvedStr, (resolvedIP != INADDR_NONE) ? 1 : 0, (unsigned long)millis());
    }
    // #endregion

    if (resolvedIP != INADDR_NONE) {
        _resolvedIP = resolvedIP;
        _status = WiFiConnectionStatus::MDNS_RESOLVED;
        char resolvedStr[16];
        formatIPv4(resolvedIP, resolvedStr);
        Serial.printf("[WiFi] mDNS resolved: %s.local -> %s\n",
                      hostname, resolvedStr);
        // #region agent log
        TAB5_AGENT_PRINTF("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"MDNS1\",\"location\":\"WiFiManager.cpp:resolveMDNS\",\"message\":\"mdns.resolved\",\"data\":{\"hostname\":\"%s\",\"ip\":\"%s\"},\"timestamp\":%lu}\n",
                          hostname, resolvedStr, (unsigned long)millis());
        // #endregion
        return true;
    } else {
        // Resolution failed, stay in CONNECTED state for retry
        _status = WiFiConnectionStatus::CONNECTED;
        Serial.printf("[WiFi] mDNS resolution failed for %s.local\n", hostname);
        // #region agent log
        TAB5_AGENT_PRINTF("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"MDNS1\",\"location\":\"WiFiManager.cpp:resolveMDNS\",\"message\":\"mdns.failed\",\"data\":{\"hostname\":\"%s\",\"attempt\":%d,\"nextRetryDelay\":%d},\"timestamp\":%lu}\n",
                          hostname, _mdnsRetryCount, NetworkConfig::MDNS_RETRY_DELAY_MS, (unsigned long)millis());
        // #endregion
        return false;
    }
}

void WiFiManager::reconnect() {
    Serial.println("[WiFi] Forcing reconnect...");
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    _useFallback = false;  // Re-evaluate from scan
    _status = WiFiConnectionStatus::DISCONNECTED;
    _resolvedIP = INADDR_NONE;
    _lastReconnectAttempt = 0;
    _reconnectDelay = NetworkConfig::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
    _lastScanAttempt = 0;
    _lastMdnsAttempt = 0;
    _mdnsRetryCount = 0;
}

void WiFiManager::enterErrorState(const char* reason) {
    Serial.printf("[WiFi] Error: %s\n", reason);
    _status = WiFiConnectionStatus::ERROR;
    _lastReconnectAttempt = millis();
}

const char* WiFiManager::activeSsid() const {
    if (_useFallback && _fallbackSsid && _fallbackSsid[0] != '\0') return _fallbackSsid;
    return _ssid ? _ssid : "";
}

const char* WiFiManager::activePassword() const {
    if (_useFallback && _fallbackPassword) return _fallbackPassword;
    return _password ? _password : "";
}

const char* WiFiManager::getStatusString() const {
    switch (_status) {
        case WiFiConnectionStatus::DISCONNECTED:   return "Disconnected";
        case WiFiConnectionStatus::SCANNING:       return "Scanning";
        case WiFiConnectionStatus::CONNECTING:     return "Connecting";
        case WiFiConnectionStatus::CONNECTED:      return "Connected";
        case WiFiConnectionStatus::MDNS_RESOLVING: return "Resolving mDNS";
        case WiFiConnectionStatus::MDNS_RESOLVED:  return "Ready";
        case WiFiConnectionStatus::AP_ONLY:        return "AP Only";
        case WiFiConnectionStatus::ERROR:          return "Error";
        default:                                   return "Unknown";
    }
}

bool WiFiManager::requestSTA() {
    if (!_hasFallback) {
        Serial.println("[WiFi] No fallback network configured");
        return false;
    }

    if (_preferred == PreferredNetwork::FALLBACK) {
        Serial.println("[WiFi] Already preferring fallback network");
        return false;
    }

    _preferred = PreferredNetwork::FALLBACK;
    Serial.printf("[WiFi] Preferring fallback network: %s\n", _fallbackSsid);
    reconnect();
    startScan(false);
    return true;
}

bool WiFiManager::requestAP() {
    if (_preferred == PreferredNetwork::PRIMARY) {
        Serial.println("[WiFi] Already preferring primary network");
        return false;
    }

    _preferred = PreferredNetwork::PRIMARY;
    Serial.printf("[WiFi] Preferring primary network: %s\n", _ssid ? _ssid : "");
    reconnect();
    startScan(false);
    return true;
}

void WiFiManager::printStatus() const {
    Serial.println("=== WiFi Status ===");
    Serial.printf("State: %s\n", getStatusString());
    Serial.printf("Active Network: %s\n", activeSsid());
    Serial.printf("Using Fallback: %s\n", _useFallback ? "Yes" : "No");
    Serial.printf("Preference: %s\n",
                  (_preferred == PreferredNetwork::PRIMARY) ? "Primary" :
                  (_preferred == PreferredNetwork::FALLBACK) ? "Fallback" : "Auto");
    
    if (isConnected()) {
        {
            char ipStr[16];
            formatIPv4(WiFi.localIP(), ipStr);
            Serial.printf("IP Address: %s\n", ipStr);
        }
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("Connected SSID: %s\n", WiFi.SSID().c_str());
    } else {
        Serial.println("IP Address: Not connected");
    }

    if (_status == WiFiConnectionStatus::AP_ONLY) {
        char apIp[16];
        formatIPv4(WiFi.softAPIP(), apIp);
        Serial.printf("SoftAP: %s (ip=%s)\n", TAB5_FALLBACK_AP_SSID, apIp);
    }
    
    if (_hasFallback) {
        Serial.printf("Primary: %s\n", _ssid ? _ssid : "(none)");
        Serial.printf("Fallback: %s\n", _fallbackSsid ? _fallbackSsid : "(none)");
    } else {
        Serial.printf("Primary: %s\n", _ssid ? _ssid : "(none)");
        Serial.println("Fallback: Not configured");
    }
    
    if (_resolvedIP != INADDR_NONE) {
        char resolvedStr[16];
        formatIPv4(_resolvedIP, resolvedStr);
        Serial.printf("mDNS Resolved: %s\n", resolvedStr);
    } else {
        Serial.println("mDNS Resolved: Not resolved");
    }
}

#endif // ENABLE_WIFI
