/**
 * Test sketch to verify MegaLUT compilation
 * 
 * This is a minimal sketch to test if the MegaLUT system compiles correctly
 */

#include <Arduino.h>
#include <FastLED.h>
#include "src/config/hardware_config.h"
#include "src/core/MegaLUTs.h"

// LED array
CRGB leds[HardwareConfig::NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Testing MegaLUT System Compilation...");
    
    // Initialize FastLED
    FastLED.addLeds<WS2812, 5, GRB>(leds, HardwareConfig::NUM_LEDS);
    
    // Initialize MegaLUTs
    Serial.println("Initializing MegaLUTs...");
    size_t heapBefore = ESP.getFreeHeap();
    
    initializeMegaLUTs();
    
    size_t heapAfter = ESP.getFreeHeap();
    size_t memUsed = heapBefore - heapAfter;
    
    Serial.print("MegaLUTs initialized! Memory used: ");
    Serial.print(memUsed / 1024);
    Serial.println(" KB");
    
    // Test some LUT accesses
    Serial.println("\nTesting LUT access...");
    
    // Test trig LUT
    int16_t sinValue = SIN16(1024);
    Serial.print("SIN16(1024) = ");
    Serial.println(sinValue);
    
    // Test distance LUT
    if (distanceFromCenterLUT != nullptr) {
        Serial.print("Distance from center at LED 0: ");
        Serial.println(distanceFromCenterLUT[0]);
    }
    
    // Test wave pattern
    uint8_t waveData[HardwareConfig::NUM_LEDS];
    getWavePattern(waveData, 0);
    Serial.print("First wave pattern value: ");
    Serial.println(waveData[0]);
    
    // Test palette
    CRGB color = getPaletteColorInterpolated(0, 128);
    Serial.print("Palette color: R=");
    Serial.print(color.r);
    Serial.print(" G=");
    Serial.print(color.g);
    Serial.print(" B=");
    Serial.println(color.b);
    
    Serial.println("\nMegaLUT test complete!");
}

void loop() {
    // Simple test pattern using LUTs
    static uint8_t phase = 0;
    phase++;
    
    for (int i = 0; i < HardwareConfig::NUM_LEDS; i++) {
        // Use wave LUT for animation
        uint8_t waveValue = wavePatternLUT[phase >> 2][i];
        
        // Use palette LUT for color
        leds[i] = getPaletteColorInterpolated(0, waveValue + phase);
        
        // Apply brightness scaling
        leds[i].fadeToBlackBy(255 - dim8_video_LUT[waveValue]);
    }
    
    FastLED.show();
    delay(10);
}