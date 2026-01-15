/**
 * @file LeaderElection.cpp
 * @brief Deterministic leader election implementation
 */

#include "LeaderElection.h"
#include <cstring>

namespace lightwaveos {
namespace sync {

LeaderElection::LeaderElection()
    : m_role(SyncRole::UNKNOWN)
    , m_leaderUuid{0}
{
}

SyncRole LeaderElection::evaluate(const char* const* connectedPeerUuids, uint8_t peerCount) {
    // Get own UUID
    const char* ownUuid = DEVICE_UUID.toString();

    // If no peers connected, we're the leader by default
    if (peerCount == 0 || !connectedPeerUuids) {
        m_role = SyncRole::LEADER;
        strncpy(m_leaderUuid, ownUuid, sizeof(m_leaderUuid) - 1);
        m_leaderUuid[sizeof(m_leaderUuid) - 1] = '\0';
        return m_role;
    }

    // Check if any connected peer has a higher UUID
    bool anyHigher = false;
    const char* highestPeerUuid = nullptr;

    for (uint8_t i = 0; i < peerCount; i++) {
        if (!connectedPeerUuids[i]) continue;

        if (!isHigherThan(connectedPeerUuids[i])) {
            // This peer has a higher (or equal) UUID
            anyHigher = true;

            // Track the highest peer UUID
            if (!highestPeerUuid ||
                strcmp(connectedPeerUuids[i], highestPeerUuid) > 0) {
                highestPeerUuid = connectedPeerUuids[i];
            }
        }
    }

    if (anyHigher) {
        // A connected peer has higher UUID - they're the leader
        m_role = SyncRole::FOLLOWER;
        if (highestPeerUuid) {
            strncpy(m_leaderUuid, highestPeerUuid, sizeof(m_leaderUuid) - 1);
            m_leaderUuid[sizeof(m_leaderUuid) - 1] = '\0';
        }
    } else {
        // No connected peer has higher UUID - we're the leader
        m_role = SyncRole::LEADER;
        strncpy(m_leaderUuid, ownUuid, sizeof(m_leaderUuid) - 1);
        m_leaderUuid[sizeof(m_leaderUuid) - 1] = '\0';
    }

    return m_role;
}

SyncRole LeaderElection::evaluate(const char (*connectedPeerUuids)[16], uint8_t peerCount) {
    // Convert 2D array to array of pointers
    const char* ptrs[MAX_PEER_CONNECTIONS];
    for (uint8_t i = 0; i < peerCount && i < MAX_PEER_CONNECTIONS; i++) {
        ptrs[i] = connectedPeerUuids[i];
    }
    return evaluate(ptrs, peerCount);
}

bool LeaderElection::isHigherThan(const char* peerUuid) const {
    return DEVICE_UUID.isHigherThan(peerUuid);
}

bool LeaderElection::findHighestUuid(const char* const* uuids, uint8_t count, char* outHighest) const {
    if (!uuids || count == 0 || !outHighest) return false;

    const char* highest = nullptr;
    for (uint8_t i = 0; i < count; i++) {
        if (!uuids[i]) continue;

        if (!highest || strcmp(uuids[i], highest) > 0) {
            highest = uuids[i];
        }
    }

    if (highest) {
        strncpy(outHighest, highest, 15);
        outHighest[15] = '\0';
        return true;
    }

    return false;
}

} // namespace sync
} // namespace lightwaveos
