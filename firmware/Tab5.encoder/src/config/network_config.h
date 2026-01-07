#pragma once
// ============================================================================
// Network Configuration - Tab5.encoder
// ============================================================================
// WiFi credentials and LightwaveOS server connection settings.
//
// IMPORTANT: Edit this file with your actual WiFi credentials before building.
// ============================================================================

// WiFi Access Point Credentials (DISABLED - Tab5 should never create its own AP)
// Tab5 is a slave to v2 device which provides the AP
// #define AP_SSID "LightwaveOS"
// #define AP_PASSWORD "lightwave123"

// WiFi Station Credentials (defaults, overridden by build flags)
#ifndef WIFI_SSID
#define WIFI_SSID "VX220-013F"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "3232AA90E0F24"
#endif

// Secondary WiFi Network (fallback)
// Tab5 automatically connects to v2 device's AP when primary WiFi unavailable
// v2 device AP: SSID="LightwaveOS-AP", Password="SpectraSynq", IP=192.168.4.1
#ifndef WIFI_SSID2
#define WIFI_SSID2 "LightwaveOS-AP"
#endif
#ifndef WIFI_PASSWORD2
#define WIFI_PASSWORD2 "SpectraSynq"
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
    
    // Number of connection attempts per network before switching
    constexpr uint8_t WIFI_ATTEMPTS_PER_NETWORK = 2;
    
    // WiFi retry timeout before showing retry button (2 minutes)
    constexpr uint32_t WIFI_RETRY_TIMEOUT_MS = 120000;  // 2 minutes
}

// OTA Update Configuration
// SECURITY: Change this token in production deployments!
#ifndef OTA_UPDATE_TOKEN
#define OTA_UPDATE_TOKEN "LW-OTA-2024-SecureUpdate"
#endif
