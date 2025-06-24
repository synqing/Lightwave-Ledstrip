#include "HeapTracer.h"

#if FEATURE_MEMORY_DEBUG

// Global heap tracer instance
HeapTracer g_heapTracer;

// Override default memory allocation functions for tracking
extern "C" {
    
void* __real_malloc(size_t size);
void __real_free(void* ptr);
void* __real_realloc(void* ptr, size_t size);
void* __real_calloc(size_t num, size_t size);

void* __wrap_malloc(size_t size) {
    void* ptr = __real_malloc(size);
    if (ptr) {
        g_heapTracer.trackAllocation(ptr, size, "malloc", 0, "system");
    } else {
        // Track allocation failure
        Serial.printf("MALLOC FAILED: %d bytes\n", size);
    }
    return ptr;
}

void __wrap_free(void* ptr) {
    if (ptr) {
        g_heapTracer.trackDeallocation(ptr);
    }
    __real_free(ptr);
}

void* __wrap_realloc(void* ptr, size_t size) {
    if (ptr) {
        g_heapTracer.trackDeallocation(ptr);
    }
    
    void* new_ptr = __real_realloc(ptr, size);
    if (new_ptr) {
        g_heapTracer.trackAllocation(new_ptr, size, "realloc", 0, "system");
    }
    return new_ptr;
}

void* __wrap_calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = __real_calloc(num, size);
    if (ptr) {
        g_heapTracer.trackAllocation(ptr, total_size, "calloc", 0, "system");
    }
    return ptr;
}

} // extern "C"

#else

// Stub implementation when memory debugging is disabled
HeapTracer g_heapTracer;

#endif // FEATURE_MEMORY_DEBUG