// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file SyncProtocol.h
 * @brief Multi-device sync protocol constants and types
 *
 * This file defines the protocol for synchronizing state across multiple
 * LightwaveOS devices using CQRS command replay over WebSocket.
 *
 * Protocol Overview:
 * 1. Device discovers peers via mDNS (_ws._tcp)
 * 2. Connects to discovered peers as WebSocket client
 * 3. Leader election: highest UUID wins (Bully algorithm)
 * 4. Leader broadcasts state changes to all followers
 * 5. Followers apply received commands/states
 *
 * Message Format:
 * {
 *   "t": "sync.<type>",      // Message type
 *   "v": 12345,              // State version for ordering
 *   "ts": 98765432,          // Timestamp (millis)
 *   "u": "LW-AABBCCDDEEFF",  // Sender UUID
 *   "p": { ... }             // Payload (type-specific)
 * }
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include <cstdint>

namespace lightwaveos {
namespace sync {

// ============================================================================
// Protocol Version
// ============================================================================

constexpr uint8_t SYNC_PROTOCOL_VERSION = 1;

// ============================================================================
// Timing Constants
// ============================================================================

/**
 * @brief mDNS peer discovery interval (milliseconds)
 */
constexpr uint32_t PEER_SCAN_INTERVAL_MS = 30000;  // 30 seconds

/**
 * @brief Time until a peer is considered stale (milliseconds)
 */
constexpr uint32_t PEER_TIMEOUT_MS = 90000;  // 90 seconds

/**
 * @brief Heartbeat ping interval (milliseconds)
 */
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 10000;  // 10 seconds

/**
 * @brief Missed heartbeats before disconnect
 */
constexpr uint8_t HEARTBEAT_MISS_LIMIT = 3;

/**
 * @brief Initial reconnect delay (milliseconds)
 */
constexpr uint32_t RECONNECT_INITIAL_MS = 1000;  // 1 second

/**
 * @brief Maximum reconnect delay (milliseconds)
 */
constexpr uint32_t RECONNECT_MAX_MS = 16000;  // 16 seconds

/**
 * @brief Version divergence threshold for full resync
 *
 * If local and remote versions differ by more than this, trigger full resync.
 */
constexpr uint32_t VERSION_DIVERGENCE_THRESHOLD = 100;

// ============================================================================
// Capacity Limits
// ============================================================================

/**
 * @brief Maximum discovered peers
 */
constexpr uint8_t MAX_DISCOVERED_PEERS = 8;

/**
 * @brief Maximum concurrent WebSocket client connections
 *
 * ESP32 has limited SSL/TCP resources, 4 is a safe limit.
 */
constexpr uint8_t MAX_PEER_CONNECTIONS = 4;

/**
 * @brief Maximum message size (bytes)
 *
 * Full state is ~450 bytes, keep some headroom.
 */
constexpr uint16_t MAX_MESSAGE_SIZE = 1024;

// ============================================================================
// Sync Roles
// ============================================================================

/**
 * @brief Device role in the sync cluster
 */
enum class SyncRole : uint8_t {
    UNKNOWN = 0,    // Role not yet determined
    LEADER,         // Broadcasts state to followers
    FOLLOWER        // Receives state from leader
};

/**
 * @brief Get string name for SyncRole
 */
inline const char* syncRoleToString(SyncRole role) {
    switch (role) {
        case SyncRole::UNKNOWN:  return "UNKNOWN";
        case SyncRole::LEADER:   return "LEADER";
        case SyncRole::FOLLOWER: return "FOLLOWER";
        default:                 return "INVALID";
    }
}

// ============================================================================
// Sync States (SyncManagerActor state machine)
// ============================================================================

/**
 * @brief Sync manager state machine states
 */
enum class SyncState : uint8_t {
    INITIALIZING = 0,   // Startup, reading UUID
    DISCOVERING,        // Scanning for peers via mDNS
    ELECTING,           // Determining leader (automatic via UUID)
    LEADING,            // This device is the leader
    FOLLOWING,          // This device is a follower
    SYNCHRONIZED,       // Steady state (leading or following)
    RECONNECTING,       // Lost connection, attempting to reconnect
    ERROR               // Unrecoverable error
};

/**
 * @brief Get string name for SyncState
 */
inline const char* syncStateToString(SyncState state) {
    switch (state) {
        case SyncState::INITIALIZING: return "INITIALIZING";
        case SyncState::DISCOVERING:  return "DISCOVERING";
        case SyncState::ELECTING:     return "ELECTING";
        case SyncState::LEADING:      return "LEADING";
        case SyncState::FOLLOWING:    return "FOLLOWING";
        case SyncState::SYNCHRONIZED: return "SYNCHRONIZED";
        case SyncState::RECONNECTING: return "RECONNECTING";
        case SyncState::ERROR:        return "ERROR";
        default:                      return "INVALID";
    }
}

// ============================================================================
// Message Types (JSON "t" field)
// ============================================================================

// Message type prefix
constexpr const char* SYNC_MSG_PREFIX = "sync.";

// Individual message type strings
constexpr const char* SYNC_MSG_HELLO = "sync.hello";   // Handshake
constexpr const char* SYNC_MSG_STATE = "sync.state";   // Full state snapshot
constexpr const char* SYNC_MSG_CMD   = "sync.cmd";     // Single command
constexpr const char* SYNC_MSG_PING  = "sync.ping";    // Heartbeat request
constexpr const char* SYNC_MSG_PONG  = "sync.pong";    // Heartbeat response
constexpr const char* SYNC_MSG_BYE   = "sync.bye";     // Graceful disconnect

// ============================================================================
// Conflict Resolution
// ============================================================================

/**
 * @brief Result of conflict resolution
 */
enum class ConflictResult : uint8_t {
    ACCEPT_LOCAL,   // Keep local state
    ACCEPT_REMOTE,  // Apply remote state
    RESYNC_NEEDED   // Versions too divergent, need full sync
};

// ============================================================================
// Peer Info Structure
// ============================================================================

/**
 * @brief Information about a discovered peer
 *
 * Populated by mDNS discovery, used for connection management.
 */
struct PeerInfo {
    char uuid[16];          // "LW-AABBCCDDEEFF\0"
    char hostname[32];      // mDNS hostname
    uint8_t ip[4];          // IPv4 address
    uint16_t port;          // WebSocket port
    uint32_t lastSeenMs;    // Last activity timestamp
    SyncRole role;          // LEADER, FOLLOWER, or UNKNOWN
    bool connected;         // Currently connected as WS client

    PeerInfo()
        : uuid{0}
        , hostname{0}
        , ip{0}
        , port(80)
        , lastSeenMs(0)
        , role(SyncRole::UNKNOWN)
        , connected(false)
    {}

    /**
     * @brief Check if peer is stale (no recent activity)
     */
    bool isStale(uint32_t nowMs) const {
        return (nowMs - lastSeenMs) > PEER_TIMEOUT_MS;
    }

    /**
     * @brief Update last seen timestamp
     */
    void touch(uint32_t nowMs) {
        lastSeenMs = nowMs;
    }
};

// ============================================================================
// mDNS Service Details
// ============================================================================

/**
 * @brief mDNS service type for peer discovery
 */
constexpr const char* MDNS_SERVICE_TYPE = "_ws";
constexpr const char* MDNS_SERVICE_PROTO = "_tcp";

/**
 * @brief TXT record key for board type filtering
 */
constexpr const char* MDNS_TXT_BOARD = "board";
constexpr const char* MDNS_TXT_BOARD_VALUE = "ESP32-S3";

/**
 * @brief TXT record key for device UUID
 */
constexpr const char* MDNS_TXT_UUID = "uuid";

/**
 * @brief TXT record key for sync protocol version
 */
constexpr const char* MDNS_TXT_SYNC_VERSION = "syncver";

} // namespace sync
} // namespace lightwaveos
