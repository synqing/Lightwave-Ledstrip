#ifndef SINELON_EFFECT_H
#define SINELON_EFFECT_H

#include <FastLED.h>
#include <stdint.h>

/**
 * SinelonEffect - Standalone oscillating dot effect plugin
 * 
 * Creates a smooth oscillating dot that moves from the center outward
 * following the CENTER ORIGIN principle (LEDs 79/80).
 * 
 * Features:
 * - Smooth sine wave motion
 * - Configurable speed and fade trail
 * - Dual color support (different hues on each side)
 * - Dual strip synchronization
 */

class SinelonEffect {
public:
    // Configuration structure
    struct Config {
        uint16_t stripLength = 160;        // Number of LEDs per strip
        uint16_t centerPoint = 79;         // Center LED position
        uint16_t halfLength = 80;          // Half the strip length
        uint8_t oscillationSpeed = 13;     // Speed of oscillation (lower = faster)
        uint8_t fadeRate = 20;             // Trail fade rate (0-255)
        uint8_t brightness = 192;          // Dot brightness
        uint8_t saturation = 255;          // Color saturation
        uint8_t hueOffset = 128;           // Hue difference between sides
        bool mirrorSides = true;           // Mirror effect on both sides
    };
    
    // Constructor
    SinelonEffect(CRGB* strip1Buffer, CRGB* strip2Buffer, uint16_t numLeds = 160);
    
    // Initialize with custom config
    void init(const Config& config);
    
    // Update the effect (call this in your main loop)
    void update();
    
    // Set parameters
    void setSpeed(uint8_t speed) { config.oscillationSpeed = speed; }
    void setFadeRate(uint8_t fade) { config.fadeRate = fade; }
    void setBrightness(uint8_t bright) { config.brightness = bright; }
    void setGlobalHue(uint8_t* hue) { globalHue = hue; }
    
    // Get current position from center
    int getCurrentDistance() const { return currentDistance; }
    
private:
    // LED buffers
    CRGB* strip1;
    CRGB* strip2;
    uint16_t numLeds;
    
    // Configuration
    Config config;
    
    // State
    int currentDistance = 0;
    
    // External references (optional)
    uint8_t* globalHue = nullptr;
    uint8_t defaultHue = 0;
};

#endif // SINELON_EFFECT_H