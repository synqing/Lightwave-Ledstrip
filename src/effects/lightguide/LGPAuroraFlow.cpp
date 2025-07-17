// Light Guide Plate Aurora Flow Effect
// Smooth flowing curtains of light with beautiful color transitions

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

// Aurora Flow effect class
class LGPAuroraFlowEffect : public LightGuideEffect {
private:
    // Aurora curtain parameters
    struct AuroraCurtain {
        float position;      // Center position of curtain
        float width;         // Width of the curtain
        float phase;         // Animation phase
        float intensity;     // Brightness intensity
        uint8_t hueOffset;   // Color offset
        float shimmerPhase;  // Shimmer animation
    };
    
    static constexpr uint8_t MAX_CURTAINS = 3;
    AuroraCurtain curtains[MAX_CURTAINS];
    float globalPhase;
    
    // Color palette for aurora (greens, blues, purples, reds)
    const uint8_t auroraHues[4] = {96, 160, 192, 0};  // Green, Blue, Purple, Red
    
public:
    LGPAuroraFlowEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Aurora Flow", s1, s2),
        globalPhase(0) {
        
        // Initialize curtains
        for (int i = 0; i < MAX_CURTAINS; i++) {
            curtains[i].position = HardwareConfig::STRIP_CENTER_POINT;
            curtains[i].width = 20 + i * 10;
            curtains[i].phase = i * TWO_PI / MAX_CURTAINS;
            curtains[i].intensity = 0.7f + i * 0.1f;
            curtains[i].hueOffset = i * 30;
            curtains[i].shimmerPhase = random(0, 628) / 100.0f;
        }
    }
    
    void render() override {
        // Update global phase
        globalPhase += paletteSpeed * 0.0003f;
        
        // Fade background to create trailing effect
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            strip1[i].fadeToBlackBy(30);
            strip2[i].fadeToBlackBy(30);
        }
        
        // Render each aurora curtain
        for (int c = 0; c < MAX_CURTAINS; c++) {
            AuroraCurtain& curtain = curtains[c];
            
            // Update curtain position - gentle swaying motion
            curtain.position = HardwareConfig::STRIP_CENTER_POINT + 
                              sin(curtain.phase + globalPhase) * 30 * visualParams.getComplexityNorm();
            curtain.phase += 0.01f;
            curtain.shimmerPhase += 0.02f;
            
            // Draw the curtain
            for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                float dist = abs(i - curtain.position);
                
                if (dist < curtain.width * 2) {
                    // Calculate curtain intensity with Gaussian profile
                    float curtainIntensity = exp(-(dist * dist) / (2 * curtain.width * curtain.width));
                    
                    // Add vertical shimmer effect
                    float shimmer = sin(i * 0.1f + curtain.shimmerPhase) * 0.3f + 0.7f;
                    curtainIntensity *= shimmer;
                    
                    // Height variation along the curtain
                    float heightVar = sin(i * 0.05f + globalPhase * 2) * 0.2f + 0.8f;
                    curtainIntensity *= heightVar;
                    
                    // Apply intensity parameter
                    curtainIntensity *= curtain.intensity * visualParams.getIntensityNorm();
                    
                    if (curtainIntensity > 0.01f) {
                        // Select aurora color with smooth transitions
                        uint8_t colorIndex = (c + (uint8_t)(globalPhase * 0.5f)) % 4;
                        uint8_t baseHue = auroraHues[colorIndex] + curtain.hueOffset;
                        
                        // Add color variation along the curtain
                        uint8_t hueShift = sin(i * 0.02f + curtain.phase) * 20;
                        uint8_t finalHue = baseHue + hueShift;
                        
                        // Saturation varies with intensity
                        uint8_t saturation = 180 + (uint8_t)(curtainIntensity * 75);
                        
                        // Create the color
                        uint8_t brightness = (uint8_t)(curtainIntensity * 255);
                        CRGB color = CHSV(finalHue, saturation, brightness);
                        
                        // Apply to both strips with slight variation
                        strip1[i] += color;
                        strip2[i] += CHSV(finalHue + 10, saturation - 20, brightness * 0.8f);
                    }
                }
            }
        }
        
        // Add subtle starfield background
        if (visualParams.variation > 50) {
            for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i += 5) {
                if (random8() < 5) {
                    uint8_t starBright = random8(20, 60) * (visualParams.variation - 50) / 205;
                    strip1[i] += CRGB(starBright, starBright, starBright);
                    strip2[i] += CRGB(starBright, starBright, starBright);
                }
            }
        }
        
        // Add bright center glow when saturation is high
        if (visualParams.saturation > 150) {
            float centerGlow = (sin(globalPhase * 4) + 1.0f) * 0.5f;
            for (int i = -5; i <= 5; i++) {
                int pos = HardwareConfig::STRIP_CENTER_POINT + i;
                if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                    float fade = 1.0f - abs(i) / 5.0f;
                    uint8_t glowIntensity = centerGlow * fade * (visualParams.saturation - 150) / 105 * 100;
                    strip1[pos] += CRGB(0, glowIntensity, glowIntensity/2);  // Cyan glow
                    strip2[pos] += CRGB(0, glowIntensity, glowIntensity/2);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPAuroraFlowEffect* auroraFlowInstance = nullptr;

// Effect function for main loop
void lgpAuroraFlow() {
    if (!auroraFlowInstance) {
        auroraFlowInstance = new LGPAuroraFlowEffect(strip1, strip2);
    }
    auroraFlowInstance->render();
}