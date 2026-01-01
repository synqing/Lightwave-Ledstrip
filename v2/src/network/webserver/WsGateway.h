/**
 * @file WsGateway.h
 * @brief WebSocket gateway for connection management and message routing
 *
 * Handles WebSocket lifecycle (connect/disconnect), rate limiting, authentication,
 * JSON parsing, and dispatches to WsCommandRouter.
 */

#pragma once

#include "WebServerContext.h"
#include "WsCommandRouter.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief WebSocket gateway
 *
 * Manages WebSocket connections, rate limiting, authentication, and message routing.
 */
class WsGateway {
public:
    /**
     * @brief Construct gateway
     * @param ws AsyncWebSocket instance
     * @param ctx WebServer context
     * @param checkRateLimit Rate limit checker function
     * @param checkAuth Auth checker function (returns true if authenticated or auth not required)
     * @param onConnect Connection callback
     * @param onDisconnect Disconnection callback
     * @param fallbackHandler Fallback handler for unhandled commands (can be nullptr)
     */
    WsGateway(
        AsyncWebSocket* ws,
        const WebServerContext& ctx,
        std::function<bool(AsyncWebSocketClient*)> checkRateLimit,
        std::function<bool(AsyncWebSocketClient*, JsonDocument&)> checkAuth,
        std::function<void(AsyncWebSocketClient*)> onConnect,
        std::function<void(AsyncWebSocketClient*)> onDisconnect,
        std::function<void(AsyncWebSocketClient*, JsonDocument&)> fallbackHandler = nullptr
    );

    /**
     * @brief Handle WebSocket event
     * @param server WebSocket server
     * @param client WebSocket client
     * @param type Event type
     * @param arg Event argument
     * @param data Event data
     * @param len Data length
     */
    static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len);

    /**
     * @brief Handle client connection
     * @param client WebSocket client
     */
    void handleConnect(AsyncWebSocketClient* client);

    /**
     * @brief Handle client disconnection
     * @param client WebSocket client
     */
    void handleDisconnect(AsyncWebSocketClient* client);

    /**
     * @brief Handle incoming message
     * @param client WebSocket client
     * @param data Message data
     * @param len Message length
     */
    void handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);

private:
    AsyncWebSocket* m_ws;
    WebServerContext m_ctx;
    std::function<bool(AsyncWebSocketClient*)> m_checkRateLimit;
    std::function<bool(AsyncWebSocketClient*, JsonDocument&)> m_checkAuth;
    std::function<void(AsyncWebSocketClient*)> m_onConnect;
    std::function<void(AsyncWebSocketClient*)> m_onDisconnect;
    std::function<void(AsyncWebSocketClient*, JsonDocument&)> m_fallbackHandler;

    // Static instance pointer for event handler
    static WsGateway* s_instance;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

