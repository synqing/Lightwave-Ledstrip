#ifndef TIME_ALPHA_EFFECT_H
#define TIME_ALPHA_EFFECT_H

#include "../EffectBase.h"

class TimeAlphaEffect : public EffectBase {
private:
    CRGB historyBuffer[HardwareConfig::NUM_LEDS][4]; // 4 frames of history
    uint8_t historyIndex = 0;
    
public:
    TimeAlphaEffect() : EffectBase("Time Alpha", 150, 10, 25) {
        // Clear history buffer
        memset(historyBuffer, 0, sizeof(historyBuffer));
    }
    
    void render() override {
        // Advance history index
        historyIndex = (historyIndex + 1) & 0x03;
        
        // Generate new frame
        uint16_t time = millis() / (51 - paletteSpeed);
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Create time-varying pattern
            uint8_t angle = angles[i];
            uint8_t radius = radii[i];
            
            // Multiple time-based waves
            uint8_t wave1 = sin8(angle * 2 + time);
            uint8_t wave2 = cos8(radius * 3 - time * 2);
            uint8_t wave3 = sin8((angle + radius) + time * 3);
            
            // Combine waves with different time offsets
            uint8_t hue = (wave1 + wave2) >> 1;
            uint8_t sat = 255 - (wave3 >> 2);
            uint8_t val = (wave1 + wave3) >> 1;
            
            CRGB newColor = ColorFromPalette(currentPalette, hue, val);
            newColor = CHSV(newColor.r, sat, newColor.b); // Abuse CHSV for color manipulation
            
            // Store in history
            historyBuffer[i][historyIndex] = newColor;
            
            // Blend multiple time frames with alpha
            uint16_t r = 0, g = 0, b = 0;
            uint8_t weights[4] = {128, 64, 32, 32}; // Temporal weights
            
            for (uint8_t h = 0; h < 4; h++) {
                uint8_t idx = (historyIndex - h) & 0x03;
                r += historyBuffer[i][idx].r * weights[h];
                g += historyBuffer[i][idx].g * weights[h];
                b += historyBuffer[i][idx].b * weights[h];
            }
            
            // Normalize (sum of weights = 256)
            leds[i].r = r >> 8;
            leds[i].g = g >> 8;
            leds[i].b = b >> 8;
        }
        
        // Subtle fade for trails
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount >> 2);
    }
};

#endif // TIME_ALPHA_EFFECT_H