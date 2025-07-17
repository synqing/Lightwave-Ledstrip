// Light Guide Plate Nebula Cloud Effect
// Swirling cosmic clouds with stellar formations and color gradients

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

// Nebula Cloud effect class
class LGPNebulaCloudEffect : public LightGuideEffect {
private:
    // Cloud layer parameters
    struct CloudLayer {
        float phase;         // Animation phase
        float frequency;     // Spatial frequency
        float amplitude;     // Wave amplitude
        uint8_t hueBase;     // Base color
        float driftSpeed;    // How fast it drifts
    };
    
    static constexpr uint8_t NUM_LAYERS = 4;
    CloudLayer layers[NUM_LAYERS];
    
    // Star field
    struct Star {
        uint8_t position;
        uint8_t brightness;
        float twinklePhase;
    };
    static constexpr uint8_t MAX_STARS = 20;
    Star stars[MAX_STARS];
    
    float cosmicPhase;
    float rotationPhase;
    
public:
    LGPNebulaCloudEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Nebula Cloud", s1, s2),
        cosmicPhase(0), rotationPhase(0) {
        
        // Initialize cloud layers with different characteristics
        for (int i = 0; i < NUM_LAYERS; i++) {
            layers[i].phase = i * PI / 2;
            layers[i].frequency = 0.02f + i * 0.01f;
            layers[i].amplitude = 0.3f + (NUM_LAYERS - i) * 0.15f;
            layers[i].hueBase = i * 60;  // Different color for each layer
            layers[i].driftSpeed = 0.001f + i * 0.0003f;
        }
        
        // Initialize starfield
        for (auto& star : stars) {
            star.position = random8(HardwareConfig::STRIP_LENGTH);
            star.brightness = random8(100, 255);
            star.twinklePhase = random(0, 628) / 100.0f;
        }
    }
    
    void render() override {
        // Update phases
        cosmicPhase += paletteSpeed * 0.0002f;
        rotationPhase += paletteSpeed * 0.0001f;
        
        // Clear buffers
        fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        
        // Render nebula cloud layers
        for (int layer = 0; layer < NUM_LAYERS; layer++) {
            CloudLayer& cloud = layers[layer];
            cloud.phase += cloud.driftSpeed * paletteSpeed;
            
            for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                // Calculate distance from center for radial effects
                float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_CENTER_POINT;
                
                // Complex wave pattern for cloud density
                float density = 0;
                density += sin(i * cloud.frequency + cloud.phase) * cloud.amplitude;
                density += sin(i * cloud.frequency * 2.3f + cloud.phase * 1.7f) * cloud.amplitude * 0.5f;
                density += cos(i * cloud.frequency * 0.7f + cloud.phase * 0.9f) * cloud.amplitude * 0.3f;
                
                // Radial falloff for cloud structure
                density *= (1.0f - distFromCenter * 0.5f);
                
                // Rotation effect
                density += sin(distFromCenter * PI + rotationPhase + layer * PI/4) * 0.2f;
                
                // Normalize and constrain
                density = (density + 1.0f) * 0.5f;
                density = constrain(density, 0, 1);
                
                if (density > 0.1f) {
                    // Color calculation with nebula-like hues
                    float colorShift = sin(i * 0.01f + cosmicPhase) * 30;
                    uint8_t hue = cloud.hueBase + gHue + (uint8_t)colorShift;
                    
                    // Add color complexity based on user control
                    if (visualParams.complexity > 100) {
                        hue += (uint8_t)(sin(density * PI * 2 + cloud.phase) * 20);
                    }
                    
                    // Saturation varies with density
                    uint8_t saturation = 150 + (uint8_t)(density * 105);
                    
                    // Brightness calculation
                    uint8_t brightness = (uint8_t)(density * 200 * visualParams.getIntensityNorm());
                    
                    // Layer blending - each layer adds to the nebula
                    CRGB color = CHSV(hue, saturation, brightness / (layer + 1));
                    
                    // Different mixing for each strip to create depth
                    strip1[i] += color;
                    strip2[i] += CHSV(hue + 20, saturation - 30, brightness / (layer + 1) * 0.8f);
                }
            }
        }
        
        // Add bright nebula core at center
        float coreIntensity = (sin(cosmicPhase * 3) + 1.0f) * 0.5f;
        for (int i = -15; i <= 15; i++) {
            int pos = HardwareConfig::STRIP_CENTER_POINT + i;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                float coreFade = exp(-(i * i) / 50.0f);  // Gaussian profile
                uint8_t coreBrightness = coreIntensity * coreFade * 100 * visualParams.getSaturationNorm();
                
                // Colorful nebula core
                uint8_t coreHue = gHue + (uint8_t)(sin(cosmicPhase * 2) * 40);
                CRGB coreColor = CHSV(coreHue, 200, coreBrightness);
                strip1[pos] += coreColor;
                strip2[pos] += CHSV(coreHue + 30, 220, coreBrightness * 0.9f);
            }
        }
        
        // Add twinkling stars
        if (visualParams.variation > 50) {
            for (auto& star : stars) {
                star.twinklePhase += 0.1f;
                float twinkle = (sin(star.twinklePhase) + 1.0f) * 0.5f;
                uint8_t starBright = star.brightness * twinkle * (visualParams.variation - 50) / 205;
                
                if (starBright > 20) {
                    // Stars have slight color variation
                    uint8_t starHue = random8(160, 255);  // Blue to red spectrum
                    uint8_t starSat = random8(0, 100);    // Some white, some colored
                    
                    strip1[star.position] += CHSV(starHue, starSat, starBright);
                    strip2[star.position] += CHSV(starHue, starSat, starBright * 0.8f);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPNebulaCloudEffect* nebulaCloudInstance = nullptr;

// Effect function for main loop
void lgpNebulaCloud() {
    if (!nebulaCloudInstance) {
        nebulaCloudInstance = new LGPNebulaCloudEffect(strip1, strip2);
    }
    nebulaCloudInstance->render();
}