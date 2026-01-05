#pragma once
// ============================================================================
// WiFiManager - Tab5.encoder
// ============================================================================
// Non-blocking WiFi connection management with mDNS resolution.
// Ported from K1.8encoderS3 with Tab5-specific adaptations.
//
// ESP32-P4 uses ESP32-C6 co-processor for WiFi via SDIO.
// Requires WiFi.setPins() before WiFi.begin() - see Config.h for pin defs.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

#include <WiFi.h>
#include <ESPmDNS.h>
#include "config/network_config.h"

	// State Machine:
	//   DISCONNECTED -> SCANNING -> CONNECTING -> CONNECTED -> MDNS_RESOLVING -> MDNS_RESOLVED
	//        |              |             |              |
	//        |              |             v              v
	//        |              |           ERROR       DISCONNECTED (on WiFi loss)
	//        |              |
	//        +-----------> AP_ONLY  (no known networks in range; rescan periodically)
	//
	// Key Features:
	//   - Non-blocking update() for main loop integration
	//   - Automatic reconnection with backoff
	//   - Scan-first: chooses an available known SSID before connecting
	//   - AP-only fallback when no known SSIDs are visible (stops reconnect storms)
	//   - mDNS resolution with retry backoff
	//   - Uses constants from network_config.h

	enum class WiFiConnectionStatus {
	    DISCONNECTED,
	    SCANNING,
	    CONNECTING,
	    CONNECTED,
	    MDNS_RESOLVING,
	    MDNS_RESOLVED,
	    AP_ONLY,
	    ERROR
	};

class WiFiManager {
public:
    WiFiManager();

    // Initialize and start WiFi connection
    // @param ssid WiFi network SSID
    // @param password WiFi network password
    void begin(const char* ssid, const char* password);

    // Initialize and start WiFi connection with optional fallback credentials.
    // Tab5 will try the primary network first, then the fallback on failure.
    // Passwords are never printed.
    void beginWithFallback(const char* ssid,
                           const char* password,
                           const char* fallbackSsid,
                           const char* fallbackPassword);

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

    // ========================================================================
    // Network Mode Control (similar to v2 firmware "NET STA" / "NET AP")
    // ========================================================================

    /**
     * @brief Switch to fallback network (STA mode equivalent)
     * 
     * Forces connection to the fallback network (typically a router/STA network).
     * This is equivalent to v2 firmware's "NET STA" command.
     * 
     * @return true if fallback is available and switch was initiated, false otherwise
     */
    bool requestSTA();

    /**
     * @brief Switch back to primary network (AP mode equivalent)
     * 
     * Forces connection back to the primary network (typically the LightwaveOS AP).
     * This is equivalent to v2 firmware's "NET AP" command.
     * 
     * @return true if switch was initiated, false if already on primary
     */
    bool requestAP();

    /**
     * @brief Get detailed status information for serial output
     * 
     * Returns formatted status string with current network, IP, RSSI, etc.
     */
    void printStatus() const;

private:
    enum class PreferredNetwork : uint8_t {
        AUTO,
        PRIMARY,
        FALLBACK
    };

    const char* _ssid;
    const char* _password;
    const char* _fallbackSsid;
    const char* _fallbackPassword;
    bool _hasFallback;
    bool _useFallback;
    PreferredNetwork _preferred = PreferredNetwork::AUTO;
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
	    void handleScanning();
	    void handleConnecting();
	    void handleConnected();
	    void handleApOnly();
	    void handleError();

	    // Internal helpers
	    void startConnection();
	    void startScan(bool keepAp);
	    void enterApOnlyMode();
	    const char* activeSsid() const;
	    const char* activePassword() const;
	    void enterErrorState(const char* reason);

    // WiFi scanning / AP-only state
    unsigned long _scanStartTime = 0;
    unsigned long _lastScanAttempt = 0;
    bool _scanKeepAp = false;
};

#else // ENABLE_WIFI == 0

// Stub class when WiFi is disabled
// NOTE: Full enum provided for code compatibility, but all states map to "disabled"
	enum class WiFiConnectionStatus {
	    DISCONNECTED,
	    SCANNING,        // Provided for compatibility, never returned
	    CONNECTING,      // Provided for compatibility, never returned
	    CONNECTED,       // Provided for compatibility, never returned
	    MDNS_RESOLVING,  // Provided for compatibility, never returned
	    MDNS_RESOLVED,   // Provided for compatibility, never returned
	    AP_ONLY,         // Provided for compatibility, never returned
	    ERROR
	};

class WiFiManager {
public:
    WiFiManager() {}
    void begin(const char*, const char*) {}
    void beginWithFallback(const char*, const char*, const char*, const char*) {}
    void update() {}
    WiFiConnectionStatus getStatus() const { return WiFiConnectionStatus::DISCONNECTED; }
    bool isConnected() const { return false; }
    bool isMDNSResolved() const { return false; }
    void reconnect() {}
    const char* getStatusString() const { return "WiFi Disabled"; }
    bool requestSTA() { return false; }
    bool requestAP() { return false; }
    void printStatus() const { Serial.println("[WiFi] WiFi disabled (ENABLE_WIFI=0)"); }
};

#endif // ENABLE_WIFI
