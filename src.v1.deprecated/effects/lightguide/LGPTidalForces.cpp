// Light Guide Plate Tidal Forces
// Massive waves crash from both sides creating explosive splashes!

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

// Tidal Forces effect class
class LGPTidalForcesEffect : public LightGuideEffect {
private:
    // Wave structure
    struct TidalWave {
        float position;      // Wave front position
        float height;        // Wave amplitude (0-1)
        float velocity;      // Current speed
        float momentum;      // Mass * velocity for collision
        CRGB color;         // Wave color
        bool active;
        float foam[30];     // Foam trail positions
    };
    
    static constexpr uint8_t MAX_WAVES = 3;
    TidalWave leftWaves[MAX_WAVES];   // Waves from left
    TidalWave rightWaves[MAX_WAVES];  // Waves from right
    
    // Splash particles for big crashes
    struct Splash {
        float x;
        float height;      // How high it splashes
        float velocity;    // Horizontal velocity
        float vHeight;     // Vertical velocity
        CRGB color;
        float life;
        bool active;
    };
    
    static constexpr uint16_t MAX_SPLASHES = 150;
    Splash splashes[MAX_SPLASHES];
    
    // Ocean parameters
    float seaLevel;
    float turbulence;
    uint32_t lastWaveTime;
    float crashMagnitude;
    
public:
    LGPTidalForcesEffect(CRGB* s1, CRGB* s2) : 
        LightGuideEffect("LGP Tidal Forces", s1, s2),
        seaLevel(0.3f), turbulence(0), lastWaveTime(0), crashMagnitude(0) {
        
        // Initialize waves
        for (auto& wave : leftWaves) wave.active = false;
        for (auto& wave : rightWaves) wave.active = false;
        for (auto& splash : splashes) splash.active = false;
    }
    
    void spawnWave(bool fromLeft) {
        TidalWave* waves = fromLeft ? leftWaves : rightWaves;
        
        for (int i = 0; i < MAX_WAVES; i++) {
            if (!waves[i].active) {
                waves[i].position = fromLeft ? 0 : HardwareConfig::STRIP_LENGTH - 1;
                waves[i].height = 0.5f + random(0, 50) / 100.0f;  // 0.5 to 1.0
                waves[i].velocity = (1.0f + waves[i].height) * (fromLeft ? 1 : -1);
                waves[i].velocity *= (0.5f + visualParams.getIntensityNorm());
                waves[i].momentum = waves[i].height * abs(waves[i].velocity);
                
                // Get ocean colors from palette
                uint8_t paletteIndex = 128 + random(0, 64);  // Middle range of palette for water
                waves[i].color = ColorFromPalette(currentPalette, paletteIndex, 255, LINEARBLEND);
                waves[i].active = true;
                
                // Initialize foam
                for (int f = 0; f < 30; f++) {
                    waves[i].foam[f] = waves[i].position;
                }
                break;
            }
        }
    }
    
    void createSplash(float pos, float magnitude, CRGB color1, CRGB color2) {
        int splashCount = magnitude * 30 * (1.0f + visualParams.getComplexityNorm());
        
        for (int i = 0; i < splashCount && i < MAX_SPLASHES; i++) {
            for (auto& splash : splashes) {
                if (!splash.active) {
                    splash.x = pos + random(-5, 6);
                    splash.height = 0;
                    splash.velocity = random(-60, 61) / 10.0f * magnitude;
                    splash.vHeight = random(30, 80) / 10.0f * magnitude;
                    
                    // Splash colors from palette with foam
                    uint8_t splashPaletteIndex = random8();
                    splash.color = ColorFromPalette(currentPalette, splashPaletteIndex, 255, LINEARBLEND);
                    
                    // Mix in wave colors and white foam
                    uint8_t mixType = random8();
                    if (mixType < 80) {
                        splash.color = color1;
                    } else if (mixType < 160) {
                        splash.color = color2;
                    } else if (mixType > 220) {
                        splash.color = CRGB(200, 220, 255);  // White foam
                    }
                    
                    splash.life = 1.0f;
                    splash.active = true;
                    break;
                }
            }
        }
    }
    
    void render() override {
        uint32_t now = millis();
        
        // Spawn waves periodically
        if (now - lastWaveTime > (1000 - paletteSpeed * 3)) {
            if (random8() < 150) spawnWave(true);
            if (random8() < 150) spawnWave(false);
            lastWaveTime = now;
        }
        
        // Update turbulence
        turbulence = sin(now * 0.001f) * 0.2f + 0.1f;
        
        // Deep ocean background using palette
        for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            // Deeper in the middle
            float depth = 1.0f - abs(i - HardwareConfig::STRIP_CENTER_POINT) / (float)HardwareConfig::STRIP_CENTER_POINT;
            uint8_t bgBright = 10 + depth * 20;
            
            // Use darker palette colors for ocean depth
            uint8_t palettePos = 160 + sin8(i * 2 + millis() / 50) / 4;  // Gentle variation
            CRGB bgColor = ColorFromPalette(currentPalette, palettePos, bgBright, LINEARBLEND);
            
            strip1[i] = bgColor;
            strip2[i] = bgColor;
        }
        
        // Update waves from left
        for (auto& wave : leftWaves) {
            if (!wave.active) continue;
            
            // Update foam trail
            for (int f = 29; f > 0; f--) {
                wave.foam[f] = wave.foam[f-1];
            }
            wave.foam[0] = wave.position;
            
            // Wave physics - acceleration based on height
            wave.velocity += wave.height * 0.05f;
            wave.position += wave.velocity;
            
            // Check for collision with opposing waves
            for (auto& rightWave : rightWaves) {
                if (!rightWave.active) continue;
                
                if (abs(wave.position - rightWave.position) < 10) {
                    // CRASH!
                    float totalMomentum = wave.momentum + rightWave.momentum;
                    createSplash((wave.position + rightWave.position) / 2, 
                               totalMomentum / 2, wave.color, rightWave.color);
                    
                    // Transfer momentum
                    if (wave.momentum > rightWave.momentum) {
                        wave.velocity *= 0.3f;  // Slow down
                        rightWave.active = false;
                    } else {
                        rightWave.velocity *= 0.3f;
                        wave.active = false;
                    }
                    
                    crashMagnitude = totalMomentum;
                }
            }
            
            // Render wave
            if (wave.active) {
                // Wave crest
                for (int w = -20; w <= 5; w++) {
                    int pos = wave.position + w;
                    if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                        float waveShape = 0;
                        
                        if (w <= 0) {
                            // Wave front - steep
                            waveShape = 1.0f - abs(w) / 20.0f;
                            waveShape = waveShape * waveShape;  // Quadratic for steeper front
                        } else {
                            // Wave back - gentler
                            waveShape = 1.0f - w / 5.0f;
                        }
                        
                        waveShape *= wave.height;
                        
                        CRGB waveColor = wave.color;
                        waveColor.nscale8(waveShape * 255);
                        
                        // Add turbulence
                        if (w == 0) {
                            // White crest at peak
                            CRGB crestColor = CRGB(255, 255, 255);
                            crestColor.nscale8(wave.height * 200);
                            waveColor += crestColor;
                        }
                        
                        strip1[pos] += waveColor;
                        strip2[pos] += waveColor;
                    }
                }
                
                // Foam trail
                for (int f = 0; f < 30; f++) {
                    int foamPos = wave.foam[f];
                    if (foamPos >= 0 && foamPos < HardwareConfig::STRIP_LENGTH) {
                        CRGB foamColor = CRGB(200, 220, 255);  // White-blue foam
                        foamColor.nscale8(255 - f * 8);  // Fade out
                        strip1[foamPos] += foamColor;
                        strip2[foamPos] += foamColor.scale8(200);
                    }
                }
                
                // Deactivate if off screen
                if (wave.position > HardwareConfig::STRIP_LENGTH + 10) {
                    wave.active = false;
                }
            }
        }
        
        // Update waves from right (similar but opposite direction)
        for (auto& wave : rightWaves) {
            if (!wave.active) continue;
            
            for (int f = 29; f > 0; f--) {
                wave.foam[f] = wave.foam[f-1];
            }
            wave.foam[0] = wave.position;
            
            wave.velocity -= wave.height * 0.05f;  // Negative acceleration
            wave.position += wave.velocity;
            
            if (wave.active) {
                for (int w = -5; w <= 20; w++) {
                    int pos = wave.position + w;
                    if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                        float waveShape = 0;
                        
                        if (w >= 0) {
                            waveShape = 1.0f - abs(w) / 20.0f;
                            waveShape = waveShape * waveShape;
                        } else {
                            waveShape = 1.0f - abs(w) / 5.0f;
                        }
                        
                        waveShape *= wave.height;
                        
                        CRGB waveColor = wave.color;
                        waveColor.nscale8(waveShape * 255);
                        
                        if (w == 0) {
                            CRGB crestColor = CRGB(255, 255, 255);
                            crestColor.nscale8(wave.height * 200);
                            waveColor += crestColor;
                        }
                        
                        strip1[pos] += waveColor;
                        strip2[pos] += waveColor;
                    }
                }
                
                for (int f = 0; f < 30; f++) {
                    int foamPos = wave.foam[f];
                    if (foamPos >= 0 && foamPos < HardwareConfig::STRIP_LENGTH) {
                        CRGB foamColor = CRGB(200, 220, 255);
                        foamColor.nscale8(255 - f * 8);
                        strip1[foamPos] += foamColor;
                        strip2[foamPos] += foamColor.scale8(200);
                    }
                }
                
                if (wave.position < -10) {
                    wave.active = false;
                }
            }
        }
        
        // Update splash particles
        for (auto& splash : splashes) {
            if (!splash.active) continue;
            
            splash.x += splash.velocity;
            splash.height += splash.vHeight;
            splash.vHeight -= 0.3f;  // Gravity
            splash.velocity *= 0.95f;  // Drag
            splash.life -= 0.02f;
            
            // Splash falls back down
            if (splash.height < 0) {
                splash.height = 0;
                splash.vHeight = -splash.vHeight * 0.5f;  // Bounce with damping
            }
            
            if (splash.life <= 0 || splash.x < 0 || splash.x >= HardwareConfig::STRIP_LENGTH) {
                splash.active = false;
            } else {
                int pos = (int)splash.x;
                
                // Height affects brightness
                float heightEffect = 1.0f + splash.height / 10.0f;
                heightEffect = min(heightEffect, 2.0f);
                
                CRGB splashColor = splash.color;
                splashColor.nscale8(splash.life * heightEffect * 255);
                
                strip1[pos] += splashColor;
                strip2[pos] += splashColor;
            }
        }
        
        // Big crash shockwave effect
        if (crashMagnitude > 0) {
            crashMagnitude -= 0.1f;
            
            // Ripples from center
            for (int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
                float dist = abs(i - HardwareConfig::STRIP_CENTER_POINT);
                float ripple = sin(dist * 0.3f - now * 0.01f) * crashMagnitude * 0.3f;
                if (ripple > 0) {
                    uint8_t rippleBright = ripple * 255;
                    strip1[i] += CRGB(rippleBright, rippleBright, rippleBright);
                    strip2[i] += CRGB(rippleBright, rippleBright, rippleBright);
                }
            }
        }
        
        // Sync to unified buffer
        memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
        memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    }
};

// Global instance
static LGPTidalForcesEffect* tidalForcesInstance = nullptr;

// Effect function for main loop
void lgpTidalForces() {
    if (!tidalForcesInstance) {
        tidalForcesInstance = new LGPTidalForcesEffect(strip1, strip2);
    }
    tidalForcesInstance->render();
}