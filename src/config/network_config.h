#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include "features.h"

#if FEATURE_NETWORK

namespace NetworkConfig {
    // WiFi credentials
    constexpr const char* WIFI_SSID = "VX220-013F";
    constexpr const char* WIFI_PASSWORD = "3232AA90E0F24";
    
    // Access Point settings (fallback)
    constexpr const char* AP_SSID = "LightwaveOS";
    constexpr const char* AP_PASSWORD = "ledcontrol";
    
    // Network settings
    constexpr uint16_t WEB_SERVER_PORT = 80;
    constexpr uint16_t WEBSOCKET_PORT = 81;
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
    constexpr uint8_t WIFI_RETRY_COUNT = 5;
    
    // mDNS settings
    constexpr const char* MDNS_HOSTNAME = "lightwaveos";
    
    // WebSocket settings
    constexpr size_t WS_MAX_CLIENTS = 4;
    constexpr uint32_t WS_PING_INTERVAL_MS = 30000;
}

#endif // FEATURE_NETWORK

#endif // NETWORK_CONFIG_H