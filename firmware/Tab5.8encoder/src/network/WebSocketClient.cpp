#include "WebSocketClient.h"
#include <cstring>

WebSocketClient::WebSocketClient()
    : _status(WebSocketStatus::DISCONNECTED)
    , _messageCallback(nullptr)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(INITIAL_RECONNECT_DELAY_MS)
    , _shouldReconnect(false)
    , _serverIP(INADDR_NONE)
    , _serverHost(nullptr)
    , _serverPort(80)
    , _serverPath("/ws")
    , _useIP(false)
    , _pendingHello(false)
{
    // Initialize rate limiter
    for (int i = 0; i < 8; i++) {
        _rateLimiter.lastSend[i] = 0;
    }

    // Set WebSocket event handler
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
}

void WebSocketClient::begin(const char* host, uint16_t port, const char* path) {
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        return;
    }

    _serverHost = host;
    _serverPort = port;
    _serverPath = path;
    _useIP = false;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;
    
    // Configure connection timeout
    _ws.setReconnectInterval(CONNECTION_TIMEOUT_MS);
    
    _ws.begin(host, port, path);
}

void WebSocketClient::begin(IPAddress ip, uint16_t port, const char* path) {
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        return;
    }

    _serverIP = ip;
    _serverPort = port;
    _serverPath = path;
    _useIP = true;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;
    
    // Configure connection timeout
    _ws.setReconnectInterval(CONNECTION_TIMEOUT_MS);
    
    _ws.begin(ip, port, path);
}

void WebSocketClient::update() {
    _ws.loop();

    // Send pending hello message (deferred from connect event to ensure connection is ready)
    if (_pendingHello && _status == WebSocketStatus::CONNECTED) {
        _pendingHello = false;
        sendHelloMessage();
    }

    // Handle reconnection logic
    if (_status == WebSocketStatus::DISCONNECTED && _shouldReconnect) {
        attemptReconnect();
    }
}

void WebSocketClient::disconnect() {
    _shouldReconnect = false;
    _ws.disconnect();
    _status = WebSocketStatus::DISCONNECTED;
}

void WebSocketClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            {
                char reason[64];
                reason[0] = '\0';
                if (payload && length > 0) {
                    const size_t n = (length < (sizeof(reason) - 1)) ? length : (sizeof(reason) - 1);
                    memcpy(reason, payload, n);
                    reason[n] = '\0';
                }
                Serial.printf("[WS] Disconnected (reason: \"%s\", len: %u, delay: %lu ms)\n",
                    reason,
                    (unsigned)length,
                    (unsigned long)_reconnectDelay);
            }
            _status = WebSocketStatus::DISCONNECTED;
            _pendingHello = false; // Clear pending hello on disconnect
            increaseReconnectBackoff();
            break;

        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to server\n");
            _status = WebSocketStatus::CONNECTED;
            resetReconnectBackoff();
            // Defer hello message to next update() to ensure connection is fully ready
            _pendingHello = true;
            break;

        case WStype_TEXT: {
            // Parse JSON message using StaticJsonDocument to avoid heap churn
            if (_messageCallback) {
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, payload, length);

                if (!error) {
                    _messageCallback(doc);
                }
            }
            break;
        }

        case WStype_ERROR:
            Serial.printf("[WS] Error occurred (delay: %lu ms)\n", _reconnectDelay);
            _status = WebSocketStatus::ERROR;
            increaseReconnectBackoff();
            break;

        case WStype_BIN:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_PING:
        case WStype_PONG:
            // Ignore these for now
            break;
    }
}

void WebSocketClient::attemptReconnect() {
    unsigned long now = millis();

    if (now - _lastReconnectAttempt >= _reconnectDelay) {
        _lastReconnectAttempt = now;
        _status = WebSocketStatus::CONNECTING;

        if (_useIP) {
            _ws.begin(_serverIP, _serverPort, _serverPath);
        } else {
            _ws.begin(_serverHost, _serverPort, _serverPath);
        }
    }
}

void WebSocketClient::resetReconnectBackoff() {
    _reconnectDelay = INITIAL_RECONNECT_DELAY_MS;
}

void WebSocketClient::increaseReconnectBackoff() {
    _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);
}

bool WebSocketClient::canSend(ParamIndex param) {
    unsigned long now = millis();
    if (now - _rateLimiter.lastSend[param] >= RateLimiter::THROTTLE_MS) {
        _rateLimiter.lastSend[param] = now;
        return true;
    }
    return false;
}

void WebSocketClient::sendJSON(const char* type, StaticJsonDocument<128>& doc) {
    if (!isConnected()) {
        return;
    }

    // Create message with type field (LightwaveOS protocol: {"type": "...", ...})
    StaticJsonDocument<256> message;
    JsonObject msgObj = message.to<JsonObject>();
    msgObj["type"] = type;
    
    // Merge payload fields directly into message (not nested under "p")
    JsonObject docObj = doc.as<JsonObject>();
    for (JsonPair kv : docObj) {
        msgObj[kv.key().c_str()] = kv.value();
    }

    // Serialize to fixed buffer (drop if too large to prevent fragmentation)
    size_t len = serializeJson(message, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        // Message too large or serialization failed - drop it
        return;
    }
    _jsonBuffer[len] = '\0';  // Ensure null termination

    // Send via WebSocket
    _ws.sendTXT(_jsonBuffer);
}

void WebSocketClient::sendEffectChange(uint8_t effectId) {
    if (!canSend(ParamIndex::EFFECT)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["effectId"] = effectId;
    sendJSON("effects.setCurrent", doc);
}

void WebSocketClient::sendBrightnessChange(uint8_t brightness) {
    if (!canSend(ParamIndex::BRIGHTNESS)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["brightness"] = brightness;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendPaletteChange(uint8_t paletteId) {
    if (!canSend(ParamIndex::PALETTE)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["paletteId"] = paletteId;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSpeedChange(uint8_t speed) {
    if (!canSend(ParamIndex::SPEED)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["speed"] = speed;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendIntensityChange(uint8_t intensity) {
    if (!canSend(ParamIndex::INTENSITY)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["intensity"] = intensity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSaturationChange(uint8_t saturation) {
    if (!canSend(ParamIndex::SATURATION)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["saturation"] = saturation;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendComplexityChange(uint8_t complexity) {
    if (!canSend(ParamIndex::COMPLEXITY)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["complexity"] = complexity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendVariationChange(uint8_t variation) {
    if (!canSend(ParamIndex::VARIATION)) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["variation"] = variation;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendHelloMessage() {
    // On connect, request current status from LightwaveOS
    // This triggers a "status" broadcast that syncs our local state
    StaticJsonDocument<128> doc;
    // Empty payload - getStatus doesn't need parameters
    sendJSON("getStatus", doc);
}

