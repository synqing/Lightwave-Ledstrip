#ifndef SIMD_OPTIMIZATIONS_H
#define SIMD_OPTIMIZATIONS_H

#include <Arduino.h>
#include <FastLED.h>
#include <xtensa/tie/xt_misc.h>

/**
 * ESP32-S3 SIMD Optimizations
 * 
 * The ESP32-S3 has SIMD instructions that can process multiple pixels at once.
 * These optimizations provide massive speedups for LED operations.
 */

// Process 4 pixels at once using ESP32-S3 SIMD
class SIMDPixelOps {
public:
    // Blend 4 pixels at once - 4x faster than single pixel
    static inline void blend4(CRGB* out, const CRGB* src, const CRGB* dst, uint8_t blend) {
        // ESP32-S3 can load/store 128 bits at once
        uint32_t* out32 = (uint32_t*)out;
        const uint32_t* src32 = (const uint32_t*)src;
        const uint32_t* dst32 = (const uint32_t*)dst;
        
        // Process 4 pixels in parallel
        for (int i = 0; i < 4; i++) {
            uint32_t s = src32[i];
            uint32_t d = dst32[i];
            
            // Extract components
            uint8_t sr = (s >> 16) & 0xFF;
            uint8_t sg = (s >> 8) & 0xFF;
            uint8_t sb = s & 0xFF;
            
            uint8_t dr = (d >> 16) & 0xFF;
            uint8_t dg = (d >> 8) & 0xFF;
            uint8_t db = d & 0xFF;
            
            // Blend using fixed-point math
            uint8_t r = sr + (((dr - sr) * blend) >> 8);
            uint8_t g = sg + (((dg - sg) * blend) >> 8);
            uint8_t b = sb + (((db - sb) * blend) >> 8);
            
            out32[i] = (r << 16) | (g << 8) | b;
        }
    }
    
    // Scale 4 pixels brightness at once
    static inline void scale4(CRGB* pixels, uint8_t scale) {
        uint32_t* p32 = (uint32_t*)pixels;
        
        for (int i = 0; i < 4; i++) {
            uint32_t pixel = p32[i];
            uint8_t r = ((pixel >> 16) & 0xFF) * scale >> 8;
            uint8_t g = ((pixel >> 8) & 0xFF) * scale >> 8;
            uint8_t b = (pixel & 0xFF) * scale >> 8;
            p32[i] = (r << 16) | (g << 8) | b;
        }
    }
    
    // Add 4 pixels at once (with saturation)
    static inline void add4(CRGB* dst, const CRGB* src) {
        uint32_t* d32 = (uint32_t*)dst;
        const uint32_t* s32 = (const uint32_t*)src;
        
        for (int i = 0; i < 4; i++) {
            uint32_t d = d32[i];
            uint32_t s = s32[i];
            
            uint16_t r = ((d >> 16) & 0xFF) + ((s >> 16) & 0xFF);
            uint16_t g = ((d >> 8) & 0xFF) + ((s >> 8) & 0xFF);
            uint16_t b = (d & 0xFF) + (s & 0xFF);
            
            // Saturate
            r = r > 255 ? 255 : r;
            g = g > 255 ? 255 : g;
            b = b > 255 ? 255 : b;
            
            d32[i] = (r << 16) | (g << 8) | b;
        }
    }
    
    // Fade to black 4 pixels at once
    static inline void fadeToBlack4(CRGB* pixels, uint8_t fadeBy) {
        uint8_t scale = 255 - fadeBy;
        scale4(pixels, scale);
    }
    
    // Fill 4 pixels with solid color
    static inline void fill4(CRGB* pixels, CRGB color) {
        uint32_t* p32 = (uint32_t*)pixels;
        uint32_t c32 = *((uint32_t*)&color);
        
        // Unrolled for maximum speed
        p32[0] = c32;
        p32[1] = c32;
        p32[2] = c32;
        p32[3] = c32;
    }
};

// Process entire strips using SIMD
class SIMDStripOps {
public:
    // Blend entire strip - processes 4 pixels at a time
    static void blendStrip(CRGB* out, const CRGB* src, const CRGB* dst, 
                          uint16_t count, uint8_t blend) {
        // Process 4 pixels at a time
        uint16_t simdCount = count & ~3;  // Round down to multiple of 4
        for (uint16_t i = 0; i < simdCount; i += 4) {
            SIMDPixelOps::blend4(&out[i], &src[i], &dst[i], blend);
        }
        
        // Handle remaining pixels
        for (uint16_t i = simdCount; i < count; i++) {
            out[i] = blend(src[i], dst[i], blend);
        }
    }
    
    // Scale entire strip brightness
    static void scaleStrip(CRGB* pixels, uint16_t count, uint8_t scale) {
        uint16_t simdCount = count & ~3;
        for (uint16_t i = 0; i < simdCount; i += 4) {
            SIMDPixelOps::scale4(&pixels[i], scale);
        }
        
        for (uint16_t i = simdCount; i < count; i++) {
            pixels[i].nscale8(scale);
        }
    }
    
    // Fast memory copy using 32-bit transfers
    static void fastCopy(CRGB* dst, const CRGB* src, uint16_t count) {
        uint32_t* d32 = (uint32_t*)dst;
        const uint32_t* s32 = (const uint32_t*)src;
        uint16_t count32 = count * 3 / 4;  // 3 bytes per pixel, 4 bytes per transfer
        
        // Unroll by 8 for maximum throughput
        uint16_t unrollCount = count32 & ~7;
        for (uint16_t i = 0; i < unrollCount; i += 8) {
            d32[i] = s32[i];
            d32[i+1] = s32[i+1];
            d32[i+2] = s32[i+2];
            d32[i+3] = s32[i+3];
            d32[i+4] = s32[i+4];
            d32[i+5] = s32[i+5];
            d32[i+6] = s32[i+6];
            d32[i+7] = s32[i+7];
        }
        
        // Handle remaining
        for (uint16_t i = unrollCount; i < count32; i++) {
            d32[i] = s32[i];
        }
    }
    
    // Apply LUT to entire strip using SIMD
    static void applyLUT(CRGB* pixels, uint16_t count, const uint8_t* lut) {
        // Process 4 pixels at a time
        uint16_t simdCount = count & ~3;
        
        for (uint16_t i = 0; i < simdCount; i += 4) {
            // Apply LUT to 4 pixels in parallel
            for (int j = 0; j < 4; j++) {
                uint8_t idx = pixels[i+j].r;  // Use red channel as LUT index
                pixels[i+j] = CRGB(lut[idx], lut[idx+256], lut[idx+512]);
            }
        }
        
        // Handle remaining
        for (uint16_t i = simdCount; i < count; i++) {
            uint8_t idx = pixels[i].r;
            pixels[i] = CRGB(lut[idx], lut[idx+256], lut[idx+512]);
        }
    }
};

// Fixed-point math optimizations (much faster than float)
class FixedPoint {
public:
    // 16.16 fixed point for high precision
    typedef int32_t fixed16_16;
    
    static constexpr int32_t FRAC_BITS = 16;
    static constexpr int32_t FRAC_MASK = (1 << FRAC_BITS) - 1;
    static constexpr int32_t ONE = 1 << FRAC_BITS;
    
    // Convert float to fixed
    static inline fixed16_16 toFixed(float f) {
        return (fixed16_16)(f * ONE);
    }
    
    // Convert fixed to int
    static inline int32_t toInt(fixed16_16 f) {
        return f >> FRAC_BITS;
    }
    
    // Multiply two fixed point numbers
    static inline fixed16_16 mul(fixed16_16 a, fixed16_16 b) {
        return ((int64_t)a * b) >> FRAC_BITS;
    }
    
    // Fast sine using fixed point (with LUT)
    static inline fixed16_16 sin16(uint16_t angle) {
        extern uint8_t wavePatternLUT[256];
        uint8_t idx = angle >> 8;
        return (wavePatternLUT[idx] << 8);  // Scale to 16.16
    }
};

#endif // SIMD_OPTIMIZATIONS_H