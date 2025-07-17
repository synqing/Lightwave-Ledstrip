// Light Guide Plate Dynamic Diffraction Grating
// Simulates light diffracting through moving virtual slits with spectral separation

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

// Dynamic Diffraction Grating effect
class LGPDiffractionGratingEffect : public LightGuideEffect {
private:
    // Grating parameters
    float gratingPosition;      // Center position of grating
    float slitSpacing;          // Distance between slits
    float slitWidth;            // Width of each slit
    uint8_t numSlits;           // Number of slits
    float gratingPhase;         // Animation phase
    
    // Wavelength simulation (different colors)
    struct SpectralComponent {
        float wavelength;       // Relative wavelength (red > green > blue)
        uint8_t hue;            // FastLED hue
        float intensity;        // Component intensity
    };
    
    static constexpr uint8_t NUM_WAVELENGTHS = 5;
    SpectralComponent spectrum[NUM_WAVELENGTHS];
    
public:
    LGPDiffractionGratingEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Diffraction Grating", s1, s2),
        gratingPosition(HardwareConfig::STRIP_CENTER_POINT),
        slitSpacing(10), slitWidth(3), numSlits(7), gratingPhase(0) {
        
        // Initialize spectrum (simulating real wavelengths)
        spectrum[0] = {1.2f, 0, 0.8f};      // Red (~700nm)
        spectrum[1] = {1.1f, 32, 0.9f};     // Orange (~620nm)
        spectrum[2] = {1.0f, 64, 1.0f};     // Yellow (~580nm)
        spectrum[3] = {0.9f, 96, 0.9f};     // Green (~550nm)
        spectrum[4] = {0.7f, 160, 0.8f};    // Blue (~450nm)
    }
    
    void render() override {
        // Update grating animation
        gratingPhase += paletteSpeed * 0.001f;
        
        // Dynamic grating parameters
        slitSpacing = 8 + visualParams.getComplexityNorm() * 12;  // 8-20 LEDs
        numSlits = 3 + visualParams.getVariationNorm() * 8;       // 3-11 slits
        
        // Move grating position
        gratingPosition = HardwareConfig::STRIP_CENTER_POINT + 
                         sin(gratingPhase) * 30 * visualParams.getComplexityNorm();
        
        // Clear strips
        fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        
        // For each LED, calculate diffraction pattern
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float totalIntensity1 = 0;
            float totalIntensity2 = 0;
            CRGB color1 = CRGB::Black;
            CRGB color2 = CRGB::Black;
            
            // For each spectral component
            for (int w = 0; w < NUM_WAVELENGTHS; w++) {
                SpectralComponent& wave = spectrum[w];
                
                // Calculate diffraction for this wavelength
                float diffractedIntensity1 = 0;
                float diffractedIntensity2 = 0;
                
                // For each slit in the grating
                for (int s = 0; s < numSlits; s++) {
                    // Slit position
                    float slitPos = gratingPosition + (s - numSlits/2.0f) * slitSpacing;
                    
                    // Check if we're at a slit or blocked
                    float distToSlit = abs(i - slitPos);
                    if (distToSlit > slitWidth / 2.0f) {
                        // We're blocked, but light diffracts around edges
                        
                        // Calculate diffraction angle based on wavelength
                        float diffractionAngle = asin(wave.wavelength / slitSpacing);
                        
                        // Distance from slit edge
                        float edgeDist = distToSlit - slitWidth / 2.0f;
                        
                        // Diffraction pattern (sinc function)
                        float x = edgeDist * sin(diffractionAngle) / wave.wavelength;
                        float sincValue = (x == 0) ? 1.0f : sin(x) / x;
                        sincValue = sincValue * sincValue;  // Intensity is amplitude squared
                        
                        // Different diffraction orders
                        for (int order = -2; order <= 2; order++) {
                            if (order == 0) continue;  // Skip zero order (straight through)
                            
                            float orderAngle = order * diffractionAngle;
                            float orderDist = edgeDist - tan(orderAngle) * 50;  // 50 is virtual screen distance
                            
                            if (abs(orderDist) < HardwareConfig::STRIP_LENGTH / 2) {
                                float orderIntensity = sincValue / (abs(order) + 1);
                                
                                // Strip 1 gets positive orders more strongly
                                if (order > 0) {
                                    diffractedIntensity1 += orderIntensity * 0.3f;
                                } else {
                                    diffractedIntensity1 += orderIntensity * 0.1f;
                                }
                                
                                // Strip 2 gets negative orders more strongly
                                if (order < 0) {
                                    diffractedIntensity2 += orderIntensity * 0.3f;
                                } else {
                                    diffractedIntensity2 += orderIntensity * 0.1f;
                                }
                            }
                        }
                    } else {
                        // We're at a slit - light passes through directly
                        diffractedIntensity1 += wave.intensity;
                        diffractedIntensity2 += wave.intensity;
                    }
                }
                
                // Add spectral component to color
                if (diffractedIntensity1 > 0) {
                    CRGB spectralColor = CHSV(wave.hue + gHue, 255, 
                                             (uint8_t)(diffractedIntensity1 * 255 * visualParams.getIntensityNorm()));
                    color1 += spectralColor;
                }
                
                if (diffractedIntensity2 > 0) {
                    CRGB spectralColor = CHSV(wave.hue + gHue + 15, 240, 
                                             (uint8_t)(diffractedIntensity2 * 255 * visualParams.getIntensityNorm()));
                    color2 += spectralColor;
                }
            }
            
            // Apply colors with edge lighting effects
            strip1[i] = color1;
            strip2[i] = color2;
            
            // Add bright spots at slit positions when saturation is high
            if (visualParams.saturation > 150) {
                for (int s = 0; s < numSlits; s++) {
                    float slitPos = gratingPosition + (s - numSlits/2.0f) * slitSpacing;
                    float distToSlit = abs(i - slitPos);
                    
                    if (distToSlit < slitWidth) {
                        uint8_t slitBright = (1.0f - distToSlit / slitWidth) * 
                                           (visualParams.saturation - 150) / 105 * 200;
                        strip1[i] += CRGB(slitBright, slitBright, slitBright);
                        strip2[i] += CRGB(slitBright, slitBright, slitBright);
                    }
                }
            }
        }
        
        // Add spectral rainbow effect at diffraction points
        for (int i = 1; i < HardwareConfig::STRIP_LENGTH - 1; i++) {
            // Find high gradient areas (diffraction edges)
            uint16_t gradient1 = abs((int16_t)strip1[i].r - strip1[i-1].r) + 
                                abs((int16_t)strip1[i].g - strip1[i-1].g) + 
                                abs((int16_t)strip1[i].b - strip1[i-1].b);
            
            if (gradient1 > 100) {
                // Add rainbow shimmer at edges
                uint8_t rainbowHue = (uint8_t)(i * 255 / HardwareConfig::STRIP_LENGTH + gratingPhase * 50);
                strip1[i] = blend(strip1[i], CHSV(rainbowHue, 255, 100), 64);
                strip2[i] = blend(strip2[i], CHSV(rainbowHue + 128, 255, 100), 64);
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPDiffractionGratingEffect* diffractionGratingInstance = nullptr;

// Effect function for main loop
void lgpDiffractionGrating() {
    if (!diffractionGratingInstance) {
        diffractionGratingInstance = new LGPDiffractionGratingEffect(strip1, strip2);
    }
    diffractionGratingInstance->render();
}