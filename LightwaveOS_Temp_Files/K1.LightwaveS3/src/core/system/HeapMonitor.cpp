/**
 * @file HeapMonitor.cpp
 * @brief Heap corruption detection and monitoring implementation
 */

#define LW_LOG_TAG "HeapMonitor"
#include "HeapMonitor.h"
#include "../../utils/Log.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_heap_trace.h>

// MALLOC_CAP_DEFAULT for heap_caps_get_largest_free_block
#ifndef MALLOC_CAP_DEFAULT
#define MALLOC_CAP_DEFAULT (1 << 0)
#endif
#endif

namespace lightwaveos {
namespace core {
namespace system {

// Static members
bool HeapMonitor::s_initialized = false;
size_t HeapMonitor::s_minFreeHeap = SIZE_MAX;

void HeapMonitor::init() {
    if (s_initialized) {
        return;
    }

#ifndef NATIVE_BUILD
    // Get initial heap state
    s_minFreeHeap = esp_get_free_heap_size();
    
    LW_LOGI("Initialized (heap corruption detection enabled)");
    LW_LOGI("Initial free heap: %u bytes", s_minFreeHeap);
    LW_LOGI("Largest free block: %u bytes", getLargestFreeBlock());
#endif
    
    s_initialized = true;
}

bool HeapMonitor::checkHeapIntegrity() {
    if (!s_initialized) {
        return false;
    }

#ifndef NATIVE_BUILD
    // ESP-IDF heap corruption detection is enabled via CONFIG_HEAP_CORRUPTION_DETECTION
    // The heap allocator will automatically detect corruption and call our hook.
    // This function performs a manual check by querying heap state.
    
    size_t freeHeap = esp_get_free_heap_size();
    size_t largestBlock = getLargestFreeBlock();
    
    // Update minimum free heap
    if (freeHeap < s_minFreeHeap) {
        s_minFreeHeap = freeHeap;
    }
    
    // Basic sanity check: largest block should not exceed free heap
    if (largestBlock > freeHeap) {
        LW_LOGE("Heap integrity check failed: largest block (%u) > free heap (%u)",
               largestBlock, freeHeap);
        return false;
    }
    
    // Check for critically low heap
    if (freeHeap < 10000) {  // Less than 10KB free
        LW_LOGW("Heap critically low: %u bytes free", freeHeap);
    }
#endif
    
    return true;
}

size_t HeapMonitor::getFreeHeap() {
#ifndef NATIVE_BUILD
    return esp_get_free_heap_size();
#else
    return 0;
#endif
}

size_t HeapMonitor::getMinFreeHeap() {
    return s_minFreeHeap;
}

size_t HeapMonitor::getLargestFreeBlock() {
#ifndef NATIVE_BUILD
    return heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
#else
    return 0;
#endif
}

uint8_t HeapMonitor::getFragmentationPercent() {
#ifndef NATIVE_BUILD
    size_t freeHeap = getFreeHeap();
    size_t largestBlock = getLargestFreeBlock();
    
    if (freeHeap == 0) {
        return 100;  // Fully fragmented
    }
    
    // Fragmentation = 100 - (largest_block / total_free * 100)
    uint8_t fragmentation = static_cast<uint8_t>(
        100 - ((largestBlock * 100) / freeHeap)
    );
    
    return fragmentation;
#else
    return 0;
#endif
}

void HeapMonitor::onHeapCorruption() {
    // CRITICAL: Heap corruption detected!
    LW_LOGE("HEAP CORRUPTION DETECTED!");
    LW_LOGE("Free heap: %u bytes", getFreeHeap());
    LW_LOGE("Largest block: %u bytes", getLargestFreeBlock());
    LW_LOGE("Fragmentation: %d%%", getFragmentationPercent());
    
    // Attempt to get stack trace if available
    // Note: Stack trace may not be available if heap is too corrupted
    
    // Trigger watchdog reset or enter safe mode
    // For now, we'll rely on ESP-IDF to handle the reset
    // In production, you might want to:
    // 1. Save crash context to RTC memory
    // 2. Enter safe mode (minimal operation)
    // 3. Trigger watchdog reset
    
    // ESP-IDF will reset the system after this hook returns
}

void HeapMonitor::onMallocFailed(size_t size) {
    // Malloc failed - critical memory issue
    LW_LOGE("MALLOC FAILED: Requested %u bytes", size);
    LW_LOGE("Free heap: %u bytes", getFreeHeap());
    LW_LOGE("Largest block: %u bytes", getLargestFreeBlock());
    LW_LOGE("Fragmentation: %d%%", getFragmentationPercent());
    LW_LOGE("Min free heap ever: %u bytes", s_minFreeHeap);
    
    // Attempt emergency cleanup
    // Note: Limited options here since we can't allocate memory
    
    // Trigger watchdog reset or enter safe mode
    // FreeRTOS will reset the system after this hook returns
}

} // namespace system
} // namespace core
} // namespace lightwaveos

// FreeRTOS/ESP-IDF hook implementations (C linkage required)
extern "C" void heap_corruption_hook(void) {
    lightwaveos::core::system::HeapMonitor::onHeapCorruption();
}

extern "C" void vApplicationMallocFailedHook(void) {
    // Note: FreeRTOS doesn't pass size, but we can still log
    lightwaveos::core::system::HeapMonitor::onMallocFailed(0);
}

