// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ValidationProfiler.h
 * @brief Validation performance profiling
 * 
 * Tracks validation function call counts and timing to measure overhead.
 */

#pragma once

#include <stdint.h>
#include <cstddef>

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief Validation performance profiler
 * 
 * Tracks validation function calls and measures CPU overhead.
 */
class ValidationProfiler {
public:
    /**
     * @brief Initialize validation profiling
     * 
     * Should be called once during system initialization.
     */
    static void init();

    /**
     * @brief Record a validation function call
     * @param functionName Name of validation function (e.g., "validateEffectId")
     * @param timeUs Time taken in microseconds
     */
    static void recordCall(const char* functionName, int64_t timeUs);

    /**
     * @brief Update frame statistics
     * 
     * Call this at the end of each frame to update per-frame metrics.
     */
    static void updateFrame();

    /**
     * @brief Get total validation calls
     * @return Total number of validation calls recorded
     */
    static uint32_t getTotalCalls() { return s_totalCalls; }

    /**
     * @brief Get average time per validation call
     * @return Average time in microseconds
     */
    static float getAvgTimeUs();

    /**
     * @brief Get total overhead per frame
     * @return Total validation time per frame in microseconds
     */
    static float getOverheadPerFrameUs();

    /**
     * @brief Get CPU overhead percentage
     * @param frameBudgetUs Frame budget in microseconds (default 8333 for 120 FPS)
     * @return CPU overhead as percentage (0-100)
     */
    static float getCPUOverheadPercent(uint32_t frameBudgetUs = 8333);

    /**
     * @brief Generate performance report
     * 
     * Prints detailed performance statistics to serial.
     */
    static void generateReport();

    /**
     * @brief Enable/disable profiling
     * @param enabled true to enable, false to disable
     */
    static void setEnabled(bool enabled) { s_enabled = enabled; }

    /**
     * @brief Check if profiling is enabled
     * @return true if enabled, false otherwise
     */
    static bool isEnabled() { return s_enabled; }

private:
    static uint32_t s_totalCalls;           // Total validation calls
    static uint64_t s_totalTimeUs;           // Total time in microseconds
    static uint32_t s_frameCalls;            // Calls in current frame
    static uint64_t s_frameTimeUs;           // Time in current frame
    static uint32_t s_frameCount;            // Number of frames processed
    static float s_peakTimeUs;               // Peak validation time
    static bool s_enabled;                   // Profiling enabled flag
    static bool s_initialized;              // Initialization flag
};

} // namespace system
} // namespace core
} // namespace lightwaveos

