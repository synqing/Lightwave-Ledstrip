// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file PerformanceMonitor.h
 * @brief Real-time performance monitoring for LightwaveOS v2
 *
 * Provides per-section timing, EMA averages, peak tracking,
 * dropped frame detection, and heap fragmentation alerts.
 *
 * Usage:
 * @code
 * lightwaveos::hardware::PerformanceMonitor perfMon;
 * perfMon.begin(120);  // Target 120 FPS
 *
 * // In render loop
 * perfMon.startFrame();
 * perfMon.startSection();
 * // ... effect rendering ...
 * perfMon.endEffectProcessing();
 * perfMon.startSection();
 * // ... FastLED.show() ...
 * perfMon.endFastLEDShow();
 * perfMon.endFrame();
 * @endcode
 *
 * @copyright 2024 LightwaveOS Project
 */

#ifndef LIGHTWAVEOS_HARDWARE_PERFORMANCE_MONITOR_H
#define LIGHTWAVEOS_HARDWARE_PERFORMANCE_MONITOR_H

#include <cstdint>
#include <cstddef>

// Forward declare ESP-IDF types to avoid including Arduino.h in header
extern "C" {
    int64_t esp_timer_get_time(void);
}

namespace lightwaveos {
namespace hardware {

/**
 * @brief Timing metrics for a single frame
 *
 * All times are in microseconds for precision.
 */
struct TimingMetrics {
    uint32_t effectProcessing = 0;   ///< Time spent rendering effects
    uint32_t fastLEDShow = 0;        ///< Time spent in FastLED.show()
    uint32_t serialProcessing = 0;   ///< Time spent processing serial input
    uint32_t networkProcessing = 0;  ///< Time spent on network/WebSocket
    uint32_t totalFrame = 0;         ///< Total frame time
    uint32_t idle = 0;               ///< Idle time (frame budget remaining)
};

/**
 * @brief Memory metrics snapshot
 */
struct MemoryMetrics {
    size_t freeHeap = 0;             ///< Current free heap bytes
    size_t minFreeHeap = 0;          ///< Minimum free heap observed
    size_t maxAllocBlock = 0;        ///< Largest allocatable block
    uint8_t fragmentationPercent = 0; ///< Heap fragmentation percentage
};

/**
 * @brief Performance statistics for REST API
 *
 * Compact struct for JSON serialization.
 */
struct PerformanceStats {
    float fps;                       ///< Current FPS (EMA smoothed)
    float cpuPercent;                ///< CPU usage percentage
    uint32_t effectTimeUs;           ///< Effect processing time (us)
    uint32_t showTimeUs;             ///< FastLED.show() time (us)
    uint32_t totalFrameTimeUs;       ///< Total frame time (us)
    size_t heapFree;                 ///< Free heap bytes
    uint8_t heapFragmentation;       ///< Fragmentation percentage
    uint32_t droppedFrames;          ///< Total dropped frames
    uint32_t totalFrames;            ///< Total frames rendered
};

/**
 * @brief Real-time performance monitoring system
 *
 * Features:
 * - Per-section timing with microsecond precision
 * - Exponential Moving Average (EMA) smoothing
 * - Peak value tracking
 * - Dropped frame detection (>1.5x target frame time)
 * - Heap fragmentation alerts
 * - History buffer for trend analysis
 *
 * Thread Safety:
 * - Single-threaded use only (call from render task on Core 1)
 * - Read methods are safe to call from any thread
 *
 * Memory Usage: ~300 bytes (plus 120 bytes for history buffers)
 */
class PerformanceMonitor {
public:
    // Fragmentation alert thresholds
    static constexpr uint8_t FRAGMENTATION_WARNING_THRESHOLD = 30;
    static constexpr uint8_t FRAGMENTATION_CRITICAL_THRESHOLD = 50;

    // History buffer size
    static constexpr uint8_t HISTORY_SIZE = 60;

    /**
     * @brief Default constructor
     */
    PerformanceMonitor() = default;

    /**
     * @brief Initialize the performance monitor
     * @param targetFPS Target frame rate (default 120)
     */
    void begin(uint16_t targetFPS = 120);

    // ========== Frame Timing ==========

    /**
     * @brief Start timing a new frame
     *
     * Call at the beginning of each render loop iteration.
     * Resets current frame metrics and records start time.
     */
    void startFrame();

    /**
     * @brief Start timing a section within a frame
     *
     * Call before each major section (effect, show, serial, etc.)
     */
    void startSection();

    /**
     * @brief End timing for effect processing section
     */
    void endEffectProcessing();

    /**
     * @brief End timing for FastLED.show() section
     */
    void endFastLEDShow();

    /**
     * @brief End timing for serial processing section
     */
    void endSerialProcessing();

    /**
     * @brief End timing for network processing section
     */
    void endNetworkProcessing();

    /**
     * @brief Complete frame timing and update all metrics
     *
     * Call at the end of each render loop iteration.
     * Updates EMA averages, peak values, and frame statistics.
     */
    void endFrame();

    // ========== Getters for REST API ==========

    /**
     * @brief Get current frames per second (EMA smoothed)
     * @return FPS value (0.0 if not enough data)
     */
    float getFPS() const;

    /**
     * @brief Get CPU usage percentage
     * @return CPU usage (0-100%)
     */
    float getCPUPercent() const;

    /**
     * @brief Get current free heap memory
     * @return Free heap in bytes
     */
    size_t getHeapFree() const;

    /**
     * @brief Get heap fragmentation percentage
     * @return Fragmentation (0-100%)
     */
    uint8_t getHeapFragmentation() const;

    /**
     * @brief Get average effect processing time
     * @return Time in microseconds (EMA smoothed)
     */
    uint32_t getEffectTimeUs() const;

    /**
     * @brief Get average FastLED.show() time
     * @return Time in microseconds (EMA smoothed)
     */
    uint32_t getShowTimeUs() const;

    /**
     * @brief Get average total frame time
     * @return Time in microseconds (EMA smoothed)
     */
    uint32_t getTotalFrameTimeUs() const;

    /**
     * @brief Get dropped frame count
     * @return Number of dropped frames since begin()
     */
    uint32_t getDroppedFrames() const;

    /**
     * @brief Get total frame count
     * @return Number of frames since begin()
     */
    uint32_t getTotalFrames() const;

    /**
     * @brief Get minimum free heap observed
     * @return Minimum free heap in bytes
     */
    size_t getMinFreeHeap() const;

    /**
     * @brief Get target frame time
     * @return Target frame time in microseconds
     */
    uint32_t getTargetFrameTimeUs() const;

    /**
     * @brief Get all performance stats in one call
     * @return PerformanceStats struct for JSON serialization
     */
    PerformanceStats getStats() const;

    // ========== Fragmentation Alerts ==========

    /**
     * @brief Check if fragmentation is at warning level (>=30%)
     * @return true if fragmentation warning
     */
    bool isFragmentationWarning() const;

    /**
     * @brief Check if fragmentation is at critical level (>=50%)
     * @return true if fragmentation critical
     */
    bool isFragmentationCritical() const;

    // ========== Timing Breakdown ==========

    /**
     * @brief Get timing breakdown as percentages
     * @param[out] effectPct Effect processing percentage
     * @param[out] ledPct FastLED.show() percentage
     * @param[out] serialPct Serial processing percentage
     * @param[out] idlePct Idle time percentage
     */
    void getTimingPercentages(float& effectPct, float& ledPct,
                               float& serialPct, float& idlePct) const;

    // ========== Serial Output ==========

    /**
     * @brief Print compact status line to serial
     *
     * Format: [PERF] FPS: XX.X | CPU: XX.X% | Effect: XXXus | LED: XXXus | Heap: XXXXX | Frag: XX%
     */
    void printStatus() const;

    /**
     * @brief Print detailed performance report to serial
     *
     * Includes timing breakdown, peak values, memory info.
     */
    void printDetailedReport() const;

    /**
     * @brief Draw ASCII FPS history graph to serial
     */
    void drawPerformanceGraph() const;

    // ========== Control ==========

    /**
     * @brief Reset peak metrics and dropped frame counter
     */
    void resetPeaks();

private:
    // Current frame metrics
    TimingMetrics m_currentFrame;

    // Averaged metrics (exponential moving average)
    TimingMetrics m_avgMetrics;

    // Peak metrics
    TimingMetrics m_peakMetrics;

    // Memory metrics
    MemoryMetrics m_memoryMetrics;

    // Frame statistics
    uint32_t m_frameCount = 0;
    uint32_t m_droppedFrames = 0;
    uint32_t m_targetFrameTime = 8333;  // 120 FPS default (microseconds)

    // Timing helpers
    int64_t m_frameStartTime = 0;
    int64_t m_sectionStartTime = 0;

    // CPU usage calculation
    uint64_t m_totalCPUTime = 0;
    uint64_t m_activeCPUTime = 0;
    float m_cpuUsagePercent = 0.0f;

    // History for graphs (last 60 samples)
    uint8_t m_fpsHistory[HISTORY_SIZE] = {0};
    uint8_t m_cpuHistory[HISTORY_SIZE] = {0};
    uint8_t m_historyIndex = 0;

    // EMA smoothing factor
    static constexpr float EMA_ALPHA = 0.1f;

    // Internal helpers
    void updateMemoryMetrics();
    void updateHistory();
};

} // namespace hardware
} // namespace lightwaveos

#endif // LIGHTWAVEOS_HARDWARE_PERFORMANCE_MONITOR_H
