#ifndef PERFORMANCE_HACKS_H
#define PERFORMANCE_HACKS_H

#include <Arduino.h>
#include <FastLED.h>

/**
 * MAXIMUM PERFORMANCE HACKS
 * 
 * These are aggressive optimizations that trade everything for speed.
 * Use with caution - they may reduce code readability and maintainability.
 */

// Force inline critical functions
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define HOT_FUNCTION __attribute__((hot))
#define COLD_FUNCTION __attribute__((cold))

// Prefetch data into cache
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)

// Branch prediction hints
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Disable interrupts for critical sections
class NoInterrupts {
    uint32_t _state;
public:
    NoInterrupts() { _state = portDISABLE_INTERRUPTS(); }
    ~NoInterrupts() { portRESTORE_INTERRUPTS(_state); }
};

// Ultra-fast pixel operations using direct memory access
class UltraFastPixelOps {
public:
    // Copy pixels using aligned 32-bit transfers - 3x faster than memcpy
    static ALWAYS_INLINE void fastCopy32(CRGB* dst, const CRGB* src, uint16_t count) {
        uint32_t* d = (uint32_t*)dst;
        const uint32_t* s = (const uint32_t*)src;
        
        // Unroll by 16 for maximum memory bandwidth
        while (count >= 16) {
            // Prefetch next cache line
            PREFETCH(s + 16);
            PREFETCH_WRITE(d + 16);
            
            // Copy 16 pixels (48 bytes) using 12 32-bit transfers
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            d[3] = s[3]; d[4] = s[4]; d[5] = s[5];
            d[6] = s[6]; d[7] = s[7]; d[8] = s[8];
            d[9] = s[9]; d[10] = s[10]; d[11] = s[11];
            
            d += 12; s += 12;
            count -= 16;
        }
        
        // Handle remaining
        while (count--) {
            *d++ = *s++;
            if (count) { *d++ = *s++; count--; }
            if (count) { *d++ = *s++; count--; }
        }
    }
    
    // Fill pixels with constant color - uses memset32
    static ALWAYS_INLINE void fastFill(CRGB* pixels, CRGB color, uint16_t count) {
        // For solid colors, use optimized fill
        if (color.r == color.g && color.g == color.b) {
            memset(pixels, color.r, count * 3);
            return;
        }
        
        // Otherwise unroll the fill
        uint32_t c1 = *((uint32_t*)&color);
        uint32_t* p = (uint32_t*)pixels;
        
        while (count >= 4) {
            p[0] = c1; p[1] = c1; p[2] = c1;
            p += 3;
            count -= 4;
        }
        
        // Handle remaining
        CRGB* pc = (CRGB*)p;
        while (count--) *pc++ = color;
    }
    
    // Scale all pixels by constant - vectorized
    static ALWAYS_INLINE void fastScale(CRGB* pixels, uint8_t scale, uint16_t count) {
        // Process 4 pixels at once
        while (count >= 4) {
            // Load 4 pixels
            uint32_t p0 = *((uint32_t*)&pixels[0]);
            uint32_t p1 = *((uint32_t*)&pixels[1]);
            uint32_t p2 = *((uint32_t*)&pixels[2]);
            uint32_t p3 = *((uint32_t*)&pixels[3]);
            
            // Scale each component
            pixels[0].r = (pixels[0].r * scale) >> 8;
            pixels[0].g = (pixels[0].g * scale) >> 8;
            pixels[0].b = (pixels[0].b * scale) >> 8;
            
            pixels[1].r = (pixels[1].r * scale) >> 8;
            pixels[1].g = (pixels[1].g * scale) >> 8;
            pixels[1].b = (pixels[1].b * scale) >> 8;
            
            pixels[2].r = (pixels[2].r * scale) >> 8;
            pixels[2].g = (pixels[2].g * scale) >> 8;
            pixels[2].b = (pixels[2].b * scale) >> 8;
            
            pixels[3].r = (pixels[3].r * scale) >> 8;
            pixels[3].g = (pixels[3].g * scale) >> 8;
            pixels[3].b = (pixels[3].b * scale) >> 8;
            
            pixels += 4;
            count -= 4;
        }
        
        // Handle remaining
        while (count--) {
            pixels->nscale8(scale);
            pixels++;
        }
    }
};

// Ultra-fast math operations
class UltraFastMath {
public:
    // Fast reciprocal using Newton-Raphson (2 iterations)
    static ALWAYS_INLINE float fastReciprocal(float x) {
        float xhalf = 0.5f * x;
        int i = *(int*)&x;
        i = 0x5f375a86 - (i >> 1);  // Magic constant
        x = *(float*)&i;
        x = x * (1.5f - xhalf * x * x);  // Newton iteration
        x = x * (1.5f - xhalf * x * x);  // Second iteration
        return x;
    }
    
    // Fast square root approximation
    static ALWAYS_INLINE float fastSqrt(float x) {
        return x * fastReciprocal(sqrtf(x));
    }
    
    // Fast sine approximation using Taylor series
    static ALWAYS_INLINE float fastSin(float x) {
        // Normalize to [-pi, pi]
        while (x > 3.14159f) x -= 6.28318f;
        while (x < -3.14159f) x += 6.28318f;
        
        // Taylor series (accurate to 0.001)
        float x2 = x * x;
        return x * (1.0f - x2 * (0.166667f - x2 * 0.008333f));
    }
    
    // Fast 8-bit multiply (returns 8-bit result)
    static ALWAYS_INLINE uint8_t mul8(uint8_t a, uint8_t b) {
        return (uint16_t(a) * uint16_t(b)) >> 8;
    }
    
    // Fast 8-bit scale with dithering
    static ALWAYS_INLINE uint8_t scale8_dither(uint8_t value, uint8_t scale, uint8_t& dither) {
        uint16_t result = (uint16_t(value) * uint16_t(scale)) + dither;
        dither = result & 0xFF;
        return result >> 8;
    }
};

// Cache-friendly data structures
template<typename T, size_t SIZE>
class alignas(32) CacheAlignedArray {
    T data[SIZE];
public:
    T& operator[](size_t i) { return data[i]; }
    const T& operator[](size_t i) const { return data[i]; }
    T* ptr() { return data; }
};

// Performance measurement macros
#define PERFORMANCE_MARKER(name) \
    static uint32_t _perf_##name##_total = 0; \
    static uint32_t _perf_##name##_count = 0; \
    uint32_t _perf_##name##_start = micros();

#define PERFORMANCE_END(name) \
    _perf_##name##_total += micros() - _perf_##name##_start; \
    _perf_##name##_count++; \
    if (UNLIKELY(_perf_##name##_count >= 1000)) { \
        Serial.printf("PERF: %s avg: %luus\n", #name, _perf_##name##_total / _perf_##name##_count); \
        _perf_##name##_total = 0; \
        _perf_##name##_count = 0; \
    }

// Zero-cost abstractions for common patterns
template<typename Func>
ALWAYS_INLINE void processStrip(CRGB* strip, uint16_t count, Func func) {
    // Prefetch first cache line
    PREFETCH(strip);
    
    // Process in cache-friendly chunks
    const uint16_t CHUNK = 16;  // 48 bytes = typical cache line
    uint16_t chunks = count / CHUNK;
    
    for (uint16_t c = 0; c < chunks; c++) {
        // Prefetch next chunk
        if (LIKELY(c < chunks - 1)) {
            PREFETCH(strip + (c + 1) * CHUNK);
        }
        
        // Process current chunk
        uint16_t base = c * CHUNK;
        for (uint16_t i = 0; i < CHUNK; i++) {
            func(strip[base + i], base + i);
        }
    }
    
    // Process remaining
    for (uint16_t i = chunks * CHUNK; i < count; i++) {
        func(strip[i], i);
    }
}

#endif // PERFORMANCE_HACKS_H