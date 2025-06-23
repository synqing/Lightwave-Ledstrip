#ifndef HDR_EFFECT_H
#define HDR_EFFECT_H

#include "../EffectBase.h"

class HDREffect : public EffectBase {
private:
    uint16_t hdrBuffer[HardwareConfig::NUM_LEDS][3]; // 11-bit HDR buffer
    uint16_t phase = 0;
    
public:
    HDREffect() : EffectBase("HDR 11-bit", 160, 8, 15) {}
    
    void render() override {
        // Update phase
        phase += paletteSpeed;
        
        // Calculate HDR values (11-bit color depth)
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Create smooth HDR gradients
            uint16_t angle11 = (angles[i] << 3) | (angles[i] >> 5); // Scale to 11-bit
            uint16_t radius11 = (radii[i] << 3) | (radii[i] >> 5);
            
            // Generate 11-bit color values
            uint16_t hue = (angle11 + phase) & 0x7FF;
            uint16_t brightness = sin16(radius11 * 8 + phase * 4) >> 5; // 0-2047
            
            // Calculate RGB with HDR precision
            uint8_t hue8 = hue >> 3; // Convert to 8-bit for palette lookup
            CRGB baseColor = ColorFromPalette(currentPalette, hue8);
            
            // Apply HDR brightness with dithering
            hdrBuffer[i][0] = (baseColor.r * brightness) >> 3;
            hdrBuffer[i][1] = (baseColor.g * brightness) >> 3;
            hdrBuffer[i][2] = (baseColor.b * brightness) >> 3;
        }
        
        // Convert HDR to standard 8-bit with temporal dithering
        static uint8_t ditherFrame = 0;
        ditherFrame++;
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Apply temporal dithering for smooth gradients
            uint8_t ditherOffset = (i + ditherFrame) & 0x07;
            
            leds[i].r = (hdrBuffer[i][0] + ditherOffset) >> 3;
            leds[i].g = (hdrBuffer[i][1] + ditherOffset) >> 3;
            leds[i].b = (hdrBuffer[i][2] + ditherOffset) >> 3;
        }
        
        // Apply fade
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount >> 2);
    }
};

#endif // HDR_EFFECT_H