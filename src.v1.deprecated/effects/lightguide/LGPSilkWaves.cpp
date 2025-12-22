// Light Guide Plate Silk Waves Effect
// Smooth, flowing waves like silk fabric in wind

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

// Silk Waves effect class
class LGPSilkWavesEffect : public LightGuideEffect {
private:
    // Wave parameters for smooth silk-like motion
    float wavePhase1;
    float wavePhase2;
    float colorPhase;
    
    // Smooth color buffer for temporal blending
    CRGB smoothBuffer1[HardwareConfig::STRIP_LENGTH];
    CRGB smoothBuffer2[HardwareConfig::STRIP_LENGTH];
    
public:
    LGPSilkWavesEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Silk Waves", s1, s2),
        wavePhase1(0), wavePhase2(0), colorPhase(0) {
        
        // Initialize smooth buffers
        fill_solid(smoothBuffer1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        fill_solid(smoothBuffer2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
    }
    
    void render() override {
        // Update phases with different speeds for layered motion
        wavePhase1 += paletteSpeed * 0.002f;
        wavePhase2 += paletteSpeed * 0.0015f;
        colorPhase += paletteSpeed * 0.0005f;
        
        // Wave complexity based on user control
        float waveComplexity = 1.0f + visualParams.getComplexityNorm() * 3.0f;
        
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Calculate position relative to center
            float position = (float)i / HardwareConfig::STRIP_LENGTH;
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_CENTER_POINT;
            
            // Primary silk wave - slow and smooth
            float wave1 = sin(position * waveComplexity * TWO_PI + wavePhase1);
            wave1 = wave1 * 0.5f + 0.5f;  // Normalize to 0-1
            
            // Secondary wave - creates shimmer
            float wave2 = sin(position * waveComplexity * 3.0f * PI + wavePhase2);
            wave2 = wave2 * 0.3f;
            
            // Combine waves with center weighting
            float combined = wave1 + wave2 * (1.0f - distFromCenter);
            combined = constrain(combined, 0, 1);
            
            // Palette-based color (no gHue rainbow cycling)
            // Use distance from center + position variation for smooth gradients
            float indexShift = sin(position * PI + colorPhase) * 30;
            uint8_t paletteIndex1 = (uint8_t)(distFromCenter * 128) + (uint8_t)(position * 60) + (uint8_t)indexShift;
            uint8_t paletteIndex2 = paletteIndex1 + 30;  // Slight offset for depth

            // Brightness modulation
            uint8_t brightness1 = 80 + (uint8_t)(combined * 175);
            uint8_t brightness2 = 80 + (uint8_t)((1.0f - combined * 0.7f) * 175);

            // Get colors at full brightness, then scale (preserves saturation)
            CRGB color1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
            CRGB color2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
            color1.nscale8(brightness1);
            color2.nscale8(brightness2);
            
            // Temporal smoothing for silk-like flow
            smoothBuffer1[i] = blend(smoothBuffer1[i], color1, 200);
            smoothBuffer2[i] = blend(smoothBuffer2[i], color2, 200);
            
            // Apply intensity control
            strip1[i] = smoothBuffer1[i];
            strip2[i] = smoothBuffer2[i];
            strip1[i].nscale8(visualParams.intensity);
            strip2[i].nscale8(visualParams.intensity);
        }
        
        // Add silk highlight from center
        float highlight = (sin(wavePhase1 * 2) + 1.0f) * 0.5f;
        for (int i = -10; i <= 10; i++) {
            int pos = HardwareConfig::STRIP_CENTER_POINT + i;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                float fade = 1.0f - abs(i) / 10.0f;
                fade = fade * fade;  // Quadratic falloff
                uint8_t highlightIntensity = highlight * fade * visualParams.getSaturationNorm() * 0.5f;
                
                // Add subtle white highlight
                strip1[pos] = blend(strip1[pos], 
                                   blend(strip1[pos], CRGB::White, highlightIntensity),
                                   128);
                strip2[pos] = blend(strip2[pos], 
                                   blend(strip2[pos], CRGB::White, highlightIntensity),
                                   128);
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPSilkWavesEffect* silkWavesInstance = nullptr;

// Effect function for main loop
void lgpSilkWaves() {
    if (!silkWavesInstance) {
        silkWavesInstance = new LGPSilkWavesEffect(strip1, strip2);
    }
    silkWavesInstance->render();
}