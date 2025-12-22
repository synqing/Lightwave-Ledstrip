#ifndef HEAP_TRACER_H
#define HEAP_TRACER_H

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_heap_trace.h>
#include "config/features.h"

#if FEATURE_MEMORY_DEBUG

// Heap allocation tracking structure
struct HeapAllocation {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* function;
    uint32_t timestamp;
    uint32_t allocation_id;
    bool active;
};

// Memory usage statistics
struct MemoryStats {
    // Heap statistics
    size_t total_heap_size;
    size_t free_heap_size;
    size_t min_free_heap_size;
    size_t largest_free_block;
    
    // PSRAM statistics
    size_t total_psram_size;
    size_t free_psram_size;
    size_t min_free_psram_size;
    
    // Allocation tracking
    uint32_t total_allocations;
    uint32_t active_allocations;
    uint32_t peak_allocations;
    size_t total_allocated_bytes;
    size_t peak_allocated_bytes;
    
    // Fragmentation metrics
    float heap_fragmentation;
    float psram_fragmentation;
    
    // Performance metrics
    uint32_t allocation_failures;
    uint32_t large_block_requests; // >1KB
    uint32_t small_block_requests; // <256B
};

class HeapTracer {
private:
    static constexpr size_t MAX_TRACKED_ALLOCATIONS = 100;
    static constexpr size_t HEAP_TRACE_RECORDS = 50;
    
    // Allocation tracking
    HeapAllocation tracked_allocations[MAX_TRACKED_ALLOCATIONS];
    uint32_t next_allocation_id = 1;
    uint32_t tracked_count = 0;
    
    // Memory statistics
    MemoryStats current_stats;
    MemoryStats baseline_stats;
    
    // Heap trace buffer for ESP-IDF tracing
    heap_trace_record_t trace_records[HEAP_TRACE_RECORDS];
    
    // Monitoring state
    bool tracing_enabled = false;
    uint32_t last_report_time = 0;
    uint32_t report_interval_ms = 10000; // 10 seconds
    
    // Memory leak detection
    uint32_t leak_check_interval = 60000; // 1 minute
    uint32_t last_leak_check = 0;
    size_t baseline_free_heap = 0;
    
public:
    HeapTracer() {
        memset(tracked_allocations, 0, sizeof(tracked_allocations));
        memset(&current_stats, 0, sizeof(current_stats));
        memset(&baseline_stats, 0, sizeof(baseline_stats));
    }
    
    // Initialize heap tracing
    void begin() {
        Serial.println("\n=== HeapTracer: Initializing Memory Monitoring ===");
        
        // Initialize ESP-IDF heap tracing
        ESP_ERROR_CHECK(heap_trace_init_standalone(trace_records, HEAP_TRACE_RECORDS));
        
        // Capture baseline statistics
        updateMemoryStats();
        baseline_stats = current_stats;
        baseline_free_heap = ESP.getFreeHeap();
        
        tracing_enabled = true;
        
        Serial.printf("Baseline - Heap: %d bytes, PSRAM: %d bytes\n", 
                     baseline_stats.free_heap_size, baseline_stats.free_psram_size);
        Serial.printf("Heap fragmentation: %.2f%%\n", baseline_stats.heap_fragmentation);
        
        // Start heap trace capture
        ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_ALL));
        
        Serial.println("HeapTracer: Memory monitoring active");
    }
    
    // Stop heap tracing
    void end() {
        if (tracing_enabled) {
            heap_trace_stop();
            generateFinalReport();
            tracing_enabled = false;
            Serial.println("HeapTracer: Monitoring stopped");
        }
    }
    
    // Update memory statistics
    void updateMemoryStats() {
        // Get heap information
        multi_heap_info_t heap_info;
        heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);
        
        current_stats.total_heap_size = heap_info.total_allocated_bytes + heap_info.total_free_bytes;
        current_stats.free_heap_size = heap_info.total_free_bytes;
        current_stats.largest_free_block = heap_info.largest_free_block;
        current_stats.min_free_heap_size = heap_info.minimum_free_bytes;
        
        // Calculate heap fragmentation
        if (current_stats.free_heap_size > 0) {
            current_stats.heap_fragmentation = 
                (1.0f - (float)current_stats.largest_free_block / current_stats.free_heap_size) * 100.0f;
        }
        
        // Get PSRAM information
        if (psramFound()) {
            multi_heap_info_t psram_info;
            heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM);
            
            current_stats.total_psram_size = psram_info.total_allocated_bytes + psram_info.total_free_bytes;
            current_stats.free_psram_size = psram_info.total_free_bytes;
            current_stats.min_free_psram_size = psram_info.minimum_free_bytes;
            
            if (current_stats.free_psram_size > 0) {
                current_stats.psram_fragmentation = 
                    (1.0f - (float)psram_info.largest_free_block / current_stats.free_psram_size) * 100.0f;
            }
        }
        
        // Update peak values
        if (current_stats.active_allocations > current_stats.peak_allocations) {
            current_stats.peak_allocations = current_stats.active_allocations;
        }
        
        if (current_stats.total_allocated_bytes > current_stats.peak_allocated_bytes) {
            current_stats.peak_allocated_bytes = current_stats.total_allocated_bytes;
        }
    }
    
    // Track memory allocation
    void trackAllocation(void* ptr, size_t size, const char* file, int line, const char* function) {
        if (!tracing_enabled || !ptr) return;
        
        // Find free slot
        for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
            if (!tracked_allocations[i].active) {
                tracked_allocations[i].ptr = ptr;
                tracked_allocations[i].size = size;
                tracked_allocations[i].file = file;
                tracked_allocations[i].line = line;
                tracked_allocations[i].function = function;
                tracked_allocations[i].timestamp = millis();
                tracked_allocations[i].allocation_id = next_allocation_id++;
                tracked_allocations[i].active = true;
                
                tracked_count++;
                current_stats.total_allocations++;
                current_stats.active_allocations++;
                current_stats.total_allocated_bytes += size;
                
                // Track allocation size categories
                if (size > 1024) {
                    current_stats.large_block_requests++;
                } else if (size < 256) {
                    current_stats.small_block_requests++;
                }
                
                break;
            }
        }
    }
    
    // Track memory deallocation
    void trackDeallocation(void* ptr) {
        if (!tracing_enabled || !ptr) return;
        
        // Find and remove allocation
        for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
            if (tracked_allocations[i].active && tracked_allocations[i].ptr == ptr) {
                current_stats.active_allocations--;
                current_stats.total_allocated_bytes -= tracked_allocations[i].size;
                tracked_allocations[i].active = false;
                tracked_count--;
                break;
            }
        }
    }
    
    // Periodic monitoring update
    void update() {
        if (!tracing_enabled) return;
        
        uint32_t now = millis();
        
        // Update statistics
        updateMemoryStats();
        
        // Periodic reporting
        if (now - last_report_time >= report_interval_ms) {
            generatePeriodicReport();
            last_report_time = now;
        }
        
        // Memory leak detection
        if (now - last_leak_check >= leak_check_interval) {
            checkForMemoryLeaks();
            last_leak_check = now;
        }
    }
    
    // Generate periodic memory report
    void generatePeriodicReport() {
        Serial.println("\n=== HeapTracer: Memory Status Report ===");
        
        // Current memory status
        Serial.printf("Heap Usage: %d / %d bytes (%.1f%% used)\n",
                     current_stats.total_heap_size - current_stats.free_heap_size,
                     current_stats.total_heap_size,
                     (1.0f - (float)current_stats.free_heap_size / current_stats.total_heap_size) * 100.0f);
        
        Serial.printf("Largest Free Block: %d bytes\n", current_stats.largest_free_block);
        Serial.printf("Heap Fragmentation: %.2f%%\n", current_stats.heap_fragmentation);
        
        if (psramFound()) {
            Serial.printf("PSRAM Usage: %d / %d bytes (%.1f%% used)\n",
                         current_stats.total_psram_size - current_stats.free_psram_size,
                         current_stats.total_psram_size,
                         (1.0f - (float)current_stats.free_psram_size / current_stats.total_psram_size) * 100.0f);
            Serial.printf("PSRAM Fragmentation: %.2f%%\n", current_stats.psram_fragmentation);
        }
        
        // Allocation statistics
        Serial.printf("Active Allocations: %d (Peak: %d)\n", 
                     current_stats.active_allocations, current_stats.peak_allocations);
        Serial.printf("Total Allocated: %d bytes (Peak: %d bytes)\n",
                     current_stats.total_allocated_bytes, current_stats.peak_allocated_bytes);
        
        // Performance metrics
        Serial.printf("Large Blocks (>1KB): %d, Small Blocks (<256B): %d\n",
                     current_stats.large_block_requests, current_stats.small_block_requests);
        
        if (current_stats.allocation_failures > 0) {
            Serial.printf("⚠️  ALLOCATION FAILURES: %d\n", current_stats.allocation_failures);
        }
        
        // Memory delta from baseline
        int32_t heap_delta = (int32_t)current_stats.free_heap_size - (int32_t)baseline_stats.free_heap_size;
        Serial.printf("Heap Delta from Baseline: %+d bytes\n", heap_delta);
        
        Serial.println("=====================================");
    }
    
    // Check for memory leaks
    void checkForMemoryLeaks() {
        size_t current_free = ESP.getFreeHeap();
        int32_t leak_delta = (int32_t)baseline_free_heap - (int32_t)current_free;
        
        if (leak_delta > 1024) { // More than 1KB difference
            Serial.printf("⚠️  POTENTIAL MEMORY LEAK: %d bytes lost from baseline\n", leak_delta);
            
            // Show top allocations by age
            Serial.println("Oldest active allocations:");
            showOldestAllocations(5);
        }
        
        // Check for fragmentation issues
        if (current_stats.heap_fragmentation > 50.0f) {
            Serial.printf("⚠️  HIGH HEAP FRAGMENTATION: %.2f%%\n", current_stats.heap_fragmentation);
        }
    }
    
    // Show oldest allocations (potential leaks)
    void showOldestAllocations(uint8_t count) {
        uint32_t current_time = millis();
        
        // Simple bubble sort by age (oldest first)
        HeapAllocation* oldest[MAX_TRACKED_ALLOCATIONS];
        uint8_t oldest_count = 0;
        
        for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS && oldest_count < count; i++) {
            if (tracked_allocations[i].active) {
                oldest[oldest_count++] = &tracked_allocations[i];
            }
        }
        
        // Sort by timestamp (oldest first)
        for (uint8_t i = 0; i < oldest_count - 1; i++) {
            for (uint8_t j = 0; j < oldest_count - i - 1; j++) {
                if (oldest[j]->timestamp > oldest[j + 1]->timestamp) {
                    HeapAllocation* temp = oldest[j];
                    oldest[j] = oldest[j + 1];
                    oldest[j + 1] = temp;
                }
            }
        }
        
        // Display oldest allocations
        for (uint8_t i = 0; i < oldest_count; i++) {
            uint32_t age_ms = current_time - oldest[i]->timestamp;
            Serial.printf("  ID:%d Size:%d bytes Age:%d.%03ds %s:%d in %s()\n",
                         oldest[i]->allocation_id,
                         oldest[i]->size,
                         age_ms / 1000, age_ms % 1000,
                         oldest[i]->file, oldest[i]->line, oldest[i]->function);
        }
    }
    
    // Generate comprehensive analysis report
    void generateAnalysisReport() {
        Serial.println("\n=== HeapTracer: Comprehensive Analysis ===");
        
        generatePeriodicReport();
        
        // ESP-IDF heap trace summary
        Serial.println("\nESP-IDF Heap Trace Summary:");
        heap_trace_dump();
        
        // Show all active allocations
        Serial.printf("\nAll Active Allocations (%d):\n", current_stats.active_allocations);
        for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
            if (tracked_allocations[i].active) {
                uint32_t age = millis() - tracked_allocations[i].timestamp;
                Serial.printf("  %s:%d %s() - %d bytes (ID:%d, Age:%ds)\n",
                             tracked_allocations[i].file,
                             tracked_allocations[i].line,
                             tracked_allocations[i].function,
                             tracked_allocations[i].size,
                             tracked_allocations[i].allocation_id,
                             age / 1000);
            }
        }
        
        // Memory optimization recommendations
        generateOptimizationRecommendations();
        
        Serial.println("==========================================");
    }
    
    // Generate optimization recommendations
    void generateOptimizationRecommendations() {
        Serial.println("\n--- Memory Optimization Recommendations ---");
        
        // Fragmentation analysis
        if (current_stats.heap_fragmentation > 30.0f) {
            Serial.println("🔧 HIGH FRAGMENTATION DETECTED:");
            Serial.println("   • Consider using memory pools for frequent allocations");
            Serial.println("   • Reduce allocation/deallocation frequency");
            Serial.println("   • Use fixed-size buffers where possible");
        }
        
        // Large allocation analysis
        if (current_stats.large_block_requests > 10) {
            Serial.println("🔧 LARGE ALLOCATIONS DETECTED:");
            Serial.printf("   • %d allocations >1KB found\n", current_stats.large_block_requests);
            Serial.println("   • Consider moving large buffers to PSRAM");
            Serial.println("   • Use static allocation for persistent data");
        }
        
        // PSRAM utilization
        if (psramFound() && current_stats.free_psram_size > current_stats.total_psram_size * 0.8f) {
            Serial.println("🔧 PSRAM UNDERUTILIZED:");
            Serial.println("   • Consider moving LED buffers to PSRAM");
            Serial.println("   • Move large effect buffers to PSRAM");
            Serial.println("   • Use PSRAM for temporary calculations");
        }
        
        // Memory leak indicators
        if (current_stats.active_allocations > 20) {
            Serial.println("⚠️  HIGH ALLOCATION COUNT:");
            Serial.printf("   • %d active allocations detected\n", current_stats.active_allocations);
            Serial.println("   • Review allocation patterns for leaks");
            Serial.println("   • Consider RAII patterns for automatic cleanup");
        }
    }
    
    // Generate final report on shutdown
    void generateFinalReport() {
        Serial.println("\n=== HeapTracer: Final Memory Report ===");
        
        generateAnalysisReport();
        
        // Calculate total memory change
        int32_t final_heap_delta = (int32_t)current_stats.free_heap_size - (int32_t)baseline_stats.free_heap_size;
        
        Serial.printf("\nFinal Memory Delta: %+d bytes\n", final_heap_delta);
        Serial.printf("Peak Memory Usage: %d bytes\n", current_stats.peak_allocated_bytes);
        Serial.printf("Total Allocations: %d\n", current_stats.total_allocations);
        
        if (final_heap_delta < -1024) {
            Serial.println("⚠️  MEMORY LEAK DETECTED: Significant memory loss from baseline");
        } else if (final_heap_delta > 1024) {
            Serial.println("✅ MEMORY IMPROVED: More free memory than baseline");
        } else {
            Serial.println("✅ MEMORY STABLE: Minimal change from baseline");
        }
        
        Serial.println("======================================");
    }
    
    // Get current memory statistics
    const MemoryStats& getStats() const {
        return current_stats;
    }
    
    // Set reporting interval
    void setReportInterval(uint32_t interval_ms) {
        report_interval_ms = interval_ms;
    }
    
    // Manual memory analysis trigger
    void analyzeNow() {
        generateAnalysisReport();
    }
};

// Global heap tracer instance
extern HeapTracer g_heapTracer;

// Memory allocation tracking macros
#define HEAP_TRACE_MALLOC(size) \
    do { \
        void* ptr = malloc(size); \
        g_heapTracer.trackAllocation(ptr, size, __FILE__, __LINE__, __FUNCTION__); \
        return ptr; \
    } while(0)

#define HEAP_TRACE_FREE(ptr) \
    do { \
        g_heapTracer.trackDeallocation(ptr); \
        free(ptr); \
    } while(0)

// Convenience macros for heap monitoring
#define HEAP_CHECKPOINT(name) \
    do { \
        Serial.printf("HEAP[%s]: %d bytes free\n", name, ESP.getFreeHeap()); \
    } while(0)

#define HEAP_TRACE_FUNCTION_ENTRY() \
    size_t heap_before_##__LINE__ = ESP.getFreeHeap()

#define HEAP_TRACE_FUNCTION_EXIT() \
    do { \
        size_t heap_after = ESP.getFreeHeap(); \
        int32_t delta = (int32_t)heap_after - (int32_t)heap_before_##__LINE__; \
        if (abs(delta) > 100) { \
            Serial.printf("HEAP[%s]: %+d bytes\n", __FUNCTION__, delta); \
        } \
    } while(0)

#else // !FEATURE_MEMORY_DEBUG

// Stub definitions when memory debugging is disabled
class HeapTracer {
public:
    void begin() {}
    void end() {}
    void update() {}
    void analyzeNow() {}
    void setReportInterval(uint32_t) {}
};

extern HeapTracer g_heapTracer;

#define HEAP_CHECKPOINT(name)
#define HEAP_TRACE_FUNCTION_ENTRY()
#define HEAP_TRACE_FUNCTION_EXIT()

#endif // FEATURE_MEMORY_DEBUG

#endif // HEAP_TRACER_H