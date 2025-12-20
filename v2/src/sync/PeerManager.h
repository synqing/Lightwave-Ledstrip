/**
 * @file PeerManager.h
 * @brief WebSocket client connection management for multi-device sync
 *
 * Manages outgoing WebSocket connections to discovered peers. Each device
 * acts as both a WebSocket server (for incoming connections) and a client
 * (for outgoing connections to peers with higher priority).
 *
 * Connection Strategy:
 * - Connect to peers with higher UUID (potential leaders)
 * - Maintain up to MAX_PEER_CONNECTIONS concurrent connections
 * - Exponential backoff on connection failures
 * - Heartbeat monitoring for connection health
 *
 * Threading:
 * - All methods should be called from SyncManagerActor (Core 0)
 * - Uses ESPAsyncTCP for non-blocking WebSocket operations
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "SyncProtocol.h"
#include <cstdint>

// Forward declarations
#ifdef NATIVE_BUILD
struct MockAsyncWebSocketClient;
using AsyncWebSocketClient = MockAsyncWebSocketClient;
#else
class AsyncWebSocketClient;
#endif

namespace lightwaveos {
namespace sync {

/**
 * @brief Callback for received WebSocket messages
 */
using PeerMessageCallback = void (*)(const char* uuid, const char* message, size_t length);

/**
 * @brief Callback for connection state changes
 */
using PeerConnectionCallback = void (*)(const char* uuid, bool connected);

/**
 * @brief Per-connection state
 */
struct PeerConnection {
    char uuid[16];              // Peer UUID
    uint8_t ip[4];              // Peer IP address
    uint16_t port;              // Peer WebSocket port
    AsyncWebSocketClient* client; // WebSocket client (nullptr if not connected)
    uint32_t lastActivityMs;    // Last message sent/received
    uint32_t lastPingMs;        // Last ping sent
    uint32_t reconnectDelayMs;  // Current backoff delay
    uint8_t missedPings;        // Consecutive missed pings
    bool connecting;            // Connection in progress
    bool connected;             // Connection established

    PeerConnection()
        : uuid{0}
        , ip{0}
        , port(80)
        , client(nullptr)
        , lastActivityMs(0)
        , lastPingMs(0)
        , reconnectDelayMs(RECONNECT_INITIAL_MS)
        , missedPings(0)
        , connecting(false)
        , connected(false)
    {}

    /**
     * @brief Check if this slot is in use
     */
    bool inUse() const { return uuid[0] != '\0'; }

    /**
     * @brief Reset the connection slot
     */
    void reset() {
        uuid[0] = '\0';
        client = nullptr;
        connecting = false;
        connected = false;
        reconnectDelayMs = RECONNECT_INITIAL_MS;
        missedPings = 0;
    }
};

/**
 * @brief Manages WebSocket client connections to peers
 */
class PeerManager {
public:
    PeerManager();
    ~PeerManager();

    // Prevent copying
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;

    /**
     * @brief Initialize the peer manager
     */
    void begin();

    /**
     * @brief Periodic update - manage connections, heartbeats, reconnects
     *
     * Should be called frequently (e.g., every 100ms) from SyncManagerActor.
     *
     * @param nowMs Current time in milliseconds
     */
    void update(uint32_t nowMs);

    /**
     * @brief Connect to a peer
     *
     * If already connected or connecting, returns true.
     * If at connection limit, returns false.
     *
     * @param peer Peer info from PeerDiscovery
     * @return true if connection started or already connected
     */
    bool connectToPeer(const PeerInfo& peer);

    /**
     * @brief Disconnect from a peer
     *
     * @param uuid Peer UUID to disconnect
     */
    void disconnectPeer(const char* uuid);

    /**
     * @brief Disconnect from all peers
     */
    void disconnectAll();

    /**
     * @brief Send a message to a specific peer
     *
     * @param uuid Peer UUID
     * @param message Message to send
     * @return true if message was queued for sending
     */
    bool sendTo(const char* uuid, const char* message);

    /**
     * @brief Broadcast a message to all connected peers
     *
     * @param message Message to send
     * @return Number of peers message was sent to
     */
    uint8_t broadcast(const char* message);

    /**
     * @brief Get number of connected peers
     */
    uint8_t getConnectedCount() const;

    /**
     * @brief Get number of connection slots in use (connected + connecting)
     */
    uint8_t getActiveSlotCount() const;

    /**
     * @brief Check if connected to a specific peer
     */
    bool isConnectedTo(const char* uuid) const;

    /**
     * @brief Get list of connected peer UUIDs
     *
     * @param outUuids Array to fill with UUIDs
     * @param maxCount Maximum number of UUIDs to return
     * @return Number of UUIDs written
     */
    uint8_t getConnectedPeerUuids(char (*outUuids)[16], uint8_t maxCount) const;

    /**
     * @brief Register callback for received messages
     */
    void setMessageCallback(PeerMessageCallback callback) {
        m_messageCallback = callback;
    }

    /**
     * @brief Register callback for connection state changes
     */
    void setConnectionCallback(PeerConnectionCallback callback) {
        m_connectionCallback = callback;
    }

private:
    /**
     * @brief Find connection slot by UUID
     * @return Pointer to slot or nullptr
     */
    PeerConnection* findSlot(const char* uuid);
    const PeerConnection* findSlot(const char* uuid) const;

    /**
     * @brief Find an empty connection slot
     * @return Pointer to empty slot or nullptr
     */
    PeerConnection* findEmptySlot();

    /**
     * @brief Handle WebSocket connection event
     */
    void onConnect(PeerConnection* conn);

    /**
     * @brief Handle WebSocket disconnect event
     */
    void onDisconnect(PeerConnection* conn);

    /**
     * @brief Handle received WebSocket message
     */
    void onMessage(PeerConnection* conn, const char* message, size_t length);

    /**
     * @brief Send heartbeat pings to connected peers
     */
    void sendHeartbeats(uint32_t nowMs);

    /**
     * @brief Check for missed heartbeats and disconnect stale peers
     */
    void checkHeartbeats(uint32_t nowMs);

    /**
     * @brief Attempt to reconnect disconnected slots
     */
    void attemptReconnects(uint32_t nowMs);

    PeerConnection m_connections[MAX_PEER_CONNECTIONS];
    PeerMessageCallback m_messageCallback;
    PeerConnectionCallback m_connectionCallback;
    bool m_initialized;
};

} // namespace sync
} // namespace lightwaveos
