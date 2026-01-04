/**
 * @file network_config.h
 * @brief Network configuration for LightwaveOS v2
 *
 * WiFi credentials are configured via build flags in platformio.ini.
 *
 * To set your credentials:
 * 1. Copy wifi_credentials.ini.template to wifi_credentials.ini
 * 2. Edit wifi_credentials.ini with your network details
 * 3. This file is gitignored - your credentials stay private
 *
 * Or add build flags directly to platformio.ini:
 *   -D WIFI_SSID=\"YourNetwork\"
 *   -D WIFI_PASSWORD=\"YourPassword\"
 */

#pragma once

#include "features.h"

#if FEATURE_WEB_SERVER

namespace lightwaveos {
namespace config {

/**
 * @brief Network configuration constants
 */
namespace NetworkConfig {
    // ========================================================================
    // WiFi Credentials (from build flags or defaults)
    // ========================================================================

    #ifdef WIFI_SSID
        constexpr const char* WIFI_SSID_VALUE = WIFI_SSID;
    #else
        // No default - must be configured via build flags or wifi_credentials.ini
        constexpr const char* WIFI_SSID_VALUE = "CONFIGURE_ME";
    #endif

    #ifdef WIFI_PASSWORD
        constexpr const char* WIFI_PASSWORD_VALUE = WIFI_PASSWORD;
    #else
        constexpr const char* WIFI_PASSWORD_VALUE = "";
    #endif

    // ========================================================================
    // WiFi Credentials - Secondary/Fallback Network (Optional)
    // ========================================================================

    #ifdef WIFI_SSID_2
        constexpr const char* WIFI_SSID_2_VALUE = WIFI_SSID_2;
    #else
        constexpr const char* WIFI_SSID_2_VALUE = "";  // Empty = disabled
    #endif

    #ifdef WIFI_PASSWORD_2
        constexpr const char* WIFI_PASSWORD_2_VALUE = WIFI_PASSWORD_2;
    #else
        constexpr const char* WIFI_PASSWORD_2_VALUE = "";
    #endif

    // ========================================================================
    // Multi-Network Settings
    // ========================================================================

    constexpr uint8_t WIFI_ATTEMPTS_PER_NETWORK = 2;  // Try each network 2 times before switching

    // ========================================================================
    // Access Point Settings (fallback when WiFi fails)
    // ========================================================================
    // AP SSID: "LightwaveOS-AP" - Tab5.encoder devices connect to this as secondary network
    // Password: "SpectraSynq" - matches Tab5 configuration
    // IP: 192.168.4.1 (default SoftAP gateway)

    #ifdef AP_SSID_CUSTOM
        constexpr const char* AP_SSID = AP_SSID_CUSTOM;
    #else
        constexpr const char* AP_SSID = "LightwaveOS-AP";
    #endif

    #ifdef AP_PASSWORD_CUSTOM
        constexpr const char* AP_PASSWORD = AP_PASSWORD_CUSTOM;
    #else
        constexpr const char* AP_PASSWORD = "SpectraSynq";
    #endif

    // ========================================================================
    // WiFi Mode Selection
    // ========================================================================
    // Set to 1 to force AP-only mode (STA architecture remains, just disabled)
#ifdef LW_FORCE_AP_MODE
    constexpr bool FORCE_AP_MODE = (LW_FORCE_AP_MODE != 0);
#else
    constexpr bool FORCE_AP_MODE = true;
#endif

    // ========================================================================
    // Network Settings
    // ========================================================================

    constexpr uint16_t WEB_SERVER_PORT = 80;
    constexpr uint16_t WEBSOCKET_PORT = 81;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
    constexpr uint8_t WIFI_RETRY_COUNT = 5;

    // ========================================================================
    // mDNS Settings
    // ========================================================================

    constexpr const char* MDNS_HOSTNAME = "lightwaveos";

    // ========================================================================
    // OTA Security
    // Override via build flag: -D OTA_TOKEN=\"your-unique-token\"
    // ========================================================================

#ifdef OTA_TOKEN
    constexpr const char* OTA_UPDATE_TOKEN = OTA_TOKEN;
#else
    constexpr const char* OTA_UPDATE_TOKEN = "LW-OTA-2024-SecureUpdate";  // Default (change in production!)
#endif

    // ========================================================================
    // API Key Authentication
    // Enable: -D FEATURE_API_AUTH=1 -D API_KEY=\"your-secret-key\"
    // ========================================================================

#ifdef API_KEY
    constexpr const char* API_KEY_VALUE = API_KEY;
#else
    constexpr const char* API_KEY_VALUE = "";  // Empty = auth disabled
#endif

    // ========================================================================
    // WebSocket Settings
    // ========================================================================

    constexpr size_t WS_MAX_CLIENTS = 4;
    constexpr uint32_t WS_PING_INTERVAL_MS = 30000;

    // ========================================================================
    // WiFiManager Settings
    // ========================================================================

    constexpr uint32_t SCAN_INTERVAL_MS = 60000;        // Re-scan every minute
    constexpr uint32_t RECONNECT_DELAY_MS = 5000;       // 5s between reconnect attempts
    constexpr uint32_t MAX_RECONNECT_DELAY_MS = 60000;  // Max 1 minute backoff

} // namespace NetworkConfig

} // namespace config
} // namespace lightwaveos

#endif // FEATURE_WEB_SERVER
