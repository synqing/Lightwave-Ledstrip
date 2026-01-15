/**
 * @file AudioBenchmarkRing.h
 * @brief Lock-free ring buffer for cross-core benchmark sample transfer
 *
 * Single-producer (AudioNode on audio core) to single-consumer (WebServer
 * or stats aggregator on main core) ring buffer using acquire/release
 * memory ordering for safe cross-core access without mutexes.
 *
 * Memory footprint: ~2KB (64 samples * 32 bytes)
 */

#pragma once

#include "AudioBenchmarkMetrics.h"
#include <atomic>
#include <cstring>

namespace lightwaveos::audio {

/**
 * @brief Lock-free SPSC ring buffer for AudioBenchmarkSample
 *
 * Uses monotonically increasing indices with masking for wrap-around.
 * This avoids the "full vs empty" ambiguity of traditional ring buffers.
 *
 * Thread safety:
 * - push(): Called only from AudioNode::processHop() (single producer)
 * - pop(), available(), peekLast(): Called from main core (single consumer)
 */
class AudioBenchmarkRing {
public:
    AudioBenchmarkRing() : m_writeIndex(0), m_readIndex(0) {
        std::memset(m_samples, 0, sizeof(m_samples));
    }

    /**
     * @brief Push a new sample (producer side - audio core)
     *
     * Non-blocking, always succeeds. Overwrites oldest sample if full.
     * Uses release semantics to ensure sample data is visible before index update.
     *
     * @param sample The timing sample to store
     */
    void push(const AudioBenchmarkSample& sample) {
        uint32_t idx = m_writeIndex.load(std::memory_order_relaxed);
        m_samples[idx & BENCHMARK_RING_MASK] = sample;
        m_writeIndex.store(idx + 1, std::memory_order_release);
    }

    /**
     * @brief Pop the oldest sample (consumer side - main core)
     *
     * Non-blocking. Returns false if buffer is empty.
     * Uses acquire semantics to ensure we see the complete sample data.
     *
     * @param[out] out The sample to populate
     * @return true if a sample was available and copied to out
     */
    bool pop(AudioBenchmarkSample& out) {
        uint32_t readIdx = m_readIndex.load(std::memory_order_relaxed);
        uint32_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

        if (readIdx == writeIdx) {
            return false;  // Empty
        }

        out = m_samples[readIdx & BENCHMARK_RING_MASK];
        m_readIndex.store(readIdx + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Number of samples available to read
     *
     * May be stale by the time caller acts on it, but safe for
     * "should I bother trying to read?" decisions.
     *
     * @return Number of unread samples (0 to BENCHMARK_RING_SIZE)
     */
    uint32_t available() const {
        uint32_t w = m_writeIndex.load(std::memory_order_acquire);
        uint32_t r = m_readIndex.load(std::memory_order_relaxed);
        return w - r;
    }

    /**
     * @brief Check if buffer has any samples
     * @return true if at least one sample is available
     */
    bool hasData() const {
        return available() > 0;
    }

    /**
     * @brief Peek at the N most recent samples without consuming them
     *
     * Useful for WebSocket streaming where we want to send recent history
     * without affecting the stats aggregation consumer.
     *
     * @param[out] out Array to populate (must have space for count samples)
     * @param count Number of samples to peek (clamped to BENCHMARK_RING_SIZE)
     * @return Actual number of samples copied (may be less if buffer not full)
     */
    size_t peekLast(AudioBenchmarkSample* out, size_t count) const {
        if (count > BENCHMARK_RING_SIZE) {
            count = BENCHMARK_RING_SIZE;
        }

        uint32_t w = m_writeIndex.load(std::memory_order_acquire);
        uint32_t r = m_readIndex.load(std::memory_order_relaxed);
        uint32_t avail = w - r;

        if (count > avail) {
            count = avail;
        }

        // Copy from most recent (w-1) backwards
        for (size_t i = 0; i < count; ++i) {
            uint32_t idx = (w - 1 - i) & BENCHMARK_RING_MASK;
            out[i] = m_samples[idx];
        }

        return count;
    }

    /**
     * @brief Get the most recent sample without consuming it
     *
     * @param[out] out The sample to populate
     * @return true if a sample was available
     */
    bool peekLatest(AudioBenchmarkSample& out) const {
        uint32_t w = m_writeIndex.load(std::memory_order_acquire);
        uint32_t r = m_readIndex.load(std::memory_order_relaxed);

        if (w == r) {
            return false;  // Empty
        }

        uint32_t idx = (w - 1) & BENCHMARK_RING_MASK;
        out = m_samples[idx];
        return true;
    }

    /**
     * @brief Reset the buffer (clears read pointer to match write)
     *
     * Call only when no producer is active or when you don't care
     * about losing in-flight samples.
     */
    void reset() {
        uint32_t w = m_writeIndex.load(std::memory_order_acquire);
        m_readIndex.store(w, std::memory_order_release);
    }

    /**
     * @brief Get total samples ever pushed (useful for sequence tracking)
     * @return Monotonically increasing count
     */
    uint32_t totalPushed() const {
        return m_writeIndex.load(std::memory_order_acquire);
    }

private:
    /// Storage for samples (statically allocated)
    AudioBenchmarkSample m_samples[BENCHMARK_RING_SIZE];

    /// Monotonically increasing write index (producer updates)
    std::atomic<uint32_t> m_writeIndex;

    /// Monotonically increasing read index (consumer updates)
    std::atomic<uint32_t> m_readIndex;
};

} // namespace lightwaveos::audio
