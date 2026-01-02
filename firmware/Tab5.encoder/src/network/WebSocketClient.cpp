// ============================================================================
// WebSocketClient Implementation - Tab5.encoder
// ============================================================================
// Bidirectional WebSocket client for LightwaveOS communication.
// Ported from K1.8encoderS3 with Tab5-specific extensions (zone support).
//
// NOTE: WiFi is currently DISABLED on Tab5 (ESP32-P4) due to SDIO pin
// configuration issues with the ESP32-C6 WiFi co-processor.
// See Config.h ENABLE_WIFI flag for details.
// ============================================================================

#include "WebSocketClient.h"

#if ENABLE_WIFI

#include <cstring>

WebSocketClient::WebSocketClient()
    : _status(WebSocketStatus::DISCONNECTED)
    , _messageCallback(nullptr)
    , _lastReconnectAttempt(0)
    , _reconnectDelay(NetworkConfig::WS_INITIAL_RECONNECT_MS)
    , _shouldReconnect(false)
    , _serverIP(INADDR_NONE)
    , _serverHost(nullptr)
    , _serverPort(80)
    , _serverPath("/ws")
    , _useIP(false)
    , _pendingHello(false)
{
    // Initialize rate limiter (all parameters start at 0)
    for (int i = 0; i < 16; i++) {
        _rateLimiter.lastSend[i] = 0;
    }

    // Set WebSocket event handler using lambda
    _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        handleEvent(type, payload, length);
    });
}

void WebSocketClient::begin(const char* host, uint16_t port, const char* path) {
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        Serial.println("[WS] Already connected/connecting, ignoring begin()");
        return;
    }

    _serverHost = host;
    _serverPort = port;
    _serverPath = path;
    _useIP = false;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;

    Serial.printf("[WS] Connecting to ws://%s:%d%s...\n", host, port, path);

    // Configure connection timeout
    // links2004/WebSockets uses setReconnectInterval for both reconnect and initial timeout
    _ws.setReconnectInterval(NetworkConfig::WS_CONNECTION_TIMEOUT_MS);

    _ws.begin(host, port, path);
}

void WebSocketClient::begin(IPAddress ip, uint16_t port, const char* path) {
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        Serial.println("[WS] Already connected/connecting, ignoring begin()");
        return;
    }

    _serverIP = ip;
    _serverPort = port;
    _serverPath = path;
    _useIP = true;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;

    Serial.printf("[WS] Connecting to ws://%s:%d%s...\n",
                  ip.toString().c_str(), port, path);

    // Configure connection timeout
    _ws.setReconnectInterval(NetworkConfig::WS_CONNECTION_TIMEOUT_MS);

    _ws.begin(ip, port, path);
}

void WebSocketClient::update() {
    // Process WebSocket events
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
    Serial.println("[WS] Disconnecting...");
    _shouldReconnect = false;
    _ws.disconnect();
    _status = WebSocketStatus::DISCONNECTED;
    _pendingHello = false;
}

void WebSocketClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            {
                // Extract disconnect reason (if available)
                char reason[64];
                reason[0] = '\0';
                if (payload && length > 0) {
                    const size_t n = (length < (sizeof(reason) - 1)) ? length : (sizeof(reason) - 1);
                    memcpy(reason, payload, n);
                    reason[n] = '\0';
                }
                Serial.printf("[WS] Disconnected (reason: \"%s\", delay: %lu ms)\n",
                              reason, _reconnectDelay);
            }
            _status = WebSocketStatus::DISCONNECTED;
            _pendingHello = false;  // Clear pending hello on disconnect
            increaseReconnectBackoff();
            break;

        case WStype_CONNECTED:
            Serial.println("[WS] Connected to server");
            _status = WebSocketStatus::CONNECTED;
            resetReconnectBackoff();
            // Defer hello message to next update() to ensure connection is fully ready
            _pendingHello = true;
            break;

        case WStype_TEXT:
            {
                // Parse JSON message
                if (_messageCallback) {
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, payload, length);

                    if (!error) {
                        _messageCallback(doc);
                    } else {
                        Serial.printf("[WS] JSON parse error: %s\n", error.c_str());
                    }
                }
            }
            break;

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

        Serial.printf("[WS] Reconnecting (delay was: %lu ms)...\n", _reconnectDelay);

        if (_useIP) {
            _ws.begin(_serverIP, _serverPort, _serverPath);
        } else {
            _ws.begin(_serverHost, _serverPort, _serverPath);
        }
    }
}

void WebSocketClient::resetReconnectBackoff() {
    _reconnectDelay = NetworkConfig::WS_INITIAL_RECONNECT_MS;
}

void WebSocketClient::increaseReconnectBackoff() {
    _reconnectDelay = min(_reconnectDelay * 2, (unsigned long)NetworkConfig::WS_MAX_RECONNECT_MS);
}

bool WebSocketClient::canSend(uint8_t paramIndex) {
    if (paramIndex >= 16) return false;

    unsigned long now = millis();
    if (now - _rateLimiter.lastSend[paramIndex] >= NetworkConfig::PARAM_THROTTLE_MS) {
        _rateLimiter.lastSend[paramIndex] = now;
        return true;
    }
    return false;
}

void WebSocketClient::sendJSON(const char* type, JsonDocument& doc) {
    if (!isConnected()) {
        return;
    }

    // Create message with type field (LightwaveOS protocol: {"type": "...", ...})
    JsonDocument message;
    JsonObject msgObj = message.to<JsonObject>();
    msgObj["type"] = type;

    // Merge payload fields directly into message (not nested)
    JsonObject docObj = doc.as<JsonObject>();
    for (JsonPair kv : docObj) {
        msgObj[kv.key().c_str()] = kv.value();
    }

    // Serialize to fixed buffer (drop if too large to prevent fragmentation)
    size_t len = serializeJson(message, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        Serial.println("[WS] Message too large, dropping");
        return;
    }
    _jsonBuffer[len] = '\0';  // Ensure null termination

    // Send via WebSocket
    _ws.sendTXT(_jsonBuffer);
}

void WebSocketClient::sendHelloMessage() {
    // On connect, request current status from LightwaveOS
    // This triggers a "status" broadcast that syncs our local state
    Serial.println("[WS] Sending hello (getStatus)");
    JsonDocument doc;
    // Empty payload - getStatus doesn't need parameters
    sendJSON("getStatus", doc);
}

// ============================================================================
// Global Parameter Commands (Unit A, encoders 0-7)
// ============================================================================

void WebSocketClient::sendEffectChange(uint8_t effectId) {
    if (!canSend(ParamIndex::EFFECT)) {
        return;
    }

    JsonDocument doc;
    doc["effectId"] = effectId;
    sendJSON("effects.setCurrent", doc);
}

void WebSocketClient::sendBrightnessChange(uint8_t brightness) {
    if (!canSend(ParamIndex::BRIGHTNESS)) {
        return;
    }

    JsonDocument doc;
    doc["brightness"] = brightness;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendPaletteChange(uint8_t paletteId) {
    if (!canSend(ParamIndex::PALETTE)) {
        return;
    }

    JsonDocument doc;
    doc["paletteId"] = paletteId;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSpeedChange(uint8_t speed) {
    if (!canSend(ParamIndex::SPEED)) {
        return;
    }

    JsonDocument doc;
    doc["speed"] = speed;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendIntensityChange(uint8_t intensity) {
    if (!canSend(ParamIndex::INTENSITY)) {
        return;
    }

    JsonDocument doc;
    doc["intensity"] = intensity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSaturationChange(uint8_t saturation) {
    if (!canSend(ParamIndex::SATURATION)) {
        return;
    }

    JsonDocument doc;
    doc["saturation"] = saturation;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendComplexityChange(uint8_t complexity) {
    if (!canSend(ParamIndex::COMPLEXITY)) {
        return;
    }

    JsonDocument doc;
    doc["complexity"] = complexity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendVariationChange(uint8_t variation) {
    if (!canSend(ParamIndex::VARIATION)) {
        return;
    }

    JsonDocument doc;
    doc["variation"] = variation;
    sendJSON("parameters.set", doc);
}

// ============================================================================
// Zone Commands (Tab5 extension for Unit B, encoders 8-15)
// ============================================================================

void WebSocketClient::sendZoneEffect(uint8_t zoneId, uint8_t effectId) {
    // Map zoneId to rate limiter index
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["effectId"] = effectId;
    sendJSON("zone.setEffect", doc);
}

void WebSocketClient::sendZoneBrightness(uint8_t zoneId, uint8_t value) {
    // Map zoneId to rate limiter index
    uint8_t paramIndex = ParamIndex::ZONE0_BRIGHTNESS + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["value"] = value;
    sendJSON("zone.setBrightness", doc);
}

void WebSocketClient::sendZoneSpeed(uint8_t zoneId, uint8_t value) {
    // Zone speed shares rate limit with zone brightness (same encoder)
    uint8_t paramIndex = ParamIndex::ZONE0_BRIGHTNESS + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["value"] = value;
    sendJSON("zone.setSpeed", doc);
}

void WebSocketClient::sendZonePalette(uint8_t zoneId, uint8_t paletteId) {
    // Zone palette shares rate limit with zone effect (same encoder)
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["paletteId"] = paletteId;
    sendJSON("zone.setPalette", doc);
}

// ============================================================================
// Generic Commands
// ============================================================================

void WebSocketClient::sendGenericParameter(const char* fieldName, uint8_t value) {
    // Generic parameters use a shared rate limiter slot (slot 7 = VARIATION)
    // This prevents spam from unmapped Unit B encoders
    if (!canSend(ParamIndex::VARIATION)) {
        return;
    }

    JsonDocument doc;
    doc[fieldName] = value;
    sendJSON("parameters.set", doc);
}

const char* WebSocketClient::getStatusString() const {
    switch (_status) {
        case WebSocketStatus::DISCONNECTED: return "Disconnected";
        case WebSocketStatus::CONNECTING:   return "Connecting";
        case WebSocketStatus::CONNECTED:    return "Connected";
        case WebSocketStatus::ERROR:        return "Error";
        default:                            return "Unknown";
    }
}

#endif // ENABLE_WIFI
