/**
 * @file WsGateway.cpp
 * @brief WebSocket gateway implementation
 */

#include "WsGateway.h"
#include "WsCommandRouter.h"
#include "../WebServer.h"
#include "../ApiResponse.h"
#include "../../utils/Log.h"
#include <cstring>
#include <Arduino.h>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsGateway"

namespace lightwaveos {
namespace network {
namespace webserver {

// Static instance for event handler
WsGateway* WsGateway::s_instance = nullptr;

// Monotonic event sequence counter (wraps at UINT32_MAX)
uint32_t WsGateway::s_eventSeq = 0;

WsGateway::WsGateway(
    AsyncWebSocket* ws,
    const WebServerContext& ctx,
    std::function<bool(AsyncWebSocketClient*)> checkRateLimit,
    std::function<bool(AsyncWebSocketClient*, JsonDocument&)> checkAuth,
    std::function<void(AsyncWebSocketClient*)> onConnect,
    std::function<void(AsyncWebSocketClient*)> onDisconnect,
    std::function<void(AsyncWebSocketClient*, JsonDocument&)> fallbackHandler
)
    : m_ws(ws)
    , m_ctx(ctx)  // Copy: ctx in WebServer::setupWebSocket() is stack-allocated
    , m_checkRateLimit(checkRateLimit)
    , m_checkAuth(checkAuth)
    , m_onConnect(onConnect)
    , m_onDisconnect(onDisconnect)
    , m_fallbackHandler(fallbackHandler)
{
    s_instance = this;
}

void WsGateway::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (!s_instance) return;

    switch (type) {
        case WS_EVT_CONNECT:
            s_instance->handleConnect(client);
            break;

        case WS_EVT_DISCONNECT:
            s_instance->handleDisconnect(client);
            break;

        case WS_EVT_DATA:
            s_instance->handleMessage(client, data, len);
            break;

        case WS_EVT_ERROR:
            LW_LOGW("WS: Error from client %u", client->id());
            break;

        case WS_EVT_PONG:
            // Pong received
            break;
    }
}

void WsGateway::handleConnect(AsyncWebSocketClient* client) {
    // Log that upgrade request was received and processed
    LW_LOGI("WS: Upgrade request received from %s (client ID: %u)", 
            client->remoteIP().toString().c_str(), client->id());
    
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();

    const uint32_t nowMs = millis();
    const IPAddress ip = client->remoteIP();
    const uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

    // #region agent log
    {
        // Hws1: Confirm connect thrash + measure reject causes (max clients vs cooldown).
        char buf[320];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.enter\",\"data\":{\"clientId\":%lu,\"wsCount\":%u,\"wsMax\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
            static_cast<unsigned long>(client->id()),
            static_cast<unsigned>(m_ws->count()),
            static_cast<unsigned>(lightwaveos::network::WebServerConfig::MAX_WS_CLIENTS),
            ip.toString().c_str(),
            static_cast<unsigned long>(nowMs)
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    // Per-IP connection cooldown + single-session guard (reduces reconnect storms / overlap)
    if (ipKey != 0) {
        uint8_t slot = 0xFF;
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                slot = i;
                break;
            }
            if (slot == 0xFF && m_connectGuard[i].ipKey == 0) {
                slot = i;  // first empty slot
            }
        }
        if (slot != 0xFF) {
            const uint32_t last = m_connectGuard[slot].lastMs;
            const bool tooSoon = (last != 0) && (nowMs - last < CONNECT_COOLDOWN_MS);
            m_connectGuard[slot].ipKey = ipKey;
            m_connectGuard[slot].lastMs = nowMs;
            if (tooSoon) {
                // #region agent log
                {
                    char buf[320];
                    const int n = snprintf(
                        buf, sizeof(buf),
                        "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.cooldown\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"cooldownMs\":%lu},\"timestamp\":%lu}",
                        static_cast<unsigned long>(client->id()),
                        ip.toString().c_str(),
                        static_cast<unsigned long>(CONNECT_COOLDOWN_MS),
                        static_cast<unsigned long>(nowMs)
                    );
                    if (n > 0) Serial.println(buf);
                }
                // #endregion
                client->close(1013, "Reconnect too fast");
                return;
            }

            // Reject overlapping WS sessions from the same IP.
            // This protects the device from clients that repeatedly call connect() without
            // closing the previous connection or without servicing the socket.
            if (m_connectGuard[slot].active >= 1) {
                // #region agent log
                {
                    char buf[320];
                    const int n = snprintf(
                        buf, sizeof(buf),
                        "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws2\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.overlap\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"active\":%u},\"timestamp\":%lu}",
                        static_cast<unsigned long>(client->id()),
                        ip.toString().c_str(),
                        static_cast<unsigned>(m_connectGuard[slot].active),
                        static_cast<unsigned long>(nowMs)
                    );
                    if (n > 0) Serial.println(buf);
                }
                // #endregion
                client->close(1008, "Only one session per device");
                return;
            }
        }
    }

    // Hard cap on connected WS clients (>=, not >)
    if (m_ws->count() >= lightwaveos::network::WebServerConfig::MAX_WS_CLIENTS) {
        LW_LOGW("WS: Max clients reached, rejecting %u", client->id());
        client->close(1008, "Connection limit");
        return;
    }

    LW_LOGI("WS: Client %u connected from %s", client->id(), client->remoteIP().toString().c_str());

    // Mark active for this IP (best-effort)
    if (ipKey != 0) {
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                if (m_connectGuard[i].active < 255) m_connectGuard[i].active++;
                break;
            }
        }
    }

    // Store client ID → IP mapping for disconnect cleanup
    // (remoteIP() may return 0.0.0.0 after disconnect)
    if (ipKey != 0) {
        uint32_t clientId = client->id();
        // Find empty slot or existing entry for this client
        uint8_t mapSlot = 0xFF;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientIpMap[i].clientId == clientId) {
                // Update existing entry
                m_clientIpMap[i].ipKey = ipKey;
                mapSlot = i;
                break;
            }
            if (mapSlot == 0xFF && m_clientIpMap[i].clientId == 0) {
                mapSlot = i;  // first empty slot
            }
        }
        if (mapSlot != 0xFF && m_clientIpMap[mapSlot].clientId == 0) {
            // Store new mapping
            m_clientIpMap[mapSlot].clientId = clientId;
            m_clientIpMap[mapSlot].ipKey = ipKey;
        }
    }

    // Track connection epoch (increments on reconnect)
    uint32_t clientId = client->id();
    uint32_t connEpoch = getOrIncrementEpoch(clientId);  // Epoch is set/incremented here
    uint32_t eventSeq = s_eventSeq++;
    uint32_t tsMonoms = millis();

    // Structured telemetry: ws.connect event
    {
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ws.connect\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"ip\":\"%s\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            ip.toString().c_str()
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }

    // Call connection callback (for status broadcasts, etc.)
    if (m_onConnect) {
        m_onConnect(client);
    }
}

uint32_t WsGateway::getOrIncrementEpoch(uint32_t clientId) {
    // Find existing epoch entry or create new one
    uint8_t epochSlot = 0xFF;
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            // Client exists - increment epoch (contract: every connect = new epoch)
            m_clientEpochs[i].connEpoch++;
            m_clientEpochs[i].connectTs = millis();
            return m_clientEpochs[i].connEpoch;
        }
        if (epochSlot == 0xFF && m_clientEpochs[i].clientId == 0) {
            epochSlot = i;  // first empty slot
        }
    }
    
    // New client - create entry with epoch 0
    if (epochSlot != 0xFF) {
        m_clientEpochs[epochSlot].clientId = clientId;
        m_clientEpochs[epochSlot].connEpoch = 0;
        m_clientEpochs[epochSlot].connectTs = millis();
        return 0;
    }
    
    // Table full - return 0 (shouldn't happen with normal client counts)
    return 0;
}

void WsGateway::handleDisconnect(AsyncWebSocketClient* client) {
    uint32_t clientId = client->id();
    LW_LOGI("WS: Client %u disconnected", clientId);

    const uint32_t nowMs = millis();
    const IPAddress ip = client->remoteIP();
    uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

    // Lookup connection epoch before cleanup
    uint32_t connEpoch = 0;
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            connEpoch = m_clientEpochs[i].connEpoch;
            // Clear epoch entry (disconnect cleanup)
            m_clientEpochs[i].clientId = 0;
            m_clientEpochs[i].connEpoch = 0;
            m_clientEpochs[i].connectTs = 0;
            break;
        }
    }
    
    uint32_t eventSeq = s_eventSeq++;
    
    // Structured telemetry: ws.disconnect event
    {
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"ws.disconnect\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"ip\":\"%s\"}",
            static_cast<unsigned long>(nowMs),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            ip.toString().c_str()
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }

    // If remoteIP() returned 0.0.0.0 (common after disconnect), lookup stored IP
    if (ipKey == 0) {
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientIpMap[i].clientId == clientId) {
                ipKey = m_clientIpMap[i].ipKey;
                // Clear the mapping entry (will be removed after cleanup)
                m_clientIpMap[i].clientId = 0;
                m_clientIpMap[i].ipKey = 0;
                break;
            }
        }
        if (ipKey == 0) {
            LW_LOGW("WS: Client %u disconnected but no IP mapping found", clientId);
        }
    } else {
        // Remove mapping entry if IP was valid (cleanup)
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientIpMap[i].clientId == clientId) {
                m_clientIpMap[i].clientId = 0;
                m_clientIpMap[i].ipKey = 0;
                break;
            }
        }
    }

    // Mark inactive for this IP (best-effort)
    if (ipKey != 0) {
        for (uint8_t i = 0; i < CONNECT_GUARD_SLOTS; i++) {
            if (m_connectGuard[i].ipKey == ipKey) {
                if (m_connectGuard[i].active > 0) m_connectGuard[i].active--;
                break;
            }
        }
    }

    // #region agent log
    {
        // Hws3: Confirm whether disconnects correlate with zero messages received.
        char buf[320];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleDisconnect\",\"message\":\"ws.disconnect\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"ipKey\":%lu},\"timestamp\":%lu}",
            static_cast<unsigned long>(clientId),
            ip.toString().c_str(),
            static_cast<unsigned long>(ipKey),
            static_cast<unsigned long>(nowMs)
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    // Call disconnection callback (for cleanup)
    if (m_onDisconnect) {
        m_onDisconnect(client);
    }
}

void WsGateway::handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Rate limit check
    if (!m_checkRateLimit(client)) {
        // Structured telemetry: msg.recv with result="rejected", reason="rate_limit"
        uint32_t clientId = client->id();
        uint32_t connEpoch = 0;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientEpochs[i].clientId == clientId) {
                connEpoch = m_clientEpochs[i].connEpoch;
                break;
            }
        }
        uint32_t eventSeq = s_eventSeq++;
        uint32_t tsMonoms = millis();
        
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"rate_limit\",\"payloadSummary\":\"\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
        return;  // Drop message silently
    }

    // Parse message
    if (len > MAX_WS_MESSAGE_SIZE) {
        // Log rejected frame (oversize)
        uint32_t clientId = client->id();
        uint32_t connEpoch = 0;
        for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
            if (m_clientEpochs[i].clientId == clientId) {
                connEpoch = m_clientEpochs[i].connEpoch;
                break;
            }
        }
        uint32_t eventSeq = s_eventSeq++;
        uint32_t tsMonoms = millis();
        
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"size_limit\",\"payloadSummary\":\"\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId)
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
        
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Message too large"));
        return;
    }

    // #region agent log
    {
        // Hws3: Prove whether the encoder ever sends WS data before ack timeouts.
        char buf[320];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.message.recv\",\"data\":{\"clientId\":%lu,\"len\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
            static_cast<unsigned long>(client->id()),
            static_cast<unsigned>(len),
            client->remoteIP().toString().c_str(),
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);

    // Structured telemetry: msg.recv event (log ALL inbound frames, before auth check)
    uint32_t clientId = client->id();
    uint32_t connEpoch = 0;
    
    // Lookup connection epoch for this client
    for (uint8_t i = 0; i < CLIENT_IP_MAP_SLOTS; i++) {
        if (m_clientEpochs[i].clientId == clientId) {
            connEpoch = m_clientEpochs[i].connEpoch;
            break;
        }
    }
    
    uint32_t eventSeq = s_eventSeq++;
    uint32_t tsMonoms = millis();
    
    if (error) {
        // Log rejected frame (parse error)
        // Create bounded payload summary from raw data (~100 chars max)
        char payloadSummary[128] = {0};
        size_t copyLen = len < (sizeof(payloadSummary) - 1) ? len : (sizeof(payloadSummary) - 1);
        memcpy(payloadSummary, data, copyLen);
        payloadSummary[copyLen] = '\0';
        
        // #region agent log
        {
            // Hwse2: Prove whether disconnects follow parse errors (invalid JSON / partial frames).
            char buf[360];
            const int n = snprintf(
                buf, sizeof(buf),
                "{\"sessionId\":\"debug-session\",\"runId\":\"ws-drop-pre\",\"hypothesisId\":\"Hwse2\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.json.parse_error\",\"data\":{\"clientId\":%lu,\"len\":%u},\"timestamp\":%lu}",
                static_cast<unsigned long>(client->id()),
                static_cast<unsigned>(len),
                static_cast<unsigned long>(millis())
            );
            if (n > 0) Serial.println(buf);
        }
        // #endregion
        
        // Structured telemetry: msg.recv with result="rejected"
        {
            char buf[512];
            const int n = snprintf(buf, sizeof(buf),
                "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"\",\"result\":\"rejected\",\"reason\":\"parse_error\",\"payloadSummary\":\"%.100s\",\"schemaVersion\":\"1.0.0\"}",
                static_cast<unsigned long>(tsMonoms),
                static_cast<unsigned long>(connEpoch),
                static_cast<unsigned long>(eventSeq),
                static_cast<unsigned long>(clientId),
                payloadSummary
            );
            if (n > 0 && n < static_cast<int>(sizeof(buf))) {
                Serial.println(buf);
            }
        }
        
        client->text(buildWsError(ErrorCodes::INVALID_JSON, "Parse error"));
        return;
    }
    
    const char* msgType = doc["type"] | "";
    
    // Create bounded payload summary (~100 chars max)
    char payloadSummary[128] = {0};
    serializeJson(doc, payloadSummary, sizeof(payloadSummary) - 1);
    
    // Auth check (before logging msg.recv to determine result)
    bool authPassed = m_checkAuth(client, doc);
    
    // Log structured msg.recv event (result="ok" if auth passed, "rejected" if auth failed)
    {
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"msg.recv\",\"ts_mono_ms\":%lu,\"connEpoch\":%lu,\"eventSeq\":%lu,\"clientId\":%lu,\"msgType\":\"%s\",\"result\":\"%s\",\"reason\":\"%s\",\"payloadSummary\":\"%.100s\",\"schemaVersion\":\"1.0.0\"}",
            static_cast<unsigned long>(tsMonoms),
            static_cast<unsigned long>(connEpoch),
            static_cast<unsigned long>(eventSeq),
            static_cast<unsigned long>(clientId),
            msgType,
            authPassed ? "ok" : "rejected",
            authPassed ? "" : "auth_failed",
            payloadSummary
        );
        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            Serial.println(buf);
        }
    }
    
    if (!authPassed) {
        // Auth failure - error response already sent by auth checker
        return;
    }

    // Route command via WsCommandRouter
    const char* typeStr = msgType;
    bool handled = WsCommandRouter::route(client, doc, m_ctx);

    // #region agent log
    {
        // Hwse1: Determine whether encoder sends unknown command types that trigger errors/closures.
        char buf[380];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-drop-pre\",\"hypothesisId\":\"Hwse1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.route.result\",\"data\":{\"clientId\":%lu,\"type\":\"%s\",\"handled\":%s},\"timestamp\":%lu}",
            static_cast<unsigned long>(client->id()),
            typeStr,
            handled ? "true" : "false",
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
    // #endregion
    
    // If not handled by router, send error (all commands should be registered)
    if (!handled) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestId));
    }
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

