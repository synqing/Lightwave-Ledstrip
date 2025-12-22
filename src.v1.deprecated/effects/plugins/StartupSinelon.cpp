#include "StartupSinelon.h"

// Constructor
StartupSinelon::StartupSinelon(CRGB* ledBuffer, uint16_t numLeds)
    : leds(ledBuffer), numLeds(numLeds) {
}

// Update the effect
void StartupSinelon::update() {
    // Fade previous frame to create trail effect
    fadeToBlackBy(leds, numLeds, fadeRate);
    
    // Calculate oscillating position using sine wave
    // beatsin16 creates smooth back-and-forth motion
    uint16_t pos = beatsin16(oscillationSpeed, 0, numLeds - 1);
    
    // Set the dot at current position
    leds[pos] += CHSV(currentHue, saturation, brightness);
    
    // Auto-increment hue for color cycling
    currentHue++;
}