// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HeapMonitor.h
 * @brief Heap corruption detection and monitoring
 * 
 * Provides runtime heap integrity monitoring and corruption detection hooks
 * to prevent memory corruption and system crashes.
 */

#pragma once

#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief Heap monitoring and corruption detection
 * 
 * Monitors heap integrity for all allocations and provides hooks
 * for heap corruption detection. Enables early detection of heap
 * corruption before it causes system crashes.
 */
class HeapMonitor {
public:
    /**
     * @brief Initialize heap monitoring
     * 
     * Sets up heap corruption hooks and begins monitoring.
     * Should be called once during system initialization.
     */
    static void init();

    /**
     * @brief Check heap integrity
     * 
     * Performs a comprehensive heap integrity check.
     * @return true if heap is healthy, false if corruption detected
     */
    static bool checkHeapIntegrity();

    /**
     * @brief Get current free heap size
     * @return Free heap in bytes
     */
    static size_t getFreeHeap();

    /**
     * @brief Get minimum free heap ever recorded
     * @return Minimum free heap in bytes
     */
    static size_t getMinFreeHeap();

    /**
     * @brief Get largest free block
     * @return Largest free block in bytes
     */
    static size_t getLargestFreeBlock();

    /**
     * @brief Calculate heap fragmentation percentage
     * @return Fragmentation percentage (0-100)
     */
    static uint8_t getFragmentationPercent();

    /**
     * @brief Heap corruption hook
     * 
     * Called by ESP-IDF when heap corruption is detected.
     * This is a C function that must be implemented in the .cpp file.
     */
    static void onHeapCorruption();

    /**
     * @brief Malloc failed hook
     * 
     * Called by FreeRTOS when malloc fails.
     * This is a C function that must be implemented in the .cpp file.
     */
    static void onMallocFailed(size_t size);

private:
    static bool s_initialized;          // Initialization flag
    static size_t s_minFreeHeap;        // Minimum free heap recorded
};

} // namespace system
} // namespace core
} // namespace lightwaveos

// FreeRTOS/ESP-IDF hooks (must be C linkage)
extern "C" void heap_corruption_hook(void);
extern "C" void vApplicationMallocFailedHook(void);

