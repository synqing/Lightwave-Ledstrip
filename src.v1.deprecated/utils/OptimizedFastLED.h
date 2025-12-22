#ifndef OPTIMIZED_FASTLED_H
#define OPTIMIZED_FASTLED_H

#include <FastLED.h>
#include "PerformanceHacks.h"

/**
 * Optimized FastLED Operations
 * 
 * These replace standard FastLED functions with ultra-optimized versions
 * that trade safety for maximum performance.
 */

class OptimizedFastLED {
private:
    static uint32_t lastShowTime;
    static uint32_t targetFrameTime;
    static bool useAsyncShow;
    
public:
    // Initialize optimized FastLED
    static void begin() {
        // Configure FastLED for maximum performance
        FastLED.setMaxRefreshRate(0);  // No refresh rate limit
        FastLED.setDither(0);          // Disable dithering for speed
        FastLED.setCorrection(UncorrectedColor);  // No color correction
        FastLED.setTemperature(UncorrectedTemperature);  // No temperature correction
        
        targetFrameTime = 8333;  // 120 FPS target
        lastShowTime = 0;
        useAsyncShow = false;
    }
    
    // Ultra-fast show with frame rate control
    static ALWAYS_INLINE void show() {
        // Skip frame if we're running too fast (impossible at 120 FPS but just in case)
        uint32_t now = micros();
        uint32_t elapsed = now - lastShowTime;
        
        if (UNLIKELY(elapsed < targetFrameTime)) {
            return;  // Skip this frame
        }
        
        // Disable interrupts during critical LED update
        {
            NoInterrupts noInt;
            FastLED.show();
        }
        
        lastShowTime = now;
    }
    
    // Show without any timing checks - maximum speed
    static ALWAYS_INLINE void showNow() {
        NoInterrupts noInt;
        FastLED.show();
    }
    
    // Ultra-fast clear
    static ALWAYS_INLINE void clear(CRGB* leds, uint16_t count) {
        // memset is highly optimized on ESP32
        memset(leds, 0, count * sizeof(CRGB));
    }
    
    // Ultra-fast fill
    static ALWAYS_INLINE void fill(CRGB* leds, uint16_t count, CRGB color) {
        UltraFastPixelOps::fastFill(leds, color, count);
    }
    
    // Ultra-fast fade to black
    static ALWAYS_INLINE void fadeToBlack(CRGB* leds, uint16_t count, uint8_t fadeBy) {
        if (fadeBy == 255) {
            clear(leds, count);
            return;
        }
        
        uint8_t scale = 255 - fadeBy;
        UltraFastPixelOps::fastScale(leds, scale, count);
    }
    
    // Ultra-fast brightness scaling
    static ALWAYS_INLINE void setBrightness(CRGB* leds, uint16_t count, uint8_t brightness) {
        if (brightness == 255) return;  // No scaling needed
        if (brightness == 0) {
            clear(leds, count);
            return;
        }
        
        UltraFastPixelOps::fastScale(leds, brightness, count);
    }
    
    // Optimized blend for transitions
    static ALWAYS_INLINE void blend(CRGB* out, const CRGB* src, const CRGB* dst, 
                                   uint16_t count, uint8_t amount) {
        if (amount == 0) {
            UltraFastPixelOps::fastCopy32(out, src, count);
            return;
        }
        if (amount == 255) {
            UltraFastPixelOps::fastCopy32(out, dst, count);
            return;
        }
        
        // Optimized blend loop
        uint8_t srcAmount = 255 - amount;
        
        // Process 4 pixels at once
        while (count >= 4) {
            // Prefetch next pixels
            PREFETCH(src + 4);
            PREFETCH(dst + 4);
            
            // Blend 4 pixels
            for (int i = 0; i < 4; i++) {
                out[i].r = (src[i].r * srcAmount + dst[i].r * amount) >> 8;
                out[i].g = (src[i].g * srcAmount + dst[i].g * amount) >> 8;
                out[i].b = (src[i].b * srcAmount + dst[i].b * amount) >> 8;
            }
            
            out += 4; src += 4; dst += 4;
            count -= 4;
        }
        
        // Handle remaining
        while (count--) {
            out->r = (src->r * srcAmount + dst->r * amount) >> 8;
            out->g = (src->g * srcAmount + dst->g * amount) >> 8;
            out->b = (src->b * srcAmount + dst->b * amount) >> 8;
            out++; src++; dst++;
        }
    }
    
    // Get optimal frame time for target FPS
    static uint32_t getTargetFrameTime(uint16_t targetFPS) {
        return 1000000 / targetFPS;
    }
    
    // Set target FPS
    static void setTargetFPS(uint16_t fps) {
        targetFrameTime = getTargetFrameTime(fps);
    }
};

// Static member initialization
uint32_t OptimizedFastLED::lastShowTime = 0;
uint32_t OptimizedFastLED::targetFrameTime = 8333;
bool OptimizedFastLED::useAsyncShow = false;

// Macros to replace standard FastLED calls
#ifdef USE_OPTIMIZED_FASTLED
    #define FastLED_show() OptimizedFastLED::show()
    #define FastLED_clear() OptimizedFastLED::clear(leds, NUM_LEDS)
    #define fill_solid(leds, count, color) OptimizedFastLED::fill(leds, count, color)
    #define fadeToBlackBy(leds, count, fadeBy) OptimizedFastLED::fadeToBlack(leds, count, fadeBy)
#endif

#endif // OPTIMIZED_FASTLED_H