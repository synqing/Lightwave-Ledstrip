/**
 * Audio-Reactive Effects for LightwaveOS
 * 
 * Example effects that utilize AudioFrame data from VP_DECODER
 */

#ifndef AUDIO_REACTIVE_EFFECTS_H
#define AUDIO_REACTIVE_EFFECTS_H

#include <FastLED.h>
#include "audio_frame.h"
#include "effects.h"

/**
 * Base class for audio-reactive effects
 */
class AudioReactiveEffect : public Effect {
protected:
    const AudioFrame* audioFrame;
    
public:
    AudioReactiveEffect(const char* name) : Effect(name) {}
    
    void setAudioFrame(const AudioFrame* frame) {
        audioFrame = frame;
    }
    
    virtual bool requiresAudio() const override { return true; }
};

/**
 * Frequency Spectrum Visualizer
 * Maps frequency bins across the LED strips
 */
class FrequencySpectrumEffect : public AudioReactiveEffect {
private:
    float binSmoothing[FFT_BIN_COUNT];
    
public:
    FrequencySpectrumEffect() : AudioReactiveEffect("Frequency Spectrum") {
        memset(binSmoothing, 0, sizeof(binSmoothing));
    }
    
    void render(CRGB* leds, const VisualParams& params) override {
        if (!audioFrame || audioFrame->silence) {
            fadeToBlackBy(leds, NUM_LEDS, 20);
            return;
        }
        
        // Map frequency bins to LED positions
        int ledsPerBin = NUM_LEDS / FFT_BIN_COUNT;
        
        for (int bin = 0; bin < FFT_BIN_COUNT; bin++) {
            // Smooth the frequency data
            float targetValue = audioFrame->frequency_bins[bin];
            binSmoothing[bin] += (targetValue - binSmoothing[bin]) * 0.3f;
            
            // Calculate color based on frequency and intensity
            uint8_t hue = map(bin, 0, FFT_BIN_COUNT, 0, 255);
            uint8_t brightness = constrain(binSmoothing[bin] * params.brightness, 0, 255);
            CRGB color = CHSV(hue, params.saturation, brightness);
            
            // Apply to corresponding LEDs
            int startLed = bin * ledsPerBin;
            int endLed = min((bin + 1) * ledsPerBin, NUM_LEDS);
            
            for (int i = startLed; i < endLed; i++) {
                leds[i] = color;
            }
        }
    }
};

/**
 * Bass Pulse Effect
 * Creates pulsing waves from center based on bass energy
 */
class BassPulseEffect : public AudioReactiveEffect {
private:
    float pulsePosition;
    float pulseVelocity;
    uint8_t pulseHue;
    
public:
    BassPulseEffect() : AudioReactiveEffect("Bass Pulse"), 
                        pulsePosition(0), pulseVelocity(0), pulseHue(0) {}
    
    void render(CRGB* leds, const VisualParams& params) override {
        fadeToBlackBy(leds, NUM_LEDS, params.complexity / 10 + 5);
        
        if (!audioFrame) return;
        
        // Trigger new pulse on bass hit
        if (audioFrame->bass_energy > 500.0f && pulseVelocity < 0.1f) {
            pulseVelocity = audioFrame->bass_energy / 1000.0f;
            pulseHue = random8();
        }
        
        // Update pulse position
        pulsePosition += pulseVelocity * (params.speed / 128.0f);
        pulseVelocity *= 0.95f; // Decay
        
        if (pulsePosition > 0) {
            // Draw pulse expanding from center
            int pulseWidth = params.variation / 10 + 3;
            
            for (int offset = -pulseWidth; offset <= pulseWidth; offset++) {
                int ledIndex1 = NUM_LEDS/2 + (int)pulsePosition + offset;
                int ledIndex2 = NUM_LEDS/2 - (int)pulsePosition - offset;
                
                if (ledIndex1 >= 0 && ledIndex1 < NUM_LEDS) {
                    uint8_t brightness = 255 - abs(offset) * (255 / pulseWidth);
                    brightness = scale8(brightness, params.brightness);
                    leds[ledIndex1] += CHSV(pulseHue + offset * 2, params.saturation, brightness);
                }
                
                if (ledIndex2 >= 0 && ledIndex2 < NUM_LEDS) {
                    uint8_t brightness = 255 - abs(offset) * (255 / pulseWidth);
                    brightness = scale8(brightness, params.brightness);
                    leds[ledIndex2] += CHSV(pulseHue + offset * 2, params.saturation, brightness);
                }
            }
        }
        
        // Reset when pulse reaches end
        if (pulsePosition > NUM_LEDS/2) {
            pulsePosition = 0;
            pulseVelocity = 0;
        }
    }
};

/**
 * Energy Flow Effect
 * Particles flow with speed based on total audio energy
 */
class EnergyFlowEffect : public AudioReactiveEffect {
private:
    struct Particle {
        float position;
        float velocity;
        uint8_t hue;
        uint8_t brightness;
    };
    
    static constexpr int MAX_PARTICLES = 20;
    Particle particles[MAX_PARTICLES];
    
public:
    EnergyFlowEffect() : AudioReactiveEffect("Energy Flow") {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].position = random16(NUM_LEDS);
            particles[i].velocity = 0;
            particles[i].hue = random8();
            particles[i].brightness = 0;
        }
    }
    
    void render(CRGB* leds, const VisualParams& params) override {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        
        if (!audioFrame) return;
        
        // Base flow speed from parameters
        float baseSpeed = params.speed / 128.0f;
        
        // Modulate speed with audio energy
        float energyMultiplier = 1.0f;
        if (!audioFrame->silence) {
            energyMultiplier = 1.0f + (audioFrame->total_energy / 1000.0f);
        }
        
        for (int i = 0; i < MAX_PARTICLES; i++) {
            Particle& p = particles[i];
            
            // Update velocity based on audio
            if (audioFrame->transient_detected && random8() < 128) {
                // Boost on transients
                p.velocity = baseSpeed * energyMultiplier * 3.0f;
                p.brightness = 255;
            } else {
                // Normal flow
                p.velocity = baseSpeed * energyMultiplier;
                p.brightness = scale8(p.brightness, 250); // Fade
            }
            
            // Update position
            p.position += p.velocity;
            
            // Wrap around
            if (p.position >= NUM_LEDS) {
                p.position -= NUM_LEDS;
                p.hue = random8();
            }
            
            // Color based on frequency bands
            if (!audioFrame->silence) {
                if (audioFrame->bass_energy > audioFrame->high_energy) {
                    p.hue = 0; // Red for bass
                } else if (audioFrame->high_energy > audioFrame->mid_energy) {
                    p.hue = 160; // Blue for highs
                } else {
                    p.hue = 96; // Green for mids
                }
            }
            
            // Draw particle with trail
            int pos = (int)p.position;
            if (pos >= 0 && pos < NUM_LEDS) {
                leds[pos] += CHSV(p.hue, params.saturation, 
                                  scale8(p.brightness, params.brightness));
                
                // Trail
                for (int t = 1; t < 5; t++) {
                    int trailPos = pos - t;
                    if (trailPos >= 0) {
                        uint8_t trailBright = p.brightness / (t + 1);
                        leds[trailPos] += CHSV(p.hue + t * 5, params.saturation, 
                                               scale8(trailBright, params.brightness));
                    }
                }
            }
        }
    }
};

/**
 * Beat Reactive Strobe
 * Flashes on beat detection with color based on frequency content
 */
class BeatStrobeEffect : public AudioReactiveEffect {
private:
    uint8_t strobeIntensity;
    uint8_t lastHue;
    bool wasOnBeat;
    
public:
    BeatStrobeEffect() : AudioReactiveEffect("Beat Strobe"), 
                         strobeIntensity(0), lastHue(0), wasOnBeat(false) {}
    
    void render(CRGB* leds, const VisualParams& params) override {
        // Fade out
        fadeToBlackBy(leds, NUM_LEDS, params.complexity + 10);
        
        if (!audioFrame) return;
        
        // Check for beat (using transient detection as proxy)
        bool onBeat = audioFrame->transient_detected || 
                      (audioFrame->bass_energy > 800.0f);
        
        if (onBeat && !wasOnBeat) {
            // New beat detected
            strobeIntensity = 255;
            
            // Choose color based on dominant frequency
            if (audioFrame->bass_energy > audioFrame->mid_energy && 
                audioFrame->bass_energy > audioFrame->high_energy) {
                lastHue = 0; // Red for bass
            } else if (audioFrame->high_energy > audioFrame->mid_energy) {
                lastHue = 160; // Blue for highs
            } else {
                lastHue = 96; // Green for mids
            }
        }
        
        wasOnBeat = onBeat;
        
        if (strobeIntensity > 0) {
            // Create strobe pattern
            uint8_t brightness = scale8(strobeIntensity, params.brightness);
            CRGB color = CHSV(lastHue + params.variation, params.saturation, brightness);
            
            // Different patterns based on intensity parameter
            if (params.intensity < 85) {
                // Full strip flash
                fill_solid(leds, NUM_LEDS, color);
            } else if (params.intensity < 170) {
                // Center burst
                int burstSize = map(strobeIntensity, 0, 255, 0, NUM_LEDS/2);
                for (int i = 0; i < burstSize; i++) {
                    leds[NUM_LEDS/2 + i] = color;
                    leds[NUM_LEDS/2 - i - 1] = color;
                }
            } else {
                // Random pixels
                for (int i = 0; i < NUM_LEDS; i++) {
                    if (random8() < params.intensity) {
                        leds[i] = color;
                    }
                }
            }
            
            // Decay strobe intensity
            strobeIntensity = scale8(strobeIntensity, 200);
        }
    }
};

/**
 * Frequency Waterfall
 * Scrolling frequency visualization
 */
class FrequencyWaterfallEffect : public AudioReactiveEffect {
private:
    CRGB history[NUM_LEDS];
    
public:
    FrequencyWaterfallEffect() : AudioReactiveEffect("Frequency Waterfall") {
        memset(history, 0, sizeof(history));
    }
    
    void render(CRGB* leds, const VisualParams& params) override {
        if (!audioFrame) {
            fadeToBlackBy(leds, NUM_LEDS, 10);
            return;
        }
        
        // Shift history
        memmove(&history[1], &history[0], (NUM_LEDS - 1) * sizeof(CRGB));
        
        // Calculate new color based on frequency content
        uint8_t hue = 0;
        uint8_t brightness = 0;
        
        if (!audioFrame->silence) {
            // Map frequency balance to hue
            float total = audioFrame->bass_energy + audioFrame->mid_energy + audioFrame->high_energy;
            if (total > 0) {
                float bassRatio = audioFrame->bass_energy / total;
                float midRatio = audioFrame->mid_energy / total;
                float highRatio = audioFrame->high_energy / total;
                
                hue = (bassRatio * 0) + (midRatio * 96) + (highRatio * 160);
                brightness = constrain(total / 10, 0, 255);
            }
        }
        
        // Add variation based on parameters
        hue += params.variation;
        brightness = scale8(brightness, params.brightness);
        
        // Set new history entry
        history[0] = CHSV(hue, params.saturation, brightness);
        
        // Copy history to LED array with optional mirroring
        if (params.complexity > 128) {
            // Mirror mode
            for (int i = 0; i < NUM_LEDS/2; i++) {
                leds[NUM_LEDS/2 + i] = history[i];
                leds[NUM_LEDS/2 - i - 1] = history[i];
            }
        } else {
            // Normal mode
            memcpy(leds, history, NUM_LEDS * sizeof(CRGB));
        }
        
        // Apply smoothing based on speed parameter
        if (params.speed < 128) {
            blur1d(leds, NUM_LEDS, 255 - params.speed * 2);
        }
    }
};

#endif // AUDIO_REACTIVE_EFFECTS_H