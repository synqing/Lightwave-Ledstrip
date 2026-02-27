// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ConflictResolver.cpp
 * @brief Conflict resolution implementation
 */

#include "ConflictResolver.h"

namespace lightwaveos {
namespace sync {

ConflictResolver::ConflictResolver() {
}

int ConflictResolver::compareVersions(uint32_t v1, uint32_t v2) {
    // Handle version wrap-around
    // If versions are very far apart, assume wrap occurred

    if (v1 == v2) return 0;

    // Calculate the "distance" in both directions
    uint32_t forward = v2 - v1;   // Distance if v2 > v1
    uint32_t backward = v1 - v2;  // Distance if v1 > v2

    // The smaller distance is likely the correct direction
    // This handles wrap-around: if v1=0xFFFFFFF0 and v2=0x00000010,
    // forward=0x20, backward=0xFFFFFFE0, so forward wins

    if (forward <= VERSION_WRAP_THRESHOLD) {
        // v2 is ahead of v1 (v1 < v2)
        return -1;
    } else {
        // v1 is ahead of v2 (v1 > v2)
        return 1;
    }
}

uint32_t ConflictResolver::versionDistance(uint32_t v1, uint32_t v2) {
    if (v1 == v2) return 0;

    uint32_t forward = v2 - v1;
    uint32_t backward = v1 - v2;

    return (forward < backward) ? forward : backward;
}

bool ConflictResolver::isVersionDivergent(uint32_t v1, uint32_t v2) const {
    return versionDistance(v1, v2) > VERSION_DIVERGENCE_THRESHOLD;
}

ConflictDecision ConflictResolver::resolveCommand(
    uint32_t localVersion,
    uint32_t remoteVersion,
    bool isFromLeader
) const {
    // Check for excessive divergence first
    if (isVersionDivergent(localVersion, remoteVersion)) {
        return ConflictDecision(
            ConflictResult::RESYNC_NEEDED,
            "Versions too divergent, full resync required"
        );
    }

    // Compare versions
    int cmp = compareVersions(localVersion, remoteVersion);

    if (cmp < 0) {
        // Remote is ahead - accept the command
        return ConflictDecision(
            ConflictResult::ACCEPT_REMOTE,
            "Remote version is higher"
        );
    } else if (cmp > 0) {
        // Local is ahead - reject the command
        return ConflictDecision(
            ConflictResult::ACCEPT_LOCAL,
            "Local version is higher"
        );
    } else {
        // Same version - leader wins
        if (isFromLeader) {
            return ConflictDecision(
                ConflictResult::ACCEPT_REMOTE,
                "Same version, leader wins"
            );
        } else {
            return ConflictDecision(
                ConflictResult::ACCEPT_LOCAL,
                "Same version, local wins (sender not leader)"
            );
        }
    }
}

ConflictDecision ConflictResolver::resolveState(
    uint32_t localVersion,
    uint32_t remoteVersion,
    bool isFromLeader
) const {
    // For full state syncs, we generally accept from leader

    if (isVersionDivergent(localVersion, remoteVersion)) {
        // Even divergent versions should be accepted from leader
        // as this is likely a recovery scenario
        if (isFromLeader) {
            return ConflictDecision(
                ConflictResult::ACCEPT_REMOTE,
                "Accepting leader state for resync"
            );
        } else {
            return ConflictDecision(
                ConflictResult::RESYNC_NEEDED,
                "Versions divergent, waiting for leader state"
            );
        }
    }

    // Normal case: compare versions
    int cmp = compareVersions(localVersion, remoteVersion);

    if (cmp < 0) {
        // Remote is ahead - definitely accept
        return ConflictDecision(
            ConflictResult::ACCEPT_REMOTE,
            "Remote state is newer"
        );
    } else if (cmp > 0) {
        // Local is ahead - this is unusual for a state sync
        // Still accept from leader to maintain consistency
        if (isFromLeader) {
            return ConflictDecision(
                ConflictResult::ACCEPT_REMOTE,
                "Accepting leader state despite lower version"
            );
        } else {
            return ConflictDecision(
                ConflictResult::ACCEPT_LOCAL,
                "Local state is newer"
            );
        }
    } else {
        // Same version - already in sync
        return ConflictDecision(
            ConflictResult::ACCEPT_LOCAL,
            "Already synchronized"
        );
    }
}

} // namespace sync
} // namespace lightwaveos
