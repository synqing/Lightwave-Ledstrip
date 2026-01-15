/**
 * @file MemoryLeakDetector.cpp
 * @brief Memory leak detection and tracking implementation
 */

#define LW_LOG_TAG "LeakDetector"
#include "MemoryLeakDetector.h"
#include "../../utils/Log.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace core {
namespace system {

// Static members
MemoryLeakDetector::AllocationRecord MemoryLeakDetector::s_records[MAX_RECORDS];
size_t MemoryLeakDetector::s_recordCount = 0;
uint32_t MemoryLeakDetector::s_leakThresholdMs = 30000;  // 30 seconds default
size_t MemoryLeakDetector::s_baselineHeap = 0;
bool MemoryLeakDetector::s_enabled = true;
bool MemoryLeakDetector::s_initialized = false;

void MemoryLeakDetector::init() {
    if (s_initialized) {
        return;
    }

    // Clear all records
    for (size_t i = 0; i < MAX_RECORDS; i++) {
        s_records[i].active = false;
    }
    s_recordCount = 0;

#ifndef NATIVE_BUILD
    s_baselineHeap = esp_get_free_heap_size();
#endif

    LW_LOGI("Initialized (leak detection enabled)");
    LW_LOGI("Leak threshold: %u seconds", s_leakThresholdMs / 1000);
    LW_LOGI("Baseline heap: %u bytes", s_baselineHeap);

    s_initialized = true;
}

void MemoryLeakDetector::recordAllocation(void* ptr, size_t size, const char* file, int line) {
    if (!s_initialized || !s_enabled || ptr == nullptr) {
        return;
    }

    // Find free slot
    size_t index = MAX_RECORDS;
    for (size_t i = 0; i < MAX_RECORDS; i++) {
        if (!s_records[i].active) {
            index = i;
            break;
        }
    }

    // If no free slot, remove oldest record (FIFO)
    if (index == MAX_RECORDS) {
        // Find oldest active record
        uint32_t oldestTime = UINT32_MAX;
        size_t oldestIndex = 0;
        for (size_t i = 0; i < MAX_RECORDS; i++) {
            if (s_records[i].active && s_records[i].timestamp < oldestTime) {
                oldestTime = s_records[i].timestamp;
                oldestIndex = i;
            }
        }
        index = oldestIndex;
        s_recordCount--;  // Decrement since we're replacing
    }

    // Record allocation
    s_records[index].ptr = ptr;
    s_records[index].size = size;
    s_records[index].timestamp = millis();
    s_records[index].file = file;
    s_records[index].line = line;
    s_records[index].active = true;
    s_recordCount++;
}

void MemoryLeakDetector::recordDeallocation(void* ptr) {
    if (!s_initialized || !s_enabled || ptr == nullptr) {
        return;
    }

    size_t index = findRecord(ptr);
    if (index != MAX_RECORDS) {
        removeRecord(index);
    }
}

void MemoryLeakDetector::scanForLeaks() {
    if (!s_initialized || !s_enabled) {
        return;
    }

    uint32_t now = millis();
    size_t leakCount = 0;
    size_t totalLeakSize = 0;

    for (size_t i = 0; i < MAX_RECORDS; i++) {
        if (s_records[i].active) {
            uint32_t age = now - s_records[i].timestamp;
            if (age > s_leakThresholdMs) {
                leakCount++;
                totalLeakSize += s_records[i].size;
                
                LW_LOGW("Potential leak: %p (%u bytes) from %s:%d (age: %u ms)",
                       s_records[i].ptr,
                       s_records[i].size,
                       s_records[i].file,
                       s_records[i].line,
                       age);
            }
        }
    }

    if (leakCount > 0) {
        LW_LOGW("Detected %u potential leaks (%u bytes total)", leakCount, totalLeakSize);
    }
}

int32_t MemoryLeakDetector::getHeapDelta() {
    if (!s_initialized) {
        return 0;
    }

#ifndef NATIVE_BUILD
    size_t currentHeap = esp_get_free_heap_size();
    int32_t delta = static_cast<int32_t>(s_baselineHeap) - static_cast<int32_t>(currentHeap);
    return delta;
#else
    return 0;
#endif
}

void MemoryLeakDetector::resetBaseline() {
    if (!s_initialized) {
        return;
    }

#ifndef NATIVE_BUILD
    s_baselineHeap = esp_get_free_heap_size();
    LW_LOGI("Baseline heap reset to: %u bytes", s_baselineHeap);
#endif
}

size_t MemoryLeakDetector::findRecord(void* ptr) {
    for (size_t i = 0; i < MAX_RECORDS; i++) {
        if (s_records[i].active && s_records[i].ptr == ptr) {
            return i;
        }
    }
    return MAX_RECORDS;
}

void MemoryLeakDetector::removeRecord(size_t index) {
    if (index >= MAX_RECORDS || !s_records[index].active) {
        return;
    }

    s_records[index].active = false;
    s_recordCount--;
}

} // namespace system
} // namespace core
} // namespace lightwaveos

