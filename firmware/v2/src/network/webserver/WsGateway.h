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
    struct Stats {
        uint32_t connectAccepted = 0;
        uint32_t connectRejectedCooldown = 0;
        uint32_t connectRejectedOverlap = 0;
        uint32_t connectRejectedLimit = 0;
        uint32_t disconnects = 0;
        uint32_t parseErrors = 0;
        uint32_t oversizedFrames = 0;
        uint32_t unknownCommands = 0;
        uint32_t binaryFramesAccepted = 0;
        uint32_t binaryFramesRejected = 0;
    };

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
    ~WsGateway();

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
    void handleMessage(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len);

    /**
     * @brief Backward-compatible wrapper for legacy call sites.
     */
    void handleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
        handleMessage(client, nullptr, data, len);
    }

    /**
     * @brief Cleanup stale guard entries that have been idle too long
     *
     * Called from WebServer::update() to clear guard entries for connections
     * that haven't sent any messages within IDLE_TIMEOUT_MS. This prevents
     * zombie entries from blocking new connections from the same IP.
     */
    void cleanupStaleConnections();

    /**
     * @brief Close WebSocket clients that were connected via a specific subnet prefix
     *
     * Uses the fixed-size clientId → IP table to avoid fragile iteration over AsyncWebSocket internals.
     * Intended for Soft-AP station disconnect cleanup (192.168.4.*).
     *
     * @param a First octet (e.g. 192)
     * @param b Second octet (e.g. 168)
     * @param c Third octet (e.g. 4)
     * @param code WebSocket close code
     * @param reason Optional close reason (may be nullptr)
     */
    void closeClientsInSubnet(uint8_t a, uint8_t b, uint8_t c, uint16_t code, const char* reason);
    Stats getStats() const { return m_stats; }

private:
    AsyncWebSocket* m_ws;
    WebServerContext m_ctx;
    std::function<bool(AsyncWebSocketClient*)> m_checkRateLimit;
    std::function<bool(AsyncWebSocketClient*, JsonDocument&)> m_checkAuth;
    std::function<void(AsyncWebSocketClient*)> m_onConnect;
    std::function<void(AsyncWebSocketClient*)> m_onDisconnect;
    std::function<void(AsyncWebSocketClient*, JsonDocument&)> m_fallbackHandler;

    // ------------------------------------------------------------------------
    // Connection thrash guard (allocation-free)
    //
    // Some clients (e.g. misconfigured embedded encoders) can rapidly reconnect,
    // causing AsyncTCP accept failures and widespread WS timeouts. We apply a
    // small per-IP cooldown to reduce churn.
    // ------------------------------------------------------------------------
    static constexpr uint8_t CONNECT_GUARD_SLOTS = 8;
    static constexpr uint32_t CONNECT_COOLDOWN_MS = 2000;
    static constexpr uint32_t IDLE_TIMEOUT_MS = 15000;  // Clear stale entries after 15s of inactivity
    struct ConnectGuardEntry {
        uint32_t ipKey;          // Packed IPv4 (0 = empty)
        uint32_t lastMs;         // Last connect attempt time (millis)
        uint32_t lastActivityMs; // Last message activity time (millis) - for idle timeout
        uint8_t active;          // Active WS connections from this IP (best-effort)
        uint8_t _pad[3];
    };
    ConnectGuardEntry m_connectGuard[CONNECT_GUARD_SLOTS] = {};

    // ------------------------------------------------------------------------
    // Client ID → IP mapping (for disconnect cleanup)
    //
    // When a client disconnects, client->remoteIP() may return 0.0.0.0,
    // preventing cleanup of the connect guard. We track clientId → ipKey
    // during connect so we can clean up properly on disconnect.
    // ------------------------------------------------------------------------
    static constexpr uint8_t CLIENT_IP_MAP_SLOTS = 16;
    struct ClientIpMapEntry {
        uint32_t clientId;    // AsyncWebSocketClient ID (0 = empty)
        uint32_t ipKey;       // Packed IPv4 from connect time
    };
    ClientIpMapEntry m_clientIpMap[CLIENT_IP_MAP_SLOTS] = {};

    // ------------------------------------------------------------------------
    // Connection epoch tracking (for Choreo telemetry)
    //
    // Tracks connection epochs per client ID. Epoch increments on reconnect
    // to distinguish messages across connection boundaries.
    // ------------------------------------------------------------------------
    struct ClientEpochEntry {
        uint32_t clientId;    // AsyncWebSocketClient ID (0 = empty)
        uint32_t connEpoch;   // Increments on reconnect (starts at 0)
        uint32_t connectTs;   // Timestamp of this epoch start (millis)
    };
    ClientEpochEntry m_clientEpochs[CLIENT_IP_MAP_SLOTS] = {};

    // ------------------------------------------------------------------------
    // WebSocket message size limit (OWASP recommendation: 64KB)
    // ------------------------------------------------------------------------
    static constexpr size_t MAX_WS_MESSAGE_SIZE = 64 * 1024;  // 64KB
    static constexpr size_t MAX_BINARY_FRAME_BUFFER = 2048;   // render.stream binary frame cap

    struct BinaryAssemblyEntry {
        uint32_t clientId = 0;
        uint32_t expectedLen = 0;
        uint32_t receivedLen = 0;
        bool active = false;
        uint8_t* buffer = nullptr;  // PSRAM-allocated in ctor; freed in dtor
    };
    BinaryAssemblyEntry m_binaryAssembly[CLIENT_IP_MAP_SLOTS] = {};

    // ------------------------------------------------------------------------
    // Monotonic event sequence counter (for telemetry)
    // ------------------------------------------------------------------------
    static uint32_t s_eventSeq;
    Stats m_stats{};

    // Helper to get or increment connection epoch for a client
    uint32_t getOrIncrementEpoch(uint32_t clientId);

    // Validate WebSocket handshake Origin header (browser CSWSH protection)
    bool validateOrigin(AsyncWebServerRequest* request);

    bool handleBinaryFrameData(AsyncWebSocketClient* client,
                               AwsFrameInfo* info,
                               const uint8_t* data,
                               size_t len);

    // Static instance pointer for event handler
    static WsGateway* s_instance;
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos
