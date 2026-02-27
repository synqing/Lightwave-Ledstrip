/**
 * @file MemoryLeakDetector.h
 * @brief Memory leak detection and tracking
 * 
 * Tracks allocations to detect memory leaks and long-lived allocations.
 */

#pragma once

#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief Memory leak detection and tracking
 * 
 * Tracks allocations with source location to detect memory leaks
 * and long-lived allocations that may indicate leaks.
 */
class MemoryLeakDetector {
public:
    /**
     * @brief Initialize memory leak detection
     * 
     * Sets up allocation tracking. Should be called once during system initialization.
     */
    static void init();

    /**
     * @brief Record an allocation
     * @param ptr Pointer to allocated memory
     * @param size Size of allocation in bytes
     * @param file Source file name (from __FILE__)
     * @param line Source line number (from __LINE__)
     */
    static void recordAllocation(void* ptr, size_t size, const char* file, int line);

    /**
     * @brief Record a deallocation
     * @param ptr Pointer to deallocated memory
     */
    static void recordDeallocation(void* ptr);

    /**
     * @brief Scan for potential leaks
     * 
     * Checks for allocations older than threshold (default 30 seconds)
     * and reports them as potential leaks.
     */
    static void scanForLeaks();

    /**
     * @brief Get heap delta since last check
     * @return Change in heap size (positive = leak, negative = freed)
     */
    static int32_t getHeapDelta();

    /**
     * @brief Reset baseline heap size
     * 
     * Call this after system initialization to establish baseline.
     */
    static void resetBaseline();

    /**
     * @brief Set leak detection threshold
     * @param seconds Age in seconds to consider a potential leak
     */
    static void setLeakThreshold(uint32_t seconds) { s_leakThresholdMs = seconds * 1000; }

    /**
     * @brief Enable/disable leak detection
     * @param enabled true to enable, false to disable
     */
    static void setEnabled(bool enabled) { s_enabled = enabled; }

private:
    struct AllocationRecord {
        void* ptr;
        size_t size;
        uint32_t timestamp;
        const char* file;
        int line;
        bool active;
    };

    static constexpr size_t MAX_RECORDS = 256;  // Maximum tracked allocations
    static AllocationRecord s_records[MAX_RECORDS];
    static size_t s_recordCount;
    static uint32_t s_leakThresholdMs;
    static size_t s_baselineHeap;
    static bool s_enabled;
    static bool s_initialized;

    static size_t findRecord(void* ptr);
    static void removeRecord(size_t index);
};

} // namespace system
} // namespace core
} // namespace lightwaveos

