#pragma once
// ============================================================================
// Network Configuration - Tab5.encoder
// ============================================================================
// WiFi credentials and LightwaveOS server connection settings.
//
// IMPORTANT: Edit this file with your actual WiFi credentials before building.
// ============================================================================

// WiFi Access Point Credentials
// Replace with your actual network SSID and password
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// LightwaveOS Server
// Default: mDNS hostname (resolved automatically)
// Alternative: Use IP address if mDNS resolution fails
#define LIGHTWAVE_HOST "lightwaveos.local"
#define LIGHTWAVE_PORT 80
#define LIGHTWAVE_WS_PATH "/ws"

// Optional: Direct IP fallback (uncomment and set if mDNS fails)
// #define LIGHTWAVE_IP "192.168.1.100"

// Connection Timeouts (milliseconds)
namespace NetworkConfig {
    // WiFi connection timeout before retry
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

    // WiFi reconnection delay after disconnect
    constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 5000;

    // mDNS resolution initial delay after WiFi connects
    constexpr uint32_t MDNS_INITIAL_DELAY_MS = 2000;

    // mDNS resolution retry interval
    constexpr uint32_t MDNS_RETRY_DELAY_MS = 10000;

    // WebSocket initial reconnect delay
    constexpr uint32_t WS_INITIAL_RECONNECT_MS = 1000;

    // WebSocket maximum reconnect delay (exponential backoff cap)
    constexpr uint32_t WS_MAX_RECONNECT_MS = 30000;

    // WebSocket connection/handshake timeout
    constexpr uint32_t WS_CONNECTION_TIMEOUT_MS = 20000;

    // Per-parameter send throttle (minimum interval between sends)
    constexpr uint32_t PARAM_THROTTLE_MS = 50;
}
