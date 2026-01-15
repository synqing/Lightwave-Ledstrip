/**
 * @file node_wifi_sta.h
 * @brief WiFi Station Mode with Reconnect Policy
 * 
 * Connects to Hub SoftAP with exponential backoff on failure.
 */

#pragma once

#include <WiFi.h>
#include "../../common/proto/proto_constants.h"

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} node_wifi_state_t;

class NodeWifiSta {
public:
    NodeWifiSta();
    
    bool init(const char* ssid, const char* password);
    void loop();
    
    bool isConnected() const { return state_ == WIFI_STATE_CONNECTED; }
    node_wifi_state_t getState() const { return state_; }
    int8_t getRSSI() const { return WiFi.RSSI(); }
    String getIP() const { return WiFi.localIP().toString(); }
    String getMAC() const { return WiFi.macAddress(); }
    
private:
    node_wifi_state_t state_;
    char ssid_[32];
    char password_[64];
    uint32_t lastConnectAttempt_ms_;
    uint32_t reconnectDelay_ms_;
    uint8_t connectAttempts_;
    
    void startConnect();
    void handleReconnect();
};
