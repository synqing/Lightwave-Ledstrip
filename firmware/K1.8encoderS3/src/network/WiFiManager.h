#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>

enum class WiFiConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    MDNS_RESOLVING,
    MDNS_RESOLVED,
    ERROR
};

class WiFiManager {
public:
    WiFiManager();

    // Initialize and connect to WiFi
    void begin(const char* ssid, const char* password);

    // Update connection state and handle reconnection
    void update();

    // Get current connection status
    WiFiConnectionStatus getStatus() const { return _status; }

    // Get resolved IP address from mDNS (returns INADDR_NONE if not resolved)
    IPAddress getResolvedIP() const { return _resolvedIP; }

    // Get WiFi local IP
    IPAddress getLocalIP() const { return WiFi.localIP(); }

    // Check if connected to WiFi
    bool isConnected() const { return _status == WiFiConnectionStatus::CONNECTED || _status == WiFiConnectionStatus::MDNS_RESOLVED; }

    // Check if mDNS resolved successfully
    bool isMDNSResolved() const { return _status == WiFiConnectionStatus::MDNS_RESOLVED; }

    // Manually trigger mDNS resolution (with internal backoff to prevent hammering)
    bool resolveMDNS(const char* hostname);

private:
    const char* _ssid;
    const char* _password;
    WiFiConnectionStatus _status;
    IPAddress _resolvedIP;
    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectDelay;
    unsigned long _lastMdnsAttempt;
    const char* _mdnsHostname;

    static constexpr unsigned long RECONNECT_DELAY_MS = 5000;
    static constexpr unsigned long MDNS_RETRY_DELAY_MS = 10000;
    static constexpr unsigned long MDNS_INITIAL_DELAY_MS = 2000;

    void handleDisconnected();
    void handleConnecting();
    void handleConnected();
};
