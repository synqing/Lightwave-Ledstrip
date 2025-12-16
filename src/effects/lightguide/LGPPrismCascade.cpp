// Light Guide Plate Prism Cascade Effect
// Beautiful spectral dispersion with cascading rainbow waves

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

// Prism Cascade effect class
class LGPPrismCascadeEffect : public LightGuideEffect {
private:
    // Wave parameters
    struct SpectrumWave {
        float position;      // Current position
        float wavelength;    // Color wavelength (affects refraction)
        float amplitude;     // Wave strength
        float velocity;      // Propagation speed
        uint8_t hue;        // Base color
        bool active;
    };
    
    static constexpr uint8_t MAX_WAVES = 8;
    SpectrumWave waves[MAX_WAVES];
    uint32_t lastWaveTime;
    
public:
    LGPPrismCascadeEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Prism Cascade", s1, s2),
        lastWaveTime(0) {
        
        // Initialize waves
        for (auto& wave : waves) {
            wave.active = false;
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Spawn new spectrum wave from center
        if (now - lastWaveTime > (1000 - paletteSpeed * 4)) {
            for (auto& wave : waves) {
                if (!wave.active) {
                    wave.position = HardwareConfig::STRIP_CENTER_POINT;
                    wave.wavelength = 0.5f + visualParams.getComplexityNorm() * 0.5f;
                    wave.amplitude = 1.0f;
                    wave.velocity = 0.3f + visualParams.getIntensityNorm() * 0.7f;
                    wave.hue = gHue;
                    wave.active = true;
                    lastWaveTime = now;
                    break;
                }
            }
        }
        
        // Fade background
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            strip1[i].fadeToBlackBy(20);
            strip2[i].fadeToBlackBy(20);
        }
        
        // Update and render waves
        for (auto& wave : waves) {
            if (!wave.active) continue;
            
            // Wave expands outward from center
            wave.amplitude *= 0.98f;  // Gentle fade
            
            // Different colors refract at different angles (dispersion)
            for (int color = 0; color < 7; color++) {
                // Each color has slightly different propagation
                float colorOffset = color * 0.1f * wave.wavelength;
                float leftPos = wave.position - (wave.velocity + colorOffset) * (now - lastWaveTime) * 0.1f;
                float rightPos = wave.position + (wave.velocity + colorOffset) * (now - lastWaveTime) * 0.1f;
                
                // Color based on spectral position
                uint8_t spectralHue = wave.hue + (color * 255 / 7);
                
                // Gaussian profile for each spectral line
                for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                    // Left side
                    float distLeft = abs(i - leftPos);
                    if (distLeft < 10) {
                        float gaussian = exp(-(distLeft * distLeft) / (2 * wave.wavelength));
                        uint8_t brightness = gaussian * wave.amplitude * 255;
                        uint8_t saturation = 255 - (color * 20);  // Slightly desaturate outer colors
                        
                        CRGB newColor = CHSV(spectralHue, saturation, brightness);
                        strip1[i] += newColor;
                        strip2[i] += CHSV(spectralHue + 128, saturation, brightness * 0.7f);
                    }
                    
                    // Right side
                    float distRight = abs(i - rightPos);
                    if (distRight < 10) {
                        float gaussian = exp(-(distRight * distRight) / (2 * wave.wavelength));
                        uint8_t brightness = gaussian * wave.amplitude * 255;
                        uint8_t saturation = 255 - (color * 20);
                        
                        CRGB newColor = CHSV(spectralHue, saturation, brightness);
                        strip1[i] += newColor;
                        strip2[i] += CHSV(spectralHue + 128, saturation, brightness * 0.7f);
                    }
                }
            }
            
            // Update wave position for next frame
            wave.position = HardwareConfig::STRIP_CENTER_POINT;  // Keep centered
            
            // Deactivate faded waves
            if (wave.amplitude < 0.1f) {
                wave.active = false;
            }
        }
        
        // Add subtle center glow
        for (int i = -3; i <= 3; i++) {
            int pos = HardwareConfig::STRIP_CENTER_POINT + i;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                float fade = 1.0f - abs(i) / 3.0f;
                uint8_t glow = 30 * fade * visualParams.getSaturationNorm();
                strip1[pos] += CRGB(glow, glow, glow);
                strip2[pos] += CRGB(glow, glow, glow);
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPPrismCascadeEffect* prismCascadeInstance = nullptr;

// Effect function for main loop
void lgpPrismCascade() {
    if (!prismCascadeInstance) {
        prismCascadeInstance = new LGPPrismCascadeEffect(strip1, strip2);
    }
    prismCascadeInstance->render();
}