#include "StripBPMEffect.h"

// Constructor
StripBPMEffect::StripBPMEffect(CRGB* strip1Buffer, CRGB* strip2Buffer, uint16_t numLeds) 
    : strip1(strip1Buffer), strip2(strip2Buffer), numLeds(numLeds) {
    // Initialize with default config
    config.stripLength = numLeds;
    config.centerPoint = numLeds / 2 - 1;  // Default center point
}

// Initialize with custom config
void StripBPMEffect::init(const Config& newConfig) {
    config = newConfig;
}

// Update the effect
void StripBPMEffect::update() {
    // Get beat value based on BPM
    currentBeat = beatsin8(config.beatsPerMinute, config.minBrightness, config.maxBrightness);
    
    // Use provided palette or default
    CRGBPalette16* palette = currentPalette ? currentPalette : &defaultPalette;
    uint8_t hue = globalHue ? *globalHue : defaultHue;
    
    // Update each LED
    for(int i = 0; i < config.stripLength; i++) {
        // Calculate distance from CENTER
        float distFromCenter = abs((float)i - config.centerPoint);
        
        // Color index varies with distance from center
        uint8_t colorIndex = hue + (distFromCenter * config.colorSpread);
        
        // Brightness calculation with distance-based variation
        uint8_t brightness = currentBeat - hue + (distFromCenter * config.brightnessSpread);
        
        // Get color from palette
        CRGB color = ColorFromPalette(*palette, colorIndex, brightness);
        
        // Apply to both strips
        if (i < numLeds) {
            strip1[i] = color;
            strip2[i] = color;
        }
    }
    
    // Auto-increment default hue if not using external
    if (!globalHue) {
        defaultHue++;
    }
}