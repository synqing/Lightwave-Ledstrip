#include "OptimizedStripEffects.h"
#include <FastLED.h>
#include "../../config/hardware_config.h"

// Pre-calculated distance lookup table
uint8_t distanceFromCenter[HardwareConfig::STRIP_LENGTH];

// Initialize lookup tables
void initOptimizedEffects() {
    // Pre-calculate quantized distances from center (0-255 range)
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t dist = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);
        // Map 0-80 to 0-255
        distanceFromCenter[i] = scale8(dist, 255 / HardwareConfig::STRIP_HALF_LENGTH);
    }
}

// OPTIMIZED stripInterference using FastLED functions
void stripInterferenceOptimized() {
    static uint16_t wave1Phase = 0;
    static uint16_t wave2Phase = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeAmount);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeAmount);
    
    // Use fixed-point phase increment
    wave1Phase += scale8(paletteSpeed, 13);  // ~= speed/20
    wave2Phase -= scale8(paletteSpeed, 9);   // ~= speed/30
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Use pre-calculated distance
        uint8_t normalizedDist = distanceFromCenter[i];
        
        // Fast sine waves using sin8()
        uint8_t wave1 = sin8((normalizedDist << 2) + (wave1Phase >> 8));
        uint8_t wave2 = sin8((normalizedDist * 6) + (wave2Phase >> 8));
        
        // Fast averaging using scale8
        uint8_t interference = scale8(wave1 + wave2, 128); // Divide by 2
        
        // Hue calculation without float
        uint8_t hue = (wave1Phase >> 10) + scale8(normalizedDist, 8);
        
        CRGB color = ColorFromPalette(currentPalette, hue, interference);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// OPTIMIZED heartbeat using beatsin8
void heartbeatEffectOptimized() {
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Heartbeat rhythm using beatsin8
    uint8_t beat1 = beatsin8(72, 0, 255);     // Main beat at 72 BPM
    uint8_t beat2 = beatsin8(151, 0, 102);    // Secondary beat (2.1x rate, 40% amplitude)
    uint8_t beatPattern = qadd8(beat1, beat2); // Saturating add
    
    // Trigger pulse when beat is high
    if (beatPattern > 230) {
        // Generate pulse from CENTER using optimized math
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            uint8_t normalizedDist = distanceFromCenter[i];
            
            // Fast brightness calculation (255 - dist)
            uint8_t brightness = 255 - normalizedDist;
            uint8_t hue = gHue + scale8(normalizedDist, 50);
            
            CRGB color = ColorFromPalette(currentPalette, hue, brightness);
            strip1[i] += color;
            strip2[i] += color;
        }
    }
}

// OPTIMIZED breathing effect using beatsin16
void breathingEffectOptimized() {
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 15);
    
    // Smooth breathing using beatsin16 for higher precision
    uint16_t breath = beatsin16(scale8(paletteSpeed, 10), 0, HardwareConfig::STRIP_HALF_LENGTH);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t dist = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);
        
        if (dist <= breath) {
            // Fast intensity calculation using scale8
            uint16_t divisor = breath > 0 ? breath : 1;  // Avoid division by zero
            uint8_t intensity = 255 - scale8(dist, 255 / divisor);
            uint8_t brightness = scale8(intensity, breath * 255 / HardwareConfig::STRIP_HALF_LENGTH);
            
            uint8_t hue = gHue + scale8(dist, 3);
            CRGB color = ColorFromPalette(currentPalette, hue, brightness);
            strip1[i] = color;
            strip2[i] = color;
        }
    }
}

// OPTIMIZED plasma using only integer math
void stripPlasmaOptimized() {
    static uint16_t time = 0;
    time += paletteSpeed;
    
    for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t normalizedDist = distanceFromCenter[i];
        
        // Multiple octaves of sine waves using sin8
        uint8_t v1 = sin8((normalizedDist << 3) + (time >> 6));
        uint8_t v2 = sin8((normalizedDist * 5) - (time >> 7));
        uint8_t v3 = sin8((normalizedDist * 3) + (time >> 8));
        
        // Combine waves
        uint8_t hue = scale8(v1 + v2 + v3, 85) + gHue;  // scale8(x, 85) ≈ x/3
        uint8_t brightness = scale8(v1 + v2, 128) + 64; // Average and offset
        
        CRGB color = CHSV(hue, 255, brightness);
        strip1[i] = color;
        strip2[i] = color;
    }
}

// OPTIMIZED vortex using trigonometric approximations
void vortexEffectOptimized() {
    static uint8_t vortexAngle = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t normalizedDist = distanceFromCenter[i];
        
        // Spiral calculation using integer math
        uint8_t spiralOffset = (normalizedDist << 3) + vortexAngle;
        uint8_t intensity = sin8(spiralOffset);
        
        // Fade towards edges using scale8
        intensity = scale8(intensity, 255 - scale8(normalizedDist, 128));
        
        uint8_t hue = gHue + scale8(normalizedDist, 5) + scale8(vortexAngle, 20);
        
        CRGB color = ColorFromPalette(currentPalette, hue, intensity);
        
        // Opposite spiral direction for strip2
        if (i < HardwareConfig::STRIP_CENTER_POINT) {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        } else {
            strip1[i] = color;
            strip2[HardwareConfig::STRIP_LENGTH - 1 - i] = color;
        }
    }
    
    vortexAngle += scale8(paletteSpeed, 5); // ~= speed/50
}

// Performance comparison function
void comparePerformance() {
    uint32_t start, floatTime, optimizedTime;
    const int TEST_RUNS = 10;  // Average over multiple runs
    
    // Clear strips before testing
    FastLED.clear();
    
    // Test float version
    floatTime = 0;
    for (int i = 0; i < TEST_RUNS; i++) {
        start = micros();
        stripInterference();
        floatTime += micros() - start;
    }
    floatTime /= TEST_RUNS;
    
    // Clear again
    FastLED.clear();
    
    // Test optimized version
    optimizedTime = 0;
    for (int i = 0; i < TEST_RUNS; i++) {
        start = micros();
        stripInterferenceOptimized();
        optimizedTime += micros() - start;
    }
    optimizedTime /= TEST_RUNS;
    
    Serial.println("\n=== PERFORMANCE COMPARISON ===");
    Serial.printf("Original (float): %luµs per frame\n", floatTime);
    Serial.printf("Optimized (FastLED): %luµs per frame\n", optimizedTime);
    Serial.printf("Performance gain: %.1fx faster\n", (float)floatTime / optimizedTime);
    Serial.printf("Time saved: %luµs per frame\n", floatTime - optimizedTime);
    Serial.printf("FPS increase: %.0f → %.0f\n", 
                  1000000.0f / floatTime, 1000000.0f / optimizedTime);
    Serial.println("==============================\n");
} 