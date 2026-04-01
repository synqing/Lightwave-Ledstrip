/**
 * Zone Mixer — WiFi Manager
 * Simplified state machine for connecting to K1's AP.
 * No mDNS needed — K1 is always at 192.168.4.1 (AP-only architecture).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <WiFi.h>
#include "../config.h"

class ZMWiFiManager {
public:
    enum class Status : uint8_t {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
    };

    void begin(const char* ssid, const char* password) {
        _ssid = ssid;
        _password = password;
        Serial.printf("[WiFi] Target SSID: %s\n", _ssid);
        startConnection();
    }

    void update() {
        switch (_status) {
            case Status::DISCONNECTED:
                handleDisconnected();
                break;
            case Status::CONNECTING:
                handleConnecting();
                break;
            case Status::CONNECTED:
                // Check if WiFi dropped
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[WiFi] Connection lost");
                    _status = Status::DISCONNECTED;
                    _lastReconnect = millis();
                }
                break;
        }
    }

    bool isConnected() const { return _status == Status::CONNECTED; }
    Status getStatus() const { return _status; }
    IPAddress getLocalIP() const { return WiFi.localIP(); }
    int32_t getRSSI() const { return WiFi.RSSI(); }

private:
    void startConnection() {
        _status = Status::CONNECTING;
        _connectStart = millis();

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);  // We handle reconnection ourselves
        WiFi.begin(_ssid, _password);

        Serial.println("[WiFi] Connecting...");
    }

    void handleConnecting() {
        wl_status_t ws = WiFi.status();
        uint32_t elapsed = millis() - _connectStart;

        if (ws == WL_CONNECTED) {
            _status = Status::CONNECTED;
            _reconnectDelay = net::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
            Serial.printf("[WiFi] Connected! IP: %s, RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else if (ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL) {
            Serial.printf("[WiFi] Failed (status: %d)\n", ws);
            _status = Status::DISCONNECTED;
            _lastReconnect = millis();
        } else if (elapsed >= net::WIFI_CONNECT_TIMEOUT_MS) {
            Serial.printf("[WiFi] Timeout after %lu ms\n", elapsed);
            WiFi.disconnect();
            _status = Status::DISCONNECTED;
            _lastReconnect = millis();
        }
    }

    void handleDisconnected() {
        uint32_t now = millis();
        if (now - _lastReconnect >= _reconnectDelay) {
            _lastReconnect = now;
            Serial.printf("[WiFi] Reconnecting (backoff: %lu ms)...\n", _reconnectDelay);
            startConnection();
            // Exponential backoff, capped
            _reconnectDelay = min(_reconnectDelay * 2, net::WIFI_MAX_RECONNECT_MS);
        }
    }

    const char* _ssid = nullptr;
    const char* _password = nullptr;
    Status _status = Status::DISCONNECTED;
    uint32_t _connectStart = 0;
    uint32_t _lastReconnect = 0;
    uint32_t _reconnectDelay = net::WIFI_RECONNECT_DELAY_MS;
};
