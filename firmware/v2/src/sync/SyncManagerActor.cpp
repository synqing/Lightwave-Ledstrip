/**
 * @file SyncManagerActor.cpp
 * @brief Multi-device sync orchestration implementation
 */

#include "SyncManagerActor.h"
#include <cstring>

#ifdef NATIVE_BUILD
#define millis() 0
#else
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace sync {

// Static instance for callbacks
SyncManagerActor* SyncManagerActor::s_instance = nullptr;

SyncManagerActor::SyncManagerActor(state::StateStore& stateStore)
    : Actor(actors::ActorConfigs::SyncManager())
    , m_stateStore(stateStore)
    , m_syncState(SyncState::INITIALIZING)
    , m_stateEnterTime(0)
    , m_lastBroadcastVersion(0)
    , m_lastDiscoveryMs(0)
    , m_pendingStateSync(false)
    , m_msgBuffer{0}
{
    s_instance = this;
}

SyncManagerActor::~SyncManagerActor() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

// ============================================================================
// Actor Lifecycle
// ============================================================================

void SyncManagerActor::onStart() {
    // Initialize device UUID (reads MAC address)
    DEVICE_UUID.toString();

    // Initialize components
    m_discovery.begin();
    m_peerManager.begin();

    // Set up callbacks
    m_discovery.setCallback(onPeerDiscovered);
    m_peerManager.setConnectionCallback(onPeerConnectionChanged);
    m_peerManager.setMessageCallback(onPeerMessage);

    // Subscribe to state changes (use static callback wrapper)
    m_stateStore.subscribe(onStateChanged);

    // Start state machine
    transitionTo(SyncState::DISCOVERING);
}

void SyncManagerActor::onMessage(const actors::Message& msg) {
    using actors::MessageType;

    switch (msg.type) {
        case MessageType::STATE_UPDATED:
            handleStateUpdated(msg);
            break;

        case MessageType::SYNC_REQUEST:
            // param1 contains index into a peer UUID lookup (simplified)
            // In practice, this would carry more context
            handleSyncRequest(nullptr);
            break;

        case MessageType::SYNC_STATE:
            // Full state sync message - handled via handleIncomingMessage
            break;

        case MessageType::SYNC_RESPONSE:
            // Response to our sync request
            break;

        case MessageType::PING:
            // Send pong back
            break;

        case MessageType::PONG:
            // Heartbeat received
            break;

        default:
            // Ignore unknown messages
            break;
    }
}

void SyncManagerActor::onTick() {
    uint32_t now = millis();

    // Update peer manager (heartbeats, reconnects)
    m_peerManager.update(now);

    // Update discovery (remove stale peers)
    m_discovery.update(now);

    // Handle state machine
    handleStateTick();
}

void SyncManagerActor::onStop() {
    m_peerManager.disconnectAll();
}

// ============================================================================
// State Machine
// ============================================================================

void SyncManagerActor::transitionTo(SyncState newState) {
    if (newState == m_syncState) return;

    m_syncState = newState;
    m_stateEnterTime = millis();

    // State entry actions
    switch (newState) {
        case SyncState::DISCOVERING:
            // Trigger initial scan
            m_discovery.scan();
            break;

        case SyncState::LEADING:
            // Broadcast our state to all followers
            m_pendingStateSync = true;
            break;

        case SyncState::FOLLOWING:
            // Request state from leader
            // (handled by leader sending on connect)
            break;

        default:
            break;
    }
}

void SyncManagerActor::handleStateTick() {
    uint32_t now = millis();

    switch (m_syncState) {
        case SyncState::INITIALIZING:
            handleInitializing();
            break;

        case SyncState::DISCOVERING:
            handleDiscovering();
            break;

        case SyncState::ELECTING:
            handleElecting();
            break;

        case SyncState::LEADING:
            handleLeading();
            break;

        case SyncState::FOLLOWING:
            handleFollowing();
            break;

        case SyncState::RECONNECTING:
            handleReconnecting();
            break;

        default:
            break;
    }

    // Periodic discovery scan
    if (now - m_lastDiscoveryMs >= PEER_SCAN_INTERVAL_MS) {
        m_discovery.scan();
        m_lastDiscoveryMs = now;
    }
}

void SyncManagerActor::handleInitializing() {
    // Should have transitioned in onStart()
    transitionTo(SyncState::DISCOVERING);
}

void SyncManagerActor::handleDiscovering() {
    // Wait for discovery to complete and try connecting to peers
    if (m_discovery.peerCount() > 0) {
        // Try connecting to discovered peers
        const PeerInfo* peers = m_discovery.getPeers();
        for (uint8_t i = 0; i < m_discovery.peerCount(); i++) {
            if (!peers[i].connected) {
                m_peerManager.connectToPeer(peers[i]);
            }
        }
    }

    // Transition to electing when we have any connections or timeout
    uint32_t elapsed = millis() - m_stateEnterTime;
    if (m_peerManager.getConnectedCount() > 0 || elapsed > 5000) {
        transitionTo(SyncState::ELECTING);
    }
}

void SyncManagerActor::handleElecting() {
    // Get connected peer UUIDs
    char connectedUuids[MAX_PEER_CONNECTIONS][16];
    uint8_t count = m_peerManager.getConnectedPeerUuids(connectedUuids, MAX_PEER_CONNECTIONS);

    // Evaluate our role
    const char* uuidPtrs[MAX_PEER_CONNECTIONS];
    for (uint8_t i = 0; i < count; i++) {
        uuidPtrs[i] = connectedUuids[i];
    }

    SyncRole role = m_election.evaluate(uuidPtrs, count);

    // Transition based on role
    if (role == SyncRole::LEADER) {
        transitionTo(SyncState::LEADING);
    } else {
        transitionTo(SyncState::FOLLOWING);
    }
}

void SyncManagerActor::handleLeading() {
    // As leader, broadcast state changes to followers

    if (m_pendingStateSync) {
        broadcastState();
        m_pendingStateSync = false;
    }

    // Check if we're still the leader
    char connectedUuids[MAX_PEER_CONNECTIONS][16];
    uint8_t count = m_peerManager.getConnectedPeerUuids(connectedUuids, MAX_PEER_CONNECTIONS);

    const char* uuidPtrs[MAX_PEER_CONNECTIONS];
    for (uint8_t i = 0; i < count; i++) {
        uuidPtrs[i] = connectedUuids[i];
    }

    SyncRole role = m_election.evaluate(uuidPtrs, count);
    if (role != SyncRole::LEADER) {
        transitionTo(SyncState::FOLLOWING);
    }

    // Transition to SYNCHRONIZED after initial broadcast
    if (m_syncState == SyncState::LEADING && !m_pendingStateSync) {
        transitionTo(SyncState::SYNCHRONIZED);
    }
}

void SyncManagerActor::handleFollowing() {
    // As follower, apply received state/commands

    // Check if leadership has changed
    char connectedUuids[MAX_PEER_CONNECTIONS][16];
    uint8_t count = m_peerManager.getConnectedPeerUuids(connectedUuids, MAX_PEER_CONNECTIONS);

    const char* uuidPtrs[MAX_PEER_CONNECTIONS];
    for (uint8_t i = 0; i < count; i++) {
        uuidPtrs[i] = connectedUuids[i];
    }

    SyncRole role = m_election.evaluate(uuidPtrs, count);
    if (role == SyncRole::LEADER) {
        transitionTo(SyncState::LEADING);
    }
}

void SyncManagerActor::handleReconnecting() {
    // Try to reconnect to peers
    const PeerInfo* peers = m_discovery.getPeers();
    for (uint8_t i = 0; i < m_discovery.peerCount(); i++) {
        if (!peers[i].connected) {
            m_peerManager.connectToPeer(peers[i]);
        }
    }

    // Transition back to electing when connected
    if (m_peerManager.getConnectedCount() > 0) {
        transitionTo(SyncState::ELECTING);
    }
}

// ============================================================================
// Message Handlers
// ============================================================================

void SyncManagerActor::handleStateUpdated(const actors::Message& msg) {
    (void)msg;

    if (m_election.isLeader()) {
        // Leader broadcasts state changes
        m_pendingStateSync = true;
    }
}

void SyncManagerActor::handleIncomingMessage(
    const char* senderUuid,
    const char* message,
    size_t length
) {
    if (!message || length == 0) return;

    // Determine message type
    if (strstr(message, "sync.state")) {
        handleRemoteState(message, length);
    } else if (strstr(message, "sync.cmd")) {
        handleRemoteCommand(message, length);
    } else if (strstr(message, "sync.hello")) {
        handleHello(message, length);
    } else if (strstr(message, "sync.ping")) {
        handlePing(senderUuid);
    } else if (strstr(message, "sync.pong")) {
        handlePong(senderUuid);
    }

    // Update peer last seen time
    m_discovery.touchPeer(senderUuid, millis());
}

void SyncManagerActor::handleSyncRequest(const char* senderUuid) {
    (void)senderUuid;
    // Send full state to requesting peer
    broadcastState();
}

void SyncManagerActor::handleRemoteState(const char* json, size_t length) {
    state::SystemState remoteState;
    if (!StateSerializer::parse(json, length, remoteState)) {
        return; // Parse failed
    }

    char senderUuid[16];
    StateSerializer::extractSenderUuid(json, length, senderUuid);

    // Check if sender is leader
    bool isFromLeader = (strcmp(senderUuid, m_election.getLeaderUuid()) == 0);

    // Resolve conflict
    uint32_t localVersion = m_stateStore.getVersion();
    ConflictDecision decision = m_resolver.resolveState(
        localVersion,
        remoteState.version,
        isFromLeader
    );

    if (decision.result == ConflictResult::ACCEPT_REMOTE) {
        applyRemoteState(remoteState);
    }
    // Otherwise, keep local state
}

void SyncManagerActor::handleRemoteCommand(const char* json, size_t length) {
    ParsedCommand cmd = CommandSerializer::parse(json, length);
    if (!cmd.valid) {
        return; // Parse failed
    }

    // Check if sender is leader
    bool isFromLeader = (strcmp(cmd.senderUuid, m_election.getLeaderUuid()) == 0);

    // Resolve conflict
    uint32_t localVersion = m_stateStore.getVersion();
    ConflictDecision decision = m_resolver.resolveCommand(
        localVersion,
        cmd.version,
        isFromLeader
    );

    if (decision.result == ConflictResult::ACCEPT_REMOTE) {
        applyRemoteCommand(cmd);
    }
}

void SyncManagerActor::handleHello(const char* json, size_t length) {
    (void)json;
    (void)length;
    // Peer is announcing itself
    // We'll receive full state from them if they're the leader
}

void SyncManagerActor::handlePing(const char* senderUuid) {
    // Respond with pong
    size_t len = snprintf(m_msgBuffer, sizeof(m_msgBuffer),
        "{\"t\":\"sync.pong\",\"u\":\"%s\"}",
        DEVICE_UUID.toString()
    );
    m_peerManager.sendTo(senderUuid, m_msgBuffer);
    (void)len;
}

void SyncManagerActor::handlePong(const char* senderUuid) {
    // Heartbeat received - peer is alive
    m_discovery.touchPeer(senderUuid, millis());
}

// ============================================================================
// Broadcasting (Leader)
// ============================================================================

void SyncManagerActor::broadcastState() {
    const state::SystemState& state = m_stateStore.getState();

    size_t len = StateSerializer::serialize(
        state,
        DEVICE_UUID.toString(),
        m_msgBuffer,
        sizeof(m_msgBuffer)
    );

    if (len > 0) {
        m_peerManager.broadcast(m_msgBuffer);
        m_lastBroadcastVersion = state.version;
    }
}

void SyncManagerActor::broadcastCommand(CommandType type, const void* params) {
    size_t len = CommandSerializer::serialize(
        type,
        m_stateStore.getVersion(),
        DEVICE_UUID.toString(),
        m_msgBuffer,
        sizeof(m_msgBuffer),
        params
    );

    if (len > 0) {
        m_peerManager.broadcast(m_msgBuffer);
    }
}

// ============================================================================
// Receiving (Follower)
// ============================================================================

void SyncManagerActor::applyRemoteState(const state::SystemState& remoteState) {
    // Apply the remote state to our state store
    // This requires direct state replacement (not via commands)

    // For now, we'll apply individual commands to reconstruct the state
    // A more efficient approach would be to add a replaceState() method

    // Apply global settings
    state::SetEffectCommand effCmd(remoteState.currentEffectId);
    m_stateStore.dispatch(effCmd);

    state::SetBrightnessCommand briCmd(remoteState.brightness);
    m_stateStore.dispatch(briCmd);

    state::SetSpeedCommand spdCmd(remoteState.speed);
    m_stateStore.dispatch(spdCmd);

    state::SetPaletteCommand palCmd(remoteState.currentPaletteId);
    m_stateStore.dispatch(palCmd);

    // Apply visual params
    state::SetVisualParamsCommand vpsCmd(
        remoteState.intensity,
        remoteState.saturation,
        remoteState.complexity,
        remoteState.variation
    );
    m_stateStore.dispatch(vpsCmd);

    // Apply zone mode
    state::SetZoneModeCommand zmmCmd(
        remoteState.zoneModeEnabled,
        remoteState.activeZoneCount
    );
    m_stateStore.dispatch(zmmCmd);

    // Apply zone settings
    for (uint8_t i = 0; i < state::MAX_ZONES; i++) {
        state::ZoneSetEffectCommand zefCmd(i, remoteState.zones[i].effectId);
        m_stateStore.dispatch(zefCmd);

        state::ZoneSetPaletteCommand zpaCmd(i, remoteState.zones[i].paletteId);
        m_stateStore.dispatch(zpaCmd);

        state::ZoneSetBrightnessCommand zbrCmd(i, remoteState.zones[i].brightness);
        m_stateStore.dispatch(zbrCmd);

        state::ZoneSetSpeedCommand zspCmd(i, remoteState.zones[i].speed);
        m_stateStore.dispatch(zspCmd);

        state::ZoneEnableCommand zenCmd(i, remoteState.zones[i].enabled);
        m_stateStore.dispatch(zenCmd);
    }
}

void SyncManagerActor::applyRemoteCommand(const ParsedCommand& cmd) {
    // Create and dispatch the command
    state::ICommand* command = CommandSerializer::createCommand(cmd);
    if (command) {
        m_stateStore.dispatch(*command);
        delete command;
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void SyncManagerActor::onPeerConnectionChanged(const char* uuid, bool connected) {
    if (!s_instance) return;

    if (connected) {
        // New peer connected - update discovery
        s_instance->m_discovery.setPeerConnected(uuid, true);

        // Re-evaluate leadership
        if (s_instance->m_syncState == SyncState::SYNCHRONIZED ||
            s_instance->m_syncState == SyncState::LEADING ||
            s_instance->m_syncState == SyncState::FOLLOWING) {
            s_instance->transitionTo(SyncState::ELECTING);
        }
    } else {
        // Peer disconnected
        s_instance->m_discovery.setPeerConnected(uuid, false);

        // Check if we need to reconnect
        if (s_instance->m_peerManager.getConnectedCount() == 0) {
            s_instance->transitionTo(SyncState::RECONNECTING);
        } else {
            // Re-evaluate leadership
            s_instance->transitionTo(SyncState::ELECTING);
        }
    }
}

void SyncManagerActor::onPeerMessage(const char* uuid, const char* message, size_t length) {
    if (!s_instance) return;
    s_instance->handleIncomingMessage(uuid, message, length);
}

void SyncManagerActor::onPeerDiscovered(const PeerInfo& peer, bool added) {
    if (!s_instance) return;

    if (added) {
        // Try to connect to new peer
        s_instance->m_peerManager.connectToPeer(peer);
    }
}

void SyncManagerActor::onStateChanged(const state::SystemState& newState) {
    if (!s_instance) return;
    s_instance->onStateStoreChanged(newState);
}

void SyncManagerActor::onStateStoreChanged(const state::SystemState& newState) {
    // State changed locally - if we're leader, broadcast
    if (m_election.isLeader() && newState.version > m_lastBroadcastVersion) {
        m_pendingStateSync = true;
    }
}

} // namespace sync
} // namespace lightwaveos
