/**
 * @file StackMonitor.h
 * @brief FreeRTOS stack overflow detection and monitoring
 * 
 * Provides runtime stack usage monitoring and overflow detection hooks
 * to prevent stack corruption and system crashes.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

namespace lightwaveos {
namespace core {
namespace system {

/**
 * @brief Stack monitoring and overflow detection
 * 
 * Monitors stack usage for all FreeRTOS tasks and provides hooks
 * for stack overflow detection. Enables early detection of stack
 * overflow conditions before they cause memory corruption.
 */
class StackMonitor {
public:
    /**
     * @brief Initialize stack monitoring
     * 
     * Sets up stack overflow hooks and begins monitoring.
     * Should be called once during system initialization.
     */
    static void init();

    /**
     * @brief Check stack usage for a specific task
     * @param taskHandle Task handle (nullptr for current task)
     * @return Stack high water mark in bytes (bytes remaining)
     */
    static uint32_t getStackHighWaterMark(TaskHandle_t taskHandle = nullptr);

    /**
     * @brief Get stack usage percentage for a task
     * @param taskHandle Task handle (nullptr for current task)
     * @param stackSize Total stack size in bytes
     * @return Usage percentage (0-100)
     */
    static uint8_t getStackUsagePercent(TaskHandle_t taskHandle, uint32_t stackSize);

    /**
     * @brief Check all tasks and log warnings for high usage
     * 
     * Iterates through all tasks and logs warnings if stack usage
     * exceeds threshold (default 80%).
     */
    static void checkAllTasks();

    /**
     * @brief Set warning threshold (default 80%)
     * @param percent Usage percentage threshold (0-100)
     */
    static void setWarningThreshold(uint8_t percent) { s_warningThreshold = percent; }

    /**
     * @brief Get warning threshold
     * @return Current threshold percentage
     */
    static uint8_t getWarningThreshold() { return s_warningThreshold; }

    /**
     * @brief FreeRTOS stack overflow hook
     * 
     * Called by FreeRTOS when stack overflow is detected.
     * This is a C function that must be implemented in the .cpp file.
     */
    static void onStackOverflow(TaskHandle_t taskHandle, char* taskName);

    // ========================================================================
    // Profiling Features
    // ========================================================================

    /**
     * @brief Start continuous profiling
     * 
     * Begins tracking stack usage over time for all tasks.
     */
    static void startProfiling();

    /**
     * @brief Stop profiling
     * 
     * Stops tracking stack usage (but keeps existing data).
     */
    static void stopProfiling();

    /**
     * @brief Generate detailed profile report
     * 
     * Prints per-task stack usage statistics including current, peak, and average.
     */
    static void generateProfileReport();

    /**
     * @brief Task stack profile data
     */
    struct TaskStackProfile {
        const char* taskName;
        uint32_t stackSize;
        uint32_t currentFree;
        uint32_t peakUsed;
        uint32_t avgUsed;
        uint8_t currentPercent;
        uint8_t peakPercent;
        uint8_t avgPercent;
    };

private:
    struct TaskProfileData {
        const char* taskName;
        uint32_t stackSize;
        uint32_t peakUsed;
        uint64_t totalUsed;
        uint32_t sampleCount;
        bool active;
    };

    static constexpr size_t MAX_PROFILED_TASKS = 16;
    static TaskProfileData s_profiles[MAX_PROFILED_TASKS];
    static bool s_profilingEnabled;
    static uint8_t s_warningThreshold;  // Warning threshold (default 80%)
    static bool s_initialized;          // Initialization flag
};

} // namespace system
} // namespace core
} // namespace lightwaveos

// FreeRTOS hook function (must be C linkage)
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName);
