#ifndef KALEIDOSCOPE_EFFECT_H
#define KALEIDOSCOPE_EFFECT_H

#include "../EffectBase.h"

class KaleidoscopeEffect : public EffectBase {
private:
    uint16_t rotationAngle = 0;
    
public:
    KaleidoscopeEffect() : EffectBase("Kaleidoscope", 140, 8, 15) {}
    
    void render() override {
        // Kaleidoscope effect with rotating patterns
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        uint16_t rotSpeed = map(paletteSpeed, 1, 50, 50, 5);
        rotationAngle += rotSpeed;
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Create symmetrical patterns
            uint8_t angle = angles[i];
            uint8_t symmetryAngle = (angle * 3 + (rotationAngle >> 8)) & 0xFF;
            
            // Multiple symmetry points
            uint8_t brightness = 0;
            for (uint8_t sym = 0; sym < 6; sym++) {
                uint8_t symPoint = (symmetryAngle + (sym * 42)) & 0xFF;
                uint8_t dist = abs(int16_t(angle) - int16_t(symPoint));
                if (dist > 127) dist = 255 - dist;
                
                uint8_t symBright = 255 - (dist * 2);
                brightness = qadd8(brightness, symBright >> 2);
            }
            
            uint8_t colorIndex = radii[i] + (rotationAngle >> 6);
            CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
            leds[i] = color;
        }
    }
};

#endif // KALEIDOSCOPE_EFFECT_H