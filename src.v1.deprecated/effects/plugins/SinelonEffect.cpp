#include "SinelonEffect.h"

// Constructor
SinelonEffect::SinelonEffect(CRGB* strip1Buffer, CRGB* strip2Buffer, uint16_t numLeds)
    : strip1(strip1Buffer), strip2(strip2Buffer), numLeds(numLeds) {
    // Initialize with default config
    config.stripLength = numLeds;
    config.centerPoint = numLeds / 2 - 1;  // Default center point (79 for 160 LEDs)
    config.halfLength = numLeds / 2;       // Half length (80 for 160 LEDs)
}

// Initialize with custom config
void SinelonEffect::init(const Config& newConfig) {
    config = newConfig;
}

// Update the effect
void SinelonEffect::update() {
    // Fade previous frame to create trail effect
    fadeToBlackBy(strip1, config.stripLength, config.fadeRate);
    fadeToBlackBy(strip2, config.stripLength, config.fadeRate);
    
    // Calculate oscillation from center outward (0 to halfLength)
    currentDistance = beatsin16(config.oscillationSpeed, 0, config.halfLength);
    
    // Use provided hue or default
    uint8_t hue = globalHue ? *globalHue : defaultHue;
    
    // Calculate positions on both sides of center
    int pos1 = config.centerPoint + currentDistance;
    int pos2 = config.centerPoint - currentDistance;
    
    // Draw on positive side (right of center)
    if (pos1 < config.stripLength && pos1 < numLeds) {
        CRGB color = CHSV(hue, config.saturation, config.brightness);
        strip1[pos1] += color;
        strip2[pos1] += color;
    }
    
    // Draw on negative side (left of center) with optional mirror
    if (pos2 >= 0 && config.mirrorSides) {
        // Use different hue on opposite side for visual variety
        CRGB color = CHSV(hue + config.hueOffset, config.saturation, config.brightness);
        strip1[pos2] += color;
        strip2[pos2] += color;
    }
    
    // Auto-increment default hue if not using external
    if (!globalHue) {
        defaultHue++;
    }
}