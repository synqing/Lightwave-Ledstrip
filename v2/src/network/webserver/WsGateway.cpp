/**
 * @file WsGateway.cpp
 * @brief WebSocket gateway implementation
 */

#include "WsGateway.h"
#include "WsCommandRouter.h"
#include "../ApiResponse.h"
#include "../../utils/Log.h"
#include <cstring>

#undef LW_LOG_TAG
#define LW_LOG_TAG "WsGateway"

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
    // Ensure stale client entries are purged before applying connection limits.
    m_ws->cleanupClients();
    constexpr uint8_t MAX_WS_CLIENTS = 8;  // From WebServerConfig::MAX_WS_CLIENTS
    if (m_ws->count() > MAX_WS_CLIENTS) {
        LW_LOGW("WS: Max clients reached, rejecting %u", client->id());
        client->close(1008, "Connection limit");
        return;
    }

    LW_LOGI("WS: Client %u connected from %s", client->id(), client->remoteIP().toString().c_str());

    // Call connection callback (for status broadcasts, etc.)
    if (m_onConnect) {
        m_onConnect(client);
    }
}

void WsGateway::handleDisconnect(AsyncWebSocketClient* client) {
    uint32_t clientId = client->id();
    LW_LOGI("WS: Client %u disconnected", clientId);

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

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        client->text(buildWsError(ErrorCodes::INVALID_JSON, "Parse error"));
        return;
    }

    // Auth check
    if (!m_checkAuth(client, doc)) {
        return;  // Auth checker sends error response
    }

    // Route command via WsCommandRouter
    bool handled = WsCommandRouter::route(client, doc, m_ctx);
    
    // If not handled by router, send error (all commands should be registered)
    if (!handled) {
        const char* requestId = doc["requestId"] | "";
        client->text(buildWsError(ErrorCodes::INVALID_VALUE, "Unknown command type", requestId));
    }
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos

