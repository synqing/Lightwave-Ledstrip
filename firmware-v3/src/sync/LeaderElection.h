// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LeaderElection.h
 * @brief Deterministic leader election using Bully algorithm
 *
 * Leader election for multi-device synchronization. Uses a deterministic
 * approach where the device with the highest UUID is always the leader.
 *
 * Key Properties:
 * - **Deterministic**: No network communication needed for election
 * - **Partition-tolerant**: Each network partition elects its own leader
 * - **Stable**: Leader only changes when it disconnects
 * - **Fast**: Instant election on connection changes
 *
 * Algorithm (Bully):
 * 1. Each device knows its own UUID and connected peer UUIDs
 * 2. Compare own UUID against all connected peers
 * 3. If own UUID is highest → become LEADER
 * 4. Otherwise → become FOLLOWER
 *
 * This is simpler than classic Bully because:
 * - UUIDs are based on MAC addresses (globally unique)
 * - Comparison is deterministic (no timeouts or voting)
 * - No message exchange needed (just peer list)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "SyncProtocol.h"
#include "DeviceUUID.h"
#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Leader election manager
 *
 * Computes the current device's role based on connected peers.
 * Thread-safe: evaluate() can be called from any context.
 */
class LeaderElection {
public:
    LeaderElection();
    ~LeaderElection() = default;

    // Prevent copying
    LeaderElection(const LeaderElection&) = delete;
    LeaderElection& operator=(const LeaderElection&) = delete;

    /**
     * @brief Evaluate role based on connected peers
     *
     * Computes whether this device should be LEADER or FOLLOWER
     * based on UUID comparison with connected peers.
     *
     * @param connectedPeerUuids Array of connected peer UUID strings
     * @param peerCount Number of connected peers
     * @return LEADER if highest UUID, FOLLOWER otherwise
     */
    SyncRole evaluate(const char* const* connectedPeerUuids, uint8_t peerCount);

    /**
     * @brief Evaluate role using 2D array of UUIDs
     *
     * Convenience overload for fixed-size UUID arrays.
     *
     * @param connectedPeerUuids Array of UUID strings [16] each
     * @param peerCount Number of connected peers
     * @return LEADER if highest UUID, FOLLOWER otherwise
     */
    SyncRole evaluate(const char (*connectedPeerUuids)[16], uint8_t peerCount);

    /**
     * @brief Get current role
     *
     * Returns the result of the last evaluate() call.
     *
     * @return Current role (UNKNOWN if never evaluated)
     */
    SyncRole getRole() const { return m_role; }

    /**
     * @brief Check if this device is the leader
     */
    bool isLeader() const { return m_role == SyncRole::LEADER; }

    /**
     * @brief Check if this device is a follower
     */
    bool isFollower() const { return m_role == SyncRole::FOLLOWER; }

    /**
     * @brief Check if role has been determined
     */
    bool isRoleDetermined() const { return m_role != SyncRole::UNKNOWN; }

    /**
     * @brief Get the leader UUID (if known)
     *
     * Returns the UUID of the current leader. If this device is the
     * leader, returns own UUID. Otherwise, returns the highest UUID
     * among connected peers.
     *
     * @return Leader UUID string or nullptr if role is UNKNOWN
     */
    const char* getLeaderUuid() const {
        return m_leaderUuid[0] != '\0' ? m_leaderUuid : nullptr;
    }

    /**
     * @brief Force re-evaluation on next call
     *
     * Clears cached role, useful when peer list might have changed.
     */
    void invalidate() { m_role = SyncRole::UNKNOWN; }

private:
    /**
     * @brief Compare own UUID against a single peer
     * @return true if own UUID is higher than peer
     */
    bool isHigherThan(const char* peerUuid) const;

    /**
     * @brief Find the highest UUID among a set
     *
     * @param uuids Array of UUID strings
     * @param count Number of UUIDs
     * @param outHighest Output: highest UUID found
     * @return true if a highest was found
     */
    bool findHighestUuid(const char* const* uuids, uint8_t count, char* outHighest) const;

    SyncRole m_role;            // Current computed role
    char m_leaderUuid[16];      // UUID of current leader
};

} // namespace sync
} // namespace lightwaveos
