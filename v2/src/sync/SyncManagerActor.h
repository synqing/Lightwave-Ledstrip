/**
 * @file SyncManagerActor.h
 * @brief Multi-device sync orchestration actor
 *
 * The SyncManagerActor coordinates all sync operations:
 * - Peer discovery via mDNS
 * - WebSocket client connections to peers
 * - Leader election (highest UUID wins)
 * - State/command broadcast from leader
 * - State/command reception as follower
 * - Conflict resolution
 *
 * State Machine:
 * ```
 * INITIALIZING → DISCOVERING → ELECTING → LEADING/FOLLOWING → SYNCHRONIZED
 *                     ↑                            │
 *                     └── RECONNECTING ←───────────┘
 * ```
 *
 * Threading:
 * - Runs on Core 0 with network stack
 * - Receives messages from WebSocket (external)
 * - Receives messages from StateStore (local state changes)
 * - Sends messages to PeerManager for broadcasting
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../core/actors/Actor.h"
#include "../core/state/StateStore.h"
#include "SyncProtocol.h"
#include "DeviceUUID.h"
#include "PeerDiscovery.h"
#include "PeerManager.h"
#include "LeaderElection.h"
#include "CommandSerializer.h"
#include "StateSerializer.h"
#include "ConflictResolver.h"

namespace lightwaveos {
namespace sync {

// Forward declaration
class StateStore;

/**
 * @brief Sync manager actor for multi-device synchronization
 *
 * Orchestrates peer discovery, connection management, leader election,
 * and state synchronization across multiple LightwaveOS devices.
 */
class SyncManagerActor : public actors::Actor {
public:
    /**
     * @brief Construct the sync manager actor
     *
     * @param stateStore Reference to the global state store
     */
    explicit SyncManagerActor(state::StateStore& stateStore);

    ~SyncManagerActor() override;

    // ========================================================================
    // Public Interface (called from other actors/threads)
    // ========================================================================

    /**
     * @brief Get current sync state
     */
    SyncState getSyncState() const { return m_syncState; }

    /**
     * @brief Get current sync role
     */
    SyncRole getRole() const { return m_election.getRole(); }

    /**
     * @brief Check if this device is the leader
     */
    bool isLeader() const { return m_election.isLeader(); }

    /**
     * @brief Get number of connected peers
     */
    uint8_t getConnectedPeerCount() const { return m_peerManager.getConnectedCount(); }

    /**
     * @brief Get number of discovered peers
     */
    uint8_t getDiscoveredPeerCount() const { return m_discovery.peerCount(); }

    /**
     * @brief Handle an incoming WebSocket sync message
     *
     * Called from NetworkActor when a sync.* message is received.
     * Thread-safe via message passing.
     *
     * @param senderUuid UUID of the sender
     * @param message JSON message string
     * @param length Message length
     */
    void handleIncomingMessage(const char* senderUuid,
                               const char* message,
                               size_t length);

protected:
    // ========================================================================
    // Actor Lifecycle
    // ========================================================================

    void onStart() override;
    void onMessage(const actors::Message& msg) override;
    void onTick() override;
    void onStop() override;

private:
    // ========================================================================
    // State Machine
    // ========================================================================

    /**
     * @brief Transition to a new sync state
     */
    void transitionTo(SyncState newState);

    /**
     * @brief Handle state machine tick in current state
     */
    void handleStateTick();

    /**
     * @brief Handle INITIALIZING state
     */
    void handleInitializing();

    /**
     * @brief Handle DISCOVERING state
     */
    void handleDiscovering();

    /**
     * @brief Handle ELECTING state
     */
    void handleElecting();

    /**
     * @brief Handle LEADING state
     */
    void handleLeading();

    /**
     * @brief Handle FOLLOWING state
     */
    void handleFollowing();

    /**
     * @brief Handle RECONNECTING state
     */
    void handleReconnecting();

    // ========================================================================
    // Message Handlers
    // ========================================================================

    /**
     * @brief Handle STATE_UPDATED message (local state changed)
     */
    void handleStateUpdated(const actors::Message& msg);

    /**
     * @brief Handle SYNC_REQUEST message
     */
    void handleSyncRequest(const char* senderUuid);

    /**
     * @brief Handle sync.state message
     */
    void handleRemoteState(const char* json, size_t length);

    /**
     * @brief Handle sync.cmd message
     */
    void handleRemoteCommand(const char* json, size_t length);

    /**
     * @brief Handle sync.hello message
     */
    void handleHello(const char* json, size_t length);

    /**
     * @brief Handle sync.ping message
     */
    void handlePing(const char* senderUuid);

    /**
     * @brief Handle sync.pong message
     */
    void handlePong(const char* senderUuid);

    // ========================================================================
    // Broadcasting (Leader)
    // ========================================================================

    /**
     * @brief Broadcast current state to all peers
     */
    void broadcastState();

    /**
     * @brief Broadcast a command to all peers
     */
    void broadcastCommand(CommandType type, const void* params);

    // ========================================================================
    // Receiving (Follower)
    // ========================================================================

    /**
     * @brief Apply received state
     */
    void applyRemoteState(const state::SystemState& remoteState);

    /**
     * @brief Apply received command
     */
    void applyRemoteCommand(const ParsedCommand& cmd);

    // ========================================================================
    // Peer Callbacks
    // ========================================================================

    /**
     * @brief Called when peer connection state changes
     */
    static void onPeerConnectionChanged(const char* uuid, bool connected);

    /**
     * @brief Called when message received from peer
     */
    static void onPeerMessage(const char* uuid, const char* message, size_t length);

    /**
     * @brief Called when peer discovered/removed
     */
    static void onPeerDiscovered(const PeerInfo& peer, bool added);

    // ========================================================================
    // State Store Subscription
    // ========================================================================

    /**
     * @brief Callback for state store changes
     */
    void onStateStoreChanged(const state::SystemState& newState);

    // ========================================================================
    // Member Data
    // ========================================================================

    state::StateStore& m_stateStore;    // Reference to global state
    SyncState m_syncState;              // Current state machine state
    uint32_t m_stateEnterTime;          // When we entered current state

    // Sync components
    PeerDiscovery m_discovery;          // mDNS peer discovery
    PeerManager m_peerManager;          // WebSocket connections
    LeaderElection m_election;          // Leader election logic
    ConflictResolver m_resolver;        // Conflict resolution

    // State tracking
    uint32_t m_lastBroadcastVersion;    // Last state version we broadcast
    uint32_t m_lastDiscoveryMs;         // Last mDNS scan time
    bool m_pendingStateSync;            // Need to send full state

    // Serialization buffer
    char m_msgBuffer[MAX_MESSAGE_SIZE]; // Reusable message buffer

    // Static instance pointer for callbacks
    static SyncManagerActor* s_instance;
};

} // namespace sync
} // namespace lightwaveos
