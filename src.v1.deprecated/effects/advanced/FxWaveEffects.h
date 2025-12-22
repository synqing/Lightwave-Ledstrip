#ifndef FX_WAVE_EFFECTS_H
#define FX_WAVE_EFFECTS_H

#include "../EffectBase.h"

// Base class for FxWave effects
class FxWaveBase : public EffectBase {
protected:
    float wavePhase = 0.0f;
    float waveSpeed = 1.0f;
    
public:
    FxWaveBase(const char* name, uint8_t brightness, uint8_t speed, uint8_t fade) :
        EffectBase(name, brightness, speed, fade) {}
};

// Ripple effect
class FxWaveRippleEffect : public FxWaveBase {
private:
    struct Ripple {
        float center;
        float radius;
        float speed;
        uint8_t hue;
        bool active;
    };
    
    static constexpr uint8_t MAX_RIPPLES = 5;
    Ripple ripples[MAX_RIPPLES];
    
public:
    FxWaveRippleEffect() : FxWaveBase("FxWave Ripple", 140, 15, 25) {
        for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
            ripples[i].active = false;
        }
    }
    
    void render() override {
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        // Spawn new ripples
        if (random8() < 20) {
            for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
                if (!ripples[i].active) {
                    ripples[i].center = random16(HardwareConfig::NUM_LEDS);
                    ripples[i].radius = 0;
                    ripples[i].speed = 0.5f + (random8() / 255.0f) * 2.0f;
                    ripples[i].hue = random8();
                    ripples[i].active = true;
                    break;
                }
            }
        }
        
        // Update and render ripples
        for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
            if (!ripples[r].active) continue;
            
            Ripple& ripple = ripples[r];
            ripple.radius += ripple.speed * (paletteSpeed / 10.0f);
            
            // Deactivate if too large
            if (ripple.radius > HardwareConfig::NUM_LEDS) {
                ripple.active = false;
                continue;
            }
            
            // Render ripple
            for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float distance = abs((float)i - ripple.center);
                float wavePos = distance - ripple.radius;
                
                if (abs(wavePos) < 5.0f) {
                    uint8_t brightness = 255 - (abs(wavePos) * 51);
                    brightness = (brightness * (HardwareConfig::NUM_LEDS - ripple.radius)) / HardwareConfig::NUM_LEDS;
                    
                    CRGB color = ColorFromPalette(currentPalette, ripple.hue + distance, brightness);
                    leds[i] += color;
                }
            }
        }
    }
};

// Interference effect
class FxWaveInterferenceEffect : public FxWaveBase {
private:
    float wave1Phase = 0;
    float wave2Phase = 0;
    
public:
    FxWaveInterferenceEffect() : FxWaveBase("FxWave Interference", 130, 12, 20) {}
    
    void render() override {
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        // Update wave phases
        wave1Phase += paletteSpeed / 20.0f;
        wave2Phase -= paletteSpeed / 30.0f;
        
        for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
            // Calculate two interfering waves
            float pos = (float)i / HardwareConfig::NUM_LEDS;
            
            float wave1 = sin(pos * PI * 4 + wave1Phase) * 127 + 128;
            float wave2 = sin(pos * PI * 6 + wave2Phase) * 127 + 128;
            
            // Interference pattern
            float interference = (wave1 + wave2) / 2.0f;
            uint8_t brightness = interference;
            
            // Color based on phase difference
            uint8_t hue = (uint8_t)(wave1Phase * 20) + angles[i];
            
            CRGB color = ColorFromPalette(currentPalette, hue, brightness);
            leds[i] = color;
        }
    }
};

// Orbital effect
class FxWaveOrbitalEffect : public FxWaveBase {
private:
    struct Orbiter {
        float position;
        float speed;
        uint8_t hue;
        uint8_t size;
    };
    
    static constexpr uint8_t NUM_ORBITERS = 3;
    Orbiter orbiters[NUM_ORBITERS];
    
public:
    FxWaveOrbitalEffect() : FxWaveBase("FxWave Orbital", 150, 18, 30) {
        // Initialize orbiters
        for (uint8_t i = 0; i < NUM_ORBITERS; i++) {
            orbiters[i].position = (float)i * HardwareConfig::NUM_LEDS / NUM_ORBITERS;
            orbiters[i].speed = 0.5f + (i * 0.3f);
            orbiters[i].hue = i * 85;
            orbiters[i].size = 5 + i * 2;
        }
    }
    
    void render() override {
        fadeToBlackBy(leds, HardwareConfig::NUM_LEDS, fadeAmount);
        
        // Update orbiters
        for (uint8_t o = 0; o < NUM_ORBITERS; o++) {
            orbiters[o].position += orbiters[o].speed * (paletteSpeed / 10.0f);
            if (orbiters[o].position >= HardwareConfig::NUM_LEDS) {
                orbiters[o].position -= HardwareConfig::NUM_LEDS;
            }
            
            // Draw orbiter with gaussian falloff
            for (uint16_t i = 0; i < HardwareConfig::NUM_LEDS; i++) {
                float distance = abs((float)i - orbiters[o].position);
                
                // Handle wraparound
                if (distance > HardwareConfig::NUM_LEDS / 2) {
                    distance = HardwareConfig::NUM_LEDS - distance;
                }
                
                if (distance < orbiters[o].size * 3) {
                    float gaussian = exp(-(distance * distance) / (2.0f * orbiters[o].size * orbiters[o].size));
                    uint8_t brightness = gaussian * 255;
                    
                    uint8_t hue = orbiters[o].hue + (millis() >> 7);
                    CRGB color = ColorFromPalette(currentPalette, hue, brightness);
                    
                    leds[i] += color;
                }
            }
        }
    }
};

#endif // FX_WAVE_EFFECTS_H