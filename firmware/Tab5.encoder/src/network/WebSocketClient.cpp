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

// ============================================================================
// Agent tracing (compile-time, default off)
// ============================================================================
#ifndef TAB5_AGENT_TRACE
#define TAB5_AGENT_TRACE 0
#endif

#if TAB5_AGENT_TRACE
#define TAB5_AGENT_PRINTF(...) Serial.printf(__VA_ARGS__)
#define TAB5_AGENT_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define TAB5_AGENT_PRINTF(...) ((void)0)
#define TAB5_AGENT_PRINTLN(...) ((void)0)
#endif

static void formatIPv4(const IPAddress& ip, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

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
    _requiresAuth = (LIGHTWAVE_API_KEY[0] != '\0');
    _authenticated = !_requiresAuth;

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
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(host)\",\"message\":\"ws.begin.entry\",\"data\":{\"host\":\"%s\",\"port\":%d,\"path\":\"%s\",\"currentStatus\":%d},\"timestamp\":%lu}\n",
        host ? host : "null", port, path ? path : "null", (int)_status, (unsigned long)millis()
    );
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        Serial.println("[WS] Already connected/connecting, ignoring begin()");
        TAB5_AGENT_PRINTF(
            "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(host)\",\"message\":\"ws.begin.alreadyConnecting\",\"data\":{\"status\":%d},\"timestamp\":%lu}\n",
            (int)_status, (unsigned long)millis()
        );
        return;
    }

    _serverHost = host;
    _serverPort = port;
    _serverPath = path;
    _useIP = false;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;
    _authenticated = !_requiresAuth;

    Serial.printf("[WS] Connecting to ws://%s:%d%s...\n", host, port, path);
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(host)\",\"message\":\"ws.begin.calling\",\"data\":{\"host\":\"%s\",\"port\":%d,\"path\":\"%s\",\"timeout\":%d},\"timestamp\":%lu}\n",
        host ? host : "null", port, path ? path : "null", NetworkConfig::WS_CONNECTION_TIMEOUT_MS, (unsigned long)millis()
    );

    // Configure connection timeout
    // links2004/WebSockets uses setReconnectInterval for both reconnect and initial timeout
    _ws.setReconnectInterval(NetworkConfig::WS_CONNECTION_TIMEOUT_MS);

    _ws.begin(host, port, path);
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(host)\",\"message\":\"ws.begin.complete\",\"data\":{\"host\":\"%s\",\"port\":%d},\"timestamp\":%lu}\n",
        host ? host : "null", port, (unsigned long)millis()
    );
}

void WebSocketClient::begin(IPAddress ip, uint16_t port, const char* path) {
    char ipStr[16];
    formatIPv4(ip, ipStr);
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(ip)\",\"message\":\"ws.begin.ip.entry\",\"data\":{\"ip\":\"%s\",\"port\":%d,\"path\":\"%s\",\"currentStatus\":%d},\"timestamp\":%lu}\n",
        ipStr, port, path ? path : "null", (int)_status, (unsigned long)millis()
    );
    // Prevent overlapping connect attempts
    if (_status == WebSocketStatus::CONNECTING || _status == WebSocketStatus::CONNECTED) {
        Serial.println("[WS] Already connected/connecting, ignoring begin()");
        // #region agent log
        Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:72\",\"message\":\"ws.begin.ip.alreadyConnecting\",\"data\":{\"status\":%d},\"timestamp\":%lu}\n", (int)_status, (unsigned long)millis());
        // #endregion
        return;
    }

    _serverIP = ip;
    _serverPort = port;
    _serverPath = path;
    _useIP = true;
    _shouldReconnect = true;

    _status = WebSocketStatus::CONNECTING;
    _authenticated = !_requiresAuth;

    Serial.printf("[WS] Connecting to ws://%s:%d%s...\n", ipStr, port, path);
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(ip)\",\"message\":\"ws.begin.ip.calling\",\"data\":{\"ip\":\"%s\",\"port\":%d,\"path\":\"%s\",\"timeout\":%d},\"timestamp\":%lu}\n",
        ipStr, port, path ? path : "null", NetworkConfig::WS_CONNECTION_TIMEOUT_MS, (unsigned long)millis()
    );

    // Configure connection timeout
    _ws.setReconnectInterval(NetworkConfig::WS_CONNECTION_TIMEOUT_MS);

    _ws.begin(ip, port, path);
    TAB5_AGENT_PRINTF(
        "[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WS1\",\"location\":\"WebSocketClient.cpp:begin(ip)\",\"message\":\"ws.begin.ip.complete\",\"data\":{\"ip\":\"%s\",\"port\":%d},\"timestamp\":%lu}\n",
        ipStr, port, (unsigned long)millis()
    );
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
    _authenticated = !_requiresAuth;
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
            _authenticated = !_requiresAuth;
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
                    _rxDoc.clear();
                    DeserializationError error = deserializeJson(_rxDoc, payload, length);

                    if (!error) {
                        const char* msgType = _rxDoc["type"] | "";
                        if (_requiresAuth && !_authenticated && strcmp(msgType, "auth") == 0) {
                            const bool ok = _rxDoc["success"].is<bool>() && _rxDoc["success"].as<bool>();
                            if (ok) {
                                _authenticated = true;
                                Serial.println("[WS] Authenticated");
                                _pendingHello = true;  // Trigger hello on next update()
                            } else {
                                Serial.println("[WS] Auth failed (check LIGHTWAVE_API_KEY)");
                            }
                        }
                        _messageCallback(_rxDoc);
                    } else {
                        Serial.printf("[WS] JSON parse error: %s\n", error.c_str());
                    }
                }
            }
            break;

        case WStype_ERROR:
            Serial.printf("[WS] Error occurred (delay: %lu ms)\n", _reconnectDelay);
            _status = WebSocketStatus::ERROR;
            _authenticated = !_requiresAuth;
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
            if (n > 0) TAB5_AGENT_PRINTLN(buf);
        }
        // #endregion
        return;
    }

    // If API auth is enabled on the server, suppress commands until authenticated.
    if (_requiresAuth && !_authenticated && type && strcmp(type, "auth") != 0) {
        TAB5_AGENT_PRINTF("[WS] Dropping command before auth: %s\n", type);
        return;
    }

    // Create message with type field (LightwaveOS protocol: {"type": "...", ...})
    _txDoc.clear();
    JsonObject msgObj = _txDoc.to<JsonObject>();
    msgObj["type"] = type;

    // Merge payload fields directly into message (not nested)
    JsonObject docObj = doc.as<JsonObject>();
    for (JsonPair kv : docObj) {
        msgObj[kv.key().c_str()] = kv.value();
    }

    if (_txDoc.overflowed()) {
        Serial.printf("[WS ERROR] JSON doc overflow, dropping type=%s\n", type ? type : "null");
        return;
    }

    // Serialize to fixed buffer (drop if too large to prevent fragmentation)
    TAB5_AGENT_PRINTF("[DEBUG] sendJSON before serialize - Heap: free=%u minFree=%u largest=%u type=%s\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), type ? type : "null");
    size_t len = serializeJson(_txDoc, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    TAB5_AGENT_PRINTF("[DEBUG] sendJSON after serialize - len=%u bufferSize=%u Heap: free=%u minFree=%u\n",
                      (unsigned)len, (unsigned)JSON_BUFFER_SIZE, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        Serial.printf("[WS ERROR] Message too large (len=%u, max=%u), dropping type=%s\n", 
                      len, JSON_BUFFER_SIZE - 1, type ? type : "null");
        // CRITICAL FIX: Log warning when message is dropped due to size
        // This helps identify when buffer size needs to be increased further
        return;
    }
    _jsonBuffer[len] = '\0';  // Ensure null termination

    // Send via WebSocket
    TAB5_AGENT_PRINTF("[DEBUG] sendJSON before sendTXT - Heap: free=%u minFree=%u largest=%u len=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), (unsigned)len);
    _ws.sendTXT(_jsonBuffer, len);
    TAB5_AGENT_PRINTF("[DEBUG] sendJSON after sendTXT - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

void WebSocketClient::sendHelloMessage() {
    // On connect, request current status from LightwaveOS
    // This triggers a "status" broadcast that syncs our local state
    if (_requiresAuth && !_authenticated) {
        Serial.println("[WS] Sending auth");
        StaticJsonDocument<128> authDoc;
        authDoc["apiKey"] = LIGHTWAVE_API_KEY;
        sendJSON("auth", authDoc);
        return;
    }

    Serial.println("[WS] Sending hello (getStatus)");
    StaticJsonDocument<32> doc;
    sendJSON("getStatus", doc);

    // Also request zone state
    requestZonesState();

    // Request color correction state (for presets)
    requestColorCorrectionConfig();
}

void WebSocketClient::requestEffectsList(uint8_t page, uint8_t limit, const char* requestId) {
    if (!isConnected()) return;
    StaticJsonDocument<128> doc;
    doc["page"] = page;
    doc["limit"] = limit;
    doc["details"] = false;
    if (requestId && *requestId) doc["requestId"] = requestId;
    sendJSON("effects.list", doc);
}

void WebSocketClient::requestPalettesList(uint8_t page, uint8_t limit, const char* requestId) {
    if (!isConnected()) return;
    StaticJsonDocument<128> doc;
    doc["page"] = page;
    doc["limit"] = limit;
    if (requestId && *requestId) doc["requestId"] = requestId;
    sendJSON("palettes.list", doc);
}

void WebSocketClient::requestZonesState() {
    if (!isConnected()) return;
    Serial.println("[WS] Requesting zone state (zones.get)");
    StaticJsonDocument<32> doc;
    sendJSON("zones.get", doc);
}

// ============================================================================
// Global Parameter Commands (Unit A, encoders 0-7)
// ============================================================================

void WebSocketClient::sendEffectChange(uint8_t effectId) {
    if (!canSend(ParamIndex::EFFECT)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["effectId"] = effectId;
    sendJSON("effects.setCurrent", doc);
}

void WebSocketClient::sendBrightnessChange(uint8_t brightness) {
    if (!canSend(ParamIndex::BRIGHTNESS)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["brightness"] = brightness;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendPaletteChange(uint8_t paletteId) {
    if (!canSend(ParamIndex::PALETTE)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["paletteId"] = paletteId;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSpeedChange(uint8_t speed) {
    if (!canSend(ParamIndex::SPEED)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["speed"] = speed;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendMoodChange(uint8_t mood) {
    if (!canSend(ParamIndex::MOOD)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["mood"] = mood;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendFadeAmountChange(uint8_t fadeAmount) {
    if (!canSend(ParamIndex::FADEAMOUNT)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["fadeAmount"] = fadeAmount;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendComplexityChange(uint8_t complexity) {
    if (!canSend(ParamIndex::COMPLEXITY)) {
        return;
    }

    StaticJsonDocument<64> doc;
    doc["complexity"] = complexity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendVariationChange(uint8_t variation) {
    if (!canSend(ParamIndex::VARIATION)) {
        return;
    }

    StaticJsonDocument<64> doc;
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

    StaticJsonDocument<64> doc;
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
            if (n > 0) TAB5_AGENT_PRINTLN(buf);
        }
        // #endregion
        return;
    }

    StaticJsonDocument<96> doc;
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

    StaticJsonDocument<96> doc;
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

    StaticJsonDocument<96> doc;
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
            if (n > 0) TAB5_AGENT_PRINTLN(buf);
        }
        // #endregion
        return;
    }

    StaticJsonDocument<96> doc;
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

    StaticJsonDocument<96> doc;
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
            if (n > 0) TAB5_AGENT_PRINTLN(buf);
        }
        // #endregion
        return;
    }

    if (_requiresAuth && !_authenticated) {
        TAB5_AGENT_PRINTLN("[WS] Dropping zones.setLayout before auth");
        return;
    }

    // Build full message directly into TX doc to avoid large temporary allocations.
    _txDoc.clear();
    _txDoc["type"] = "zones.setLayout";
    JsonArray zonesArray = _txDoc["zones"].to<JsonArray>();

    for (uint8_t i = 0; i < zoneCount; i++) {
        JsonObject zoneObj = zonesArray.add<JsonObject>();
        zoneObj["zoneId"] = segments[i].zoneId;
        zoneObj["s1LeftStart"] = segments[i].s1LeftStart;
        zoneObj["s1LeftEnd"] = segments[i].s1LeftEnd;
        zoneObj["s1RightStart"] = segments[i].s1RightStart;
        zoneObj["s1RightEnd"] = segments[i].s1RightEnd;
        zoneObj["totalLeds"] = segments[i].totalLeds;
    }

    if (_txDoc.overflowed()) {
        Serial.println("[WS ERROR] zones.setLayout JSON overflow, dropping");
        return;
    }

    size_t len = serializeJson(_txDoc, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        Serial.printf("[WS ERROR] zones.setLayout too large (len=%u, max=%u), dropping\n",
                      (unsigned)len, (unsigned)(JSON_BUFFER_SIZE - 1));
        return;
    }
    _ws.sendTXT(_jsonBuffer, len);
}

// ============================================================================
// Color Correction Commands
// ============================================================================

void WebSocketClient::requestColorCorrectionConfig() {
    if (!isConnected()) {
        return;
    }

    TAB5_AGENT_PRINTF("[DEBUG] requestColorCorrectionConfig entry - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    StaticJsonDocument<96> doc;
    doc["requestId"] = "cc_sync";
    TAB5_AGENT_PRINTF("[DEBUG] requestColorCorrectionConfig before sendJSON - Heap: free=%u minFree=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap());
    sendJSON("colorCorrection.getConfig", doc);
    TAB5_AGENT_PRINTF("[DEBUG] requestColorCorrectionConfig after sendJSON - Heap: free=%u minFree=%u largest=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    Serial.println("[WS] Requested colorCorrection.getConfig");
}

void WebSocketClient::sendColorCorrectionConfig(bool gammaEnabled, float gammaValue,
                                                bool aeEnabled, uint8_t aeTarget,
                                                bool brownEnabled) {
    if (!isConnected()) {
        return;
    }

    StaticJsonDocument<192> doc;
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

    StaticJsonDocument<128> doc;
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

    StaticJsonDocument<128> doc;
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

    StaticJsonDocument<96> doc;
    doc["brownGuardrailEnabled"] = enabled;
    sendJSON("colorCorrection.setConfig", doc);

    // Update local cache
    _colorCorrectionState.brownGuardrailEnabled = enabled;
}

void WebSocketClient::sendColourCorrectionMode(uint8_t mode) {
    if (!isConnected()) {
        return;
    }

    // HIGH PRIORITY FIX: Use dedicated colorCorrection.setMode command when only mode changes
    // This is more efficient than sending full config
    StaticJsonDocument<96> doc;
    doc["mode"] = mode;
    sendJSON("colorCorrection.setMode", doc);

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

    StaticJsonDocument<128> doc;
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
