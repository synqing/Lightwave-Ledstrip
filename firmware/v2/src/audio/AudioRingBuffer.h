/**
 * @file AudioRingBuffer.h
 * @brief Fixed-capacity ring buffer for audio sample storage
 *
 * Template-based ring buffer designed for time-domain sample windowing.
 * Provides O(1) push operations with automatic wrap-around and bounded
 * copyLast() for extracting recent N samples.
 *
 * Used by: GoertzelBank for per-bin windowing (Phase 2B)
 *
 * Memory: CAPACITY * sizeof(T) bytes (static allocation)
 * Thread-safety: Single producer, single consumer (no mutex needed)
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Tempo Tracking Dual-Bank Architecture
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace lightwaveos {
namespace audio {

/**
 * @brief Fixed-capacity ring buffer with wrap-around indexing
 *
 * Template parameters:
 * @tparam T Element type (typically float for audio samples)
 * @tparam CAPACITY Maximum number of elements (must be power of 2 for efficiency)
 *
 * Example usage:
 * @code
 * AudioRingBuffer<float, 256> sampleBuffer;
 * sampleBuffer.push(0.5f);
 * float recent[128];
 * sampleBuffer.copyLast(recent, 128);  // Get last 128 samples
 * @endcode
 */
template<typename T, size_t CAPACITY>
class AudioRingBuffer {
public:
    /**
     * @brief Default constructor - initializes empty buffer
     */
    AudioRingBuffer() : m_writeIndex(0), m_size(0) {
        // Zero-initialize buffer for deterministic behavior
        memset(m_buffer, 0, sizeof(m_buffer));
    }

    /**
     * @brief Push a new value into the ring buffer
     *
     * O(1) operation with automatic wrap-around. Overwrites oldest
     * value when buffer is full.
     *
     * @param value Value to push
     */
    void push(T value) {
        m_buffer[m_writeIndex] = value;
        m_writeIndex = (m_writeIndex + 1) % CAPACITY;  // Wrap-around
        if (m_size < CAPACITY) {
            m_size++;
        }
    }

    /**
     * @brief Copy the last N values to destination buffer
     *
     * Copies most recent 'count' samples in chronological order
     * (oldest first, newest last).
     *
     * @param dest Destination buffer (must have space for 'count' elements)
     * @param count Number of samples to copy (must be <= size() and <= CAPACITY)
     *
     * Preconditions:
     * - count <= size() (caller must check if enough samples available)
     * - count <= CAPACITY (enforced by buffer design)
     * - dest != nullptr (caller responsibility)
     *
     * Example:
     * @code
     * AudioRingBuffer<float, 256> buf;
     * // ... push 200 samples ...
     * float window[128];
     * buf.copyLast(window, 128);  // Get last 128 of 200
     * @endcode
     */
    void copyLast(T* dest, size_t count) const {
        if (count == 0 || dest == nullptr) {
            return;  // Nothing to copy
        }

        // Clamp count to available samples
        count = std::min(count, m_size);

        // Calculate start position (oldest sample in the requested window)
        // writeIndex points to NEXT write position, so current oldest is at:
        // (writeIndex - size + CAPACITY) % CAPACITY
        size_t startIndex;
        if (m_size < CAPACITY) {
            // Buffer not yet full - start from beginning
            startIndex = 0;
            // Copy from index (size - count) to get last 'count' samples
            size_t offsetIndex = (m_size >= count) ? (m_size - count) : 0;
            memcpy(dest, &m_buffer[offsetIndex], count * sizeof(T));
        } else {
            // Buffer full - compute wrap-around start position
            // Start at (writeIndex - count + CAPACITY) % CAPACITY
            startIndex = (m_writeIndex + CAPACITY - count) % CAPACITY;

            // Check if we need to handle wrap-around in the copy
            if (startIndex + count <= CAPACITY) {
                // Contiguous block - single memcpy
                memcpy(dest, &m_buffer[startIndex], count * sizeof(T));
            } else {
                // Wrapped block - two memcpy calls
                size_t firstChunk = CAPACITY - startIndex;
                size_t secondChunk = count - firstChunk;
                memcpy(dest, &m_buffer[startIndex], firstChunk * sizeof(T));
                memcpy(dest + firstChunk, &m_buffer[0], secondChunk * sizeof(T));
            }
        }
    }

    /**
     * @brief Get current number of valid samples in buffer
     * @return Number of samples (0 to CAPACITY)
     */
    size_t size() const {
        return m_size;
    }

    /**
     * @brief Get maximum capacity
     * @return Buffer capacity (compile-time constant)
     */
    constexpr size_t capacity() const {
        return CAPACITY;
    }

    /**
     * @brief Check if buffer is full
     * @return True if size() == CAPACITY
     */
    bool isFull() const {
        return m_size == CAPACITY;
    }

    /**
     * @brief Clear the buffer (reset to empty state)
     */
    void clear() {
        m_writeIndex = 0;
        m_size = 0;
        memset(m_buffer, 0, sizeof(m_buffer));
    }

private:
    T m_buffer[CAPACITY];        ///< Fixed-size storage array
    size_t m_writeIndex;         ///< Index of next write position
    size_t m_size;               ///< Current number of valid samples (0 to CAPACITY)
};

} // namespace audio
} // namespace lightwaveos
