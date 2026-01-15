/**
 * Hub SoftAP + DHCP Implementation
 */

#include "hub_softap_dhcp.h"
#include "../../common/util/logging.h"
#include <WiFi.h>
#include <esp_wifi.h>

bool HubSoftApDhcp::init(const char* ssid, const char* password, const char* ip) {
    // Configure AP IP
    IPAddress local_ip, gateway, subnet;
    if (!local_ip.fromString(ip)) {
        LW_LOGE("Invalid IP address: %s", ip);
        return false;
    }
    gateway = local_ip;
    subnet.fromString("255.255.255.0");
    
    if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        LW_LOGE("softAPConfig failed");
        return false;
    }
    
    // Start SoftAP
    if (!WiFi.softAP(ssid, password)) {
        LW_LOGE("softAP failed to start");
        return false;
    }
    
    // Disable WiFi Power Save for low-latency time sync
    esp_wifi_set_ps(WIFI_PS_NONE);
    LW_LOGI("WiFi Power Save DISABLED for low-latency");
    
    LW_LOGI("SoftAP started: %s", ssid);
    LW_LOGI("  IP: %s", WiFi.softAPIP().toString().c_str());
    LW_LOGI("  Password: %s", password);
    LW_LOGI("  BSSID: %s", WiFi.softAPmacAddress().c_str());
    LW_LOGI("  Channel: %d", WiFi.channel());
    LW_LOGI("  Stations (Arduino): %d", WiFi.softAPgetStationNum());
    wifi_sta_list_t list;
    if (esp_wifi_ap_get_sta_list(&list) == ESP_OK) {
        LW_LOGI("  Stations (esp_wifi): %u", (unsigned)list.num);
    }
    
    running_ = true;
    return true;
}
