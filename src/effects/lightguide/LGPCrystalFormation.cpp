// Light Guide Plate Crystal Formation Effect
// Growing and shrinking crystalline patterns with faceted light reflections

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

// Crystal Formation effect class
class LGPCrystalFormationEffect : public LightGuideEffect {
private:
    // Crystal structure
    struct Crystal {
        float center;        // Center position
        float size;          // Current size
        float targetSize;    // Target size for growth/shrink
        float growthRate;    // How fast it grows/shrinks
        uint8_t hue;         // Base color
        float rotation;      // Rotation angle for facets
        float sparklePhase;  // For sparkling effect
        bool active;
    };
    
    static constexpr uint8_t MAX_CRYSTALS = 6;
    Crystal crystals[MAX_CRYSTALS];
    uint32_t lastSpawnTime;
    float globalRotation;
    
public:
    LGPCrystalFormationEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Crystal Formation", s1, s2),
        lastSpawnTime(0), globalRotation(0) {
        
        // Initialize crystals
        for (auto& crystal : crystals) {
            crystal.active = false;
        }
        
        // Start with one crystal at center
        crystals[0].center = HardwareConfig::STRIP_CENTER_POINT;
        crystals[0].size = 10;
        crystals[0].targetSize = 30;
        crystals[0].growthRate = 0.5f;
        crystals[0].hue = gHue;
        crystals[0].rotation = 0;
        crystals[0].sparklePhase = 0;
        crystals[0].active = true;
    }
    
    void render() override {
        uint32_t now = millis();
        globalRotation += paletteSpeed * 0.0005f;
        
        // Spawn new crystals
        if (now - lastSpawnTime > (3000 - paletteSpeed * 10)) {
            for (auto& crystal : crystals) {
                if (!crystal.active) {
                    crystal.center = HardwareConfig::STRIP_CENTER_POINT + 
                                   random(-40, 41) * visualParams.getComplexityNorm();
                    crystal.size = 0;
                    crystal.targetSize = random(15, 40);
                    crystal.growthRate = 0.3f + random(0, 70) / 100.0f;
                    crystal.hue = gHue + random(0, 60);
                    crystal.rotation = random(0, 628) / 100.0f;
                    crystal.sparklePhase = random(0, 628) / 100.0f;
                    crystal.active = true;
                    lastSpawnTime = now;
                    break;
                }
            }
        }
        
        // Clear with fade for trailing effect
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            strip1[i].fadeToBlackBy(40);
            strip2[i].fadeToBlackBy(40);
        }
        
        // Update and render crystals
        for (auto& crystal : crystals) {
            if (!crystal.active) continue;
            
            // Update crystal size
            if (crystal.size < crystal.targetSize) {
                crystal.size += crystal.growthRate;
            } else {
                crystal.size -= crystal.growthRate * 0.3f;  // Shrink slower
                if (crystal.size <= 0) {
                    crystal.active = false;
                    continue;
                }
            }
            
            // Update rotation and sparkle
            crystal.rotation += 0.02f;
            crystal.sparklePhase += 0.05f;
            
            // Draw crystal with faceted structure
            float facets = 3 + visualParams.getComplexityNorm() * 5;  // 3-8 facets
            
            for (int facet = 0; facet < (int)facets; facet++) {
                float facetAngle = (TWO_PI / facets) * facet + crystal.rotation + globalRotation;
                
                // Each facet has different reflectivity
                float reflectivity = 0.5f + 0.5f * sin(facetAngle * 3 + crystal.sparklePhase);
                
                // Draw facet edges and interior
                for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                    float dist = abs(i - crystal.center);
                    
                    if (dist <= crystal.size) {
                        // Calculate position within crystal
                        float normalizedDist = dist / crystal.size;
                        
                        // Facet visibility based on angle
                        float facetVisibility = (1.0f + cos(facetAngle + normalizedDist * PI)) * 0.5f;
                        
                        // Edge highlighting
                        float edgeIntensity = 0;
                        if (normalizedDist > 0.8f) {
                            edgeIntensity = (normalizedDist - 0.8f) * 5.0f;
                        }
                        
                        // Combined intensity
                        float intensity = (facetVisibility * reflectivity + edgeIntensity) * 
                                        (1.0f - normalizedDist * 0.3f);  // Fade toward edges
                        
                        // Sparkle effect
                        if (random8() < 10 * visualParams.getVariationNorm()) {
                            intensity += 0.5f;
                        }
                        
                        // Apply crystal growth fade
                        intensity *= min(crystal.size / 10.0f, 1.0f);
                        
                        // Color calculation
                        uint8_t hue = crystal.hue + (facet * 15) + (uint8_t)(normalizedDist * 30);
                        uint8_t saturation = 150 + (uint8_t)(reflectivity * 105);
                        uint8_t brightness = (uint8_t)(intensity * 255 * visualParams.getIntensityNorm());
                        
                        // Different refractive index for each strip
                        CRGB color1 = CHSV(hue, saturation, brightness);
                        CRGB color2 = CHSV(hue + 15, saturation + 20, brightness * 0.9f);
                        
                        // Additive blending for crystal transparency
                        strip1[i] += color1;
                        strip2[i] += color2;
                    }
                }
            }
            
            // Add bright center core
            for (int i = -2; i <= 2; i++) {
                int pos = (int)crystal.center + i;
                if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                    float coreFade = 1.0f - abs(i) / 2.0f;
                    uint8_t coreIntensity = crystal.size / crystal.targetSize * 100 * coreFade * 
                                          visualParams.getSaturationNorm();
                    CRGB coreColor = CRGB(coreIntensity, coreIntensity, coreIntensity);
                    strip1[pos] += coreColor;
                    strip2[pos] += coreColor;
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPCrystalFormationEffect* crystalFormationInstance = nullptr;

// Effect function for main loop
void lgpCrystalFormation() {
    if (!crystalFormationInstance) {
        crystalFormationInstance = new LGPCrystalFormationEffect(strip1, strip2);
    }
    crystalFormationInstance->render();
}