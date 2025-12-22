#ifndef PERFORMANCE_OPTIMIZER_H
#define PERFORMANCE_OPTIMIZER_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"

// SINGLE CORE PERFORMANCE OPTIMIZATION
// All audio/visual processing on Core 1 for perfect sync
// Core 0 handles ONLY network/background tasks

class PerformanceOptimizer {
public:
    // Initialize with proper core pinning
    static void init();
    
    // Pin WiFi and network tasks to Core 0
    static void pinNetworkToSystemCore();
    
    // Ensure all audio/visual runs on Core 1
    static void ensureAudioVisualAffinity();
    
    // Performance helpers (single core optimized)
    static void beginFrame();
    static void endFrame();
    static uint32_t getFrameTime() { return frameTime; }
    static float getFPS() { return currentFPS; }
    
    // Memory optimization
    static void* alignedAlloc(size_t size);
    static void alignedFree(void* ptr);
    
    // Cache-friendly operations
    static void prefetchData(const void* addr);
    
private:
    static uint32_t frameStartTime;
    static uint32_t frameTime;
    static float currentFPS;
    static uint8_t frameCount;
};

// Fast math optimizations (single core, no sync needed)
class FastMath {
public:
    // Lookup tables for common operations
    static void initTables();
    
    // Fast approximations
    static inline float fastSin(float x) {
        // Map to 0-1023 range
        int idx = ((int)(x * 162.974662f)) & 1023;
        return sinTable[idx];
    }
    
    static inline float fastCos(float x) {
        return fastSin(x + 1.570796f);  // cos(x) = sin(x + π/2)
    }
    
    static inline uint8_t fastScale8(uint8_t i, uint8_t scale) {
        return ((uint16_t)i * (uint16_t)scale) >> 8;
    }
    
private:
    static float sinTable[1024];
    static bool tablesInitialized;
};

// Memory pool for zero-allocation effects
template<typename T, size_t SIZE>
class MemoryPool {
    T pool[SIZE];
    bool used[SIZE];
    size_t nextFree;
    
public:
    MemoryPool() : nextFree(0) {
        memset(used, 0, sizeof(used));
    }
    
    T* alloc() {
        for(size_t i = 0; i < SIZE; i++) {
            size_t idx = (nextFree + i) % SIZE;
            if (!used[idx]) {
                used[idx] = true;
                nextFree = (idx + 1) % SIZE;
                return &pool[idx];
            }
        }
        return nullptr;  // Pool exhausted
    }
    
    void free(T* ptr) {
        size_t idx = ptr - pool;
        if (idx < SIZE) {
            used[idx] = false;
        }
    }
};

// SIMD-style operations (ESP32 optimized)
class ColorOps {
public:
    // Process 2 pixels at once using 32-bit operations
    static inline void blend2Pixels(uint32_t* dest, uint32_t* src1, uint32_t* src2, uint8_t blend) {
        // Pack 2 RGB pixels into one operation
        uint32_t mask = 0x00FF00FF;
        uint32_t rb1 = *src1 & mask;
        uint32_t ag1 = (*src1 >> 8) & mask;
        uint32_t rb2 = *src2 & mask;
        uint32_t ag2 = (*src2 >> 8) & mask;
        
        uint32_t rb = (rb1 + (((rb2 - rb1) * blend) >> 8)) & mask;
        uint32_t ag = ((ag1 + (((ag2 - ag1) * blend) >> 8)) & mask) << 8;
        
        *dest = rb | ag;
    }
    
    // Fast HSV to RGB using approximations
    static CRGB fastHSV2RGB(uint8_t h, uint8_t s, uint8_t v) {
        // Optimized for common cases
        if (s == 0) return CRGB(v, v, v);  // Gray
        
        uint8_t region = h / 43;
        uint8_t remainder = (h - (region * 43)) * 6;
        
        uint8_t p = (v * (255 - s)) >> 8;
        uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
        
        switch (region) {
            case 0: return CRGB(v, t, p);
            case 1: return CRGB(q, v, p);
            case 2: return CRGB(p, v, t);
            case 3: return CRGB(p, q, v);
            case 4: return CRGB(t, p, v);
            default: return CRGB(v, p, q);
        }
    }
};

#endif // PERFORMANCE_OPTIMIZER_H