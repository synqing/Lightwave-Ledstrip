# Light Crystals System - Comprehensive Heap Trace Analysis

## Overview

This document provides detailed instructions for conducting comprehensive memory analysis on the Light Crystals system to identify memory leaks, fragmentation issues, and optimization opportunities.

## Memory Analysis Tools

### HeapTracer System
The `HeapTracer` class provides comprehensive memory monitoring capabilities:

- **Real-time allocation tracking** with source location tracking
- **Memory leak detection** with automatic baseline comparison
- **Heap fragmentation analysis** with optimization recommendations
- **ESP-IDF heap trace integration** for low-level debugging
- **Performance impact monitoring** with allocation failure tracking

### Memory Analysis Test Suite
The `MemoryAnalysisTest` class provides systematic testing scenarios:

1. **Baseline System** - Normal operation without effects
2. **Basic Effects** - Standard LED effect calculations
3. **Light Guide Effects** - CRITICAL TEST for stack overflow detection
4. **FastLED Operations** - LED buffer and display operations
5. **Effect Transitions** - Transition buffer allocation/deallocation
6. **Encoder Operations** - I2C communication patterns
7. **Stress Test** - Rapid allocation/deallocation patterns
8. **Fragmentation Test** - Deliberate fragmentation creation and analysis

## Running Memory Analysis

### Step 1: Prepare for Testing

1. **Enable Memory Debugging**:
   ```cpp
   // In src/config/features.h
   #define FEATURE_MEMORY_DEBUG 1
   ```

2. **Use Memory Debug Build Environment**:
   ```bash
   pio run -e memory_debug -t upload
   pio run -e memory_debug -t monitor
   ```

3. **Alternative: Replace main.cpp temporarily**:
   ```bash
   # Backup current main.cpp
   cp src/main.cpp src/main.cpp.backup
   
   # Use memory test main
   cp src/test/memory_test_main.cpp src/main.cpp
   
   # Build and upload
   pio run -t upload -t monitor
   
   # Restore original main.cpp when done
   mv src/main.cpp.backup src/main.cpp
   ```

### Step 2: Monitor Serial Output

The memory analysis will generate comprehensive output:

```
█████████████████████████████████████████████████
█       LIGHT CRYSTALS MEMORY ANALYSIS         █
█              TEST SUITE                       █
█████████████████████████████████████████████████

ESP32-S3 Board: ESP32S3 Dev Module
CPU Frequency: 240 MHz
Flash Size: 16777216 bytes
Free Heap: 298234 bytes
PSRAM Found: 8388608 bytes
Free PSRAM: 8388608 bytes

=== HeapTracer: Initializing Memory Monitoring ===
Baseline - Heap: 298234 bytes, PSRAM: 8388608 bytes
Heap fragmentation: 12.34%
HeapTracer: Memory monitoring active

▶️  Running Test: Baseline System
Expected Heap Usage: 50000 bytes, Expect Leaks: NO
✅ Test Complete: Baseline System
   Duration: 10234 ms
   Heap Delta: -512 bytes

⚠️  Running Test: Light Guide Effects (CRITICAL TEST)
   ⚠️  INITIALIZING LIGHT GUIDE EFFECTS - EXPECT ISSUES!
   💥 STACK OVERFLOW DETECTED - SYSTEM RESET IMMINENT!
```

### Step 3: Analyze Results

#### Critical Issues to Look For

1. **Stack Overflow Indicators**:
   ```
   💥 EXCEPTION CAUGHT in Light Guide Effects!
   MALLOC FAILED: 51200 bytes
   ⚠️  SIGNIFICANT HEAP CHANGE: -51200 bytes
   ```

2. **Memory Leak Detection**:
   ```
   ⚠️  POTENTIAL MEMORY LEAK: 2048 bytes lost from baseline
   Oldest active allocations:
     ID:15 Size:1024 bytes Age:45.234s LightGuideBase.h:76 in calculateInterference()
     ID:23 Size:512 bytes Age:23.456s PlasmaFieldEffect.h:134 in updateParticles()
   ```

3. **Heap Fragmentation Issues**:
   ```
   ⚠️  HIGH HEAP FRAGMENTATION: 67.89%
   🔧 HIGH FRAGMENTATION DETECTED:
      • Consider using memory pools for frequent allocations
      • Reduce allocation/deallocation frequency
   ```

4. **Allocation Failures**:
   ```
   ❌ Allocation failed at iteration 456 (size: 1024)
   🔴 CRITICAL: 15 allocation failures detected
      → Move large buffers to PSRAM immediately
   ```

## Expected Test Results and Issues

### Baseline System Test
**Expected Result**: ✅ PASS
- Heap delta: <500 bytes
- No allocation failures
- Fragmentation: <20%

### Basic Effects Test  
**Expected Result**: ✅ PASS
- Heap delta: <1KB
- Minimal allocations
- Stable memory usage

### Light Guide Effects Test
**Expected Result**: ❌ CRITICAL FAILURE
- **Stack overflow imminent** - 51.2KB allocation on 12KB stack
- System reset or crash expected
- This test validates the critical issues identified

### Stress Test Results
**Expected Results**:
- Some allocation failures under extreme load
- Fragmentation increase during rapid alloc/free cycles
- Recovery after stress period

## Memory Optimization Recommendations

### Immediate Actions Required

1. **Move Large Buffers to PSRAM**:
   ```cpp
   // BEFORE (DANGEROUS):
   float interference_map[160][80];  // 51.2KB on stack!
   
   // AFTER (SAFE):
   float* interference_map = (float*)ps_malloc(160 * 80 * sizeof(float));
   ```

2. **Replace Raw Pointers with Smart Pointers**:
   ```cpp
   // BEFORE (LEAK RISK):
   StandingWaveEffect* standingWave = new StandingWaveEffect();
   
   // AFTER (SAFE):
   std::unique_ptr<StandingWaveEffect> standingWave = 
       std::make_unique<StandingWaveEffect>();
   ```

3. **Implement Memory Pools**:
   ```cpp
   // For frequent small allocations
   class ParticlePool {
       alignas(Particle) uint8_t pool[MAX_PARTICLES * sizeof(Particle)];
       std::bitset<MAX_PARTICLES> allocated;
   };
   ```

### Performance Optimizations

1. **Reduce Allocation Frequency**:
   - Cache calculated values instead of reallocating
   - Use static buffers for temporary calculations
   - Implement lazy initialization for large objects

2. **PSRAM Utilization Strategy**:
   ```cpp
   // Priority order for PSRAM allocation:
   // 1. Interference calculation maps (51.2KB each)
   // 2. LED strip buffers (960 bytes each)
   // 3. Particle system buffers
   // 4. Transition buffers
   // 5. Temporary calculation arrays
   ```

3. **Memory Pool Implementation**:
   ```cpp
   template<typename T, size_t N>
   class ObjectPool {
   private:
       alignas(T) uint8_t storage[N * sizeof(T)];
       std::bitset<N> used;
       
   public:
       T* acquire() {
           for (size_t i = 0; i < N; ++i) {
               if (!used[i]) {
                   used.set(i);
                   return new(storage + i * sizeof(T)) T();
               }
           }
           return nullptr;
       }
       
       void release(T* obj) {
           if (obj) {
               obj->~T();
               size_t index = (reinterpret_cast<uint8_t*>(obj) - storage) / sizeof(T);
               used.reset(index);
           }
       }
   };
   ```

## Advanced Analysis Techniques

### Stack Usage Analysis
```cpp
// Monitor stack high water mark
void checkStackUsage() {
    UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("Stack free: %d words (%d bytes)\n", 
                 stackHighWaterMark, stackHighWaterMark * 4);
    
    if (stackHighWaterMark < 512) { // Less than 2KB free
        Serial.println("⚠️  LOW STACK WARNING");
    }
}
```

### Real-time Memory Monitoring
```cpp
// Continuous monitoring during operation
void monitorMemoryHealth() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 1000) {
        lastCheck = millis();
        
        size_t freeHeap = ESP.getFreeHeap();
        size_t freePsram = ESP.getFreePsram();
        
        // Log significant changes
        static size_t lastHeap = freeHeap;
        if (abs((int32_t)freeHeap - (int32_t)lastHeap) > 1024) {
            Serial.printf("Heap change: %+d bytes\n", 
                         (int32_t)freeHeap - (int32_t)lastHeap);
            lastHeap = freeHeap;
        }
        
        // Alert on critical conditions
        if (freeHeap < 50000) {
            Serial.println("🔴 CRITICAL: Low heap memory");
        }
        
        if (psramFound() && freePsram < 1000000) {
            Serial.println("🟠 WARNING: Low PSRAM");
        }
    }
}
```

### Memory Leak Detection Algorithm
```cpp
// Automated leak detection
class LeakDetector {
private:
    struct AllocationRecord {
        void* ptr;
        size_t size;
        uint32_t timestamp;
        const char* source;
    };
    
    std::vector<AllocationRecord> longLivedAllocations;
    
public:
    void checkForLeaks() {
        uint32_t now = millis();
        
        // Find allocations older than 30 seconds
        for (const auto& record : longLivedAllocations) {
            if (now - record.timestamp > 30000) {
                Serial.printf("Potential leak: %p (%d bytes) from %s\n",
                             record.ptr, record.size, record.source);
            }
        }
    }
};
```

## Troubleshooting Common Issues

### Issue: System Resets During Memory Test
**Cause**: Stack overflow from large stack allocations
**Solution**: 
- Increase stack size temporarily: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768`
- Move allocations to heap immediately

### Issue: Allocation Failures Despite Available Memory
**Cause**: Heap fragmentation
**Solutions**:
- Implement memory pools for frequent allocations
- Reduce allocation size variance
- Use PSRAM for large buffers

### Issue: Memory Usage Continuously Increases
**Cause**: Memory leaks from missing deallocations
**Solutions**:
- Use smart pointers for automatic cleanup
- Implement RAII patterns
- Add explicit cleanup in destructors

## Integration with Production Code

### Conditional Memory Monitoring
```cpp
#if FEATURE_MEMORY_DEBUG
    g_heapTracer.update();
    
    static uint32_t lastMemCheck = 0;
    if (millis() - lastMemCheck > 10000) {
        g_heapTracer.analyzeNow();
        lastMemCheck = millis();
    }
#endif
```

### Production Memory Safety
```cpp
// Safe allocation with fallback
void* safeAllocate(size_t size, const char* context) {
    void* ptr = nullptr;
    
    // Try PSRAM first for large allocations
    if (size > 1024 && psramFound()) {
        ptr = ps_malloc(size);
        if (ptr) {
            Serial.printf("Allocated %d bytes in PSRAM for %s\n", size, context);
            return ptr;
        }
    }
    
    // Fallback to heap
    ptr = malloc(size);
    if (ptr) {
        Serial.printf("Allocated %d bytes in heap for %s\n", size, context);
    } else {
        Serial.printf("⚠️  ALLOCATION FAILED: %d bytes for %s\n", size, context);
        
        // Emergency cleanup
        emergencyMemoryCleanup();
        
        // Retry once
        ptr = malloc(size);
    }
    
    return ptr;
}
```

## Conclusion

The heap trace analysis system provides comprehensive tools for identifying and resolving memory issues in the Light Crystals system. The expected critical failure in the Light Guide Effects test validates the stack overflow risk identified in the stability analysis.

**Key Takeaways**:
1. **Stack overflow is imminent** with current light guide implementation
2. **PSRAM allocation is essential** for large buffers
3. **Smart pointers reduce leak risks** significantly
4. **Memory pools improve performance** and reduce fragmentation
5. **Continuous monitoring enables early detection** of issues

Implementing the recommendations from this analysis will transform the system from experimental prototype to production-ready embedded application with enterprise-grade memory management.