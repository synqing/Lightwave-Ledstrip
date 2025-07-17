// Light Guide Plate Mach-Zehnder Interferometer
// Simulates split beam paths with phase modulation creating interference fringes

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

// Mach-Zehnder Interferometer effect
class LGPMachZehnderEffect : public LightGuideEffect {
private:
    // Interferometer arms
    struct BeamPath {
        float phaseDelay;       // Additional phase in this path
        float pathLength;       // Relative path length
        float modulation;       // Dynamic phase modulation
        float splitRatio;       // How much light goes into this path
    };
    
    // Each edge has its own interferometer
    BeamPath strip1_pathA, strip1_pathB;
    BeamPath strip2_pathA, strip2_pathB;
    
    float globalPhase;
    float modulationPhase;
    
    // Wavelength-dependent phase shifts
    struct WavelengthData {
        float wavelength;
        uint8_t hue;
        float refractiveIndex;  // Different "materials" affect different wavelengths
    };
    
    static constexpr uint8_t NUM_WAVELENGTHS = 4;
    WavelengthData wavelengths[NUM_WAVELENGTHS];
    
public:
    LGPMachZehnderEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Mach-Zehnder", s1, s2),
        globalPhase(0), modulationPhase(0) {
        
        // Initialize beam paths
        strip1_pathA = {0, 1.0f, 0, 0.5f};
        strip1_pathB = {PI, 1.05f, 0, 0.5f};  // Slightly longer path
        strip2_pathA = {PI/2, 1.0f, 0, 0.5f};
        strip2_pathB = {3*PI/2, 1.03f, 0, 0.5f};
        
        // Initialize wavelengths with different refractive indices
        wavelengths[0] = {1.0f, 0, 1.45f};     // Red
        wavelengths[1] = {0.9f, 64, 1.46f};    // Yellow  
        wavelengths[2] = {0.8f, 96, 1.47f};    // Green
        wavelengths[3] = {0.6f, 160, 1.48f};   // Blue (highest refraction)
    }
    
    void render() override {
        // Update phase modulation
        globalPhase += paletteSpeed * 0.001f;
        modulationPhase += paletteSpeed * 0.0005f;
        
        // Modulate path delays dynamically
        float modAmount = visualParams.getComplexityNorm();
        strip1_pathA.modulation = sin(modulationPhase) * modAmount * PI;
        strip1_pathB.modulation = cos(modulationPhase * 1.3f) * modAmount * PI;
        strip2_pathA.modulation = sin(modulationPhase * 0.7f) * modAmount * PI;
        strip2_pathB.modulation = cos(modulationPhase * 0.9f) * modAmount * PI;
        
        // Adjust split ratios based on variation
        float splitVar = 0.3f + visualParams.getVariationNorm() * 0.4f;  // 0.3 to 0.7
        strip1_pathA.splitRatio = splitVar;
        strip1_pathB.splitRatio = 1.0f - splitVar;
        strip2_pathA.splitRatio = 1.0f - splitVar;
        strip2_pathB.splitRatio = splitVar;
        
        // Clear strips
        fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        
        // Process each position
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Position normalized 0-1
            float pos = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Initialize color accumulators
            float r1 = 0, g1 = 0, b1 = 0;
            float r2 = 0, g2 = 0, b2 = 0;
            
            // For each wavelength
            for (int w = 0; w < NUM_WAVELENGTHS; w++) {
                WavelengthData& wave = wavelengths[w];
                
                // === STRIP 1 INTERFEROMETER ===
                // Path A amplitude and phase
                float amp1A = strip1_pathA.splitRatio;
                float phase1A = TWO_PI * pos / wave.wavelength + 
                               strip1_pathA.phaseDelay + 
                               strip1_pathA.modulation +
                               strip1_pathA.pathLength * wave.refractiveIndex * TWO_PI;
                
                // Path B amplitude and phase  
                float amp1B = strip1_pathB.splitRatio;
                float phase1B = TWO_PI * pos / wave.wavelength + 
                               strip1_pathB.phaseDelay + 
                               strip1_pathB.modulation +
                               strip1_pathB.pathLength * wave.refractiveIndex * TWO_PI;
                
                // Interference at recombination
                float field1_real = amp1A * cos(phase1A + globalPhase) + amp1B * cos(phase1B + globalPhase);
                float field1_imag = amp1A * sin(phase1A + globalPhase) + amp1B * sin(phase1B + globalPhase);
                float intensity1 = field1_real * field1_real + field1_imag * field1_imag;
                
                // === STRIP 2 INTERFEROMETER ===
                // Similar but with light traveling from the other direction
                float amp2A = strip2_pathA.splitRatio;
                float phase2A = TWO_PI * (1.0f - pos) / wave.wavelength + 
                               strip2_pathA.phaseDelay + 
                               strip2_pathA.modulation +
                               strip2_pathA.pathLength * wave.refractiveIndex * TWO_PI;
                
                float amp2B = strip2_pathB.splitRatio;
                float phase2B = TWO_PI * (1.0f - pos) / wave.wavelength + 
                               strip2_pathB.phaseDelay + 
                               strip2_pathB.modulation +
                               strip2_pathB.pathLength * wave.refractiveIndex * TWO_PI;
                
                float field2_real = amp2A * cos(phase2A + globalPhase) + amp2B * cos(phase2B + globalPhase);
                float field2_imag = amp2A * sin(phase2A + globalPhase) + amp2B * sin(phase2B + globalPhase);
                float intensity2 = field2_real * field2_real + field2_imag * field2_imag;
                
                // Convert to color
                CRGB color = CHSV(wave.hue + gHue, 255, 255);
                
                // Add to RGB accumulators
                r1 += color.r * intensity1 / 255.0f;
                g1 += color.g * intensity1 / 255.0f;
                b1 += color.b * intensity1 / 255.0f;
                
                r2 += color.r * intensity2 / 255.0f;
                g2 += color.g * intensity2 / 255.0f;
                b2 += color.b * intensity2 / 255.0f;
            }
            
            // Apply accumulated colors
            strip1[i] = CRGB(
                (uint8_t)min(r1 * 128 * visualParams.getIntensityNorm(), 255.0f),
                (uint8_t)min(g1 * 128 * visualParams.getIntensityNorm(), 255.0f),
                (uint8_t)min(b1 * 128 * visualParams.getIntensityNorm(), 255.0f)
            );
            
            strip2[i] = CRGB(
                (uint8_t)min(r2 * 128 * visualParams.getIntensityNorm(), 255.0f),
                (uint8_t)min(g2 * 128 * visualParams.getIntensityNorm(), 255.0f),
                (uint8_t)min(b2 * 128 * visualParams.getIntensityNorm(), 255.0f)
            );
            
            // Add interference fringe enhancement at center
            float centerDist = abs(i - HardwareConfig::STRIP_CENTER_POINT) / (float)HardwareConfig::STRIP_CENTER_POINT;
            if (centerDist < 0.3f && visualParams.saturation > 150) {
                // Where the two counter-propagating interferometers meet
                float enhancement = (1.0f - centerDist / 0.3f) * (visualParams.saturation - 150) / 105;
                
                // Cross-interference creates white light
                uint8_t crossLight = enhancement * 100;
                strip1[i] += CRGB(crossLight, crossLight, crossLight);
                strip2[i] += CRGB(crossLight, crossLight, crossLight);
            }
        }
        
        // Add beam splitter/combiner visualization points
        for (int s = 0; s < 2; s++) {
            CRGB* strip = (s == 0) ? strip1 : strip2;
            
            // Beam splitter at 1/3
            int splitPos = HardwareConfig::STRIP_LENGTH / 3;
            if (splitPos > 0 && splitPos < HardwareConfig::STRIP_LENGTH) {
                strip[splitPos] += CRGB(50, 50, 100);  // Blue tint for splitter
            }
            
            // Beam combiner at 2/3
            int combinePos = 2 * HardwareConfig::STRIP_LENGTH / 3;
            if (combinePos > 0 && combinePos < HardwareConfig::STRIP_LENGTH) {
                strip[combinePos] += CRGB(100, 50, 50);  // Red tint for combiner
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPMachZehnderEffect* machZehnderInstance = nullptr;

// Effect function for main loop
void lgpMachZehnder() {
    if (!machZehnderInstance) {
        machZehnderInstance = new LGPMachZehnderEffect(strip1, strip2);
    }
    machZehnderInstance->render();
}