/**
 * @file hub_softap_dhcp.h
 * @brief SoftAP + DHCP Configuration
 * 
 * Starts SoftAP with configured SSID/password and DHCP server.
 */

#pragma once

#include <WiFi.h>

class HubSoftApDhcp {
public:
    bool init(const char* ssid, const char* password, const char* ip = "192.168.4.1");
    bool isRunning() const { return running_; }
    String getIP() const { return WiFi.softAPIP().toString(); }
    uint8_t getStationCount() const { return WiFi.softAPgetStationNum(); }
    
private:
    bool running_;
};
