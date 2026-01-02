#include "WiFiManager.h"

WiFiManager::WiFiManager()
    : _ssid(nullptr)
    , _password(nullptr)
    , _status(WiFiConnectionStatus::DISCONNECTED)
    , _resolvedIP(INADDR_NONE)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(RECONNECT_DELAY_MS)
    , _lastMdnsAttempt(0)
    , _mdnsHostname(nullptr)
{
}

void WiFiManager::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    _status = WiFiConnectionStatus::CONNECTING;

    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);
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
        case WiFiConnectionStatus::MDNS_RESOLVED:
            handleConnected();
            break;

        case WiFiConnectionStatus::MDNS_RESOLVING:
            // Handled by resolveMDNS() calls
            break;

        case WiFiConnectionStatus::ERROR:
            // Stay in error state until manual reconnect
            break;
    }
}

void WiFiManager::handleDisconnected() {
    unsigned long now = millis();

    // Attempt reconnect with delay
    if (now - _lastReconnectAttempt >= _reconnectDelay) {
        _lastReconnectAttempt = now;
        _status = WiFiConnectionStatus::CONNECTING;
        WiFi.begin(_ssid, _password);
    }
}

void WiFiManager::handleConnecting() {
    wl_status_t wifiStatus = WiFi.status();

    if (wifiStatus == WL_CONNECTED) {
        _status = WiFiConnectionStatus::CONNECTED;

        // Initialize mDNS
        if (!MDNS.begin("tab5encoder")) {
            // mDNS failed but WiFi is connected
            // Continue without mDNS
        }
    } else if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL) {
        _status = WiFiConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis();
    }
}

void WiFiManager::handleConnected() {
    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        _status = WiFiConnectionStatus::DISCONNECTED;
        _resolvedIP = INADDR_NONE;
        _lastReconnectAttempt = millis();
        _lastMdnsAttempt = 0;  // Reset mDNS attempt timer on disconnect
    }
}

bool WiFiManager::resolveMDNS(const char* hostname) {
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
    unsigned long delay = (_lastMdnsAttempt == 0) ? MDNS_INITIAL_DELAY_MS : MDNS_RETRY_DELAY_MS;
    if (now - _lastMdnsAttempt < delay) {
        return false;  // Still in backoff period
    }

    _lastMdnsAttempt = now;
    _status = WiFiConnectionStatus::MDNS_RESOLVING;

    // Query mDNS for the hostname
    IPAddress resolvedIP = MDNS.queryHost(hostname);

    if (resolvedIP != INADDR_NONE) {
        _resolvedIP = resolvedIP;
        _status = WiFiConnectionStatus::MDNS_RESOLVED;
        return true;
    } else {
        _status = WiFiConnectionStatus::CONNECTED;
        return false;
    }
}

