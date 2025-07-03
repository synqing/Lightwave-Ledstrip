// Tuned versions of Fire, Strip BPM, Sinelon, and Gravity Well effects
// with enhanced encoder parameter mappings

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== TUNED FIRE EFFECT ==============
// Enhanced fire effect with better parameter control
void fireTuned() {
    static byte heat[HardwareConfig::STRIP_LENGTH];
    
    // Get normalized parameters
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // ENCODER 3 (Speed/paletteSpeed): Controls fire animation speed
    // ENCODER 4 (Intensity): Controls fire height and spark frequency
    // ENCODER 5 (Saturation): Controls color richness (0=white fire, 255=colored fire)
    // ENCODER 6 (Complexity): Controls turbulence and flame detail
    // ENCODER 7 (Variation): Controls flame color mode (0=normal, 128=blue, 255=green)
    
    // Cool down every cell - complexity controls cooling rate
    uint8_t coolingRate = 55 + (complexity * 0.3f * 55); // 55-120
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((coolingRate * 10) / HardwareConfig::STRIP_LENGTH) + 2));
    }
    
    // Heat diffusion with turbulence based on complexity
    if (complexity > 0.5f) {
        // Add turbulence for more realistic flames
        for(int k = 2; k < HardwareConfig::STRIP_LENGTH - 2; k++) {
            heat[k] = (heat[k - 2] + heat[k - 1] + heat[k] + heat[k + 1] + heat[k + 2]) / 5;
        }
    } else {
        // Simple diffusion
        for(int k = 1; k < HardwareConfig::STRIP_LENGTH - 1; k++) {
            heat[k] = (heat[k - 1] + heat[k] + heat[k + 1]) / 3;
        }
    }
    
    // Ignite new sparks at CENTER with intensity control
    uint8_t sparkChance = 60 + (intensity * 140); // 60-200
    uint8_t sparkHeat = 160 + (intensity * 95);   // 160-255
    
    if(random8() < sparkChance) {
        // Multiple spark points for fuller fire
        int numSparks = 1 + (intensity * 2); // 1-3 sparks
        for(int s = 0; s < numSparks; s++) {
            int sparkPos = HardwareConfig::STRIP_CENTER_POINT + random8(5) - 2; // Center ±2
            heat[constrain(sparkPos, 0, HardwareConfig::STRIP_LENGTH-1)] = 
                qadd8(heat[sparkPos], random8(sparkHeat * 0.7f, sparkHeat));
        }
    }
    
    // Map heat to colors with variation control
    for(int j = 0; j < HardwareConfig::STRIP_LENGTH; j++) {
        // Scale heat by distance from center for more focused fire
        float distFromCenter = abs(j - HardwareConfig::STRIP_CENTER_POINT);
        float distanceFactor = 1.0f - (distFromCenter / HardwareConfig::STRIP_LENGTH * 0.3f);
        
        uint8_t scaledHeat = heat[j] * intensity * distanceFactor;
        
        CRGB color;
        // Variation controls flame color mode
        if (variation < 85) {
            // Normal fire colors (red/orange/yellow)
            color = HeatColor(scaledHeat);
        } else if (variation < 170) {
            // Blue fire
            uint8_t heatByte = scale8(scaledHeat, 240);
            color = CRGB(0, 0, heatByte);
            if (heatByte > 80) {
                color.b = 255;
                color.g = heatByte / 3;
                color.r = heatByte / 5;
            }
        } else {
            // Green/chemical fire
            uint8_t heatByte = scale8(scaledHeat, 240);
            color = CRGB(0, heatByte, 0);
            if (heatByte > 80) {
                color.g = 255;
                color.b = heatByte / 4;
                color.r = heatByte / 8;
            }
        }
        
        // Apply saturation control
        if (saturation < 255) {
            color = blend(CRGB::White, color, saturation);
        }
        
        strip1[j] = color;
        strip2[j] = color;
    }
}

// ============== TUNED STRIP BPM EFFECT ==============
// Enhanced BPM effect with better rhythm control
void stripBPMTuned() {
    // ENCODER 3 (Speed/paletteSpeed): Controls BPM (maps to 30-180 BPM)
    // ENCODER 4 (Intensity): Controls pulse brightness and reach
    // ENCODER 5 (Saturation): Controls color saturation
    // ENCODER 6 (Complexity): Controls number of simultaneous pulses
    // ENCODER 7 (Variation): Controls pulse pattern (single/double/triple)
    
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // Map paletteSpeed to actual BPM (30-180)
    uint8_t BeatsPerMinute = 30 + (paletteSpeed * 150 / 255);
    
    // Fade trails based on intensity
    uint8_t fadeRate = 10 + (intensity * 30); // 10-40
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeRate);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeRate);
    
    // Number of pulses based on complexity
    int numPulses = 1 + (complexity * 3); // 1-4 pulses
    
    for(int p = 0; p < numPulses; p++) {
        // Phase offset for multiple pulses
        uint16_t phaseOffset = (65535 / numPulses) * p;
        
        // Variation controls pulse pattern
        uint8_t beat;
        if (variation < 85) {
            // Single pulse
            beat = beatsin8(BeatsPerMinute, 0, 255, 0, phaseOffset >> 8);
        } else if (variation < 170) {
            // Double pulse
            uint8_t beat1 = beatsin8(BeatsPerMinute, 0, 255, 0, phaseOffset >> 8);
            uint8_t beat2 = beatsin8(BeatsPerMinute * 2, 0, 128, 0, phaseOffset >> 8);
            beat = qadd8(beat1 >> 1, beat2 >> 1);
        } else {
            // Triple pulse with swing
            uint8_t beat1 = beatsin8(BeatsPerMinute, 0, 255, 0, phaseOffset >> 8);
            uint8_t beat2 = beatsin8(BeatsPerMinute * 3, 0, 170, 0, phaseOffset >> 8);
            uint8_t beat3 = beatsin8(BeatsPerMinute * 1.5, 0, 128, 0, phaseOffset >> 8);
            beat = (beat1 >> 2) + (beat2 >> 2) + (beat3 >> 1);
        }
        
        // Create pulse from center
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
            
            // Pulse width based on beat intensity
            float pulseWidth = 10 + (beat / 255.0f * 30) + (intensity * 20);
            
            // Calculate brightness based on distance from pulse edge
            uint8_t brightness = 0;
            if (distFromCenter < pulseWidth) {
                float edge = 1.0f - (distFromCenter / pulseWidth);
                brightness = beat * edge * edge * intensity; // Quadratic falloff
            }
            
            if (brightness > 0) {
                uint8_t colorIndex = gHue + (p * 64) + (distFromCenter * 2);
                CRGB color = ColorFromPalette(currentPalette, colorIndex, brightness);
                
                // Apply saturation
                if (saturation < 255) {
                    color = blend(CRGB::White, color, saturation);
                }
                
                strip1[i] += color;
                strip2[i] += color;
            }
        }
    }
}

// ============== TUNED SINELON EFFECT ==============
// Enhanced sinelon with multiple wave controls
void sinelonTuned() {
    // ENCODER 3 (Speed/paletteSpeed): Controls oscillation speed
    // ENCODER 4 (Intensity): Controls trail length and brightness
    // ENCODER 5 (Saturation): Controls color saturation
    // ENCODER 6 (Complexity): Controls number of dots
    // ENCODER 7 (Variation): Controls movement pattern
    
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // Fade rate based on intensity (longer trails with higher intensity)
    uint8_t fadeRate = 50 - (intensity * 45); // 50-5
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeRate);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeRate);
    
    // Number of dots based on complexity
    int numDots = 1 + (complexity * 4); // 1-5 dots
    
    for(int d = 0; d < numDots; d++) {
        // Base frequency with speed control
        uint8_t baseFreq = 7 + (paletteSpeed >> 4); // 7-23
        
        // Movement pattern based on variation
        int distFromCenter;
        if (variation < 64) {
            // Simple sine wave
            distFromCenter = beatsin16(baseFreq + d, 0, HardwareConfig::STRIP_HALF_LENGTH);
        } else if (variation < 128) {
            // Double frequency overlay
            int dist1 = beatsin16(baseFreq + d, 0, HardwareConfig::STRIP_HALF_LENGTH);
            int dist2 = beatsin16((baseFreq + d) * 2, 0, HardwareConfig::STRIP_HALF_LENGTH / 2);
            distFromCenter = (dist1 + dist2) / 1.5f;
        } else if (variation < 192) {
            // Bouncing pattern
            int dist = beatsin16(baseFreq + d, 0, HardwareConfig::STRIP_HALF_LENGTH * 2);
            distFromCenter = dist > HardwareConfig::STRIP_HALF_LENGTH ? 
                           HardwareConfig::STRIP_HALF_LENGTH * 2 - dist : dist;
        } else {
            // Chaotic pattern
            int dist1 = beatsin16(baseFreq + d, 0, HardwareConfig::STRIP_HALF_LENGTH);
            int dist2 = beatsin16((baseFreq + d) * 1.414f, 0, HardwareConfig::STRIP_HALF_LENGTH);
            int dist3 = beatsin16((baseFreq + d) * 0.667f, 0, HardwareConfig::STRIP_HALF_LENGTH / 3);
            distFromCenter = (dist1 + dist2 + dist3) / 2.5f;
        }
        
        // Set positions on both sides of center
        int pos1 = HardwareConfig::STRIP_CENTER_POINT + distFromCenter;
        int pos2 = HardwareConfig::STRIP_CENTER_POINT - distFromCenter;
        
        // Brightness based on intensity
        uint8_t brightness = 192 + (intensity * 63); // 192-255
        
        // Draw dots with motion blur
        if (pos1 >= 0 && pos1 < HardwareConfig::STRIP_LENGTH) {
            CRGB color = CHSV(gHue + (d * 51), saturation, brightness);
            strip1[pos1] += color;
            strip2[pos1] += color;
            
            // Motion blur
            for(int blur = 1; blur <= 3; blur++) {
                int blurPos = pos1 + blur;
                if (blurPos < HardwareConfig::STRIP_LENGTH) {
                    uint8_t blurBright = brightness / (blur + 1);
                    CRGB blurColor = CHSV(gHue + (d * 51), saturation, blurBright);
                    strip1[blurPos] += blurColor;
                    strip2[blurPos] += blurColor;
                }
            }
        }
        
        if (pos2 >= 0 && pos2 < HardwareConfig::STRIP_LENGTH) {
            CRGB color = CHSV(gHue + (d * 51) + 128, saturation, brightness);
            strip1[pos2] += color;
            strip2[pos2] += color;
            
            // Motion blur
            for(int blur = 1; blur <= 3; blur++) {
                int blurPos = pos2 - blur;
                if (blurPos >= 0) {
                    uint8_t blurBright = brightness / (blur + 1);
                    CRGB blurColor = CHSV(gHue + (d * 51) + 128, saturation, blurBright);
                    strip1[blurPos] += blurColor;
                    strip2[blurPos] += blurColor;
                }
            }
        }
    }
}

// ============== TUNED GRAVITY WELL EFFECT ==============
// Enhanced gravity simulation with more parameters
void gravityWellTuned() {
    // ENCODER 3 (Speed/paletteSpeed): Controls gravity strength
    // ENCODER 4 (Intensity): Controls particle brightness and trail
    // ENCODER 5 (Saturation): Controls color saturation
    // ENCODER 6 (Complexity): Controls number of particles
    // ENCODER 7 (Variation): Controls physics behavior
    
    static struct GravityParticle {
        float position;
        float velocity;
        uint8_t hue;
        float mass;
        bool active;
    } particles[30]; // Increased max particles
    
    static bool initialized = false;
    
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // Active particle count based on complexity
    int activeParticles = 5 + (complexity * 25); // 5-30 particles
    
    if (!initialized) {
        for (int p = 0; p < 30; p++) {
            particles[p].position = random16(HardwareConfig::STRIP_LENGTH);
            particles[p].velocity = 0;
            particles[p].hue = random8();
            particles[p].mass = 0.5f + (random8() / 255.0f * 1.5f); // 0.5-2.0
            particles[p].active = (p < activeParticles);
        }
        initialized = true;
    }
    
    // Update active particle count
    for (int p = 0; p < 30; p++) {
        particles[p].active = (p < activeParticles);
    }
    
    // Fade rate based on intensity
    uint8_t fadeRate = 40 - (intensity * 35); // 40-5
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, fadeRate);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, fadeRate);
    
    // Gravity strength based on speed control
    float gravityStrength = 0.002f + (paletteSpeed / 255.0f * 0.018f); // 0.002-0.02
    
    // Physics variation modes
    float dampingFactor = 0.95f;
    float centerAttraction = HardwareConfig::STRIP_CENTER_POINT;
    
    if (variation < 64) {
        // Standard gravity well
        dampingFactor = 0.95f;
    } else if (variation < 128) {
        // Low friction space
        dampingFactor = 0.99f;
        gravityStrength *= 0.7f;
    } else if (variation < 192) {
        // Oscillating gravity well
        centerAttraction = HardwareConfig::STRIP_CENTER_POINT + 
                          beatsin16(10, -20, 20);
    } else {
        // Chaotic attractor
        dampingFactor = 0.93f;
        gravityStrength *= 1.5f;
        // Add random perturbations
    }
    
    // Update particles
    for (int p = 0; p < 30; p++) {
        if (particles[p].active) {
            float distFromCenter = particles[p].position - centerAttraction;
            
            // Gravity calculation with mass
            float gravity = -distFromCenter * gravityStrength / particles[p].mass;
            
            // Chaotic mode adds perturbations
            if (variation >= 192) {
                gravity += (random8() - 128) / 1000.0f;
            }
            
            particles[p].velocity += gravity;
            particles[p].velocity *= dampingFactor;
            particles[p].position += particles[p].velocity;
            
            // Boundary behavior
            if (abs(particles[p].position - HardwareConfig::STRIP_CENTER_POINT) < 2) {
                // Reached center - respawn
                particles[p].position = random8(2) ? 0 : HardwareConfig::STRIP_LENGTH - 1;
                particles[p].velocity = random8(2) ? 2.0f : -2.0f; // Initial push
                particles[p].hue = random8();
                particles[p].mass = 0.5f + (random8() / 255.0f * 1.5f);
            }
            
            // Keep in bounds
            if (particles[p].position < 0 || particles[p].position >= HardwareConfig::STRIP_LENGTH) {
                particles[p].position = constrain(particles[p].position, 0, HardwareConfig::STRIP_LENGTH - 1);
                particles[p].velocity *= -0.8f; // Bounce with energy loss
            }
            
            // Draw particle
            int pos = particles[p].position;
            if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
                // Particle brightness based on velocity and intensity
                float speed = abs(particles[p].velocity);
                uint8_t brightness = 128 + (speed * 20) + (intensity * 127);
                brightness = constrain(brightness, 0, 255);
                
                CRGB color = ColorFromPalette(currentPalette, particles[p].hue, brightness);
                
                // Apply saturation
                if (saturation < 255) {
                    color = blend(CRGB::White, color, saturation);
                }
                
                // Different colors for each strip based on mass
                strip1[pos] += color;
                CRGB color2 = ColorFromPalette(currentPalette, 
                                              particles[p].hue + (particles[p].mass * 50), 
                                              brightness);
                if (saturation < 255) {
                    color2 = blend(CRGB::White, color2, saturation);
                }
                strip2[pos] += color2;
                
                // Enhanced motion blur trail
                int blurLength = 1 + (speed * 2) + (intensity * 3);
                for (int blur = 1; blur <= blurLength; blur++) {
                    int blurPos = pos - (particles[p].velocity > 0 ? blur : -blur);
                    if (blurPos >= 0 && blurPos < HardwareConfig::STRIP_LENGTH) {
                        uint8_t blurBright = brightness / (blur + 1);
                        CRGB blurColor1 = ColorFromPalette(currentPalette, particles[p].hue, blurBright);
                        CRGB blurColor2 = ColorFromPalette(currentPalette, 
                                                          particles[p].hue + (particles[p].mass * 50), 
                                                          blurBright);
                        if (saturation < 255) {
                            blurColor1 = blend(CRGB::White, blurColor1, saturation);
                            blurColor2 = blend(CRGB::White, blurColor2, saturation);
                        }
                        strip1[blurPos] += blurColor1;
                        strip2[blurPos] += blurColor2;
                    }
                }
            }
        }
    }
}