/**
 * @file K1DebugRing.h
 * @brief Lock-Free Ring Buffer for K1 Debug Samples
 *
 * Thin wrapper around LockFreeQueue specialized for K1 debug capture.
 * Used for cross-core transfer of debug samples from AudioActor (Core 0)
 * to RendererActor/WebSocket (Core 1).
 *
 * Memory: 32 samples × 64 bytes = 2KB
 *
 * Usage:
 *   Producer (AudioActor):
 *     K1DebugSample sample = ...;
 *     ring.push(sample);  // Non-blocking, drops if full
 *
 *   Consumer (RendererActor):
 *     K1DebugSample sample;
 *     while (ring.pop(sample)) {
 *         // Process sample
 *     }
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_K1_DEBUG

#include "../../utils/LockFreeQueue.h"
#include "K1DebugMetrics.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// Configuration
// ============================================================================

/**
 * Ring buffer capacity (samples, not bytes).
 * 32 samples at 10 Hz = 3.2 seconds of debug history.
 * Memory: 32 × 64 = 2048 bytes + ~24 bytes overhead = ~2KB
 */
constexpr size_t K1_DEBUG_RING_CAPACITY = 32;

// ============================================================================
// K1DebugRing - SPSC Queue for Debug Samples
// ============================================================================

using K1DebugRing = ::lightwaveos::utils::LockFreeQueue<K1DebugSample, K1_DEBUG_RING_CAPACITY>;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Drain ring buffer and return the most recent sample
 *
 * Useful when you only care about the latest state, not history.
 * Empties the buffer in the process.
 *
 * @param ring The debug ring to drain
 * @param out Output for most recent sample
 * @return true if at least one sample was available
 */
inline bool k1_debug_ring_drain_latest(K1DebugRing& ring, K1DebugSample& out) {
    K1DebugSample temp;
    bool found = false;

    while (ring.pop(temp)) {
        out = temp;
        found = true;
    }

    return found;
}

/**
 * @brief Count samples in ring (approximate, for debugging)
 */
inline size_t k1_debug_ring_count(const K1DebugRing& ring) {
    return ring.size();
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_K1_DEBUG
