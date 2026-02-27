/**
 * @file Sync.h
 * @brief Main include header for multi-device synchronization
 *
 * Include this single file to get access to all sync components.
 * The sync system requires FEATURE_MULTI_DEVICE to be enabled.
 *
 * Usage:
 * ```cpp
 * #if FEATURE_MULTI_DEVICE
 * #include "sync/Sync.h"
 *
 * // In setup:
 * lightwaveos::sync::SyncManagerActor syncActor(stateStore);
 * syncActor.start();
 * #endif
 * ```
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_MULTI_DEVICE

// Protocol and types
#include "SyncProtocol.h"
#include "CommandType.h"

// Core components
#include "DeviceUUID.h"
#include "PeerDiscovery.h"
#include "PeerManager.h"
#include "LeaderElection.h"

// Serialization
#include "CommandSerializer.h"
#include "StateSerializer.h"

// Conflict resolution
#include "ConflictResolver.h"

// Main orchestrator
#include "SyncManagerActor.h"

namespace lightwaveos {
namespace sync {

/**
 * @brief Get sync status summary as JSON
 *
 * Useful for debugging and API endpoints.
 *
 * @param syncActor Reference to the sync manager
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return Number of bytes written
 */
inline size_t getSyncStatusJson(
    const SyncManagerActor& syncActor,
    char* buffer,
    size_t bufferSize
) {
    return snprintf(buffer, bufferSize,
        "{\"enabled\":true,"
        "\"uuid\":\"%s\","
        "\"state\":\"%s\","
        "\"role\":\"%s\","
        "\"peers\":{\"discovered\":%u,\"connected\":%u},"
        "\"isLeader\":%s}",
        DEVICE_UUID.toString(),
        syncStateToString(syncActor.getSyncState()),
        syncRoleToString(syncActor.getRole()),
        syncActor.getDiscoveredPeerCount(),
        syncActor.getConnectedPeerCount(),
        syncActor.isLeader() ? "true" : "false"
    );
}

} // namespace sync
} // namespace lightwaveos

#else // FEATURE_MULTI_DEVICE not enabled

namespace lightwaveos {
namespace sync {

// Stub for when sync is disabled
inline size_t getSyncStatusJson(char* buffer, size_t bufferSize) {
    return snprintf(buffer, bufferSize, "{\"enabled\":false}");
}

} // namespace sync
} // namespace lightwaveos

#endif // FEATURE_MULTI_DEVICE
