#pragma once
// ============================================================================
// WiFiManager - Tab5.encoder
// ============================================================================
// Non-blocking WiFi connection management with mDNS resolution.
// Ported from K1.8encoderS3 with Tab5-specific adaptations.
//
// NOTE: WiFi is currently DISABLED on Tab5 (ESP32-P4) due to SDIO pin
// configuration issues with the ESP32-C6 WiFi co-processor.
// See Config.h ENABLE_WIFI flag for details.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

#include <WiFi.h>
#include <ESPmDNS.h>
#include "config/network_config.h"

// State Machine:
//   DISCONNECTED -> CONNECTING -> CONNECTED -> MDNS_RESOLVING -> MDNS_RESOLVED
//                       |              |
//                       v              v
//                     ERROR       DISCONNECTED (on WiFi loss)
//
// Key Features:
//   - Non-blocking update() for main loop integration
//   - Automatic reconnection with backoff
//   - mDNS resolution with retry backoff
//   - Uses constants from network_config.h

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

    // Initialize and start WiFi connection
    // @param ssid WiFi network SSID
    // @param password WiFi network password
    void begin(const char* ssid, const char* password);

    // Update connection state machine (call from loop())
    // Non-blocking: returns immediately, handles state transitions internally
    void update();

    // Get current connection status
    WiFiConnectionStatus getStatus() const { return _status; }

    // Get resolved IP address from mDNS (returns INADDR_NONE if not resolved)
    IPAddress getResolvedIP() const { return _resolvedIP; }

    // Get WiFi local IP address
    IPAddress getLocalIP() const { return WiFi.localIP(); }

    // Get WiFi RSSI (signal strength)
    int32_t getRSSI() const { return WiFi.RSSI(); }

    // Get connected SSID
    String getSSID() const { return WiFi.SSID(); }

    // Check if WiFi is connected (regardless of mDNS state)
    bool isConnected() const {
        return _status == WiFiConnectionStatus::CONNECTED ||
               _status == WiFiConnectionStatus::MDNS_RESOLVING ||
               _status == WiFiConnectionStatus::MDNS_RESOLVED;
    }

    // Check if mDNS resolved successfully
    bool isMDNSResolved() const {
        return _status == WiFiConnectionStatus::MDNS_RESOLVED;
    }

    // Trigger mDNS resolution (with internal backoff to prevent hammering)
    // @param hostname mDNS hostname to resolve (without .local suffix)
    // @return true if resolution succeeded, false if pending/failed
    bool resolveMDNS(const char* hostname);

    // Force reconnection (disconnect and reconnect)
    void reconnect();

    // Get status as human-readable string
    const char* getStatusString() const;

private:
    const char* _ssid;
    const char* _password;
    WiFiConnectionStatus _status;
    IPAddress _resolvedIP;

    // Timing state
    unsigned long _connectStartTime;
    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectDelay;
    unsigned long _lastMdnsAttempt;

    // mDNS state
    const char* _mdnsHostname;
    uint8_t _mdnsRetryCount;

    // State handlers
    void handleDisconnected();
    void handleConnecting();
    void handleConnected();
    void handleError();

    // Internal helpers
    void startConnection();
    void enterErrorState(const char* reason);
};

#else // ENABLE_WIFI == 0

// Stub class when WiFi is disabled
// NOTE: Full enum provided for code compatibility, but all states map to "disabled"
enum class WiFiConnectionStatus {
    DISCONNECTED,
    CONNECTING,      // Provided for compatibility, never returned
    CONNECTED,       // Provided for compatibility, never returned
    MDNS_RESOLVING,  // Provided for compatibility, never returned
    MDNS_RESOLVED,   // Provided for compatibility, never returned
    ERROR
};

class WiFiManager {
public:
    WiFiManager() {}
    void begin(const char*, const char*) {}
    void update() {}
    WiFiConnectionStatus getStatus() const { return WiFiConnectionStatus::DISCONNECTED; }
    bool isConnected() const { return false; }
    bool isMDNSResolved() const { return false; }
    void reconnect() {}
    const char* getStatusString() const { return "WiFi Disabled"; }
};

#endif // ENABLE_WIFI
