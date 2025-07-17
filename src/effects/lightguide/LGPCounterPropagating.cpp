// Light Guide Plate Counter-Propagating Wave Interference
// Simulates actual optical physics of opposing light beams meeting in a waveguide

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

// Counter-Propagating Wave Interference effect
class LGPCounterPropagatingEffect : public LightGuideEffect {
private:
    // Wave parameters for each strip (traveling in opposite directions)
    struct WaveBeam {
        float wavelength;    // In LED units
        float phase;         // Current phase
        float amplitude;     // Wave strength
        uint8_t hue;        // Color
    };
    
    static constexpr uint8_t MAX_BEAMS = 3;  // Per strip
    WaveBeam beams1[MAX_BEAMS];  // Left to right
    WaveBeam beams2[MAX_BEAMS];  // Right to left
    
    float globalPhase;
    float modulationPhase;
    
public:
    LGPCounterPropagatingEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Counter-Propagating", s1, s2),
        globalPhase(0), modulationPhase(0) {
        
        // Initialize beams with different wavelengths for rich interference
        for (int i = 0; i < MAX_BEAMS; i++) {
            // Strip 1 beams (left edge, traveling right)
            beams1[i].wavelength = 20.0f + i * 15.0f;  // 20, 35, 50
            beams1[i].phase = i * TWO_PI / MAX_BEAMS;
            beams1[i].amplitude = 1.0f - i * 0.2f;
            beams1[i].hue = i * 85;  // Spread across spectrum
            
            // Strip 2 beams (right edge, traveling left) - slightly different wavelengths
            beams2[i].wavelength = 25.0f + i * 12.0f;  // 25, 37, 49
            beams2[i].phase = i * TWO_PI / MAX_BEAMS + PI/4;
            beams2[i].amplitude = 1.0f - i * 0.2f;
            beams2[i].hue = 128 + i * 85;  // Complementary colors
        }
    }
    
    void render() override {
        // Update phases
        globalPhase += paletteSpeed * 0.001f;
        modulationPhase += paletteSpeed * 0.0003f;
        
        // Clear strips
        fill_solid(strip1, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        fill_solid(strip2, HardwareConfig::STRIP_LENGTH, CRGB::Black);
        
        // For each LED position, calculate the interference pattern
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Distance from each edge (normalized 0-1)
            float distFromLeft = (float)i / HardwareConfig::STRIP_LENGTH;
            float distFromRight = 1.0f - distFromLeft;
            
            // Complex field amplitudes for each position
            float field1_real = 0, field1_imag = 0;
            float field2_real = 0, field2_imag = 0;
            
            // Calculate wave contributions from strip1 beams (left to right)
            for (int b = 0; b < MAX_BEAMS; b++) {
                WaveBeam& beam = beams1[b];
                
                // Wave number k = 2π/λ
                float k = TWO_PI / beam.wavelength;
                
                // Phase at this position (accounting for propagation distance)
                float localPhase = k * i + beam.phase + globalPhase;
                
                // Amplitude decreases with distance (simulating absorption)
                float localAmp = beam.amplitude * exp(-distFromLeft * 0.5f * visualParams.getVariationNorm());
                
                // Add to complex field (real + imaginary parts)
                field1_real += localAmp * cos(localPhase);
                field1_imag += localAmp * sin(localPhase);
            }
            
            // Calculate wave contributions from strip2 beams (right to left)
            for (int b = 0; b < MAX_BEAMS; b++) {
                WaveBeam& beam = beams2[b];
                
                float k = TWO_PI / beam.wavelength;
                
                // Note: traveling in opposite direction, so phase depends on distance from RIGHT
                float localPhase = k * (HardwareConfig::STRIP_LENGTH - i) + beam.phase + globalPhase;
                
                float localAmp = beam.amplitude * exp(-distFromRight * 0.5f * visualParams.getVariationNorm());
                
                field2_real += localAmp * cos(localPhase);
                field2_imag += localAmp * sin(localPhase);
            }
            
            // Total field is the superposition of both
            float total_real = field1_real + field2_real;
            float total_imag = field1_imag + field2_imag;
            
            // Intensity is the square of the field magnitude
            float intensity = sqrt(total_real * total_real + total_imag * total_imag);
            
            // Phase gives us color information
            float phase = atan2(total_imag, total_real);
            
            // Standing wave enhancement at center (where beams meet strongest)
            float centerDist = abs(i - HardwareConfig::STRIP_CENTER_POINT) / (float)HardwareConfig::STRIP_CENTER_POINT;
            float standingWaveBoost = 1.0f + (1.0f - centerDist) * visualParams.getComplexityNorm();
            intensity *= standingWaveBoost;
            
            // Color based on interference phase
            uint8_t hue = gHue + (uint8_t)(phase * 255 / TWO_PI);
            
            // Modulate saturation based on field strength ratio
            float ratio = abs(field1_real + field1_imag) / (abs(field2_real + field2_imag) + 0.001f);
            uint8_t saturation = 200 + (uint8_t)(55 * (1.0f - abs(ratio - 1.0f) / (ratio + 1.0f)));
            
            // Brightness from interference intensity
            uint8_t brightness = (uint8_t)(min(intensity * 255, 255.0f) * visualParams.getIntensityNorm());
            
            // Apply to strips with slight variation to show wave direction
            CRGB color = CHSV(hue, saturation, brightness);
            
            // Strip1 shows more of the rightward-traveling component
            strip1[i] = color;
            strip1[i].r = (strip1[i].r * (uint16_t)(128 + abs(field1_real) * 127)) >> 8;
            
            // Strip2 shows more of the leftward-traveling component  
            strip2[i] = color;
            strip2[i].b = (strip2[i].b * (uint16_t)(128 + abs(field2_real) * 127)) >> 8;
        }
        
        // Add bright interference nodes where standing waves form
        if (visualParams.saturation > 150) {
            for (int node = 1; node < 8; node++) {
                // Standing wave nodes occur at specific positions based on wavelength
                float nodePos = HardwareConfig::STRIP_LENGTH * node / 8.0f;
                int pos = (int)nodePos;
                
                if (pos > 0 && pos < HardwareConfig::STRIP_LENGTH) {
                    // Pulsing node brightness
                    uint8_t nodeBright = (sin(globalPhase * 4 + node) + 1.0f) * 127;
                    nodeBright = (nodeBright * (visualParams.saturation - 150)) / 105;
                    
                    // Add to both strips
                    strip1[pos] += CRGB(nodeBright, nodeBright, nodeBright);
                    strip2[pos] += CRGB(nodeBright, nodeBright, nodeBright);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPCounterPropagatingEffect* counterPropagatingInstance = nullptr;

// Effect function for main loop
void lgpCounterPropagating() {
    if (!counterPropagatingInstance) {
        counterPropagatingInstance = new LGPCounterPropagatingEffect(strip1, strip2);
    }
    counterPropagatingInstance->render();
}