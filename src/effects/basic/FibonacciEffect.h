#ifndef FIBONACCI_EFFECT_H
#define FIBONACCI_EFFECT_H

#include "../EffectBase.h"

class FibonacciEffect : public EffectBase {
private:
    uint16_t sPseudotime = 0;
    uint16_t sLastMillis = 0;
    uint16_t sHue16 = 0;
    
public:
    FibonacciEffect() : EffectBase("Fibonacci", 150, 15, 25) {}
    
    void render() override {
        // Debug info - only show occasionally
        static uint32_t lastDebugTime = 0;
        #if FEATURE_DEBUG_OUTPUT
        if (millis() - lastDebugTime > 2000) {
            Serial.print(F("[EFFECT] Fibonacci - Fade: "));
            Serial.print(fadeAmount);
            Serial.print(F(", Speed: "));
            Serial.print(paletteSpeed);
            Serial.print(F(", Palette: "));
            Serial.println(currentPaletteIndex);
            lastDebugTime = millis();
        }
        #endif
        
        // Make paletteSpeed have a more dramatic effect on the Fibonacci pattern
        uint8_t effectiveSpeed = constrain(paletteSpeed, 1, 40);
        
        // Enhanced Fibonacci values for more dramatic effect
        uint8_t brightdepth = beatsin88(341, 96, 224);
        uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
        uint8_t msmultiplier = beatsin88(147, 23, 60);
        
        // Apply speed scaling here
        msmultiplier = map(effectiveSpeed, 1, 40, 80, 20);
        
        uint16_t hue16 = sHue16;
        uint16_t hueinc16 = beatsin88(113, 300, 1500);
        
        uint16_t ms = millis();
        uint16_t deltams = ms - sLastMillis;
        sLastMillis = ms;
        sPseudotime += deltams * msmultiplier;
        sHue16 += deltams * beatsin88(400, 5, 9);
        uint16_t brightnesstheta16 = sPseudotime;
        
        // Apply fadeAmount to the effect
        uint8_t blendAmt = map(constrain(fadeAmount, 10, 255), 10, 255, 160, 240);
        
        // Enhanced for better visibility on linear strips
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            hue16 += hueinc16;
            uint8_t hue8;
            uint16_t h16_128 = hue16 >> 7;
            if (h16_128 & 0x100) hue8 = 255 - (h16_128 >> 1);
            else hue8 = h16_128 >> 1;
            
            brightnesstheta16 += brightnessthetainc16;
            uint16_t b16 = sin16(brightnesstheta16) + 32768;
            uint16_t bri16 = ((uint32_t)b16 * (uint32_t)b16) / 65536;
            uint8_t bri8 = (((uint32_t)bri16) * brightdepth) / 65536;
            bri8 += (255 - brightdepth);
            
            // Use angles[] array for color variation by position
            uint8_t index = scale8(hue8 + angles[i], 240);
            
            CRGB newcolor = ColorFromPalette(currentPalette, index, bri8);
            
            // Apply the calculated blend amount based on fadeAmount
            leds[i] = blend(leds[i], newcolor, blendAmt);
        }
    }
};

#endif // FIBONACCI_EFFECT_H