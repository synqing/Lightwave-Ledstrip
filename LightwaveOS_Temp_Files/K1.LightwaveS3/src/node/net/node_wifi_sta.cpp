/**
 * Node WiFi Station Implementation
 */

#include "node_wifi_sta.h"
#include "../../common/util/logging.h"
#include <esp_wifi.h>

NodeWifiSta::NodeWifiSta()
    : state_(WIFI_STATE_DISCONNECTED), lastConnectAttempt_ms_(0),
      reconnectDelay_ms_(1000), connectAttempts_(0) {
}

bool NodeWifiSta::init(const char* ssid, const char* password) {
    strlcpy(ssid_, ssid, sizeof(ssid_));
    strlcpy(password_, password, sizeof(password_));
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection
    
    LW_LOGI("WiFi STA initialized, will connect to: %s", ssid_);
    
    startConnect();
    return true;
}

void NodeWifiSta::loop() {
    if (state_ == WIFI_STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            state_ = WIFI_STATE_CONNECTED;
            reconnectDelay_ms_ = 1000;  // Reset backoff
            connectAttempts_ = 0;
            
            // Disable WiFi Power Save immediately after connection for low-latency time sync
            esp_wifi_set_ps(WIFI_PS_NONE);
            
            LW_LOGI("WiFi connected! IP: %s, RSSI: %d dBm",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
            LW_LOGI("WiFi link: ssid=%s bssid=%s chan=%d gw=%s",
                    WiFi.SSID().c_str(),
                    WiFi.BSSIDstr().c_str(),
                    WiFi.channel(),
                    WiFi.gatewayIP().toString().c_str());
            LW_LOGI("WiFi Power Save DISABLED for low-latency");
        } else if (millis() - lastConnectAttempt_ms_ > 10000) {
            // Connection attempt timeout
            LW_LOGW("WiFi connection timeout (attempt %d)", connectAttempts_);
            state_ = WIFI_STATE_FAILED;
        }
    } else if (state_ == WIFI_STATE_CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            LW_LOGW("WiFi disconnected");
            state_ = WIFI_STATE_DISCONNECTED;
        }
    } else if (state_ == WIFI_STATE_DISCONNECTED || state_ == WIFI_STATE_FAILED) {
        handleReconnect();
    }
}

void NodeWifiSta::startConnect() {
    state_ = WIFI_STATE_CONNECTING;
    lastConnectAttempt_ms_ = millis();
    connectAttempts_++;
    
    LW_LOGI("Connecting to WiFi: %s (attempt %d)", ssid_, connectAttempts_);
    WiFi.begin(ssid_, password_);
}

void NodeWifiSta::handleReconnect() {
    uint32_t now = millis();
    if (now - lastConnectAttempt_ms_ >= reconnectDelay_ms_) {
        // Exponential backoff (max 32 seconds)
        reconnectDelay_ms_ = min(reconnectDelay_ms_ * 2, 32000U);
        startConnect();
    }
}
