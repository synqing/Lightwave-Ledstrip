#ifndef LUT_OPTIMIZED_EFFECTS_H
#define LUT_OPTIMIZED_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "../core/MegaLUTs.h"
#include "../config/hardware_config.h"
#include "EffectBase.h"

/**
 * LUT-OPTIMIZED EFFECTS ENGINE
 * 
 * Ultra-fast effects using pre-calculated lookup tables
 * Zero floating point operations in render loops
 * Maximum performance through memory trade-off
 */

class LUTPlasmaEffect : public EffectBase {
private:
    uint8_t plasmaTime = 0;
    uint8_t plasmaSpeed = 1;
    
public:
    LUTPlasmaEffect() : EffectBase("LUT Plasma") {}
    
    void update() override {
        // Update time with LUT-based speed curve
        plasmaTime += encoderExponentialLUT[plasmaSpeed];
        
        // Direct plasma LUT lookup - ZERO calculations
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Map LED position to plasma coordinates
            uint8_t x = (i * 128) / NUM_LEDS;
            uint8_t y = plasmaTime;
            
            // Direct LUT access
            uint8_t plasmaValue = plasmaLUT[x & 127][(y + spiralAngleLUT[i]) & 127];
            
            // Use pre-calculated palette
            leds[i] = paletteInterpolationLUT[currentPaletteIndex][plasmaValue];
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        if (encoder == 3) {  // Speed control
            plasmaSpeed = constrain(plasmaSpeed + delta, 0, 255);
        }
    }
};

class LUTFireEffect : public EffectBase {
private:
    uint8_t fireFrame = 0;
    uint8_t cooling = 50;
    uint8_t sparking = 120;
    
public:
    LUTFireEffect() : EffectBase("LUT Fire") {}
    
    void update() override {
        // Use pre-calculated fire animation frames
        fireFrame = (fireFrame + 1) & 63;  // 64 frames
        
        // Direct fire LUT lookup
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            uint8_t heat = fireLUT[fireFrame][i];
            
            // Apply cooling using LUT
            uint8_t cooled = heat > cooling ? heat - cooling : 0;
            
            // Sparking chance using noise LUT
            if (noiseLUT[fireFrame][i & 63] < sparking) {
                cooled = qadd8(cooled, random8(160, 255));
            }
            
            // Map heat to color using pre-calculated fire palette
            leds[i] = paletteInterpolationLUT[1][cooled];  // Palette 1 is fire
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 4: cooling = constrain(cooling + delta * 5, 0, 255); break;
            case 5: sparking = constrain(sparking + delta * 5, 0, 255); break;
        }
    }
};

class LUTWaveEffect : public EffectBase {
private:
    uint8_t wavePattern = 0;
    uint8_t waveSpeed = 128;
    uint8_t waveOffset = 0;
    
public:
    LUTWaveEffect() : EffectBase("LUT Wave") {}
    
    void update() override {
        // Update wave offset using speed LUT
        waveOffset += encoderSCurveLUT[waveSpeed] >> 3;
        
        // Get current wave pattern with offset
        uint8_t patternIndex = (wavePattern + waveOffset) & 255;
        
        // Direct wave pattern copy - ULTRA FAST
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            uint8_t waveValue = wavePatternLUT[patternIndex][i];
            
            // Apply HDR gamma correction from LUT
            uint16_t hdrValue = hdrGammaLUT[waveValue];
            
            // Convert back to 8-bit with compression
            uint8_t compressed = hdrCompressLUT[hdrValue >> 6];
            
            leds[i] = ColorFromPalette(currentPalette, compressed);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: wavePattern = (wavePattern + delta) & 255; break;
            case 4: waveSpeed = constrain(waveSpeed + delta * 2, 0, 255); break;
        }
    }
};

class LUTMandelbrotEffect : public EffectBase {
private:
    uint8_t zoomLevel = 32;
    int16_t offsetX = 64;
    int16_t offsetY = 64;
    uint8_t colorShift = 0;
    
public:
    LUTMandelbrotEffect() : EffectBase("LUT Mandelbrot") {}
    
    void update() override {
        colorShift++;
        
        // Map LEDs to Mandelbrot coordinates
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Use geometry LUT for position mapping
            uint8_t x = ((i * zoomLevel) / NUM_LEDS + offsetX) & 127;
            uint8_t y = (distanceFromCenterLUT[i] + offsetY) & 127;
            
            // Direct Mandelbrot LUT access
            uint8_t iterations = mandelbrotLUT[x][y];
            
            // Color based on iterations with shifting
            uint8_t hue = iterations + colorShift;
            leds[i] = CRGB(hueToRgbLUT[hue][0], 
                          hueToRgbLUT[hue][1], 
                          hueToRgbLUT[hue][2]);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: zoomLevel = constrain(zoomLevel + delta, 16, 128); break;
            case 4: offsetX = (offsetX + delta) & 127; break;
            case 5: offsetY = (offsetY + delta) & 127; break;
        }
    }
};

class LUTParticleEffect : public EffectBase {
private:
    struct Particle {
        uint8_t position;
        uint8_t velocity;
        uint8_t life;
        uint8_t color;
    };
    
    static const uint8_t MAX_PARTICLES = 20;
    Particle particles[MAX_PARTICLES];
    uint8_t activeParticles = 10;
    
public:
    LUTParticleEffect() : EffectBase("LUT Particles") {
        // Initialize particles
        for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
            particles[i].position = random8();
            particles[i].velocity = random8();
            particles[i].life = random8();
            particles[i].color = random8(64);
        }
    }
    
    void update() override {
        // Fade all LEDs using brightness LUT
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            leds[i].r = dim8_video_LUT[leds[i].r];
            leds[i].g = dim8_video_LUT[leds[i].g];
            leds[i].b = dim8_video_LUT[leds[i].b];
        }
        
        // Update particles using LUTs
        for (uint8_t p = 0; p < activeParticles; p++) {
            Particle& particle = particles[p];
            
            // Update position using velocity LUT
            int16_t newPos = particle.position + particleVelocityLUT[particle.velocity][0];
            
            // Bounce at edges
            if (newPos < 0 || newPos >= NUM_LEDS) {
                particle.velocity = 255 - particle.velocity;  // Reverse
                newPos = constrain(newPos, 0, NUM_LEDS - 1);
            }
            particle.position = newPos;
            
            // Update life using decay LUT
            if (particle.life > 0) {
                particle.life = particleDecayLUT[particle.life];
                
                // Draw particle with pre-calculated color
                uint8_t ledIndex = particle.position;
                if (ledIndex < NUM_LEDS) {
                    CRGB color = particleColorLUT[particle.color & 63];
                    
                    // Apply life-based brightness
                    color.fadeToBlackBy(255 - particle.life);
                    leds[ledIndex] += color;
                    
                    // Add glow using distance LUT
                    if (ledIndex > 0) {
                        leds[ledIndex - 1] += color.scale8(64);
                    }
                    if (ledIndex < NUM_LEDS - 1) {
                        leds[ledIndex + 1] += color.scale8(64);
                    }
                }
            } else {
                // Respawn particle
                particle.position = random8(NUM_LEDS);
                particle.velocity = random8();
                particle.life = 255;
                particle.color = random8(64);
            }
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        if (encoder == 4) {
            activeParticles = constrain(activeParticles + delta, 1, MAX_PARTICLES);
        }
    }
};

class LUTPerlinNoiseEffect : public EffectBase {
private:
    uint8_t noiseScale = 64;
    uint8_t noiseSpeed = 10;
    uint16_t noiseOffset = 0;
    
public:
    LUTPerlinNoiseEffect() : EffectBase("LUT Perlin") {}
    
    void update() override {
        noiseOffset += noiseSpeed;
        
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Multi-octave noise using pre-calculated tables
            uint8_t x = (i * noiseScale / NUM_LEDS + noiseOffset) & 127;
            uint8_t y = (noiseOffset >> 1) & 127;
            
            // Combine octaves using bit shifts instead of multiplication
            uint16_t noise = perlinOctave1[x][y];
            noise += perlinOctave2[x >> 1][y >> 1] >> 1;  // Half amplitude
            noise += perlinOctave3[x >> 2][y >> 2] >> 2;  // Quarter amplitude
            
            uint8_t finalNoise = noise >> 2;  // Average
            
            // Use noise for both color and brightness
            leds[i] = ColorFromPalette(currentPalette, finalNoise + noiseOffset);
            leds[i].fadeToBlackBy(255 - quadraticScaleLUT[finalNoise]);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: noiseScale = constrain(noiseScale + delta * 4, 16, 255); break;
            case 4: noiseSpeed = constrain(noiseSpeed + delta, 0, 50); break;
        }
    }
};

class LUTComplexWaveEffect : public EffectBase {
private:
    uint8_t waveformType = 0;
    uint16_t wavePosition = 0;
    uint8_t waveSpeed = 20;
    uint8_t waveformMix[4] = {255, 0, 0, 0};  // Mixing levels
    
public:
    LUTComplexWaveEffect() : EffectBase("LUT Complex Wave") {}
    
    void update() override {
        wavePosition += waveSpeed;
        
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Calculate phase for this LED
            uint16_t phase = (i * 512 / NUM_LEDS + wavePosition) & 511;
            
            // Mix multiple waveforms using LUTs
            int32_t mixedWave = 0;
            uint16_t totalMix = 0;
            
            for (uint8_t w = 0; w < 4; w++) {
                if (waveformMix[w] > 0) {
                    mixedWave += (complexWaveformLUT[w][phase] * waveformMix[w]) >> 8;
                    totalMix += waveformMix[w];
                }
            }
            
            // Normalize
            if (totalMix > 0) {
                mixedWave /= totalMix;
            }
            
            // Convert to 0-255 range
            uint8_t waveValue = (mixedWave + 32768) >> 8;
            
            // Apply wave to color
            leds[i] = ColorFromPalette(currentPalette, waveValue);
            
            // Apply envelope using easing LUT
            uint8_t envelope = easingLUT[8][i * 255 / NUM_LEDS];  // Sine envelope
            leds[i].fadeToBlackBy(255 - envelope);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: // Select waveform
                waveformType = (waveformType + delta) & 3;
                break;
            case 4: // Adjust mix level
                waveformMix[waveformType] = constrain(waveformMix[waveformType] + delta * 8, 0, 255);
                break;
            case 5: // Speed
                waveSpeed = constrain(waveSpeed + delta, 0, 100);
                break;
        }
    }
};

class LUTShaderEffect : public EffectBase {
private:
    uint8_t shaderTime = 0;
    uint8_t shaderSpeed = 5;
    uint8_t shaderZoom = 32;
    
public:
    LUTShaderEffect() : EffectBase("LUT Shader") {}
    
    void update() override {
        shaderTime += shaderSpeed;
        
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Map LED to shader coordinates
            uint8_t x = (i * shaderZoom / NUM_LEDS + shaderTime) & 63;
            uint8_t y = (distanceFromCenterLUT[i] * shaderZoom / 255) & 63;
            
            // Get shader value
            uint8_t shaderValue = shaderEffectLUT[x][y];
            
            // Apply motion blur using LUT
            static uint8_t history[HardwareConfig::NUM_LEDS][8];
            
            // Shift history
            for (int h = 7; h > 0; h--) {
                history[i][h] = history[i][h-1];
            }
            history[i][0] = shaderValue;
            
            // Accumulate with motion blur
            uint16_t blurred = 0;
            for (int h = 0; h < 8; h++) {
                blurred += motionBlurLUT[history[i][h]][h];
            }
            shaderValue = blurred >> 3;
            
            // Apply to LED with palette
            leds[i] = ColorFromPalette(currentPalette, shaderValue + shaderTime);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: shaderSpeed = constrain(shaderSpeed + delta, 1, 20); break;
            case 4: shaderZoom = constrain(shaderZoom + delta * 2, 16, 64); break;
        }
    }
};

// Ultra-fast transition effect using pre-rendered frames
class LUTTransitionShowcase : public EffectBase {
private:
    uint8_t currentTransition = 0;
    uint8_t transitionFrame = 0;
    uint8_t transitionSpeed = 1;
    uint8_t basePattern = 0;
    
public:
    LUTTransitionShowcase() : EffectBase("LUT Transitions") {}
    
    void update() override {
        // Update frame
        transitionFrame += transitionSpeed;
        if (transitionFrame >= 16) {
            transitionFrame = 0;
            currentTransition = (currentTransition + 1) % 5;
        }
        
        // Get transition mask for current frame
        uint8_t mask[HardwareConfig::NUM_LEDS];
        getTransitionFrame(mask, currentTransition, transitionFrame >> 4);
        
        // Apply transition between two patterns
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            // Pattern A: Wave
            uint8_t patternA = wavePatternLUT[basePattern][i];
            
            // Pattern B: Another wave
            uint8_t patternB = wavePatternLUT[(basePattern + 128) & 255][i];
            
            // Blend using transition mask
            uint8_t blended = scale8(patternA, 255 - mask[i]) + scale8(patternB, mask[i]);
            
            // Apply with palette
            leds[i] = ColorFromPalette(currentPalette, blended);
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 3: basePattern = (basePattern + delta * 4) & 255; break;
            case 4: transitionSpeed = constrain(transitionSpeed + delta, 1, 16); break;
        }
    }
};

// Frequency visualization using FFT LUTs
class LUTFrequencyEffect : public EffectBase {
private:
    uint8_t sensitivity = 128;
    uint8_t frequencyBand = 0;
    uint8_t visualMode = 0;
    
    // Simulated frequency data
    uint8_t frequencyData[32];
    
public:
    LUTFrequencyEffect() : EffectBase("LUT Frequency") {}
    
    void update() override {
        // Simulate frequency data (in real use, this would come from audio input)
        for (int i = 0; i < 32; i++) {
            // Create interesting patterns
            frequencyData[i] = beatsinx(i + 1, 0, 255, 0, shaderTime);
        }
        
        // Clear LEDs
        fadeToBlackBy(leds, NUM_LEDS, 20);
        
        // Visualize frequencies using pre-calculated mappings
        for (uint8_t band = 0; band < 32; band++) {
            uint8_t energy = scale8(frequencyData[band], sensitivity);
            
            // Apply beat detection LUT
            uint8_t beatType = energy > 200 ? 0 : energy > 150 ? 1 : energy > 100 ? 2 : 3;
            uint8_t beatResponse = beatDetectionLUT[energy][beatType];
            
            // Map frequency band to LEDs using LUT
            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint8_t influence = frequencyBinLUT[band][i];
                
                if (influence > 0) {
                    uint8_t brightness = scale8(beatResponse, influence);
                    
                    // Color based on frequency
                    CRGB color = ColorFromPalette(currentPalette, band * 8);
                    color.fadeToBlackBy(255 - brightness);
                    
                    leds[i] += color;
                }
            }
        }
    }
    
    void onEncoderChange(uint8_t encoder, int16_t delta) override {
        switch (encoder) {
            case 4: sensitivity = constrain(sensitivity + delta * 4, 0, 255); break;
            case 5: visualMode = (visualMode + delta) % 4; break;
        }
    }
    
private:
    uint16_t shaderTime = 0;
    
    uint16_t beatsinx(uint16_t beat, uint16_t lowest, uint16_t highest, uint32_t timebase, uint32_t phase) {
        uint16_t beatsin = fastSin8((beat * timebase) + phase);
        uint16_t rangewidth = highest - lowest;
        uint16_t scaledbeat = scale16(beatsin, rangewidth);
        uint16_t result = lowest + scaledbeat;
        return result;
    }
};

#endif // LUT_OPTIMIZED_EFFECTS_H