#include "PerformanceOptimizer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530718f
#endif

// Static member definitions
uint32_t PerformanceOptimizer::frameStartTime = 0;
uint32_t PerformanceOptimizer::frameTime = 0;
float PerformanceOptimizer::currentFPS = 0;
uint8_t PerformanceOptimizer::frameCount = 0;
float FastMath::sinTable[1024];
bool FastMath::tablesInitialized = false;

void PerformanceOptimizer::init() {
    Serial.println(F("[PERF] Initializing performance optimizer..."));
    
    // Initialize fast math tables
    FastMath::initTables();
    
    // Pin network tasks to Core 0
    pinNetworkToSystemCore();
    
    // Ensure audio/visual on Core 1
    ensureAudioVisualAffinity();
    
    Serial.println(F("[PERF] Single-core A/V pipeline ready!"));
    Serial.println(F("[PERF] Core 0: Network only"));
    Serial.println(F("[PERF] Core 1: All audio/visual processing"));
}

void PerformanceOptimizer::pinNetworkToSystemCore() {
    // This is called from WiFi init to ensure network stays on Core 0
    // The ESP32 Arduino framework already does this correctly
    // We just need to NOT mess with it!
    
    // Get current task handle
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
    // Safely check task name
    if (currentTask != NULL) {
        const char* taskName = pcTaskGetName(currentTask);
        if (taskName != NULL && strstr(taskName, "wifi") != NULL) {
            // Already on correct core
            return;
        }
    }
}

void PerformanceOptimizer::ensureAudioVisualAffinity() {
    // The main Arduino loop() runs on Core 1 by default - perfect!
    // We just need to ensure we NEVER create audio/visual tasks on Core 0
    
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    BaseType_t currentCore = xPortGetCoreID();
    
    if (currentCore == 0) {
        Serial.println(F("[PERF] WARNING: A/V code running on Core 0!"));
        // In production, we'd want to migrate or error here
    }
}

void PerformanceOptimizer::beginFrame() {
    frameStartTime = micros();
}

void PerformanceOptimizer::endFrame() {
    uint32_t now = micros();
    frameTime = now - frameStartTime;
    
    // Update FPS every 30 frames
    frameCount++;
    if (frameCount >= 30) {
        currentFPS = 1000000.0f / frameTime;
        frameCount = 0;
    }
}

void* PerformanceOptimizer::alignedAlloc(size_t size) {
    // Align to 32-byte boundary for cache efficiency
    void* ptr = heap_caps_aligned_alloc(32, size, MALLOC_CAP_8BIT);
    return ptr;
}

void PerformanceOptimizer::alignedFree(void* ptr) {
    heap_caps_free(ptr);
}

void PerformanceOptimizer::prefetchData(const void* addr) {
    // ESP32 doesn't have explicit prefetch, but we can
    // trigger a cache load by reading
    volatile uint32_t dummy = *(uint32_t*)addr;
    (void)dummy;  // Suppress unused warning
}

// ============== Fast Math Implementation ==============

void FastMath::initTables() {
    if (tablesInitialized) return;
    
    Serial.println(F("[PERF] Building fast math lookup tables..."));
    
    // Build sine table
    for (int i = 0; i < 1024; i++) {
        float angle = (i * TWO_PI) / 1024.0f;
        sinTable[i] = sin(angle);
    }
    
    tablesInitialized = true;
    Serial.println(F("[PERF] Fast math tables ready"));
}