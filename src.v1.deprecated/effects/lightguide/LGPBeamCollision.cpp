// Light Guide Plate Beam Collision Explosion
// Two laser beams shoot from opposite edges and EXPLODE when they meet!

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

// Beam Collision effect class
class LGPBeamCollisionEffect : public LightGuideEffect {
private:
    // Laser beam structure
    struct LaserBeam {
        float position;      // Current head position
        float velocity;      // Speed
        CRGB color;         // Beam color
        float intensity;     // Beam brightness
        bool active;        // Is beam alive?
        float trail[20];    // Trail positions for afterglow
    };
    
    static constexpr uint8_t MAX_BEAMS = 4;
    LaserBeam beams1[MAX_BEAMS];  // From left edge
    LaserBeam beams2[MAX_BEAMS];  // From right edge
    
    // Explosion particles
    struct Particle {
        float x;
        float velocity;
        CRGB color;
        float life;
        bool active;
    };
    
    static constexpr uint16_t MAX_PARTICLES = 100;
    Particle particles[MAX_PARTICLES];
    
    uint32_t lastBeamTime;
    float explosionPhase;
    
public:
    LGPBeamCollisionEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Beam Collision", s1, s2),
        lastBeamTime(0), explosionPhase(0) {
        
        // Initialize everything inactive
        for (auto& beam : beams1) beam.active = false;
        for (auto& beam : beams2) beam.active = false;
        for (auto& particle : particles) particle.active = false;
    }
    
    void spawnBeam(bool fromLeft) {
        LaserBeam* beams = fromLeft ? beams1 : beams2;
        
        for (int i = 0; i < MAX_BEAMS; i++) {
            if (!beams[i].active) {
                beams[i].position = fromLeft ? 0 : HardwareConfig::STRIP_LENGTH - 1;
                beams[i].velocity = (2.0f + random(0, 30) / 10.0f) * (fromLeft ? 1 : -1);
                beams[i].velocity *= (0.5f + visualParams.getIntensityNorm());
                
                // Get color from palette
                uint8_t paletteIndex = random8();  // Random position in palette
                beams[i].color = ColorFromPalette(currentPalette, paletteIndex, 255, LINEARBLEND);
                beams[i].intensity = 1.0f;
                beams[i].active = true;
                
                // Clear trail
                for (int t = 0; t < 20; t++) {
                    beams[i].trail[t] = beams[i].position;
                }
                break;
            }
        }
    }
    
    void createExplosion(float pos, CRGB color1, CRGB color2) {
        // Spawn explosion particles
        int particleCount = 20 + visualParams.getComplexityNorm() * 30;
        
        for (int i = 0; i < particleCount && i < MAX_PARTICLES; i++) {
            for (auto& particle : particles) {
                if (!particle.active) {
                    particle.x = pos;
                    particle.velocity = random(-80, 81) / 10.0f;
                    particle.life = 1.0f;
                    
                    // Mix colors for explosion - use palette colors
                    uint8_t explosionPaletteIndex = random8();
                    particle.color = ColorFromPalette(currentPalette, explosionPaletteIndex, 255, LINEARBLEND);
                    
                    // Sometimes use the beam colors for continuity
                    uint8_t colorChoice = random8();
                    if (colorChoice < 85) {
                        particle.color = color1;
                    } else if (colorChoice < 170) {
                        particle.color = color2;
                    }
                    
                    // Add some white for extra pop
                    if (random8() < 50) {
                        particle.color = CRGB::White;
                    }
                    
                    particle.active = true;
                    break;
                }
            }
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Spawn new beams
        if (now - lastBeamTime > (500 - paletteSpeed * 2)) {
            if (random8() < 200) spawnBeam(true);   // From left
            if (random8() < 200) spawnBeam(false);  // From right
            lastBeamTime = now;
        }
        
        // Fade background
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            strip1[i].fadeToBlackBy(20);
            strip2[i].fadeToBlackBy(20);
        }
        
        // Update and render beams from left
        for (auto& beam : beams1) {
            if (!beam.active) continue;
            
            // Update trail
            for (int t = 19; t > 0; t--) {
                beam.trail[t] = beam.trail[t-1];
            }
            beam.trail[0] = beam.position;
            
            // Move beam
            beam.position += beam.velocity;
            
            // Check for collision with beams from right
            for (auto& beam2 : beams2) {
                if (!beam2.active) continue;
                
                if (abs(beam.position - beam2.position) < 5) {
                    // COLLISION!
                    createExplosion((beam.position + beam2.position) / 2, beam.color, beam2.color);
                    beam.active = false;
                    beam2.active = false;
                    
                    // Flash effect
                    explosionPhase = 1.0f;
                }
            }
            
            // Render beam and trail
            if (beam.active) {
                // Bright head
                int headPos = (int)beam.position;
                if (headPos >= 0 && headPos < HardwareConfig::STRIP_LENGTH) {
                    strip1[headPos] = beam.color;
                    strip2[headPos] = beam.color;
                    
                    // Glow around head
                    for (int g = -2; g <= 2; g++) {
                        int glowPos = headPos + g;
                        if (glowPos >= 0 && glowPos < HardwareConfig::STRIP_LENGTH && g != 0) {
                            float glowIntensity = 1.0f - abs(g) / 3.0f;
                            CRGB glowColor = beam.color;
                            glowColor.nscale8(glowIntensity * 200);
                            strip1[glowPos] += glowColor;
                            strip2[glowPos] += glowColor;
                        }
                    }
                }
                
                // Trail
                for (int t = 1; t < 20; t++) {
                    int trailPos = (int)beam.trail[t];
                    if (trailPos >= 0 && trailPos < HardwareConfig::STRIP_LENGTH) {
                        CRGB trailColor = beam.color;
                        trailColor.nscale8(255 - t * 12);  // Fade trail
                        strip1[trailPos] += trailColor;
                        
                        // Strip2 gets complementary color trail
                        CHSV hsvColor = rgb2hsv_approximate(beam.color);
                        trailColor = CHSV(hsvColor.h + 128, 255, 255 - t * 12);
                        strip2[trailPos] += trailColor;
                    }
                }
                
                // Deactivate if off screen
                if (beam.position < -5 || beam.position > HardwareConfig::STRIP_LENGTH + 5) {
                    beam.active = false;
                }
            }
        }
        
        // Update and render beams from right (similar logic)
        for (auto& beam : beams2) {
            if (!beam.active) continue;
            
            // Update trail
            for (int t = 19; t > 0; t--) {
                beam.trail[t] = beam.trail[t-1];
            }
            beam.trail[0] = beam.position;
            
            beam.position += beam.velocity;
            
            if (beam.active) {
                int headPos = (int)beam.position;
                if (headPos >= 0 && headPos < HardwareConfig::STRIP_LENGTH) {
                    strip1[headPos] = beam.color;
                    strip2[headPos] = beam.color;
                    
                    for (int g = -2; g <= 2; g++) {
                        int glowPos = headPos + g;
                        if (glowPos >= 0 && glowPos < HardwareConfig::STRIP_LENGTH && g != 0) {
                            float glowIntensity = 1.0f - abs(g) / 3.0f;
                            CRGB glowColor = beam.color;
                            glowColor.nscale8(glowIntensity * 200);
                            strip1[glowPos] += glowColor;
                            strip2[glowPos] += glowColor;
                        }
                    }
                }
                
                for (int t = 1; t < 20; t++) {
                    int trailPos = (int)beam.trail[t];
                    if (trailPos >= 0 && trailPos < HardwareConfig::STRIP_LENGTH) {
                        CRGB trailColor = beam.color;
                        trailColor.nscale8(255 - t * 12);
                        strip1[trailPos] += trailColor;
                        CHSV hsvColor = rgb2hsv_approximate(beam.color);
                        trailColor = CHSV(hsvColor.h + 128, 255, 255 - t * 12);
                        strip2[trailPos] += trailColor;
                    }
                }
                
                if (beam.position < -5 || beam.position > HardwareConfig::STRIP_LENGTH + 5) {
                    beam.active = false;
                }
            }
        }
        
        // Update explosion particles
        for (auto& particle : particles) {
            if (!particle.active) continue;
            
            particle.x += particle.velocity;
            particle.life -= 0.05f;
            particle.velocity *= 0.95f;  // Drag
            
            if (particle.life <= 0 || particle.x < 0 || particle.x >= HardwareConfig::STRIP_LENGTH) {
                particle.active = false;
            } else {
                int pos = (int)particle.x;
                CRGB pColor = particle.color;
                pColor.nscale8(particle.life * 255);
                strip1[pos] += pColor;
                strip2[pos] += pColor;
            }
        }
        
        // Global explosion flash
        if (explosionPhase > 0) {
            explosionPhase -= 0.1f;
            if (visualParams.saturation > 150) {
                // White flash on collision
                uint8_t flashIntensity = explosionPhase * 100 * (visualParams.saturation - 150) / 105;
                for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                    float centerDist = abs(i - HardwareConfig::STRIP_CENTER_POINT) / (float)HardwareConfig::STRIP_CENTER_POINT;
                    uint8_t localFlash = flashIntensity * (1.0f - centerDist);
                    strip1[i] += CRGB(localFlash, localFlash, localFlash);
                    strip2[i] += CRGB(localFlash, localFlash, localFlash);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPBeamCollisionEffect* beamCollisionInstance = nullptr;

// Effect function for main loop
void lgpBeamCollision() {
    if (!beamCollisionInstance) {
        beamCollisionInstance = new LGPBeamCollisionEffect(strip1, strip2);
    }
    beamCollisionInstance->render();
}