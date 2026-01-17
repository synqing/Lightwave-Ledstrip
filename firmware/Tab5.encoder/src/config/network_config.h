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
#define WIFI_SSID "OPTUS_738CC0N"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "parrs45432vw"
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
//
// Multi-tier fallback strategy (in priority order):
//   1. Compile-time LIGHTWAVE_IP (if defined) → Immediate connection, bypasses mDNS
//   2. Manual IP from NVS (if configured via UI) → User-configured IP address
//   3. mDNS resolution → Attempts with backoff (10s intervals, max 6 attempts)
//   4. Timeout-based fallback → After 60s or 6 failed attempts:
//      - Uses manual IP from NVS (if set)
//      - Falls back to gateway IP (if on secondary network: LightwaveOS-AP)
//
// NVS Storage:
//   - Namespace: "tab5net"
//   - Keys: "manual_ip" (string), "use_manual" (bool)
//   - Manual IP can be configured via UI (DisplayUI::showNetworkConfigScreen())
//
#define LIGHTWAVE_HOST "lightwaveos.local"
#define LIGHTWAVE_PORT 80
#define LIGHTWAVE_WS_PATH "/ws"

// Optional: Direct IP fallback (uncomment and set if mDNS fails)
// This bypasses mDNS entirely and connects directly to the specified IP.
// Highest priority - takes precedence over all other methods.
// Useful for debugging or when mDNS is unreliable.
//
// To find v2 device IP:
//  1. Check v2 device serial logs for "STA IP: x.x.x.x"
//  2. Or use router admin interface to find device IP
//  3. Or connect to v2's AP (LightwaveOS-AP) - gateway IP is 192.168.4.1
//
// Example for v2 device on local network (replace with actual IP):
// #define LIGHTWAVE_IP "192.168.1.102"
//
// Example for v2 device's AP fallback:
#define LIGHTWAVE_IP "192.168.4.1"

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
    
    // mDNS timeout before fallback (milliseconds)
    // After this timeout or MDNS_MAX_ATTEMPTS, system will use fallback IP
    // (manual IP from NVS or gateway IP on secondary network)
    constexpr uint32_t MDNS_FALLBACK_TIMEOUT_MS = 60000;  // 60 seconds
    
    // Maximum mDNS attempts before fallback
    // System will attempt mDNS resolution this many times before using fallback
    constexpr uint8_t MDNS_MAX_ATTEMPTS = 6;  // 6 attempts × 10s = 60s

    // WebSocket initial reconnect delay
    constexpr uint32_t WS_INITIAL_RECONNECT_MS = 1000;

    // WebSocket maximum reconnect delay (exponential backoff cap)
    constexpr uint32_t WS_MAX_RECONNECT_MS = 30000;

    // WebSocket connection/handshake timeout
    constexpr uint32_t WS_CONNECTION_TIMEOUT_MS = 20000;

    // Per-parameter send throttle (minimum interval between sends)
    constexpr uint32_t PARAM_THROTTLE_MS = 50;
    
    // Send queue stale message timeout (drop messages older than this)
    constexpr uint32_t SEND_QUEUE_STALE_TIMEOUT_MS = 500;  // 500ms max age
    
    // Number of connection attempts per network before switching
    constexpr uint8_t WIFI_ATTEMPTS_PER_NETWORK = 2;
    
    // WiFi retry timeout before showing retry button (2 minutes)
    constexpr uint32_t WIFI_RETRY_TIMEOUT_MS = 120000;  // 2 minutes
    
    // Default fallback IP for primary network (when mDNS fails)
    // This should match v2 device's typical IP on primary network
    constexpr const char* MDNS_FALLBACK_IP_PRIMARY = "192.168.1.102";
}

// NVS namespace and keys for network configuration
namespace NetworkNVS {
    constexpr const char* NAMESPACE = "tab5net";
    constexpr const char* KEY_MANUAL_IP = "manual_ip";
    constexpr const char* KEY_USE_MANUAL_IP = "use_manual";
}

// OTA Update Configuration
// SECURITY: Change this token in production deployments!
#ifndef OTA_UPDATE_TOKEN
#define OTA_UPDATE_TOKEN "LW-OTA-2024-SecureUpdate"
#endif
