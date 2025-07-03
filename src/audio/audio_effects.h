#pragma once

#include "audio_sync.h"
#include <FastLED.h>

// Global audio sync instance
extern AudioSync audioSync;

// Forward declarations for audio effect functions
void bassReactiveEffect();
void spectrumEffect();
void energyFlowEffect();

// Audio effect implementations
inline void bassReactiveEffect() {
    // Import globals from main.cpp
    extern CRGB strip1[];
    extern CRGB strip2[];
    extern uint8_t gHue;
    
    AudioFrame frame = audioSync.getCurrentFrame();
    
    // Map bass energy to brightness
    uint8_t brightness = 0;
    if (!frame.silence) {
        brightness = map(frame.bass_energy, 0, 1000, 0, 255);
    }
    
    // Pulse all LEDs with bass
    CRGB color = CHSV(gHue, 255, brightness);
    fill_solid(strip1, 144, color);
    fill_solid(strip2, 176, color);
    
    // Add sparkles on transients
    if (frame.transient_detected) {
        for (int i = 0; i < 20; i++) {
            strip1[random16(144)] = CRGB::White;
            strip2[random16(176)] = CRGB::White;
        }
    }
}

inline void spectrumEffect() {
    // Import globals from main.cpp
    extern CRGB strip1[];
    extern CRGB strip2[];
    
    AudioFrame frame = audioSync.getCurrentFrame();
    
    if (frame.silence || !frame.frequency_bins) {
        fadeToBlackBy(strip1, 144, 20);
        fadeToBlackBy(strip2, 176, 20);
        return;
    }
    
    // Map frequency bins to strip1 (144 LEDs)
    int binsPerLed = FFT_BIN_COUNT / 144;
    if (binsPerLed < 1) binsPerLed = 1;
    
    for (int i = 0; i < 144; i++) {
        // Average bins for this LED
        float binValue = 0;
        int binCount = 0;
        
        for (int b = 0; b < binsPerLed; b++) {
            int binIndex = i * binsPerLed + b;
            if (binIndex < FFT_BIN_COUNT) {
                binValue += frame.frequency_bins[binIndex];
                binCount++;
            }
        }
        
        if (binCount > 0) {
            binValue /= binCount;
        }
        
        // Map to color and brightness
        uint8_t hue = map(i, 0, 143, 0, 255);
        uint8_t brightness = constrain(binValue * 255, 0, 255);
        strip1[i] = CHSV(hue, 255, brightness);
    }
    
    // Mirror to strip2 with slight variation
    for (int i = 0; i < 176; i++) {
        int srcIndex = map(i, 0, 175, 0, 143);
        strip2[i] = strip1[srcIndex];
    }
}

inline void energyFlowEffect() {
    // Import globals from main.cpp
    extern CRGB strip1[];
    extern CRGB strip2[];
    
    static float position = 0;
    
    AudioFrame frame = audioSync.getCurrentFrame();
    
    // Move position based on energy
    if (!frame.silence) {
        float speed = frame.total_energy / 500.0f;
        position += speed;
        if (position >= 144) position -= 144;
    }
    
    // Fade all LEDs
    fadeToBlackBy(strip1, 144, 10);
    fadeToBlackBy(strip2, 176, 10);
    
    // Draw energy wave
    if (!frame.silence) {
        int waveWidth = map(frame.total_energy, 0, 2000, 5, 36);
        
        for (int i = 0; i < waveWidth; i++) {
            int pos1 = ((int)position + i) % 144;
            int pos2 = map(pos1, 0, 143, 0, 175);
            uint8_t brightness = map(i, 0, waveWidth-1, 255, 0);
            
            // Color based on frequency distribution
            uint8_t hue = 0;
            if (frame.bass_energy > frame.mid_energy && frame.bass_energy > frame.high_energy) {
                hue = 0; // Red for bass
            } else if (frame.mid_energy > frame.high_energy) {
                hue = 96; // Green for mids
            } else {
                hue = 160; // Blue for highs
            }
            
            strip1[pos1] = CHSV(hue, 255, brightness);
            strip2[pos2] = CHSV(hue, 255, brightness);
        }
    }
}