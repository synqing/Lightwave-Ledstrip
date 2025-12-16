#ifndef STARTUP_SINELON_H
#define STARTUP_SINELON_H

#include <FastLED.h>
#include <stdint.h>

/**
 * StartupSinelon - Simple oscillating dot for startup animation
 * 
 * Basic Sinelon effect: single dot oscillating smoothly across the strip
 * with a fading trail. Simple and clean for startup sequences.
 */

class StartupSinelon {
public:
    // Constructor
    StartupSinelon(CRGB* ledBuffer, uint16_t numLeds);
    
    // Update the effect (call this in your animation loop)
    void update();
    
    // Set parameters
    void setSpeed(uint8_t speed) { oscillationSpeed = speed; }
    void setFadeRate(uint8_t fade) { fadeRate = fade; }
    void setBrightness(uint8_t bright) { brightness = bright; }
    void setHue(uint8_t hue) { currentHue = hue; }
    void setSaturation(uint8_t sat) { saturation = sat; }
    
    // Reset the effect
    void reset() { 
        currentPhase = 0;
        for(uint16_t i = 0; i < numLeds; i++) {
            leds[i] = CRGB::Black;
        }
    }
    
private:
    // LED buffer
    CRGB* leds;
    uint16_t numLeds;
    
    // Parameters
    uint8_t oscillationSpeed = 13;
    uint8_t fadeRate = 20;
    uint8_t brightness = 192;
    uint8_t currentHue = 0;
    uint8_t saturation = 255;
    
    // State
    uint16_t currentPhase = 0;
};

#endif // STARTUP_SINELON_H