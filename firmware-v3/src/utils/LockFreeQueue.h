/**
 * @file LockFreeQueue.h
 * @brief Lock-Free SPSC Queue for Cross-Core Communication
 *
 * Single-Producer Single-Consumer (SPSC) queue for passing data between
 * audio processing (Core 0) and rendering (Core 1) without mutex contention.
 *
 * Thread Safety:
 * - Producer calls push() from one core
 * - Consumer calls pop() from another core
 * - No locks, no blocking, no contention
 */

#pragma once

#include <atomic>
#include <cstddef>

namespace lightwaveos {
namespace utils {

template<typename T, size_t Capacity>
class LockFreeQueue {
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    LockFreeQueue() : head_(0), tail_(0) {}

    /**
     * @brief Push an item (producer only)
     * @return true if pushed, false if queue full
     */
    bool push(const T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (tail + 1) % (Capacity + 1);

        // Check if full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop an item (consumer only)
     * @return true if popped, false if queue empty
     */
    bool pop(T& item) {
        size_t head = head_.load(std::memory_order_relaxed);

        // Check if empty
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        item = buffer_[head];
        head_.store((head + 1) % (Capacity + 1), std::memory_order_release);
        return true;
    }

    /**
     * @brief Peek at front item without removing (consumer only)
     * @return true if item available, false if empty
     */
    bool peek(T& item) const {
        size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        item = buffer_[head];
        return true;
    }

    /**
     * @brief Check if queue is empty (approximate)
     */
    bool isEmpty() const {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Check if queue is full (approximate)
     */
    bool isFull() const {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (tail + 1) % (Capacity + 1);
        return next_tail == head_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get approximate queue size
     */
    size_t size() const {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail >= head) {
            return tail - head;
        }
        return (Capacity + 1) - head + tail;
    }

    /**
     * @brief Clear queue (consumer only)
     */
    void clear() {
        head_.store(tail_.load(std::memory_order_relaxed),
                    std::memory_order_release);
    }

private:
    T buffer_[Capacity + 1];  // +1 for full/empty disambiguation
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

} // namespace utils
} // namespace lightwaveos
