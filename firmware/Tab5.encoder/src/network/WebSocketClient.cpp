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
#include <esp_task_wdt.h>  // For watchdog reset
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
    , _consecutiveSendFailures(0)
    , _sendDegraded(false)
    , _sendMutex(nullptr)
    , _sendAttemptStartTime(0)
{
    // Initialize rate limiter (all parameters start at 0)
    for (int i = 0; i < 16; i++) {
        _rateLimiter.lastSend[i] = 0;
    }

    // Initialize send queue
    for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
        _sendQueue[i].reset();
    }

    // Create mutex for send protection (binary semaphore)
    _sendMutex = xSemaphoreCreateMutex();
    if (_sendMutex == nullptr) {
        Serial.println("[WS] ERROR: Failed to create send mutex!");
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

    // CRITICAL FIX: Feed watchdog before begin() which can trigger DNS resolution
    esp_task_wdt_reset();
    
    _ws.begin(host, port, path);

    // Enable ping/pong heartbeat for dead-connection detection
    // 15s ping interval, 10s pong timeout, 2 missed pongs = disconnect
    _ws.enableHeartbeat(15000, 10000, 2);

    // CRITICAL FIX: Feed watchdog after begin() returns
    esp_task_wdt_reset();
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
    Serial.printf("[WS] Connection timeout: %lu ms\n", NetworkConfig::WS_CONNECTION_TIMEOUT_MS);
    Serial.printf("[WS] Local IP: %s\n", WiFi.localIP().toString().c_str());

    // Configure connection timeout
    _ws.setReconnectInterval(NetworkConfig::WS_CONNECTION_TIMEOUT_MS);

    // CRITICAL FIX: Feed watchdog before begin() which can trigger TCP connection
    esp_task_wdt_reset();
    
    _ws.begin(ip, port, path);

    // Enable ping/pong heartbeat for dead-connection detection
    _ws.enableHeartbeat(15000, 10000, 2);

    // CRITICAL FIX: Feed watchdog after begin() returns
    esp_task_wdt_reset();
}

void WebSocketClient::update() {
    // Reset watchdog before potentially blocking operations
    esp_task_wdt_reset();

    // Process WebSocket events
    // #region agent log (DISABLED)
// #if ENABLE_WS_DIAGNOSTICS
    // static uint32_t s_lastLoopLog = 0;
    // if ((uint32_t)(millis() - s_lastLoopLog) >= 500) {  // Log every 500ms
        // Serial.printf("[DEBUG] WebSocketClient::update before loop - Heap: free=%u minFree=%u largest=%u status=%d\n",
                      // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), (int)_status);
        // s_lastLoopLog = millis();
    // }
// #endif
        // #endregion
    
    // CRITICAL FIX: The third-party WebSocketsClient::loop() can block for >5s during
    // connection attempts (DNS resolution, TCP connect, SSL handshake, etc).
    // To prevent watchdog timeouts, we:
    // 1. Add an extra WDT reset immediately before the call
    // 2. Track execution time and log warnings if it exceeds threshold
    // 3. Add another WDT reset immediately after
    
    esp_task_wdt_reset();  // Extra reset before potentially-blocking _ws.loop()
    
    uint32_t loopStartMs = millis();
    _ws.loop();
    uint32_t loopDurationMs = millis() - loopStartMs;
    
    // Warn if _ws.loop() took >1s (normal is <10ms)
    if (loopDurationMs > 1000) {
        Serial.printf("[WS] WARNING: _ws.loop() took %lu ms (status=%d)\n", 
                      loopDurationMs, (int)_status);
    }
    
    esp_task_wdt_reset();  // Extra reset after potentially-blocking _ws.loop()
    
    // #region agent log (DISABLED)
// #if ENABLE_WS_DIAGNOSTICS
    // if ((uint32_t)(millis() - s_lastLoopLog) >= 500) {
        // Serial.printf("[DEBUG] WebSocketClient::update after loop - Heap: free=%u minFree=%u largest=%u\n",
                      // ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    // }
// #endif
        // #endregion

    // Reset watchdog after WebSocket loop
    esp_task_wdt_reset();

    // Process send queue (non-blocking, handles rapid encoder changes)
    processSendQueue();

    // Send pending hello message (deferred from connect event to ensure connection is ready)
    if (_pendingHello && _status == WebSocketStatus::CONNECTED) {
        _pendingHello = false;
        sendHelloMessage();
    }

    // Deferred zones.get (set by WsMessageRouter on "zones.changed" to avoid sending inside WS callback)
    if (_pendingZonesRefresh && _status == WebSocketStatus::CONNECTED) {
        _pendingZonesRefresh = false;
        requestZonesState();
    }

    // Check if stuck in CONNECTING state too long (timeout protection)
    static uint32_t s_connectingStartTime = 0;
    if (_status == WebSocketStatus::CONNECTING) {
        if (s_connectingStartTime == 0) {
            s_connectingStartTime = millis();
        } else if ((millis() - s_connectingStartTime) > NetworkConfig::WS_CONNECTION_TIMEOUT_MS) {
            // Stuck in CONNECTING too long, reset state to allow retry
            Serial.println("[WS] Connection timeout, resetting state");
            _status = WebSocketStatus::DISCONNECTED;
            s_connectingStartTime = 0;
            increaseReconnectBackoff();
        }
    } else {
        s_connectingStartTime = 0;  // Reset when not connecting
    }

    // Handle reconnection logic
    if (_status == WebSocketStatus::DISCONNECTED && _shouldReconnect) {
        attemptReconnect();
    }

    // Reset watchdog at end of update
    esp_task_wdt_reset();
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
            _pendingZonesRefresh = false;  // Clear deferred zone refresh
            increaseReconnectBackoff();
            break;

        case WStype_CONNECTED:
            {
                Serial.println("[WS] Connected to server");
                Serial.printf("[WS] Server IP: %s:%d%s\n", 
                              _useIP ? _serverIP.toString().c_str() : (_serverHost ? _serverHost : "unknown"),
                              _serverPort, _serverPath);
                Serial.printf("[WS] Local IP: %s\n", WiFi.localIP().toString().c_str());
            }
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
            {
                Serial.printf("[WS] Error occurred (delay: %lu ms)\n", _reconnectDelay);
                Serial.printf("[WS] Target: ws://%s:%d%s\n",
                              _useIP ? _serverIP.toString().c_str() : (_serverHost ? _serverHost : "unknown"),
                              _serverPort, _serverPath);
                Serial.printf("[WS] Local IP: %s, WiFi Status: %d\n", 
                              WiFi.localIP().toString().c_str(), (int)WiFi.status());
            }
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

        // CRITICAL FIX: Feed watchdog before reconnect attempt
        esp_task_wdt_reset();

        // Full teardown: close any lingering socket before creating new connection
        _ws.disconnect();

        if (_useIP) {
            _ws.begin(_serverIP, _serverPort, _serverPath);
        } else {
            _ws.begin(_serverHost, _serverPort, _serverPath);
        }

        // Re-enable heartbeat after fresh begin() (begin() resets library state)
        _ws.enableHeartbeat(15000, 10000, 2);

        // CRITICAL FIX: Feed watchdog after reconnect attempt
        esp_task_wdt_reset();
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
        #ifdef ENABLE_VERBOSE_DEBUG
        Serial.printf("[WS] Drop: not connected (type=%s, status=%d)\n",
                      type ? type : "null", static_cast<int>(_status));
        #endif
        return;
    }
    if (!takeSendLock(SEND_MUTEX_TIMEOUT_MS)) {
        #ifdef ENABLE_VERBOSE_DEBUG
        Serial.printf("[WS] Drop: send mutex busy (type=%s)\n", type ? type : "null");
        #endif
        _consecutiveSendFailures++;
        if (_consecutiveSendFailures > 3) {
            _sendDegraded = true;
            Serial.println("[WS] WARNING: Multiple send failures, marking as degraded");
        }
        return;
    }
    sendJSONUnlocked(type, doc);
    releaseSendLock();
}

void WebSocketClient::sendJSONUnlocked(const char* type, JsonDocument& doc) {
    if (!isConnected()) {
        return;
    }
    esp_task_wdt_reset();
    _sendAttemptStartTime = millis();

    JsonDocument message;
    JsonObject msgObj = message.to<JsonObject>();
    msgObj["type"] = type;
    JsonObject docObj = doc.as<JsonObject>();
    for (JsonPair kv : docObj) {
        msgObj[kv.key().c_str()] = kv.value();
    }

    size_t len = serializeJson(message, _jsonBuffer, JSON_BUFFER_SIZE - 1);
    if (len == 0 || len >= JSON_BUFFER_SIZE) {
        #ifdef ENABLE_VERBOSE_DEBUG
        Serial.printf("[WS] Message too large, dropping (type=%s, len=%u, max=%u)\n",
                      type ? type : "null", (unsigned)len, JSON_BUFFER_SIZE);
        #endif
        return;
    }
    _jsonBuffer[len] = '\0';

    esp_task_wdt_reset();
    bool sendResult = _ws.sendTXT(_jsonBuffer);
    uint32_t sendDuration = millis() - _sendAttemptStartTime;
    if (sendDuration > SEND_TIMEOUT_MS) {
        Serial.printf("[WS] WARNING: Send took %lu ms (threshold: %lu ms, type=%s)\n",
                      sendDuration, SEND_TIMEOUT_MS, type ? type : "null");
    }

    if (sendResult) {
        _consecutiveSendFailures = 0;
        if (_sendDegraded) {
            Serial.println("[WS] Send succeeded, clearing degraded state");
            _sendDegraded = false;
        }
    } else {
        _consecutiveSendFailures++;
        if (_consecutiveSendFailures > 3) {
            _sendDegraded = true;
            Serial.printf("[WS] WARNING: %u consecutive send failures, marking as degraded\n",
                          _consecutiveSendFailures);
        }
    }
    esp_task_wdt_reset();
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
    if (!isConnected()) {
        return;
    }

    // Queue if throttled, send immediately if allowed
    if (!canSend(ParamIndex::EFFECT)) {
        queueParameterChange(ParamIndex::EFFECT, effectId, "effects.setCurrent");
        return;
    }

    JsonDocument doc;
    doc["effectId"] = effectId;
    sendJSON("effects.setCurrent", doc);
}

void WebSocketClient::sendBrightnessChange(uint8_t brightness) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::BRIGHTNESS)) {
        queueParameterChange(ParamIndex::BRIGHTNESS, brightness, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["brightness"] = brightness;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendPaletteChange(uint8_t paletteId) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::PALETTE)) {
        queueParameterChange(ParamIndex::PALETTE, paletteId, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["paletteId"] = paletteId;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendSpeedChange(uint8_t speed) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::SPEED)) {
        queueParameterChange(ParamIndex::SPEED, speed, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["speed"] = speed;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendMoodChange(uint8_t mood) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::MOOD)) {
        queueParameterChange(ParamIndex::MOOD, mood, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["mood"] = mood;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendFadeAmountChange(uint8_t fadeAmount) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::FADEAMOUNT)) {
        queueParameterChange(ParamIndex::FADEAMOUNT, fadeAmount, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["fadeAmount"] = fadeAmount;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendComplexityChange(uint8_t complexity) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::COMPLEXITY)) {
        queueParameterChange(ParamIndex::COMPLEXITY, complexity, "parameters.set");
        return;
    }

    JsonDocument doc;
    doc["complexity"] = complexity;
    sendJSON("parameters.set", doc);
}

void WebSocketClient::sendVariationChange(uint8_t variation) {
    if (!isConnected()) {
        return;
    }

    if (!canSend(ParamIndex::VARIATION)) {
        queueParameterChange(ParamIndex::VARIATION, variation, "parameters.set");
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
    if (!isConnected()) {
        return;
    }

    // Map zoneId to rate limiter index
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3) {
        return;
    }

    if (!canSend(paramIndex)) {
        queueParameterChange(paramIndex, effectId, "zone.setEffect", zoneId);
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
    doc["brightness"] = value;
    sendJSON("zone.setBrightness", doc);
}

void WebSocketClient::sendZoneSpeed(uint8_t zoneId, uint8_t value) {
    if (!isConnected()) {
        return;
    }

    // Zone speed uses rate limiter slots (indices 9, 11, 13, 15)
    uint8_t paramIndex = ParamIndex::ZONE0_SPEED + (zoneId * 2);
    if (zoneId > 3) {
        return;
    }

    if (!canSend(paramIndex)) {
        queueParameterChange(paramIndex, value, "zone.setSpeed", zoneId);
        return;
    }

    JsonDocument doc;
    doc["zoneId"] = zoneId;
    doc["speed"] = value;
    sendJSON("zone.setSpeed", doc);
}

void WebSocketClient::sendZonePalette(uint8_t zoneId, uint8_t paletteId) {
    if (!isConnected()) {
        return;
    }

    // Zone palette shares rate limit with zone effect (same encoder)
    uint8_t paramIndex = ParamIndex::ZONE0_EFFECT + (zoneId * 2);
    if (zoneId > 3) {
        return;
    }

    if (!canSend(paramIndex)) {
        queueParameterChange(paramIndex, paletteId, "zone.setPalette", zoneId);
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
        // #region agent log (DISABLED)
        // {
            // HWS4: Prove layout pushes are being skipped due to connection/state gating.
            // char buf[240];
            // const int n = snprintf(
                // buf, sizeof(buf),
                // "{\"sessionId\":\"debug-session\",\"runId\":\"tab5-zone-ui-pre\",\"hypothesisId\":\"HWS4\",\"location\":\"Tab5.encoder/src/network/WebSocketClient.cpp:sendZonesSetLayout\",\"message\":\"ws.zonesSetLayout.skipped\",\"data\":{\"connected\":%s,\"segmentsNull\":%s,\"zoneCount\":%u},\"timestamp\":%lu}",
                // isConnected() ? "true" : "false",
                // segments ? "false" : "true",
                // static_cast<unsigned>(zoneCount),
                // static_cast<unsigned long>(millis())
            // );
            // if (n > 0) Serial.println(buf);
        // }
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

    JsonDocument doc;
    // Empty payload
    sendJSON("colorCorrection.getConfig", doc);
}

void WebSocketClient::sendColorCorrectionConfig(bool gammaEnabled, float gammaValue,
                               bool autoExposureEnabled, uint8_t autoExposureTarget,
                               bool brownGuardrailEnabled, uint8_t mode) {
    // #region agent log (DISABLED)
    // NOTE: Do NOT write to host filesystem paths from firmware. Serial-only tracing is safe.
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1,H2\",\"location\":\"WebSocketClient.cpp:sendColorCorrectionConfig\",\"message\":\"entry\",\"data\":{\"connected\":%d,\"gammaEnabled\":%d,\"gammaValue\":%.1f,\"aeEnabled\":%d,\"aeTarget\":%d,\"brownEnabled\":%d,\"mode\":%d},\"timestamp\":%lu}\n",
                  // isConnected() ? 1 : 0, gammaEnabled ? 1 : 0, gammaValue, autoExposureEnabled ? 1 : 0,
                  // autoExposureTarget, brownGuardrailEnabled ? 1 : 0, mode, (unsigned long)millis());
        // #endregion
    
    if (!isConnected()) {
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H2\",\"location\":\"WebSocketClient.cpp:594\",\"message\":\"sendColorCorrectionConfig.notConnected\",\"data\":{\"status\":%d},\"timestamp\":%lu}\n",
                      // (int)_status, (unsigned long)millis());
                // #endregion
        return;
    }

    JsonDocument doc;
    doc["gammaEnabled"] = gammaEnabled;
    doc["gammaValue"] = gammaValue;
    doc["autoExposureEnabled"] = autoExposureEnabled;
    doc["autoExposureTarget"] = autoExposureTarget;
    doc["brownGuardrailEnabled"] = brownGuardrailEnabled;
    doc["mode"] = mode;  // Include mode field - server requires all fields in setConfig
    
    // #region agent log (DISABLED)
    // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H3\",\"location\":\"WebSocketClient.cpp:606\",\"message\":\"sendColorCorrectionConfig.jsonPrepared\",\"data\":{\"command\":\"colorCorrection.setConfig\"},\"timestamp\":%lu}\n",
                  // (unsigned long)millis());
        // #endregion
    
    Serial.printf("[WS] Sending colorCorrection.setConfig: gamma=%s(%.1f) ae=%s brown=%s mode=%d\n",
                  gammaEnabled ? "ON" : "OFF", gammaValue,
                  autoExposureEnabled ? "ON" : "OFF",
                  brownGuardrailEnabled ? "ON" : "OFF",
                  mode);
    
    sendJSON("colorCorrection.setConfig", doc);
}

void WebSocketClient::sendGammaChange(bool enabled, float value) {
    if (!isConnected()) {
        return;
    }
    
    JsonDocument doc;
    doc["enabled"] = enabled;
    doc["value"] = value;
    sendJSON("colorCorrection.setGamma", doc);
}

void WebSocketClient::sendAutoExposureChange(bool enabled, uint8_t target) {
    if (!isConnected()) {
        return;
    }
    
    JsonDocument doc;
    doc["enabled"] = enabled;
    doc["target"] = target;
    sendJSON("colorCorrection.setAutoExposure", doc);
}

void WebSocketClient::sendBrownGuardrailChange(bool enabled) {
    if (!isConnected()) {
        return;
    }
    
    JsonDocument doc;
    doc["enabled"] = enabled;
    sendJSON("colorCorrection.setBrownGuardrail", doc);
}

void WebSocketClient::sendColourCorrectionMode(uint8_t mode) {
    if (!isConnected()) {
        return;
    }
    
    JsonDocument doc;
    doc["mode"] = mode;
    sendJSON("colorCorrection.setMode", doc);
}

// ============================================================================
// Send Queue Management (non-blocking, prevents freeze on rapid encoder changes)
// ============================================================================

bool WebSocketClient::takeSendLock(uint32_t timeoutMs) {
    if (_sendMutex == nullptr) {
        return false;
    }
    
    TickType_t timeoutTicks = (timeoutMs == 0) ? 0 : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTake(_sendMutex, timeoutTicks) == pdTRUE;
}

void WebSocketClient::releaseSendLock() {
    if (_sendMutex != nullptr) {
        xSemaphoreGive(_sendMutex);
    }
}

void WebSocketClient::queueParameterChange(uint8_t paramIndex, uint8_t value, const char* type, uint8_t zoneId) {
    if (paramIndex >= SEND_QUEUE_SIZE) {
        return;
    }

    // Drop-oldest strategy: simply update the queue entry for this parameter
    // If a send is in progress, this value will be sent next time
    _sendQueue[paramIndex].paramIndex = paramIndex;
    _sendQueue[paramIndex].value = value;
    _sendQueue[paramIndex].zoneId = zoneId;  // 255 = not a zone parameter
    _sendQueue[paramIndex].timestamp = millis();
    _sendQueue[paramIndex].type = type;
    _sendQueue[paramIndex].valid = true;
}

void WebSocketClient::processSendQueue() {
    if (!isConnected() || _sendDegraded) {
        // Clear queue if WebSocket unavailable
        for (size_t i = 0; i < SEND_QUEUE_SIZE; i++) {
            _sendQueue[i].reset();
        }
        return;
    }

    uint32_t now = millis();

    // Process queue entries
    for (size_t i = 0; i < SEND_QUEUE_SIZE; i++) {
        if (!_sendQueue[i].valid) {
            continue;
        }

        // Drop stale messages (older than 500ms)
        if (now - _sendQueue[i].timestamp > NetworkConfig::SEND_QUEUE_STALE_TIMEOUT_MS) {
            _sendQueue[i].reset();
            continue;
        }

        // Check if throttle allows this send
        if (!canSend(_sendQueue[i].paramIndex)) {
            continue;  // Skip for now, will retry next update
        }

        // Try to send (non-blocking)
        if (takeSendLock(0)) {  // Try lock, don't wait
            // Reset watchdog before send attempt
            esp_task_wdt_reset();

            // Create JSON document with parameter value
            JsonDocument doc;
            const char* sendType = _sendQueue[i].type;  // Default to stored type
            
            // Handle zone parameters vs global parameters
            if (_sendQueue[i].zoneId < 4) {
                // Zone parameter
                doc["zoneId"] = _sendQueue[i].zoneId;
                // Extract field name from type (e.g., "zone.setEffect" -> "effectId")
                if (strstr(_sendQueue[i].type, "setEffect") != nullptr) {
                    doc["effectId"] = _sendQueue[i].value;
                } else if (strstr(_sendQueue[i].type, "setSpeed") != nullptr) {
                    doc["speed"] = _sendQueue[i].value;
                } else if (strstr(_sendQueue[i].type, "setPalette") != nullptr) {
                    doc["paletteId"] = _sendQueue[i].value;
                } else {
                    doc["value"] = _sendQueue[i].value;
                }
            } else {
                // Global parameter - map paramIndex to field name
                switch (_sendQueue[i].paramIndex) {
                    case ParamIndex::EFFECT:
                        doc["effectId"] = _sendQueue[i].value;
                        sendType = "effects.setCurrent";
                        break;
                    case ParamIndex::BRIGHTNESS:
                        doc["brightness"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::PALETTE:
                        doc["paletteId"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::SPEED:
                        doc["speed"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::MOOD:
                        doc["mood"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::FADEAMOUNT:
                        doc["fadeAmount"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::COMPLEXITY:
                        doc["complexity"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    case ParamIndex::VARIATION:
                        doc["variation"] = _sendQueue[i].value;
                        sendType = "parameters.set";
                        break;
                    default:
                        // Unknown parameter - drop
                        _sendQueue[i].reset();
                        releaseSendLock();
                        continue;
                }
            }
            
            // Send (caller holds mutex - use Unlocked to avoid nested take)
            _sendAttemptStartTime = millis();
            sendJSONUnlocked(sendType, doc);
            uint32_t sendDuration = millis() - _sendAttemptStartTime;
            if (sendDuration > SEND_TIMEOUT_MS) {
                Serial.printf("[WS] WARNING: Send took %lu ms (threshold: %lu ms)\n",
                              sendDuration, SEND_TIMEOUT_MS);
            }
            esp_task_wdt_reset();
            _sendQueue[i].reset();
            releaseSendLock();
            break;  // Only send one message per update() call to prevent blocking
        }
        // If lock not available, try again next update
    }
}

// ============================================================================
// Generic Commands
// ============================================================================

void WebSocketClient::sendGenericParameter(const char* fieldName, uint8_t value) {
    if (!isConnected()) {
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
