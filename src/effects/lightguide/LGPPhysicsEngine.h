#ifndef LGP_PHYSICS_ENGINE_H
#define LGP_PHYSICS_ENGINE_H

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "LightGuideEffect.h"

// Physics simulation constants
namespace LGPPhysics {
    constexpr float SPEED_OF_LIGHT_IN_ACRYLIC = 2.01e8f;  // m/s (c/n where n=1.49)
    constexpr float WAVE_VELOCITY_SCALE = 0.001f;  // Scale factor for LED animation
    constexpr uint8_t MAX_WAVE_SOURCES = 8;
    constexpr uint8_t MAX_PARTICLES = 32;
    constexpr uint8_t FIELD_RESOLUTION = 16;  // Resolution for field calculations
}

// Wave source for interference patterns
struct WaveSource {
    float position;      // Position along edge (0.0-1.0)
    float amplitude;     // Wave amplitude
    float frequency;     // Wave frequency
    float phase;         // Current phase
    bool active;         // Is this source active?
    uint8_t edge;        // Which edge (0 or 1)
};

// Particle for collision simulations
struct Particle {
    float x, y;          // Position in normalized plate space
    float vx, vy;        // Velocity components
    float mass;          // Particle mass
    float charge;        // Particle charge (for EM fields)
    uint8_t lifetime;    // Remaining lifetime
    CRGB color;          // Particle color
};

// Field point for electromagnetic simulations
struct FieldPoint {
    float Ex, Ey;        // Electric field components
    float Bz;            // Magnetic field (perpendicular to plate)
    float potential;     // Scalar potential
};

// Light Guide Plate Physics Engine
class LGPPhysicsEngine {
private:
    // Wave sources
    WaveSource waveSources[LGPPhysics::MAX_WAVE_SOURCES];
    uint8_t activeWaveCount;
    
    // Particle system
    Particle particles[LGPPhysics::MAX_PARTICLES];
    uint8_t activeParticleCount;
    
    // Field grid for EM simulations
    FieldPoint fieldGrid[LGPPhysics::FIELD_RESOLUTION][LGPPhysics::FIELD_RESOLUTION];
    
    // Time tracking
    uint32_t lastUpdateTime;
    float timeStep;
    
public:
    LGPPhysicsEngine() : activeWaveCount(0), activeParticleCount(0), lastUpdateTime(0), timeStep(0.016f) {
        reset();
    }
    
    // Reset all physics state
    void reset() {
        // Clear wave sources
        for (auto& source : waveSources) {
            source.active = false;
        }
        activeWaveCount = 0;
        
        // Clear particles
        for (auto& particle : particles) {
            particle.lifetime = 0;
        }
        activeParticleCount = 0;
        
        // Clear field grid
        for (int i = 0; i < LGPPhysics::FIELD_RESOLUTION; i++) {
            for (int j = 0; j < LGPPhysics::FIELD_RESOLUTION; j++) {
                fieldGrid[i][j] = {0, 0, 0, 0};
            }
        }
    }
    
    // ============== WAVE PHYSICS ==============
    
    // Add a wave source
    void addWaveSource(float position, float amplitude, float frequency, uint8_t edge = 0) {
        if (activeWaveCount < LGPPhysics::MAX_WAVE_SOURCES) {
            waveSources[activeWaveCount] = {
                position, amplitude, frequency, 0, true, edge
            };
            activeWaveCount++;
        }
    }
    
    // Calculate wave interference at a point
    float calculateWaveInterference(float x, float y, float time) {
        float totalAmplitude = 0;
        
        for (uint8_t i = 0; i < activeWaveCount; i++) {
            if (waveSources[i].active) {
                // Calculate distance from source
                float sx = waveSources[i].edge == 0 ? 0 : 1;
                float sy = waveSources[i].position;
                float distance = sqrt((x - sx) * (x - sx) + (y - sy) * (y - sy));
                
                // Wave equation: A * sin(kr - wt + phase)
                float k = TWO_PI * waveSources[i].frequency;  // Wave number
                float w = k * LGPPhysics::WAVE_VELOCITY_SCALE;  // Angular frequency
                float waveValue = waveSources[i].amplitude * sin(k * distance - w * time + waveSources[i].phase);
                
                totalAmplitude += waveValue;
            }
        }
        
        return totalAmplitude;
    }
    
    // Update wave phases
    void updateWaves(float deltaTime) {
        for (uint8_t i = 0; i < activeWaveCount; i++) {
            if (waveSources[i].active) {
                waveSources[i].phase += waveSources[i].frequency * deltaTime * TWO_PI * LGPPhysics::WAVE_VELOCITY_SCALE;
                if (waveSources[i].phase > TWO_PI) {
                    waveSources[i].phase -= TWO_PI;
                }
            }
        }
    }
    
    // ============== PARTICLE PHYSICS ==============
    
    // Add a particle
    void addParticle(float x, float y, float vx, float vy, float mass = 1.0f, float charge = 0.0f, CRGB color = CRGB::White) {
        if (activeParticleCount < LGPPhysics::MAX_PARTICLES) {
            particles[activeParticleCount] = {
                x, y, vx, vy, mass, charge, 255, color
            };
            activeParticleCount++;
        }
    }
    
    // Update particle physics
    void updateParticles(float deltaTime) {
        for (uint8_t i = 0; i < activeParticleCount; i++) {
            if (particles[i].lifetime > 0) {
                // Update position
                particles[i].x += particles[i].vx * deltaTime;
                particles[i].y += particles[i].vy * deltaTime;
                
                // Edge collisions (elastic)
                if (particles[i].x < 0 || particles[i].x > 1) {
                    particles[i].vx = -particles[i].vx;
                    particles[i].x = constrain(particles[i].x, 0, 1);
                }
                if (particles[i].y < 0 || particles[i].y > 1) {
                    particles[i].vy = -particles[i].vy;
                    particles[i].y = constrain(particles[i].y, 0, 1);
                }
                
                // Apply electromagnetic forces if charged
                if (particles[i].charge != 0) {
                    int gridX = particles[i].x * (LGPPhysics::FIELD_RESOLUTION - 1);
                    int gridY = particles[i].y * (LGPPhysics::FIELD_RESOLUTION - 1);
                    
                    // Lorentz force: F = q(E + v × B)
                    float fx = particles[i].charge * fieldGrid[gridX][gridY].Ex;
                    float fy = particles[i].charge * fieldGrid[gridX][gridY].Ey;
                    
                    // Magnetic force component (simplified for 2D)
                    fx += particles[i].charge * particles[i].vy * fieldGrid[gridX][gridY].Bz;
                    fy -= particles[i].charge * particles[i].vx * fieldGrid[gridX][gridY].Bz;
                    
                    // Update velocity
                    particles[i].vx += (fx / particles[i].mass) * deltaTime;
                    particles[i].vy += (fy / particles[i].mass) * deltaTime;
                }
                
                // Decay lifetime
                particles[i].lifetime = max(0, particles[i].lifetime - 1);
            }
        }
        
        // Check particle collisions
        for (uint8_t i = 0; i < activeParticleCount - 1; i++) {
            if (particles[i].lifetime == 0) continue;
            
            for (uint8_t j = i + 1; j < activeParticleCount; j++) {
                if (particles[j].lifetime == 0) continue;
                
                float dx = particles[i].x - particles[j].x;
                float dy = particles[i].y - particles[j].y;
                float dist = sqrt(dx * dx + dy * dy);
                
                if (dist < 0.02f) {  // Collision threshold
                    // Elastic collision
                    float v1x = particles[i].vx;
                    float v1y = particles[i].vy;
                    float v2x = particles[j].vx;
                    float v2y = particles[j].vy;
                    float m1 = particles[i].mass;
                    float m2 = particles[j].mass;
                    
                    // Conservation of momentum
                    particles[i].vx = ((m1 - m2) * v1x + 2 * m2 * v2x) / (m1 + m2);
                    particles[i].vy = ((m1 - m2) * v1y + 2 * m2 * v2y) / (m1 + m2);
                    particles[j].vx = ((m2 - m1) * v2x + 2 * m1 * v1x) / (m1 + m2);
                    particles[j].vy = ((m2 - m1) * v2y + 2 * m1 * v1y) / (m1 + m2);
                }
            }
        }
        
        // Remove dead particles
        compactParticles();
    }
    
    // ============== ELECTROMAGNETIC FIELDS ==============
    
    // Set up a dipole field
    void setupDipoleField(float x1, float y1, float charge1, float x2, float y2, float charge2) {
        for (int i = 0; i < LGPPhysics::FIELD_RESOLUTION; i++) {
            for (int j = 0; j < LGPPhysics::FIELD_RESOLUTION; j++) {
                float x = (float)i / (LGPPhysics::FIELD_RESOLUTION - 1);
                float y = (float)j / (LGPPhysics::FIELD_RESOLUTION - 1);
                
                // Calculate field from charge 1
                float dx1 = x - x1;
                float dy1 = y - y1;
                float r1 = sqrt(dx1 * dx1 + dy1 * dy1) + 0.01f;  // Avoid singularity
                float E1 = charge1 / (r1 * r1);
                
                // Calculate field from charge 2
                float dx2 = x - x2;
                float dy2 = y - y2;
                float r2 = sqrt(dx2 * dx2 + dy2 * dy2) + 0.01f;
                float E2 = charge2 / (r2 * r2);
                
                // Superposition
                fieldGrid[i][j].Ex = E1 * dx1 / r1 + E2 * dx2 / r2;
                fieldGrid[i][j].Ey = E1 * dy1 / r1 + E2 * dy2 / r2;
                fieldGrid[i][j].potential = charge1 / r1 + charge2 / r2;
            }
        }
    }
    
    // Set up a magnetic field
    void setupMagneticField(float Bz_uniform) {
        for (int i = 0; i < LGPPhysics::FIELD_RESOLUTION; i++) {
            for (int j = 0; j < LGPPhysics::FIELD_RESOLUTION; j++) {
                fieldGrid[i][j].Bz = Bz_uniform;
            }
        }
    }
    
    // ============== VISUALIZATION HELPERS ==============
    
    // Map particle to LED position
    void renderParticlesToStrips(CRGB* strip1, CRGB* strip2) {
        for (uint8_t i = 0; i < activeParticleCount; i++) {
            if (particles[i].lifetime > 0) {
                // Map to LED position
                uint16_t ledPos = particles[i].y * HardwareConfig::STRIP_LENGTH;
                
                // Fade based on lifetime
                uint8_t brightness = particles[i].lifetime;
                CRGB color = particles[i].color;
                color.nscale8(brightness);
                
                // Render to appropriate edge
                if (particles[i].x < 0.5f) {
                    if (ledPos < HardwareConfig::STRIP_LENGTH) {
                        strip1[ledPos] += color;
                    }
                } else {
                    if (ledPos < HardwareConfig::STRIP_LENGTH) {
                        strip2[ledPos] += color;
                    }
                }
            }
        }
    }
    
    // Render wave interference pattern
    void renderWaveInterference(CRGB* strip1, CRGB* strip2, CRGBPalette16& palette, uint8_t hue_offset = 0) {
        float time = millis() * 0.001f;
        
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float y = (float)i / HardwareConfig::STRIP_LENGTH;
            
            // Calculate interference at both edges
            float interference1 = calculateWaveInterference(0, y, time);
            float interference2 = calculateWaveInterference(1, y, time);
            
            // Map to brightness (normalize to 0-255)
            uint8_t brightness1 = 128 + (int8_t)(interference1 * 127);
            uint8_t brightness2 = 128 + (int8_t)(interference2 * 127);
            
            // Apply to strips
            strip1[i] = ColorFromPalette(palette, hue_offset + i, brightness1);
            strip2[i] = ColorFromPalette(palette, hue_offset + i + 128, brightness2);
        }
    }
    
    // Get field value at position
    FieldPoint getFieldAt(float x, float y) {
        int gridX = constrain(x * (LGPPhysics::FIELD_RESOLUTION - 1), 0, LGPPhysics::FIELD_RESOLUTION - 1);
        int gridY = constrain(y * (LGPPhysics::FIELD_RESOLUTION - 1), 0, LGPPhysics::FIELD_RESOLUTION - 1);
        return fieldGrid[gridX][gridY];
    }
    
    // Update physics engine
    void update() {
        uint32_t now = millis();
        float deltaTime = (now - lastUpdateTime) * 0.001f;
        lastUpdateTime = now;
        
        updateWaves(deltaTime);
        updateParticles(deltaTime);
    }
    
private:
    // Remove dead particles from array
    void compactParticles() {
        uint8_t writeIndex = 0;
        for (uint8_t readIndex = 0; readIndex < activeParticleCount; readIndex++) {
            if (particles[readIndex].lifetime > 0) {
                if (writeIndex != readIndex) {
                    particles[writeIndex] = particles[readIndex];
                }
                writeIndex++;
            }
        }
        activeParticleCount = writeIndex;
    }
};

#endif // LGP_PHYSICS_ENGINE_H