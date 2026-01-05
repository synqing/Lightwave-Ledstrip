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

#include <ESP.h>  // For heap monitoring
#include <cstring>
#include "../zones/ZoneDefinition.h"

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
    // #region agent log
#if ENABLE_WS_DIAGNOSTICS
    static uint32_t s_lastLoopLog = 0;
    if ((uint32_t)(millis() - s_lastLoopLog) >= 500) {  // Log every 500ms
        Serial.printf("[DEBUG] WebSocketClient::update before loop - Heap: free=%u minFree=%u largest=%u status=%d\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), (int)_status);
        s_lastLoopLog = millis();
    }
#endif
    // #endregion
    _ws.loop();
    // #region agent log
#if ENABLE_WS_DIAGNOSTICS
    if ((uint32_t)(millis() - s_lastLoopLog) >= 500) {
        Serial.printf("[DEBUG] WebSocketClient::update after loop - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    }
#endif
    // #endregion

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
        // #region agent log
        {
            // HWS1: Prove commands are being dropped due to disconnected WS state.
            char buf[240];
            const int n = snprintf(
                buf, sizeof(buf),
                "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HWS1\",\"location\":\"Tab5.encoder/src/network/WebSocketClient.cpp:sendJSON\",\"message\":\"ws.drop.not_connected\",\"data\":{\"type\":\"%s\",\"status\":%u,\"reconnectDelayMs\":%lu},\"timestamp\":%lu}",
                type ? type : "",
                static_cast<unsigned>(_status),
                static_cast<unsigned long>(_reconnectDelay),
                static_cast<unsigned long>(millis())
            );
            if (n > 0) Serial.println(buf);
        }
        // #endregion
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
    // #region agent log
    Serial.printf("[DEBUG] sendJSON before serialize - Heap: free=%u minFree=%u largest=%u type=%s\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), type ? type : "null");
    // #endregion
    size_t len = serializeJson(message, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    // #region agent log
    Serial.printf("[DEBUG] sendJSON after serialize - len=%u bufferSize=%u Heap: free=%u minFree=%u\n",
                  len, JSON_BUFFER_SIZE, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        Serial.println("[WS] Message too large, dropping");
        return;
    }
    _jsonBuffer[len] = '\0';  // Ensure null termination

    // Send via WebSocket
    // #region agent log
    static bool s_sendInProgress = false;
    static uint32_t s_lastSendTime = 0;
    uint32_t now = millis();
    uint32_t timeSinceLastSend = (s_lastSendTime > 0) ? (now - s_lastSendTime) : 0;
    Serial.printf("[DEBUG] sendJSON before sendTXT - Heap: free=%u minFree=%u largest=%u len=%u sendInProgress=%d timeSinceLastSend=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), len, s_sendInProgress, timeSinceLastSend);
    if (s_sendInProgress) {
        Serial.printf("[WARNING] Concurrent send detected! Previous send may not have completed.\n");
    }
    s_sendInProgress = true;
    s_lastSendTime = now;
    // #endregion
    _ws.sendTXT(_jsonBuffer);
    // #region agent log
    s_sendInProgress = false;
    Serial.printf("[DEBUG] sendJSON after sendTXT - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // #endregion
}

void WebSocketClient::sendHelloMessage() {
    // On connect, request current status from LightwaveOS
    // This triggers a "status" broadcast that syncs our local state
    Serial.println("[WS] Sending hello (getStatus)");
    JsonDocument doc;
    // Empty payload - getStatus doesn't need parameters
    sendJSON("getStatus", doc);

    // Also request zone state
    requestZonesState();

    // Request color correction state (for presets)
    requestColorCorrectionConfig();
}

void WebSocketClient::requestEffectsList(uint8_t page, uint8_t limit, const char* requestId) {
    if (!isConnected()) return;
    JsonDocument doc;
    doc["page"] = page;
    doc["limit"] = limit;
    doc["details"] = false;
    if (requestId && *requestId) doc["requestId"] = requestId;
    sendJSON("effects.list", doc);
}

void WebSocketClient::requestPalettesList(uint8_t page, uint8_t limit, const char* requestId) {
    if (!isConnected()) return;
    JsonDocument doc;
    doc["page"] = page;
    doc["limit"] = limit;
    if (requestId && *requestId) doc["requestId"] = requestId;
    sendJSON("palettes.list", doc);
}

void WebSocketClient::requestZonesState() {
    if (!isConnected()) return;
    Serial.println("[WS] Requesting zone state (zones.get)");
    JsonDocument doc;
    // Empty payload - zones.get doesn't need parameters
    sendJSON("zones.get", doc);
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

void WebSocketClient::sendMoodChange(uint8_t mood) {
    if (!canSend(ParamIndex::MOOD)) {
        return;
    }

    JsonDocument doc;
    doc["mood"] = mood;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendFadeAmountChange(uint8_t fadeAmount) {
    if (!canSend(ParamIndex::FADEAMOUNT)) {
        return;
    }

    JsonDocument doc;
    doc["fadeAmount"] = fadeAmount;
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

void WebSocketClient::sendZoneEnable(bool enable) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["enable"] = enable;
    sendJSON("zone.enable", doc);
}

void WebSocketClient::sendZoneEffect(uint8_t zoneId, uint8_t effectId) {
    // Map zoneId to rate limiter index
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        // #region agent log
        {
            // HWS2: Prove zone effect sends are being blocked (rate limit / invalid zone).
            char buf[220];
            const int n = snprintf(
                buf, sizeof(buf),
                "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HWS2\",\"location\":\"Tab5.encoder/src/network/WebSocketClient.cpp:sendZoneEffect\",\"message\":\"ws.zoneEffect.blocked\",\"data\":{\"zoneId\":%u,\"effectId\":%u,\"paramIndex\":%u,\"connected\":%s},\"timestamp\":%lu}",
                static_cast<unsigned>(zoneId),
                static_cast<unsigned>(effectId),
                static_cast<unsigned>(paramIndex),
                isConnected() ? "true" : "false",
                static_cast<unsigned long>(millis())
            );
            if (n > 0) Serial.println(buf);
        }
        // #endregion
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["effectId"] = effectId;
    sendJSON("zone.setEffect", doc);
}

void WebSocketClient::sendZoneBrightness(uint8_t zoneId, uint8_t value) {
    // Zone brightness uses rate limiter slot for zone effect (same encoder pair)
    // Note: Brightness is no longer a parameter in new layout, but API still supports it
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["value"] = value;
    sendJSON("zone.setBrightness", doc);
}

void WebSocketClient::sendZoneSpeed(uint8_t zoneId, uint8_t value) {
    // Zone speed uses rate limiter slots (indices 9, 11, 13, 15)
    uint8_t paramIndex = ParamIndex::ZONE0_SPEED + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["speed"] = value;
    sendJSON("zone.setSpeed", doc);
}

void WebSocketClient::sendZonePalette(uint8_t zoneId, uint8_t paletteId) {
    // Zone palette shares rate limit with zone effect (same encoder)
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || !canSend(paramIndex)) {
        // #region agent log
        {
            // HWS3: Prove zone palette sends are being blocked (rate limit / invalid zone).
            char buf[220];
            const int n = snprintf(
                buf, sizeof(buf),
                "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HWS3\",\"location\":\"Tab5.encoder/src/network/WebSocketClient.cpp:sendZonePalette\",\"message\":\"ws.zonePalette.blocked\",\"data\":{\"zoneId\":%u,\"paletteId\":%u,\"paramIndex\":%u,\"connected\":%s},\"timestamp\":%lu}",
                static_cast<unsigned>(zoneId),
                static_cast<unsigned>(paletteId),
                static_cast<unsigned>(paramIndex),
                isConnected() ? "true" : "false",
                static_cast<unsigned long>(millis())
            );
            if (n > 0) Serial.println(buf);
        }
        // #endregion
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["paletteId"] = paletteId;
    sendJSON("zone.setPalette", doc);
}

void WebSocketClient::sendZoneBlend(uint8_t zoneId, uint8_t blendMode) {
    // Zone blend uses rate limiter slot for zone effect (same encoder pair)
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3 || blendMode > 7 || !canSend(paramIndex)) {
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["blendMode"] = blendMode;
    sendJSON("zone.setBlend", doc);
}

void WebSocketClient::sendZonesSetLayout(const struct zones::ZoneSegment* segments, uint8_t zoneCount) {
    if (!isConnected() || !segments || zoneCount == 0 || zoneCount > zones::MAX_ZONES) {
        // #region agent log
        {
            // HWS4: Prove layout pushes are being skipped due to connection/state gating.
            char buf[240];
            const int n = snprintf(
                buf, sizeof(buf),
                "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HWS4\",\"location\":\"Tab5.encoder/src/network/WebSocketClient.cpp:sendZonesSetLayout\",\"message\":\"ws.zonesSetLayout.skipped\",\"data\":{\"connected\":%s,\"segmentsNull\":%s,\"zoneCount\":%u},\"timestamp\":%lu}",
                isConnected() ? "true" : "false",
                segments ? "false" : "true",
                static_cast<unsigned>(zoneCount),
                static_cast<unsigned long>(millis())
            );
            if (n > 0) Serial.println(buf);
        }
        // #endregion
        return;
    }

    // Serialize segments array to JSON
    JsonDocument doc;
    JsonArray zonesArray = doc["zones"].to<JsonArray>();

    for (uint8_t i = 0; i < zoneCount; i++) {
        JsonObject zoneObj = zonesArray.add<JsonObject>();
        zoneObj["zoneId"] = segments[i].zoneId;
        zoneObj["s1LeftStart"] = segments[i].s1LeftStart;
        zoneObj["s1LeftEnd"] = segments[i].s1LeftEnd;
        zoneObj["s1RightStart"] = segments[i].s1RightStart;
        zoneObj["s1RightEnd"] = segments[i].s1RightEnd;
        // Note: totalLeds is calculated by server, but we can include it for validation
        zoneObj["totalLeds"] = segments[i].totalLeds;
    }

    sendJSON("zones.setLayout", doc);
}

// ============================================================================
// Color Correction Commands
// ============================================================================

void WebSocketClient::requestColorCorrectionConfig() {
    if (!isConnected()) {
        return;
    }

    // #region agent log
    Serial.printf("[DEBUG] requestColorCorrectionConfig entry - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // #endregion
    JsonDocument doc;
    doc["requestId"] = "cc_sync";
    // #region agent log
    Serial.printf("[DEBUG] requestColorCorrectionConfig before sendJSON - Heap: free=%u minFree=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
    // #endregion
    sendJSON("colorCorrection.getConfig", doc);
    // #region agent log
    Serial.printf("[DEBUG] requestColorCorrectionConfig after sendJSON - Heap: free=%u minFree=%u largest=%u\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // #endregion
    Serial.println("[WS] Requested colorCorrection.getConfig");
}

void WebSocketClient::sendColorCorrectionConfig(bool gammaEnabled, float gammaValue,
                                                bool aeEnabled, uint8_t aeTarget,
                                                bool brownEnabled) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["gammaEnabled"] = gammaEnabled;
    doc["gammaValue"] = gammaValue;
    doc["autoExposureEnabled"] = aeEnabled;
    doc["autoExposureTarget"] = aeTarget;
    doc["brownGuardrailEnabled"] = brownEnabled;
    sendJSON("colorCorrection.setConfig", doc);

    Serial.printf("[WS] Sent colorCorrection.setConfig: gamma=%s (%.1f), ae=%s, brown=%s\n",
                  gammaEnabled ? "ON" : "OFF", gammaValue,
                  aeEnabled ? "ON" : "OFF",
                  brownEnabled ? "ON" : "OFF");
}

void WebSocketClient::sendGammaChange(bool enabled, float value) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["gammaEnabled"] = enabled;
    doc["gammaValue"] = value;
    sendJSON("colorCorrection.setConfig", doc);

    // Update local cache
    _colorCorrectionState.gammaEnabled = enabled;
    _colorCorrectionState.gammaValue = value;
}

void WebSocketClient::sendAutoExposureChange(bool enabled, uint8_t target) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["autoExposureEnabled"] = enabled;
    doc["autoExposureTarget"] = target;
    sendJSON("colorCorrection.setConfig", doc);

    // Update local cache
    _colorCorrectionState.autoExposureEnabled = enabled;
    _colorCorrectionState.autoExposureTarget = target;
}

void WebSocketClient::sendBrownGuardrailChange(bool enabled) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["brownGuardrailEnabled"] = enabled;
    sendJSON("colorCorrection.setConfig", doc);

    // Update local cache
    _colorCorrectionState.brownGuardrailEnabled = enabled;
}

void WebSocketClient::sendColourCorrectionMode(uint8_t mode) {
    if (!isConnected()) {
        return;
    }

    JsonDocument doc;
    doc["mode"] = mode;
    sendJSON("colorCorrection.setConfig", doc);

    _colorCorrectionState.mode = mode;
    _colorCorrectionState.valid = true;
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
