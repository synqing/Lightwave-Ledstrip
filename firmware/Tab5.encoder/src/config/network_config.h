#pragma once
// ============================================================================
// Network Configuration - Tab5.encoder
// ============================================================================
// WiFi credentials and LightwaveOS server connection settings.
//
// IMPORTANT:
// - Do NOT commit real WiFi credentials into git-tracked files.
// - Prefer overriding these defaults via `firmware/Tab5.encoder/wifi_credentials.ini`
//   (loaded by `firmware/Tab5.encoder/platformio.ini` via `extra_configs`).
// ============================================================================

// WiFi Access Point Credentials (primary)
#ifndef WIFI_SSID
#define WIFI_SSID "LightwaveOS"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// Optional WiFi fallback credentials (secondary)
// If provided, Tab5 will try the primary first, then the fallback on failure.
#ifndef WIFI_SSID2
#define WIFI_SSID2 ""
#endif
#ifndef WIFI_PASSWORD2
#define WIFI_PASSWORD2 ""
#endif

// Tab5 fallback SoftAP (used when neither primary nor fallback SSID is visible).
// This stops endless reconnect storms and gives you a predictable “device present”
// network for diagnostics (no captive portal yet).
#ifndef TAB5_FALLBACK_AP_SSID
#define TAB5_FALLBACK_AP_SSID "Tab5Encoder"
#endif
#ifndef TAB5_FALLBACK_AP_PASSWORD
// Leave empty for an open AP. If set, ESP32 requires >=8 chars for WPA2.
#define TAB5_FALLBACK_AP_PASSWORD ""
#endif

// LightwaveOS Server
// Default: mDNS hostname (resolved automatically)
// Alternative: Use IP address if mDNS resolution fails
#define LIGHTWAVE_HOST "lightwaveos.local"
#define LIGHTWAVE_PORT 80
#define LIGHTWAVE_WS_PATH "/ws"

// Optional API key (matches LightwaveOS v2 FEATURE_API_AUTH WebSocket auth).
// If set (non-empty), Tab5 will send {"type":"auth","apiKey":"..."} after connect.
// Provide via `wifi_credentials.ini` (gitignored), e.g.:
//   -DLIGHTWAVE_API_KEY="\"your-key\""
#ifndef LIGHTWAVE_API_KEY
#define LIGHTWAVE_API_KEY ""
#endif

// Optional: Direct IP fallback (uncomment and set if mDNS fails)
// #define LIGHTWAVE_IP "192.168.1.100"

// Connection Timeouts (milliseconds)
namespace NetworkConfig {
    // WiFi connection timeout before retry
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

    // WiFi reconnection delay after disconnect
    constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 5000;

    // Scan-first WiFi strategy
    // - Scan before attempting to connect, to avoid repeated WL_NO_SSID_AVAIL loops
    // - While AP-only, rescan periodically to discover the known SSIDs returning
    constexpr uint32_t WIFI_SCAN_INTERVAL_MS = 8000;
    constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 12000;
    constexpr uint32_t WIFI_AP_ONLY_RESCAN_MS = 30000;

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
