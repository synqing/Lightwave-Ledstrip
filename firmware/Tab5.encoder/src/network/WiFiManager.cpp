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

#include <esp_log.h>
#include <mdns.h>

WiFiManager::WiFiManager()
    : _ssid(nullptr)
    , _password(nullptr)
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

    Serial.println("[WiFi] Starting connection...");
    Serial.printf("[WiFi] SSID: %s\n", _ssid);

    startConnection();
}

void WiFiManager::startConnection() {
    _status = WiFiConnectionStatus::CONNECTING;
    _connectStartTime = millis();

    // Configure WiFi for station mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection ourselves

    // Start connection
    WiFi.begin(_ssid, _password);

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

        // Silence ESPmDNS internal warnings while keeping resolution active.
        esp_log_level_set("mdns", ESP_LOG_ERROR);

        // Reset mDNS resolution state for target host
        _lastMdnsAttempt = 0;
        _mdnsRetryCount = 0;

    } else if (wifiStatus == WL_CONNECT_FAILED ||
               wifiStatus == WL_NO_SSID_AVAIL) {
        // Connection failed
        Serial.printf("[WiFi] Connection failed (status: %d)\n", wifiStatus);
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();

    } else if (elapsed >= NetworkConfig::WIFI_CONNECT_TIMEOUT_MS) {
        // Connection timeout
        Serial.printf("[WiFi] Connection timeout after %lu ms\n", elapsed);
        WiFi.disconnect();
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

    // Query mDNS for the hostname (direct IDF API to avoid noisy warnings)
    esp_ip4_addr_t addr;
    addr.addr = 0;
    esp_err_t err = mdns_query_a(hostname, 2000, &addr);

    if (err == ESP_OK && addr.addr != 0) {
        _resolvedIP = IPAddress(addr.addr);
        _status = WiFiConnectionStatus::MDNS_RESOLVED;
        return true;
    } else {
        // Resolution failed, stay in CONNECTED state for retry
        _status = WiFiConnectionStatus::CONNECTED;
        return false;
    }
}

void WiFiManager::reconnect() {
    Serial.println("[WiFi] Forcing reconnect...");
    WiFi.disconnect();
    _status = WiFiConnectionStatus::DISCONNECTED;
    _resolvedIP = INADDR_NONE;
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
