/**
 * @file AudioRingBuffer.h
 * @brief Ring buffer for audio history access (no memmove)
 *
 * Provides deterministic bounded copy of last N samples with wrap handling.
 * Used for Goertzel analysis requiring history windows.
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "K1Spec.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Ring buffer for audio sample history
 *
 * Capacity must be >= N_MAX + HOP_SAMPLES + margin (recommend 4096 samples).
 * Provides O(1) push and copyLast operations with wrap handling.
 */
class AudioRingBuffer {
public:
    /**
     * @brief Constructor
     *
     * Does not allocate memory. Call init() before use.
     */
    AudioRingBuffer();

    /**
     * @brief Destructor
     */
    ~AudioRingBuffer();

    /**
     * @brief Initialize ring buffer
     *
     * @param capacity_samples Buffer capacity in samples (must be >= N_MAX + HOP_SAMPLES)
     * @return true if successful, false on failure
     */
    bool init(size_t capacity_samples);

    /**
     * @brief Reset buffer (clear all data)
     */
    void reset();

    /**
     * @brief Push samples into buffer
     *
     * @param samples Pointer to sample array
     * @param n Number of samples to push
     * @param end_sample_counter Sample counter at end of this chunk (inclusive)
     */
    void push(const int16_t* samples, size_t n, uint64_t end_sample_counter);

    /**
     * @brief Copy last N samples into destination
     *
     * Handles wrap automatically. Deterministic bounded copy.
     *
     * @param N Number of samples to copy (must be <= capacity)
     * @param dst Destination buffer (must have space for N samples)
     * @return true if successful, false if N > capacity or buffer not initialized
     */
    bool copyLast(size_t N, int16_t* dst) const;

    /**
     * @brief Get sample counter at end of buffer
     *
     * @return Sample counter of most recently pushed sample
     */
    uint64_t sampleCounterEnd() const { return m_endCounter; }

    /**
     * @brief Check if buffer is initialized
     */
    bool isInitialized() const { return m_buffer != nullptr; }

private:
    int16_t* m_buffer;                   ///< Ring buffer storage
    size_t m_capacity;                    ///< Buffer capacity in samples
    size_t m_writePos;                    ///< Current write position (circular)
    uint64_t m_endCounter;                ///< Sample counter at end of buffer
    bool m_initialized;                   ///< Initialization flag
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos

