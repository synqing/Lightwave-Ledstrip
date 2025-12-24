#pragma once
#include <atomic>
#include <stdint.h>

namespace lightwaveos::audio {

/**
 * @brief Lock-free double buffer (publish on one core, read on another).
 *
 * - Writer calls Publish(value). No dynamic allocation.
 * - Reader calls ReadLatest(out) and receives a BY-VALUE copy.
 * - Sequence increments on each publish; reader can detect staleness.
 */
template <typename T>
class SnapshotBuffer final {
public:
    SnapshotBuffer() = default;

    SnapshotBuffer(const SnapshotBuffer&) = delete;
    SnapshotBuffer& operator=(const SnapshotBuffer&) = delete;
    SnapshotBuffer(SnapshotBuffer&&) = delete;
    SnapshotBuffer& operator=(SnapshotBuffer&&) = delete;

    /**
     * @brief Publish a new snapshot (writer thread).
     */
    void Publish(const T& v) {
        // Write into the inactive buffer first.
        const uint32_t cur = m_active.load(std::memory_order_relaxed);
        const uint32_t nxt = cur ^ 1U;

        m_buf[nxt] = v;

        // Make sure payload write lands before we flip active + seq.
        std::atomic_thread_fence(std::memory_order_release);
        m_active.store(nxt, std::memory_order_release);
        m_seq.fetch_add(1U, std::memory_order_release);
    }

    /**
     * @brief Read latest snapshot by value (reader thread). Returns sequence id.
     *
     * If writer publishes during the copy, we retry once.
     */
    uint32_t ReadLatest(T& out) const {
        uint32_t s0 = m_seq.load(std::memory_order_acquire);
        uint32_t idx = m_active.load(std::memory_order_acquire);

        out = m_buf[idx];

        std::atomic_thread_fence(std::memory_order_acquire);
        uint32_t s1 = m_seq.load(std::memory_order_acquire);

        if (s1 != s0) {
            // One retry for consistency.
            idx = m_active.load(std::memory_order_acquire);
            out = m_buf[idx];
            s1 = m_seq.load(std::memory_order_acquire);
        }
        return s1;
    }

    /**
     * @brief Current sequence counter (monotonic, wraps at 2^32).
     */
    uint32_t Sequence() const { return m_seq.load(std::memory_order_acquire); }

private:
    alignas(4) T m_buf[2]{};
    mutable std::atomic<uint32_t> m_active{0};
    mutable std::atomic<uint32_t> m_seq{0};
};

} // namespace lightwaveos::audio
