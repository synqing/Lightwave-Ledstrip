// Light Guide Plate Plasma Field Effect
// Simulates charged particle interactions between edges

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "LightGuideEffect.h"
#include "LGPPhysicsEngine.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// Plasma field effect class
class LGPPlasmaFieldEffect : public LightGuideEffect {
private:
    LGPPhysicsEngine physics;
    uint32_t lastParticleSpawn;
    uint32_t lastChargeUpdate;
    
public:
    LGPPlasmaFieldEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Plasma Field", s1, s2),
        lastParticleSpawn(0),
        lastChargeUpdate(0) {
        physics.reset();
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Update charge positions (create moving dipoles)
        if (now - lastChargeUpdate > 50) {
            float t = now * 0.001f;
            
            // Oscillating charges at edges
            float charge1_y = 0.5f + 0.3f * sin(t * visualParams.getComplexityNorm() * 3.0f);
            float charge2_y = 0.5f + 0.3f * cos(t * visualParams.getComplexityNorm() * 3.0f);
            
            // Charge magnitude based on intensity
            float chargeMag = visualParams.getIntensityNorm() * 10.0f;
            
            // Setup dipole field
            physics.setupDipoleField(0.0f, charge1_y, chargeMag, 1.0f, charge2_y, -chargeMag);
            
            lastChargeUpdate = now;
        }
        
        // Spawn charged particles from center
        if (now - lastParticleSpawn > (100 - paletteSpeed)) {
            // Spawn at center with random velocity
            float vx = random8() / 255.0f - 0.5f;
            float vy = random8() / 255.0f - 0.5f;
            float charge = random8() < 128 ? 1.0f : -1.0f;
            
            // Color based on charge
            CRGB color = charge > 0 ? 
                ColorFromPalette(currentPalette, gHue, 255) : 
                ColorFromPalette(currentPalette, gHue + 128, 255);
            
            // Add particles at center positions
            physics.addParticle(0.5f, 0.5f, vx * 0.2f, vy * 0.2f, 1.0f, charge, color);
            
            lastParticleSpawn = now;
        }
        
        // Update physics
        physics.update();
        
        // Clear strips
        fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
        fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
        
        // Render field lines as background
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float y = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Get field strength at edges
            auto field1 = physics.getFieldAt(0.0f, y);
            auto field2 = physics.getFieldAt(1.0f, y);
            
            // Map field strength to brightness
            uint8_t brightness1 = min((uint8_t)255, (uint8_t)(abs(field1.potential) * 50 * visualParams.getSaturationNorm()));
            uint8_t brightness2 = min((uint8_t)255, (uint8_t)(abs(field2.potential) * 50 * visualParams.getSaturationNorm()));
            
            // Apply to strips with color based on potential
            uint8_t hue1 = gHue + (field1.potential > 0 ? 0 : 128);
            uint8_t hue2 = gHue + (field2.potential > 0 ? 0 : 128);
            
            strip1[i] = blend(strip1[i], ColorFromPalette(currentPalette, hue1, brightness1), 128);
            strip2[i] = blend(strip2[i], ColorFromPalette(currentPalette, hue2, brightness2), 128);
        }
        
        // Render particles on top
        physics.renderParticlesToStrips(strip1, strip2);
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPPlasmaFieldEffect* plasmaFieldInstance = nullptr;

// Effect function for main loop
void lgpPlasmaField() {
    if (!plasmaFieldInstance) {
        plasmaFieldInstance = new LGPPlasmaFieldEffect(strip1, strip2);
    }
    plasmaFieldInstance->render();
}