// Light Guide Plate Particle Collider Effect
// Simulates high-energy particle collisions with tracks

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

// Particle collider effect class
class LGPParticleColliderEffect : public LightGuideEffect {
private:
    LGPPhysicsEngine physics;
    uint32_t lastCollision;
    uint32_t nextCollisionTime;
    
    // Create collision debris
    void createCollisionDebris(float x, float y, uint8_t count) {
        for (uint8_t i = 0; i < count; i++) {
            float angle = (TWO_PI * i) / count + random8() * 0.01f;
            float speed = 0.1f + random8() * 0.002f;
            float vx = cos(angle) * speed;
            float vy = sin(angle) * speed;
            
            CRGB color = ColorFromPalette(currentPalette, gHue + random8(), 255);
            physics.addParticle(x, y, vx, vy, 0.5f, 0, color);
        }
    }
    
public:
    LGPParticleColliderEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Particle Collider", s1, s2),
        lastCollision(0),
        nextCollisionTime(1000) {
        physics.reset();
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Schedule collisions
        if (now > nextCollisionTime) {
            // Launch particles from opposite edges toward center
            float y1 = 0.3f + random8() * 0.004f;
            float y2 = 0.7f - random8() * 0.004f;
            float velocity = 0.2f + visualParams.getIntensityNorm() * 0.3f;
            
            // Particle 1 from left edge
            CRGB color1 = ColorFromPalette(currentPalette, gHue, 255);
            physics.addParticle(0.0f, y1, velocity, 0, 1.0f, 1.0f, color1);
            
            // Particle 2 from right edge
            CRGB color2 = ColorFromPalette(currentPalette, gHue + 128, 255);
            physics.addParticle(1.0f, y2, -velocity, 0, 1.0f, -1.0f, color2);
            
            lastCollision = now;
            nextCollisionTime = now + (2000 - paletteSpeed * 8);
        }
        
        // Update physics
        physics.update();
        
        // Clear strips with trail effect
        fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 10);
        fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 10);
        
        // Render particle tracks
        physics.renderParticlesToStrips(strip1, strip2);
        
        // Add collision flash at center when particles meet
        for (uint8_t i = 0; i < LGPPhysics::MAX_PARTICLES - 1; i++) {
            for (uint8_t j = i + 1; j < LGPPhysics::MAX_PARTICLES; j++) {
                // Simple collision detection (would be better in physics engine)
                // This is a simplified version for demonstration
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPParticleColliderEffect* colliderInstance = nullptr;

// Effect function for main loop
void lgpParticleCollider() {
    if (!colliderInstance) {
        colliderInstance = new LGPParticleColliderEffect(strip1, strip2);
    }
    colliderInstance->render();
}