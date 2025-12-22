#ifndef GRADIENT_EFFECT_H
#define GRADIENT_EFFECT_H

#include "../EffectBase.h"

class GradientEffect : public EffectBase {
private:
    uint8_t effectiveFade;
    uint8_t effectiveSpeed;
    
public:
    GradientEffect() : EffectBase("Gradient", 128, 10, 20) {}
    
    void render() override {
        // Make fadeAmount have a more dramatic effect
        effectiveFade = constrain(fadeAmount, 5, 255);
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, effectiveFade);
        
        // Make paletteSpeed have a more dramatic effect
        effectiveSpeed = constrain(paletteSpeed, 1, 50);
        uint8_t paletteOffset = (millis() / effectiveSpeed) & 0xFF;
        
        // Debug info - only show occasionally
        static uint32_t lastDebugTime = 0;
        #if FEATURE_DEBUG_OUTPUT
        if (millis() - lastDebugTime > 2000) {
            Serial.print(F("[EFFECT] Gradient - Fade: "));
            Serial.print(effectiveFade);
            Serial.print(F(", Speed: "));
            Serial.print(effectiveSpeed);
            Serial.print(F(", Palette: "));
            Serial.println(currentPaletteIndex);
            lastDebugTime = millis();
        }
        #endif
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            uint8_t ledAngle = angles[i];
            uint8_t paletteIndex = (ledAngle + paletteOffset) % 255;
            
            CRGB color = ColorFromPalette(currentPalette, paletteIndex);
            
            uint8_t adjustedBrightness = map(radii[i], 0, 255, 128, 255);
            color.nscale8_video(adjustedBrightness);
            leds[i] = color;
        }
    }
};

#endif // GRADIENT_EFFECT_H