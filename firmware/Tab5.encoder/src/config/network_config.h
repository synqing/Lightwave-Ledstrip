#pragma once
// ============================================================================
// Network Configuration - Tab5.encoder
// ============================================================================
// WiFi credentials and LightwaveOS server connection settings.
//
// IMPORTANT: Edit this file with your actual WiFi credentials before building.
// ============================================================================

// WiFi Access Point Credentials (fallback when both networks fail)
// Matches firmware/v2 AP name for consistency
// NOTE: v2 firmware uses "lightwave123" password
#define AP_SSID "LightwaveOS"
#define AP_PASSWORD "lightwave123"

// WiFi Station Credentials (defaults, overridden by build flags)
#ifndef WIFI_SSID
#define WIFI_SSID "VX220-013F"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "3232AA90E0F24"
#endif

// Secondary WiFi Network (fallback)
// Configured to connect to firmware/v2 AP ("LightwaveOS") if primary fails
#ifndef WIFI_SSID2
#define WIFI_SSID2 "LightwaveOS"
#endif
#ifndef WIFI_PASSWORD2
#define WIFI_PASSWORD2 "lightwave123"
#endif

// LightwaveOS Server
// Default: mDNS hostname (resolved automatically)
// Alternative: Use IP address if mDNS resolution fails
#define LIGHTWAVE_HOST "lightwaveos.local"
#define LIGHTWAVE_PORT 80
#define LIGHTWAVE_WS_PATH "/ws"

// Optional: Direct IP fallback (uncomment and set if mDNS fails)
// #define LIGHTWAVE_IP "192.168.4.1"

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

    // AP fallback settings (matches firmware/v2)
    constexpr const char* AP_SSID_VALUE = AP_SSID;
    constexpr const char* AP_PASSWORD_VALUE = AP_PASSWORD;
    
    // Number of connection attempts per network before switching
    constexpr uint8_t WIFI_ATTEMPTS_PER_NETWORK = 2;
    
    // Delay before falling back to AP mode (after both networks fail)
    constexpr uint32_t AP_FALLBACK_DELAY_MS = 10000;  // 10 seconds
}

// OTA Update Configuration
// SECURITY: Change this token in production deployments!
#ifndef OTA_UPDATE_TOKEN
#define OTA_UPDATE_TOKEN "LW-OTA-2024-SecureUpdate"
#endif
