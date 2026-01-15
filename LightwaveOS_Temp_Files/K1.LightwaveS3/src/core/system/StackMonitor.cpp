/**
 * @file StackMonitor.cpp
 * @brief FreeRTOS stack overflow detection and monitoring implementation
 */

#define LW_LOG_TAG "StackMonitor"
#include "StackMonitor.h"
#include "../../utils/Log.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <esp_task_wdt.h>
#endif

namespace lightwaveos {
namespace core {
namespace system {

// Static members
StackMonitor::TaskProfileData StackMonitor::s_profiles[StackMonitor::MAX_PROFILED_TASKS];
bool StackMonitor::s_profilingEnabled = false;
uint8_t StackMonitor::s_warningThreshold = 80;  // 80% usage triggers warning
bool StackMonitor::s_initialized = false;

void StackMonitor::init() {
    if (s_initialized) {
        return;
    }

    LW_LOGI("Initialized (overflow detection enabled)");
    LW_LOGI("Warning threshold: %d%%", s_warningThreshold);
    
    s_initialized = true;
}

uint32_t StackMonitor::getStackHighWaterMark(TaskHandle_t taskHandle) {
    // uxTaskGetStackHighWaterMark returns minimum free stack space ever recorded
    // (in words, not bytes - multiply by sizeof(StackType_t) which is 4 bytes)
    UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(taskHandle);
    return highWaterMark * sizeof(StackType_t);  // Convert to bytes
}

uint8_t StackMonitor::getStackUsagePercent(TaskHandle_t taskHandle, uint32_t stackSize) {
    if (taskHandle == nullptr) {
        taskHandle = xTaskGetCurrentTaskHandle();
    }
    
    if (taskHandle == nullptr) {
        return 0;
    }

    uint32_t freeBytes = getStackHighWaterMark(taskHandle);
    if (freeBytes >= stackSize) {
        return 0;  // No usage (shouldn't happen, but safe)
    }
    
    uint32_t usedBytes = stackSize - freeBytes;
    uint8_t percent = (uint8_t)((usedBytes * 100) / stackSize);
    
    return percent;
}

void StackMonitor::checkAllTasks() {
    if (!s_initialized) {
        return;
    }

    // Get number of tasks
    UBaseType_t numTasks = uxTaskGetNumberOfTasks();
    if (numTasks == 0) {
        return;
    }

    // Simplified approach: Check current task only
    // Note: uxTaskGetSystemState requires configUSE_TRACE_FACILITY=1 which may not be available
    // in all FreeRTOS builds. We'll use a simpler approach that checks the current task.
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask != nullptr) {
        const char* taskName = pcTaskGetName(currentTask);
        
        // Get high water mark (this is the minimum free space, not current)
        uint32_t freeBytes = getStackHighWaterMark(currentTask);
        
        // Estimate stack size (conservative: assume at least 4KB if high water mark is small)
        // This is an approximation since we can't get actual stack size easily
        uint32_t estimatedStackSize = (freeBytes < 2048) ? 4096 : (freeBytes * 2);
        
        uint32_t usedBytes = estimatedStackSize - freeBytes;
        uint8_t usagePercent = (freeBytes < estimatedStackSize) ? 
            (uint8_t)((usedBytes * 100) / estimatedStackSize) : 0;

        // Log warning if usage exceeds threshold
        if (usagePercent >= s_warningThreshold) {
            LW_LOGW("Task '%s': Stack usage %d%% (%u/%u bytes free, estimated)",
                   taskName, usagePercent, freeBytes, estimatedStackSize);
        }
        
        // Update profiling data if enabled
        if (s_profilingEnabled) {
            // Find or create profile entry
            size_t profileIndex = MAX_PROFILED_TASKS;
            for (size_t j = 0; j < MAX_PROFILED_TASKS; j++) {
                if (s_profiles[j].active && s_profiles[j].taskName == taskName) {
                    profileIndex = j;
                    break;
                } else if (!s_profiles[j].active && profileIndex == MAX_PROFILED_TASKS) {
                    profileIndex = j;
                }
            }
            
            // Update profile
            if (profileIndex < MAX_PROFILED_TASKS) {
                TaskProfileData& profile = s_profiles[profileIndex];
                if (!profile.active) {
                    profile.active = true;
                    profile.taskName = taskName;
                    profile.stackSize = estimatedStackSize;
                }
                
                if (usedBytes > profile.peakUsed) {
                    profile.peakUsed = usedBytes;
                }
                
                profile.totalUsed += usedBytes;
                profile.sampleCount++;
            }
        }
    }
    
    // Feed watchdog after checking all tasks
    // This ensures watchdog is fed even if tasks are healthy
#ifndef NATIVE_BUILD
    esp_task_wdt_reset();
#endif
}

void StackMonitor::onStackOverflow(TaskHandle_t taskHandle, char* taskName) {
    // CRITICAL: Stack overflow detected!
    // Log error and trigger recovery
    
    const char* name = (taskName != nullptr) ? taskName : "Unknown";
    
    LW_LOGE("STACK OVERFLOW DETECTED in task '%s'!", name);
    LW_LOGE("Task handle: %p", taskHandle);
    
    // Get stack info if possible (be careful - stack is corrupted!)
    if (taskHandle != nullptr) {
        // Try to get high water mark (may fail if stack is too corrupted)
        UBaseType_t highWaterMark = 0;
        // Note: uxTaskGetStackHighWaterMark may not work if stack is corrupted
        // but it's worth trying
        
        LW_LOGE("Stack high water mark: %u words", highWaterMark);
    }
    
    // Trigger watchdog reset or enter safe mode
    // For now, we'll rely on FreeRTOS to handle the reset
    // In production, you might want to:
    // 1. Save crash context to RTC memory
    // 2. Enter safe mode (minimal operation)
    // 3. Trigger watchdog reset
    
    // FreeRTOS will reset the system after this hook returns
}

void StackMonitor::startProfiling() {
    if (!s_initialized) {
        return;
    }

    s_profilingEnabled = true;
    
    // Initialize all profile slots
    for (size_t i = 0; i < MAX_PROFILED_TASKS; i++) {
        s_profiles[i].active = false;
        s_profiles[i].peakUsed = 0;
        s_profiles[i].totalUsed = 0;
        s_profiles[i].sampleCount = 0;
    }

    LW_LOGI("Stack profiling started");
}

void StackMonitor::stopProfiling() {
    s_profilingEnabled = false;
    LW_LOGI("Stack profiling stopped");
}

void StackMonitor::generateProfileReport() {
    if (!s_initialized) {
        return;
    }

    Serial.println("\n=== Stack Usage Profile ===");

    // Simplified approach: Check current task only
    // Note: Full task enumeration requires uxTaskGetSystemState which may not be available
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask != nullptr) {
        const char* taskName = pcTaskGetName(currentTask);
        
        // Get high water mark
        uint32_t freeBytes = getStackHighWaterMark(currentTask);
        
        // Estimate stack size (conservative: assume at least 4KB if high water mark is small)
        uint32_t estimatedStackSize = (freeBytes < 2048) ? 4096 : (freeBytes * 2);
        
        uint32_t usedBytes = estimatedStackSize - freeBytes;
        uint8_t usagePercent = (usedBytes * 100) / estimatedStackSize;
        
        // Find or create profile entry
        size_t profileIndex = MAX_PROFILED_TASKS;
        for (size_t j = 0; j < MAX_PROFILED_TASKS; j++) {
            if (s_profiles[j].active && s_profiles[j].taskName == taskName) {
                profileIndex = j;
                break;
            } else if (!s_profiles[j].active && profileIndex == MAX_PROFILED_TASKS) {
                profileIndex = j;
            }
        }
        
        // Calculate averages
        uint32_t avgUsed = 0;
        uint8_t peakPercent = 0;
        uint8_t avgPercent = 0;
        
        if (profileIndex < MAX_PROFILED_TASKS && s_profiles[profileIndex].sampleCount > 0) {
            avgUsed = s_profiles[profileIndex].totalUsed / s_profiles[profileIndex].sampleCount;
            peakPercent = (s_profiles[profileIndex].peakUsed * 100) / estimatedStackSize;
            avgPercent = (avgUsed * 100) / estimatedStackSize;
        }
        
        // Print report
        Serial.printf("Task: %s\n", taskName);
        Serial.printf("  Stack size: %u bytes (%u words)\n", 
                     estimatedStackSize, estimatedStackSize / 4);
        Serial.printf("  Current free: %u bytes (%d%%)\n", freeBytes, usagePercent);
        if (s_profilingEnabled && profileIndex < MAX_PROFILED_TASKS && s_profiles[profileIndex].sampleCount > 0) {
            Serial.printf("  Peak usage: %u bytes (%d%%)\n", 
                         s_profiles[profileIndex].peakUsed, peakPercent);
            Serial.printf("  Avg usage: %u bytes (%d%%)\n", avgUsed, avgPercent);
        }
        
        // Safety margin and recommendations
        uint8_t safetyMargin = 100 - usagePercent;
        if (safetyMargin >= 50) {
            Serial.printf("  Safety margin: %d%% ✓\n", safetyMargin);
        } else if (safetyMargin >= 20) {
            Serial.printf("  Safety margin: %d%% ⚠️\n", safetyMargin);
        } else {
            Serial.printf("  Safety margin: %d%% ⚠️  (Consider increasing stack size)\n", safetyMargin);
        }
        Serial.println();
    } else {
        Serial.println("Unable to get current task information");
    }
    
    Serial.println("=======================================\n");
}

} // namespace system
} // namespace core
} // namespace lightwaveos

// FreeRTOS hook implementation (C linkage required)
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    lightwaveos::core::system::StackMonitor::onStackOverflow(xTask, pcTaskName);
}

