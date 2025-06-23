#ifndef WAVE_EFFECT_H
#define WAVE_EFFECT_H

#include "../EffectBase.h"

class WaveEffect : public EffectBase {
private:
    uint16_t wavePosition = 0;
    
public:
    WaveEffect() : EffectBase("Wave", 120, 12, 30) {}
    
    void render() override {
        // Simple wave effect implementation
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        uint16_t waveSpeed = map(paletteSpeed, 1, 50, 100, 10);
        wavePosition += waveSpeed;
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            uint8_t brightness = sin8((i * 10) + (wavePosition >> 4));
            uint8_t colorIndex = angles[i] + (wavePosition >> 6);
            
            CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
            leds[i] = color;
        }
    }
};

#endif // WAVE_EFFECT_H