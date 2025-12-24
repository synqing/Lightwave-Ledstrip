#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include <atomic>
#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @brief Lock-free double-buffer for single-writer/multi-reader cross-core data
 *
 * Writer (audio core) calls Publish() to swap buffers.
 * Readers (render core) call ReadLatest() to get a copy.
 */
template <typename T>
class SnapshotBuffer {
public:
    SnapshotBuffer() = default;

    /**
     * @brief Publish a new snapshot (writer side)
     * Atomically swaps buffers and increments sequence number.
     */
    void Publish(const T& snapshot) {
        uint32_t back = 1 - m_frontIndex.load(std::memory_order_acquire);
        m_buffer[back] = snapshot;
        m_frontIndex.store(back, std::memory_order_release);
        m_sequence.fetch_add(1, std::memory_order_release);
    }

    /**
     * @brief Read the latest snapshot (reader side)
     * @param out Destination for the snapshot copy
     * @return Sequence number (increments on each Publish)
     */
    uint32_t ReadLatest(T& out) const {
        uint32_t front = m_frontIndex.load(std::memory_order_acquire);
        out = m_buffer[front];
        return m_sequence.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current sequence number without copying data
     */
    uint32_t GetSequence() const {
        return m_sequence.load(std::memory_order_acquire);
    }

private:
    T m_buffer[2];
    std::atomic<uint32_t> m_frontIndex{0};
    std::atomic<uint32_t> m_sequence{0};
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
