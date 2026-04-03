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

        // Clean disconnect before retry — clears stale driver state
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.begin(_ssid, _password);

        Serial.println("[WiFi] Connecting...");
    }

    void handleConnecting() {
        wl_status_t ws = WiFi.status();
        uint32_t elapsed = millis() - _connectStart;

        if (ws == WL_CONNECTED) {
            _status = Status::CONNECTED;
            _reconnectDelay = net::WIFI_RECONNECT_DELAY_MS;  // Reset backoff
            _retryCount = 0;
            Serial.printf("[WiFi] Connected! IP: %s, RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else if (ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL) {
            _retryCount++;
            Serial.printf("[WiFi] Failed (status: %d, retry %d)\n", ws, _retryCount);
            WiFi.disconnect(true);
            _status = Status::DISCONNECTED;
            _lastReconnect = millis();
        } else if (elapsed >= net::WIFI_CONNECT_TIMEOUT_MS) {
            _retryCount++;
            Serial.printf("[WiFi] Timeout after %lu ms (retry %d)\n", elapsed, _retryCount);
            WiFi.disconnect(true);
            _status = Status::DISCONNECTED;
            _lastReconnect = millis();
        }
    }

    void handleDisconnected() {
        uint32_t now = millis();
        // Fast retry for first 5 attempts (2s), then backoff (5s→10s→30s cap)
        uint32_t delay = (_retryCount < 5) ? 2000 : _reconnectDelay;
        if (now - _lastReconnect >= delay) {
            _lastReconnect = now;
            Serial.printf("[WiFi] Reconnecting (attempt %d, delay %lu ms)...\n",
                          _retryCount + 1, delay);
            startConnection();
            if (_retryCount >= 5) {
                _reconnectDelay = min(_reconnectDelay * 2, net::WIFI_MAX_RECONNECT_MS);
            }
        }
    }

    const char* _ssid = nullptr;
    const char* _password = nullptr;
    Status _status = Status::DISCONNECTED;
    uint32_t _connectStart = 0;
    uint32_t _lastReconnect = 0;
    uint32_t _reconnectDelay = net::WIFI_RECONNECT_DELAY_MS;
    uint8_t _retryCount = 0;
};
