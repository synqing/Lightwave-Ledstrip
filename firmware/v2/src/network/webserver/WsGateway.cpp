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

#ifndef LW_AGENT_TRACE
#define LW_AGENT_TRACE 0
#endif

namespace lightwaveos {
namespace network {
namespace webserver {

// Static instance for event handler
WsGateway* WsGateway::s_instance = nullptr;

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
    const IPAddress ip = client->remoteIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    // Log that upgrade request was received and processed
    LW_LOGI("WS: Upgrade request received from %s (client ID: %u)", 
            ipStr, client->id());
    
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();

    const uint32_t nowMs = millis();
    const uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

#if LW_AGENT_TRACE
    // Hws1: Confirm connect thrash + measure reject causes (max clients vs cooldown).
    {
        char buf[320];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.enter\",\"data\":{\"clientId\":%lu,\"wsCount\":%u,\"wsMax\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
            static_cast<unsigned long>(client->id()),
            static_cast<unsigned>(m_ws->count()),
            static_cast<unsigned>(lightwaveos::network::WebServerConfig::MAX_WS_CLIENTS),
            ipStr,
            static_cast<unsigned long>(nowMs)
        );
        if (n > 0) Serial.println(buf);
    }
#endif

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
#if LW_AGENT_TRACE
                {
                    char buf[320];
                    const int n = snprintf(
                        buf, sizeof(buf),
                        "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws1\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.cooldown\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"cooldownMs\":%lu},\"timestamp\":%lu}",
                        static_cast<unsigned long>(client->id()),
                        ipStr,
                        static_cast<unsigned long>(CONNECT_COOLDOWN_MS),
                        static_cast<unsigned long>(nowMs)
                    );
                    if (n > 0) Serial.println(buf);
                }
#endif
                client->close(1013, "Reconnect too fast");
                return;
            }

            // Reject overlapping WS sessions from the same IP.
            // This protects the device from clients that repeatedly call connect() without
            // closing the previous connection or without servicing the socket.
            if (m_connectGuard[slot].active >= 1) {
#if LW_AGENT_TRACE
                {
                    char buf[320];
                    const int n = snprintf(
                        buf, sizeof(buf),
                        "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws2\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleConnect\",\"message\":\"ws.connect.reject.overlap\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"active\":%u},\"timestamp\":%lu}",
                        static_cast<unsigned long>(client->id()),
                        ipStr,
                        static_cast<unsigned>(m_connectGuard[slot].active),
                        static_cast<unsigned long>(nowMs)
                    );
                    if (n > 0) Serial.println(buf);
                }
#endif
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

    LW_LOGI("WS: Client %u connected from %s", client->id(), ipStr);

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

    // Call connection callback (for status broadcasts, etc.)
    if (m_onConnect) {
        m_onConnect(client);
    }
}

void WsGateway::handleDisconnect(AsyncWebSocketClient* client) {
    uint32_t clientId = client->id();
    LW_LOGI("WS: Client %u disconnected", clientId);

    const uint32_t nowMs = millis();
    const IPAddress ip = client->remoteIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    uint32_t ipKey =
        (static_cast<uint32_t>(ip[0]) << 24) |
        (static_cast<uint32_t>(ip[1]) << 16) |
        (static_cast<uint32_t>(ip[2]) << 8)  |
        (static_cast<uint32_t>(ip[3]) << 0);

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

#if LW_AGENT_TRACE
    // Hws3: Confirm whether disconnects correlate with zero messages received.
    {
        char buf[320];
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleDisconnect\",\"message\":\"ws.disconnect\",\"data\":{\"clientId\":%lu,\"ip\":\"%s\",\"ipKey\":%lu},\"timestamp\":%lu}",
            static_cast<unsigned long>(clientId),
            ipStr,
            static_cast<unsigned long>(ipKey),
            static_cast<unsigned long>(nowMs)
        );
        if (n > 0) Serial.println(buf);
    }
#endif

    // Call disconnection callback (for cleanup)
    if (m_onDisconnect) {
        m_onDisconnect(client);
    }
}

void WsGateway::handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    // Rate limit check
    if (!m_checkRateLimit(client)) {
        return;  // Rate limiter sends error response
    }

    // Parse message
    if (len > 1024) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Message too large"));
        return;
    }

#if LW_AGENT_TRACE
    // Hws3: Prove whether the encoder ever sends WS data before ack timeouts.
    {
        char buf[320];
        const IPAddress ip = client->remoteIP();
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        const int n = snprintf(
            buf, sizeof(buf),
            "{\"sessionId\":\"debug-session\",\"runId\":\"ws-guard-pre\",\"hypothesisId\":\"Hws3\",\"location\":\"v2/src/network/webserver/WsGateway.cpp:handleMessage\",\"message\":\"ws.message.recv\",\"data\":{\"clientId\":%lu,\"len\":%u,\"ip\":\"%s\"},\"timestamp\":%lu}",
            static_cast<unsigned long>(client->id()),
            static_cast<unsigned>(len),
            ipStr,
            static_cast<unsigned long>(millis())
        );
        if (n > 0) Serial.println(buf);
    }
#endif

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
#if LW_AGENT_TRACE
        // Hwse2: Prove whether disconnects follow parse errors (invalid JSON / partial frames).
        {
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
#endif
        client->text(buildWsError(ErrorCodes::INVALID_JSON, "Parse error"));
        return;
    }

    // Auth check
    if (!m_checkAuth(client, doc)) {
        return;  // Auth checker sends error response
    }

    // Route command via WsCommandRouter
    const char* typeStr = doc["type"] | "";
    bool handled = WsCommandRouter::route(client, doc, m_ctx);

#if LW_AGENT_TRACE
    // Hwse1: Determine whether encoder sends unknown command types that trigger errors/closures.
    {
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
#endif
    
    // If not handled by router, send error (all commands should be registered)
    if (!handled) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestId));
    }
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
