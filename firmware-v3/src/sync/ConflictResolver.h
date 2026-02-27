// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ConflictResolver.h
 * @brief Version-based conflict resolution for multi-device sync
 *
 * Resolves conflicts when multiple devices modify state simultaneously.
 * Uses a version-based ordering with last-write-wins semantics.
 *
 * Resolution Rules:
 * 1. Higher version number always wins
 * 2. Same version → leader's state wins
 * 3. Versions diverge >100 → full resync required
 *
 * Version Wrapping:
 * - Versions are uint32_t, will wrap at ~4 billion
 * - If local=0xFFFFFF00 and remote=0x00000100, assumes wrap occurred
 * - Wrap detection uses VERSION_WRAP_THRESHOLD
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "SyncProtocol.h"
#include <cstdint>

namespace lightwaveos {
namespace sync {

/**
 * @brief Threshold for detecting version wrap-around
 *
 * If versions differ by more than this, assume wrap occurred.
 */
constexpr uint32_t VERSION_WRAP_THRESHOLD = 0x80000000;

/**
 * @brief Conflict resolution decisions
 */
struct ConflictDecision {
    ConflictResult result;      // What to do
    const char* reason;         // Human-readable reason

    ConflictDecision(ConflictResult r, const char* msg)
        : result(r), reason(msg) {}
};

/**
 * @brief Conflict resolver for multi-device sync
 */
class ConflictResolver {
public:
    ConflictResolver();
    ~ConflictResolver() = default;

    /**
     * @brief Resolve a command conflict
     *
     * Called when a remote command is received to determine if it
     * should be applied or rejected.
     *
     * @param localVersion Current local state version
     * @param remoteVersion Version from remote command
     * @param isFromLeader Whether the remote sender is the leader
     * @return Conflict decision
     */
    ConflictDecision resolveCommand(
        uint32_t localVersion,
        uint32_t remoteVersion,
        bool isFromLeader
    ) const;

    /**
     * @brief Resolve a full state sync conflict
     *
     * Called when a remote full state is received.
     *
     * @param localVersion Current local state version
     * @param remoteVersion Version from remote state
     * @param isFromLeader Whether the remote sender is the leader
     * @return Conflict decision
     */
    ConflictDecision resolveState(
        uint32_t localVersion,
        uint32_t remoteVersion,
        bool isFromLeader
    ) const;

    /**
     * @brief Check if versions are too divergent
     *
     * Versions diverging by more than VERSION_DIVERGENCE_THRESHOLD
     * indicate a full resync is needed.
     *
     * @param v1 First version
     * @param v2 Second version
     * @return true if divergence exceeds threshold
     */
    bool isVersionDivergent(uint32_t v1, uint32_t v2) const;

    /**
     * @brief Compare two versions with wrap-around handling
     *
     * Handles the case where versions have wrapped around the
     * uint32_t boundary.
     *
     * @param v1 First version
     * @param v2 Second version
     * @return negative if v1<v2, 0 if equal, positive if v1>v2
     */
    static int compareVersions(uint32_t v1, uint32_t v2);

    /**
     * @brief Calculate version distance with wrap handling
     *
     * @param v1 First version
     * @param v2 Second version
     * @return Absolute distance between versions
     */
    static uint32_t versionDistance(uint32_t v1, uint32_t v2);
};

} // namespace sync
} // namespace lightwaveos
