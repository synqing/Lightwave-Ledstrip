#ifndef OPTIMIZED_WAVEFORM_H
#define OPTIMIZED_WAVEFORM_H

#include <Arduino.h>
#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External dependencies
extern CRGB strip1[HardwareConfig::STRIP1_LED_COUNT];
extern CRGB strip2[HardwareConfig::STRIP2_LED_COUNT];
extern CRGB leds[HardwareConfig::NUM_LEDS];
extern uint8_t gHue;
extern CRGBPalette16 currentPalette;
extern VisualParams visualParams;

// FastLED Optimized Wave Engine
// Based on document lines 346-397: 10x performance gain
class OptimizedWaveEngine {
private:
    // Pre-calculated position scaling (eliminate 320 divisions/frame)
    static constexpr uint16_t POSITION_STEP = 65535 / HardwareConfig::STRIP_LENGTH;
    
    // Wave parameters using integer math
    uint16_t frequency16[8];      // 16-bit frequency values
    uint16_t phase16[8];          // 16-bit phase values
    uint16_t amplitude16[8];      // 16-bit amplitude values
    
    // Pre-calculated sine LUT indices
    uint16_t lutIndices[HardwareConfig::STRIP_LENGTH];
    
public:
    void init() {
        // Pre-calculate position scaling once
        for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            lutIndices[i] = i * POSITION_STEP;
        }
        
        // Initialize wave parameters
        for(int w = 0; w < 8; w++) {
            frequency16[w] = 10430 * (w + 1);  // Different frequencies
            phase16[w] = 0;
            amplitude16[w] = 32768;  // 50% amplitude
        }
    }
    
    // OPTIMIZED: 10x faster than float sin()
    // From document line 435-442
    inline int16_t generateWave(uint16_t position16, uint8_t waveIndex) {
        uint16_t arg16 = (frequency16[waveIndex] * position16) + phase16[waveIndex];
        return sin16(arg16) >> 8;  // 10x faster than sin()
    }
    
    // Update phase using beatsin16 for automatic progression
    void updatePhases() {
        for(int w = 0; w < 8; w++) {
            phase16[w] = beatsin16(30 + w * 10);  // Different speeds
        }
    }
    
    // Render waveform with CENTER ORIGIN
    void renderWaveform(uint8_t waveIndex = 0) {
        updatePhases();
        
        for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Use pre-calculated position (no division!)
            uint16_t position16 = lutIndices[i];
            
            // Generate wave using sin16 (10x faster)
            int16_t wave = generateWave(position16, waveIndex);
            
            // Scale amplitude using scale8 (no float math)
            uint8_t scaledWave = scale8(wave + 128, visualParams.intensity);
            
            // Map to color using distance from center
            uint8_t colorIndex = abs((int)i - HardwareConfig::STRIP_CENTER_POINT) * 3;
            CRGB color = ColorFromPalette(currentPalette, 
                                        colorIndex, 
                                        scaledWave);
            
            // Apply to both strips
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    // Interference pattern using sqrt16
    void renderInterference() {
        updatePhases();
        
        for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            uint16_t position16 = lutIndices[i];
            
            // Calculate distance from center using integer math
            uint16_t distFromCenter16 = abs((int32_t)position16 - 32768);
            
            // Two waves from edges meeting at center
            int16_t wave1 = generateWave(position16, 0);
            int16_t wave2 = generateWave(65535 - position16, 1);
            
            // Interference calculation with sqrt16 (faster than sqrt)
            int32_t interference = (wave1 * wave1 + wave2 * wave2);
            uint16_t magnitude = sqrt16(interference >> 16);
            
            // Scale and map to brightness
            uint8_t brightness = scale8(magnitude >> 8, visualParams.intensity);
            
            CRGB color = ColorFromPalette(currentPalette, 
                                        scale8(distFromCenter16 >> 8, 255), 
                                        brightness);
            
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    // Multi-wave composition
    void renderMultiWave() {
        updatePhases();
        
        for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            uint16_t position16 = lutIndices[i];
            int32_t composite = 0;
            
            // Composite multiple waves
            for(int w = 0; w < 4; w++) {
                int16_t wave = generateWave(position16, w);
                // Scale by complexity parameter
                wave = scale8(wave, 64 + (visualParams.complexity >> 2));
                composite += wave;
            }
            
            // Normalize composite
            composite = composite >> 2;  // Divide by 4
            uint8_t brightness = constrain(composite + 128, 0, 255);
            
            // Color based on distance from center
            uint8_t colorIndex = abs((int)i - HardwareConfig::STRIP_CENTER_POINT) * 4;
            CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
            
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    // Beat-synchronized waveform
    void renderBeatWave() {
        // Use beatsin16 for beat-synchronized amplitude
        uint16_t beatAmplitude = beatsin16(60);  // 60 BPM
        
        for(uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            uint16_t position16 = lutIndices[i];
            
            // Generate base wave
            int16_t wave = generateWave(position16, 0);
            
            // Modulate with beat
            wave = scale16(wave, beatAmplitude);
            
            // Distance from center affects intensity
            uint16_t distFromCenter = abs((int16_t)i - HardwareConfig::STRIP_CENTER_POINT);
            uint8_t distScale = 255 - scale8(distFromCenter, 255 / HardwareConfig::STRIP_HALF_LENGTH);
            
            uint8_t brightness = scale8((wave >> 8) + 128, distScale);
            
            // Color intensity based on distance from center
            uint8_t colorIndex = scale8(distFromCenter, 128);
            CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
            
            strip1[i] = color;
            strip2[i] = color;
        }
    }
};

// Global instance
extern OptimizedWaveEngine waveEngine;

// Effect functions following the optimized pipeline
void waveformSine();        // Basic sine wave visualization
void waveformInterference(); // Dual wave interference pattern  
void waveformMulti();       // Multiple wave composition
void waveformBeat();        // Beat-synchronized waves

// Initialize the wave engine
void initWaveEngine();

#endif // OPTIMIZED_WAVEFORM_H