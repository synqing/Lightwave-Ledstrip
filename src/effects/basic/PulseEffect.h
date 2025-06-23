#ifndef PULSE_EFFECT_H
#define PULSE_EFFECT_H

#include "../EffectBase.h"

class PulseEffect : public EffectBase {
private:
    uint8_t pulsePhase = 0;
    
public:
    PulseEffect() : EffectBase("Pulse", 160, 20, 10) {}
    
    void render() override {
        // Pulse effect - all LEDs pulse together
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        uint8_t pulseSpeed = map(paletteSpeed, 1, 50, 10, 100);
        pulsePhase += pulseSpeed;
        
        uint8_t brightness = beatsin8(pulseSpeed, 64, 255);
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Use radius to create concentric pulses
            uint8_t phasedBrightness = brightness;
            if (radii[i] > 0) {
                uint8_t radiusPhase = pulsePhase + (radii[i] >> 2);
                phasedBrightness = beatsin8(pulseSpeed, 64, 255, 0, radiusPhase);
            }
            
            uint8_t colorIndex = angles[i] + pulsePhase;
            CRGB color = ColorFromPalette(currentPalette, colorIndex, phasedBrightness);
            leds[i] = color;
        }
    }
};

#endif // PULSE_EFFECT_H