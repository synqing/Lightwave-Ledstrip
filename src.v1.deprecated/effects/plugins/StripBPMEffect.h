#ifndef STRIP_BPM_EFFECT_H
#define STRIP_BPM_EFFECT_H

#include <FastLED.h>
#include <stdint.h>

/**
 * StripBPMEffect - Standalone BPM pulsing effect plugin
 * 
 * Creates rhythmic pulses that emanate from the center of LED strips
 * following the CENTER ORIGIN principle (LEDs 79/80).
 * 
 * Features:
 * - Configurable BPM (beats per minute)
 * - Distance-based color variation
 * - Palette support
 * - Dual strip synchronization
 */

class StripBPMEffect {
public:
    // Configuration structure
    struct Config {
        uint16_t stripLength = 160;        // Number of LEDs per strip
        uint16_t centerPoint = 79;         // Center LED position
        uint8_t beatsPerMinute = 62;       // BPM rate
        uint8_t minBrightness = 64;        // Minimum brightness
        uint8_t maxBrightness = 255;       // Maximum brightness
        uint8_t colorSpread = 2;           // Color variation per LED distance
        uint8_t brightnessSpread = 10;     // Brightness variation per LED distance
    };
    
    // Constructor
    StripBPMEffect(CRGB* strip1Buffer, CRGB* strip2Buffer, uint16_t numLeds = 160);
    
    // Initialize with custom config
    void init(const Config& config);
    
    // Update the effect (call this in your main loop)
    void update();
    
    // Set parameters
    void setBPM(uint8_t bpm) { config.beatsPerMinute = bpm; }
    void setPalette(CRGBPalette16* palette) { currentPalette = palette; }
    void setGlobalHue(uint8_t* hue) { globalHue = hue; }
    
    // Get current beat value (0-255)
    uint8_t getCurrentBeat() const { return currentBeat; }
    
private:
    // LED buffers
    CRGB* strip1;
    CRGB* strip2;
    uint16_t numLeds;
    
    // Configuration
    Config config;
    
    // State
    uint8_t currentBeat = 128;
    
    // External references (optional)
    CRGBPalette16* currentPalette = nullptr;
    uint8_t* globalHue = nullptr;
    
    // Default palette if none provided
    CRGBPalette16 defaultPalette = RainbowColors_p;
    uint8_t defaultHue = 0;
};

#endif // STRIP_BPM_EFFECT_H