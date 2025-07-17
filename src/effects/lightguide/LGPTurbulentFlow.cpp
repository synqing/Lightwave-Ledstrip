// Light Guide Plate Turbulent Flow Effect
// Simulates fluid dynamics with vortices and color mixing

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

// Vortex structure
struct Vortex {
    float position;      // Position along strip
    float strength;      // Rotation strength
    float radius;        // Influence radius
    float phase;         // Current rotation phase
    int8_t direction;    // Clockwise or counter-clockwise
};

// Turbulent flow effect class
class LGPTurbulentFlowEffect : public LightGuideEffect {
private:
    static constexpr uint8_t MAX_VORTICES = 5;
    Vortex vortices[MAX_VORTICES];
    uint8_t activeVortices;
    
    // Fluid field (velocity at each point)
    float velocityField[HardwareConfig::STRIP_LENGTH];
    float colorField[HardwareConfig::STRIP_LENGTH];
    
    uint32_t lastVortexSpawn;
    
public:
    LGPTurbulentFlowEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Turbulent Flow", s1, s2),
        activeVortices(0),
        lastVortexSpawn(0) {
        
        // Initialize vortices
        for (auto& v : vortices) {
            v.strength = 0;
        }
        
        // Initialize fields
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            velocityField[i] = 0;
            colorField[i] = i * 255.0f / HardwareConfig::STRIP_LENGTH;
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Spawn new vortices from center
        if (now - lastVortexSpawn > (500 - paletteSpeed * 2) && activeVortices < MAX_VORTICES) {
            // Find inactive vortex
            for (auto& v : vortices) {
                if (v.strength <= 0) {
                    // Initialize at center with random parameters
                    v.position = HardwareConfig::STRIP_CENTER_POINT + random8(20) - 10;
                    v.strength = 0.5f + visualParams.getIntensityNorm() * 0.5f;
                    v.radius = 10.0f + visualParams.getComplexityNorm() * 20.0f;
                    v.phase = 0;
                    v.direction = random8() < 128 ? 1 : -1;
                    activeVortices++;
                    break;
                }
            }
            lastVortexSpawn = now;
        }
        
        // Update vortices
        activeVortices = 0;
        for (auto& v : vortices) {
            if (v.strength > 0) {
                // Move vortex outward from center
                float distFromCenter = v.position - HardwareConfig::STRIP_CENTER_POINT;
                v.position += (distFromCenter > 0 ? 0.5f : -0.5f);
                
                // Rotate phase
                v.phase += v.direction * v.strength * 0.1f;
                
                // Decay strength
                v.strength *= 0.98f;
                if (v.strength < 0.01f) v.strength = 0;
                else activeVortices++;
                
                // Boundary check
                if (v.position < 0 || v.position >= HardwareConfig::STRIP_LENGTH) {
                    v.strength = 0;
                }
            }
        }
        
        // Calculate velocity field from vortices
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float totalVelocity = 0;
            
            for (const auto& v : vortices) {
                if (v.strength > 0) {
                    float dist = abs(i - v.position);
                    if (dist < v.radius) {
                        // Velocity induced by vortex (simplified 2D)
                        float influence = (1.0f - dist / v.radius) * v.strength;
                        float vel = sin(v.phase + dist * 0.2f) * influence;
                        totalVelocity += vel;
                    }
                }
            }
            
            // Update velocity with damping
            velocityField[i] = velocityField[i] * 0.9f + totalVelocity * 0.1f;
        }
        
        // Advect color field (fluid carries color)
        float newColorField[HardwareConfig::STRIP_LENGTH];
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Calculate where this color came from
            float sourcePos = i - velocityField[i] * 2.0f;
            
            // Interpolate color from source position
            if (sourcePos < 0) sourcePos = 0;
            if (sourcePos >= HardwareConfig::STRIP_LENGTH - 1) sourcePos = HardwareConfig::STRIP_LENGTH - 1;
            
            int sourceLow = (int)sourcePos;
            int sourceHigh = sourceLow + 1;
            float frac = sourcePos - sourceLow;
            
            if (sourceHigh >= HardwareConfig::STRIP_LENGTH) sourceHigh = HardwareConfig::STRIP_LENGTH - 1;
            
            newColorField[i] = colorField[sourceLow] * (1.0f - frac) + colorField[sourceHigh] * frac;
        }
        
        // Add some diffusion for smoothness
        for (int i = 1; i < HardwareConfig::STRIP_LENGTH - 1; i++) {
            colorField[i] = newColorField[i] * 0.8f + 
                           (newColorField[i-1] + newColorField[i+1]) * 0.1f;
        }
        
        // Inject new color at center
        colorField[HardwareConfig::STRIP_CENTER_POINT] = gHue;
        colorField[HardwareConfig::STRIP_CENTER_POINT + 1] = gHue;
        
        // Render to strips
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Map velocity to saturation (fast flow = less saturated)
            uint8_t saturation = 255 - abs(velocityField[i]) * 50;
            saturation = max(saturation, (uint8_t)180);
            
            // Brightness based on vortex proximity
            uint8_t brightness = 100;
            for (const auto& v : vortices) {
                if (v.strength > 0) {
                    float dist = abs(i - v.position);
                    if (dist < v.radius) {
                        brightness = max(brightness, (uint8_t)(255 * (1.0f - dist / v.radius) * v.strength));
                    }
                }
            }
            
            // Apply color mixing
            uint8_t hue1 = (uint8_t)colorField[i];
            uint8_t hue2 = hue1 + (velocityField[i] * 30);  // Doppler-like shift
            
            strip1[i] = CHSV(hue1, saturation, brightness);
            strip2[i] = CHSV(hue2, saturation, brightness);
        }
        
        // Add turbulence indicators (optional)
        if (visualParams.variation > 200) {
            for (const auto& v : vortices) {
                if (v.strength > 0.3f) {
                    int pos = (int)v.position;
                    if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                        // Vortex core indicator
                        strip1[pos] = CRGB::White;
                        strip2[pos] = CRGB::White;
                    }
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPTurbulentFlowEffect* turbulentFlowInstance = nullptr;

// Effect function for main loop
void lgpTurbulentFlow() {
    if (!turbulentFlowInstance) {
        turbulentFlowInstance = new LGPTurbulentFlowEffect(strip1, strip2);
    }
    turbulentFlowInstance->render();
}