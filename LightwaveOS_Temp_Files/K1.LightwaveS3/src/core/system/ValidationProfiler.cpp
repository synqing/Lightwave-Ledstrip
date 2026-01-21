/**
 * @file ValidationProfiler.cpp
 * @brief Validation performance profiling implementation
 */

#define LW_LOG_TAG "ValProfiler"
#include "ValidationProfiler.h"
#include "../../utils/Log.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace core {
namespace system {

// Static members
uint32_t ValidationProfiler::s_totalCalls = 0;
uint64_t ValidationProfiler::s_totalTimeUs = 0;
uint32_t ValidationProfiler::s_frameCalls = 0;
uint64_t ValidationProfiler::s_frameTimeUs = 0;
uint32_t ValidationProfiler::s_frameCount = 0;
float ValidationProfiler::s_peakTimeUs = 0.0f;
bool ValidationProfiler::s_enabled = false;  // Disabled by default (enable via FEATURE_VALIDATION_PROFILING)
bool ValidationProfiler::s_initialized = false;

void ValidationProfiler::init() {
    if (s_initialized) {
        return;
    }

    s_totalCalls = 0;
    s_totalTimeUs = 0;
    s_frameCalls = 0;
    s_frameTimeUs = 0;
    s_frameCount = 0;
    s_peakTimeUs = 0.0f;

#if FEATURE_VALIDATION_PROFILING
    s_enabled = true;
    LW_LOGI("Initialized (validation profiling enabled)");
#else
    s_enabled = false;
    LW_LOGI("Initialized (validation profiling disabled - enable FEATURE_VALIDATION_PROFILING)");
#endif

    s_initialized = true;
}

void ValidationProfiler::recordCall(const char* functionName, int64_t timeUs) {
    if (!s_initialized || !s_enabled) {
        return;
    }

    s_totalCalls++;
    s_totalTimeUs += timeUs;
    s_frameCalls++;
    s_frameTimeUs += timeUs;

    // Track peak time
    if (static_cast<float>(timeUs) > s_peakTimeUs) {
        s_peakTimeUs = static_cast<float>(timeUs);
    }
}

void ValidationProfiler::updateFrame() {
    if (!s_initialized || !s_enabled) {
        return;
    }

    s_frameCount++;

    // Generate report every 1000 frames
    if (s_frameCount % 1000 == 0) {
        generateReport();
    }

    // Reset frame counters
    s_frameCalls = 0;
    s_frameTimeUs = 0;
}

float ValidationProfiler::getAvgTimeUs() {
    if (s_totalCalls == 0) {
        return 0.0f;
    }
    return static_cast<float>(s_totalTimeUs) / static_cast<float>(s_totalCalls);
}

float ValidationProfiler::getOverheadPerFrameUs() {
    if (s_frameCount == 0) {
        return 0.0f;
    }
    return static_cast<float>(s_totalTimeUs) / static_cast<float>(s_frameCount);
}

float ValidationProfiler::getCPUOverheadPercent(uint32_t frameBudgetUs) {
    float overheadUs = getOverheadPerFrameUs();
    if (frameBudgetUs == 0) {
        return 0.0f;
    }
    return (overheadUs / static_cast<float>(frameBudgetUs)) * 100.0f;
}

void ValidationProfiler::generateReport() {
    if (!s_initialized || !s_enabled) {
        return;
    }

    Serial.println("\n=== Validation Performance Report ===");
    Serial.printf("Total calls: %u\n", s_totalCalls);
    Serial.printf("Total frames: %u\n", s_frameCount);
    
    if (s_totalCalls > 0) {
        float avgTime = getAvgTimeUs();
        float overheadPerFrame = getOverheadPerFrameUs();
        float cpuOverhead = getCPUOverheadPercent(8333);  // 120 FPS = 8333us per frame
        
        Serial.printf("Avg time per call: %.2f us\n", avgTime);
        Serial.printf("Peak time: %.2f us\n", s_peakTimeUs);
        Serial.printf("Total overhead per frame: %.2f us\n", overheadPerFrame);
        Serial.printf("CPU overhead: %.3f%% (of 8.33ms frame budget)\n", cpuOverhead);
        Serial.printf("Theoretical estimate: 0.1%% (actual: %.3f%%)\n", cpuOverhead);
    } else {
        Serial.println("No validation calls recorded yet");
    }
    
    Serial.println("=======================================\n");
}

} // namespace system
} // namespace core
} // namespace lightwaveos

