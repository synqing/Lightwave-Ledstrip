/**
 * Zone Mixer — WebSocket Client
 * Connects to K1's WebSocket server for bidirectional JSON communication.
 * Based on Tab5 encoder patterns (links2004/WebSocketsClient).
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <freertos/semphr.h>
#include "../config.h"

class ZMWebSocketClient {
public:
    using MessageCallback = void(*)(JsonDocument& doc);

    ZMWebSocketClient() {
        _sendMutex = xSemaphoreCreateMutex();
        for (uint8_t i = 0; i < net::SEND_QUEUE_SIZE; i++) {
            _queue[i].valid = false;
        }
        for (uint8_t i = 0; i < net::SEND_QUEUE_SIZE; i++) {
            _rateLimiter[i] = 0;
        }
        _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
            handleEvent(type, payload, length);
        });
    }

    void begin(IPAddress ip, uint16_t port, const char* path) {
        if (_status == Status::CONNECTING || _status == Status::CONNECTED) return;
        _serverIP = ip;
        _port = port;
        _path = path;
        _shouldReconnect = true;
        _status = Status::CONNECTING;
        Serial.printf("[WS] Connecting to ws://%s:%d%s...\n",
                      ip.toString().c_str(), port, path);
        _ws.setReconnectInterval(net::WS_CONNECTION_TIMEOUT_MS);
        _ws.begin(ip, port, path);
    }

    void update() {
        _ws.loop();
        processSendQueue();

        if (_pendingHello && _status == Status::CONNECTED) {
            _pendingHello = false;
            sendHello();
        }

        // Stuck-in-connecting timeout
        if (_status == Status::CONNECTING) {
            if (_connectingStart == 0) {
                _connectingStart = millis();
            } else if (millis() - _connectingStart > net::WS_CONNECTION_TIMEOUT_MS) {
                Serial.println("[WS] Connection timeout, resetting");
                _status = Status::DISCONNECTED;
                _connectingStart = 0;
                increaseBackoff();
            }
        } else {
            _connectingStart = 0;
        }

        // Reconnect logic
        if (_status == Status::DISCONNECTED && _shouldReconnect) {
            uint32_t now = millis();
            if (now - _lastReconnect >= _reconnectDelay) {
                _lastReconnect = now;
                _status = Status::CONNECTING;
                Serial.printf("[WS] Reconnecting (backoff: %lu ms)...\n", _reconnectDelay);
                _ws.begin(_serverIP, _port, _path);
            }
        }
    }

    void onMessage(MessageCallback cb) { _messageCallback = cb; }
    bool isConnected() const { return _status == Status::CONNECTED; }

    // --- Send helpers ---

    void sendJSON(const char* type, JsonDocument& doc) {
        if (!isConnected()) return;
        if (!takeLock()) return;

        JsonDocument msg;
        msg["type"] = type;
        JsonObject src = doc.as<JsonObject>();
        for (JsonPair kv : src) {
            msg[kv.key()] = kv.value();
        }

        size_t len = serializeJson(msg, _jsonBuf, net::JSON_BUFFER_SIZE - 1);
        if (len > 0 && len < net::JSON_BUFFER_SIZE) {
            _jsonBuf[len] = '\0';
            _ws.sendTXT(_jsonBuf);
        }
        releaseLock();
    }

    void sendSimple(const char* type) {
        if (!isConnected()) return;
        if (!takeLock()) return;
        JsonDocument msg;
        msg["type"] = type;
        size_t len = serializeJson(msg, _jsonBuf, net::JSON_BUFFER_SIZE - 1);
        if (len > 0) {
            _jsonBuf[len] = '\0';
            _ws.sendTXT(_jsonBuf);
        }
        releaseLock();
    }

    bool canSend(uint8_t paramIdx) {
        if (paramIdx >= net::SEND_QUEUE_SIZE) return false;
        uint32_t now = millis();
        if (now - _rateLimiter[paramIdx] >= net::PARAM_THROTTLE_MS) {
            _rateLimiter[paramIdx] = now;
            return true;
        }
        return false;
    }

    void queueParam(uint8_t paramIdx, int32_t value, const char* type, uint8_t zoneId = 0xFF) {
        if (paramIdx >= net::SEND_QUEUE_SIZE) return;
        _queue[paramIdx] = { paramIdx, value, zoneId, millis(), type, true };
    }

    enum class Status : uint8_t {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR,
    };

    Status getStatus() const { return _status; }

private:
    void handleEvent(WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.println("[WS] Disconnected");
                _status = Status::DISCONNECTED;
                _pendingHello = false;
                increaseBackoff();
                break;

            case WStype_CONNECTED:
                Serial.printf("[WS] Connected to %s:%d%s\n",
                              _serverIP.toString().c_str(), _port, _path);
                _status = Status::CONNECTED;
                resetBackoff();
                _pendingHello = true;
                break;

            case WStype_TEXT:
                if (_messageCallback) {
                    JsonDocument doc;
                    if (deserializeJson(doc, payload, length) == DeserializationError::Ok) {
                        _messageCallback(doc);
                    } else {
                        Serial.println("[WS] JSON parse error");
                    }
                }
                break;

            case WStype_ERROR:
                Serial.println("[WS] Error");
                _status = Status::ERROR;
                increaseBackoff();
                // Fall through to disconnected for reconnect
                _status = Status::DISCONNECTED;
                break;

            default:
                break;
        }
    }

    void sendHello() {
        Serial.println("[WS] Sending hello (getStatus + zones.get + edge_mixer.get + cameraMode.get)");
        sendSimple("getStatus");
        sendSimple("zones.get");
        sendSimple("edge_mixer.get");
        sendSimple("cameraMode.get");
    }

    void processSendQueue() {
        if (!isConnected()) return;
        uint32_t now = millis();
        for (uint8_t i = 0; i < net::SEND_QUEUE_SIZE; i++) {
            if (!_queue[i].valid) continue;
            if (now - _queue[i].timestamp > net::SEND_QUEUE_STALE_MS) {
                _queue[i].valid = false;
                continue;
            }
            if (!canSend(_queue[i].paramIdx)) continue;
            if (!takeLock()) break;

            JsonDocument doc;
            if (_queue[i].zoneId != 0xFF) {
                doc["zoneId"] = _queue[i].zoneId;
            }
            // Determine field name from type
            const char* t = _queue[i].type;
            if (strcmp(t, "parameters.set") == 0) {
                doc["brightness"] = _queue[i].value;  // TODO: map paramIdx→field name
            } else if (strcmp(t, "zone.setSpeed") == 0) {
                doc["speed"] = _queue[i].value;
            } else if (strcmp(t, "zone.setBrightness") == 0) {
                doc["brightness"] = _queue[i].value;
            }

            size_t len = serializeJson(doc, _jsonBuf, net::JSON_BUFFER_SIZE - 1);
            if (len > 0) {
                // Wrap in message envelope
                JsonDocument msg;
                msg["type"] = t;
                JsonObject src = doc.as<JsonObject>();
                for (JsonPair kv : src) msg[kv.key()] = kv.value();
                len = serializeJson(msg, _jsonBuf, net::JSON_BUFFER_SIZE - 1);
                _jsonBuf[len] = '\0';
                _ws.sendTXT(_jsonBuf);
            }

            _queue[i].valid = false;
            releaseLock();
            break;  // One message per update() cycle
        }
    }

    void resetBackoff() { _reconnectDelay = net::WS_INITIAL_RECONNECT_MS; }
    void increaseBackoff() {
        _reconnectDelay = min(_reconnectDelay * 2, net::WS_MAX_RECONNECT_MS);
    }

    bool takeLock() {
        return xSemaphoreTake(_sendMutex, pdMS_TO_TICKS(net::SEND_MUTEX_TIMEOUT_MS)) == pdTRUE;
    }
    void releaseLock() { xSemaphoreGive(_sendMutex); }

    // --- State ---
    WebSocketsClient _ws;
    Status _status = Status::DISCONNECTED;
    MessageCallback _messageCallback = nullptr;
    bool _shouldReconnect = false;
    bool _pendingHello = false;

    IPAddress _serverIP;
    uint16_t _port = 80;
    const char* _path = "/ws";

    uint32_t _lastReconnect = 0;
    uint32_t _reconnectDelay = net::WS_INITIAL_RECONNECT_MS;
    uint32_t _connectingStart = 0;

    // Rate limiter: last send time per parameter
    uint32_t _rateLimiter[net::SEND_QUEUE_SIZE];

    // Send queue
    struct QueueEntry {
        uint8_t paramIdx;
        int32_t value;
        uint8_t zoneId;
        uint32_t timestamp;
        const char* type;
        bool valid;
    };
    QueueEntry _queue[net::SEND_QUEUE_SIZE];

    char _jsonBuf[net::JSON_BUFFER_SIZE];
    SemaphoreHandle_t _sendMutex = nullptr;
};
