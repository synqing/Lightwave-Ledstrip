// Light Guide Plate Liquid Crystal Flow Effect
// Smooth, organic color transitions inspired by liquid crystal displays

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "LightGuideEffect.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Liquid Crystal Flow effect class
class LGPLiquidCrystalEffect : public LightGuideEffect {
private:
    // Phase angles for smooth animation
    float phase1;
    float phase2;
    float phase3;
    
    // Color flow positions
    float colorFlow[HardwareConfig::STRIP_LENGTH];
    
public:
    LGPLiquidCrystalEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Liquid Crystal", s1, s2),
        phase1(0), phase2(0), phase3(0) {
        
        // Initialize color flow
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            colorFlow[i] = i * 360.0f / HardwareConfig::STRIP_LENGTH;
        }
    }
    
    void render() override {
        // Update phases for smooth animation
        phase1 += paletteSpeed * 0.001f;
        phase2 += paletteSpeed * 0.0007f;
        phase3 += paletteSpeed * 0.0013f;
        
        // Create smooth color flow from center
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Distance from center (0 to 1)
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_CENTER_POINT;
            
            // Multi-layered sine waves for organic motion
            float wave1 = sin(distFromCenter * PI + phase1) * 0.5f + 0.5f;
            float wave2 = sin(distFromCenter * TWO_PI + phase2) * 0.3f;
            float wave3 = sin(distFromCenter * 3 * PI + phase3) * 0.2f;
            
            // Combine waves for complex but smooth motion
            float combinedWave = wave1 + wave2 + wave3;
            combinedWave = constrain(combinedWave, 0, 1);
            
            // Color flow speed varies with position (no rainbow cycling)
            float flowSpeed = (1.0f - distFromCenter) * visualParams.getComplexityNorm() * 2.0f;
            colorFlow[i] += flowSpeed;
            if (colorFlow[i] > 360) colorFlow[i] -= 360;

            // Palette-based color (no rainbow cycling)
            // Use distance from center + flow position for palette index
            uint8_t paletteIndex1 = (uint8_t)(distFromCenter * 128) + (uint8_t)(colorFlow[i] * 0.35f);
            uint8_t paletteIndex2 = paletteIndex1 + (uint8_t)(combinedWave * 64);

            // Brightness based on wave and distance
            uint8_t brightness1 = 100 + (uint8_t)(combinedWave * 155);
            uint8_t brightness2 = 100 + (uint8_t)((1.0f - combinedWave) * 155);

            // Get colors at full brightness, then scale (preserves saturation)
            CRGB color1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
            CRGB color2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
            color1.nscale8(brightness1);
            color2.nscale8(brightness2);
            strip1[i] = color1;
            strip2[i] = color2;
        }
        
        // Add gentle color pulse from center
        if (visualParams.variation > 100) {
            float pulse = (sin(phase1 * 2) + 1.0f) * 0.5f;
            uint8_t pulseIntensity = pulse * (visualParams.variation - 100) / 155.0f * 255;
            
            for (int i = -5; i <= 5; i++) {
                int pos = HardwareConfig::STRIP_CENTER_POINT + i;
                if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                    float fade = 1.0f - abs(i) / 5.0f;
                    uint8_t blendAmount = pulseIntensity * fade;
                    strip1[pos] = blend(strip1[pos], CRGB::White, blendAmount);
                    strip2[pos] = blend(strip2[pos], CRGB::White, blendAmount);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPLiquidCrystalEffect* liquidCrystalInstance = nullptr;

// Effect function for main loop
void lgpLiquidCrystal() {
    if (!liquidCrystalInstance) {
        liquidCrystalInstance = new LGPLiquidCrystalEffect(strip1, strip2);
    }
    liquidCrystalInstance->render();
}