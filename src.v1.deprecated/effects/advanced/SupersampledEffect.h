#ifndef SUPERSAMPLED_EFFECT_H
#define SUPERSAMPLED_EFFECT_H

#include "../EffectBase.h"

class SupersampledEffect : public EffectBase {
private:
    static constexpr uint8_t SUPERSAMPLE_FACTOR = 4;
    uint16_t* supersampledBuffer;
    
public:
    SupersampledEffect() : EffectBase("Super-sampled", 140, 12, 20) {
        // Allocate supersampled buffer
        supersampledBuffer = new uint16_t[HardwareConfig::NUM_LEDS * SUPERSAMPLE_FACTOR * 3];
    }
    
    ~SupersampledEffect() {
        delete[] supersampledBuffer;
    }
    
    void render() override {
        // Clear supersampled buffer
        memset(supersampledBuffer, 0, HardwareConfig::NUM_LEDS * SUPERSAMPLE_FACTOR * 3 * sizeof(uint16_t));
        
        // Render at higher resolution
        uint16_t time = millis() / (51 - paletteSpeed);
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS * SUPERSAMPLE_FACTOR; i++) {
            // Calculate position in original space
            float position = (float)i / SUPERSAMPLE_FACTOR;
            uint16_t ledIndex = position;
            if (ledIndex >= HardwareConfig::NUM_LEDS) ledIndex = HardwareConfig::NUM_LEDS - 1;
            
            // Interpolate angle and radius
            float fractional = position - ledIndex;
            uint8_t angle = angles[ledIndex];
            uint8_t radius = radii[ledIndex];
            
            if (ledIndex < HardwareConfig::NUM_LEDS - 1) {
                angle = angle + (angles[ledIndex + 1] - angle) * fractional;
                radius = radius + (radii[ledIndex + 1] - radius) * fractional;
            }
            
            // Generate high-resolution pattern
            uint8_t hue = sin8(angle * 3 + time) + cos8(radius * 2 - time);
            uint8_t brightness = sin8(i * 10 + time * 2);
            
            CRGB color = ColorFromPalette(currentPalette, hue, brightness);
            
            // Store in supersampled buffer
            supersampledBuffer[i * 3] = color.r;
            supersampledBuffer[i * 3 + 1] = color.g;
            supersampledBuffer[i * 3 + 2] = color.b;
        }
        
        // Downsample with antialiasing
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            uint32_t r = 0, g = 0, b = 0;
            
            // Average SUPERSAMPLE_FACTOR samples
            for (uint8_t s = 0; s < SUPERSAMPLE_FACTOR; s++) {
                uint16_t idx = (i * SUPERSAMPLE_FACTOR + s) * 3;
                r += supersampledBuffer[idx];
                g += supersampledBuffer[idx + 1];
                b += supersampledBuffer[idx + 2];
            }
            
            leds[i].r = r / SUPERSAMPLE_FACTOR;
            leds[i].g = g / SUPERSAMPLE_FACTOR;
            leds[i].b = b / SUPERSAMPLE_FACTOR;
        }
        
        // Apply fade
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount >> 1);
    }
};

#endif // SUPERSAMPLED_EFFECT_H