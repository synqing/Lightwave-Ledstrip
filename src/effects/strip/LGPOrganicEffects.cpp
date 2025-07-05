// LGP Organic Pattern Effects
// Natural and fluid patterns leveraging Light Guide Plate diffusion
// These effects create organic, living visuals through optical blending

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGB leds[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== AURORA BOREALIS ==============
// Northern lights simulation with waveguide color mixing
void lgpAuroraBorealis() {
    static uint16_t time = 0;
    static uint8_t curtainPhase[5] = {0, 51, 102, 153, 204};
    
    time += paletteSpeed >> 4;  // Slow down 4x
    
    // Number of aurora curtains
    uint8_t curtainCount = 2 + (visualParams.complexity >> 6);
    
    // Clear strips
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    // Generate aurora curtains
    for (uint8_t c = 0; c < curtainCount; c++) {
        // Curtain position oscillates - SLOWER
        curtainPhase[c] += (c + 1);  // Reduced from 3 to 1
        uint16_t curtainCenter = beatsin16(1, 20, HardwareConfig::STRIP_LENGTH - 20, 0, curtainPhase[c] << 8);  // Slower beat
        
        // Curtain width varies - MORE STABLE
        uint8_t curtainWidth = beatsin8(1, 20, 35, 0, curtainPhase[c]);  // Slower, less variation
        
        // Aurora colors - greens, blues, purples
        uint8_t hue = 96 + (c * 32);  // Green to purple range
        
        // Draw curtain with gaussian-like falloff
        for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            int16_t dist = abs((int16_t)i - curtainCenter);
            if (dist < curtainWidth) {
                uint8_t brightness = qsub8(255, (dist * 255) / curtainWidth);
                brightness = scale8(brightness, visualParams.intensity);
                
                // Shimmer effect - REDUCED
                brightness = scale8(brightness, 220 + (inoise8(i * 5, time >> 3) >> 3));  // Much subtler
                
                // Different colors on each strip for depth
                CRGB color1 = ColorFromPalette(currentPalette, hue, brightness);
                CRGB color2 = ColorFromPalette(currentPalette, hue + 20, brightness);
                
                strip1[i] += color1;
                strip2[i] += color2;
            }
        }
    }
    
    // Add corona at top (edges)
    for (uint8_t i = 0; i < 20; i++) {
        uint8_t corona = scale8(255 - i * 12, visualParams.intensity >> 1);
        strip1[i] += CRGB(0, corona >> 2, corona >> 1);
        strip1[HardwareConfig::STRIP_LENGTH - 1 - i] += CRGB(0, corona >> 2, corona >> 1);
        strip2[i] += CRGB(0, corona >> 3, corona);
        strip2[HardwareConfig::STRIP_LENGTH - 1 - i] += CRGB(0, corona >> 3, corona);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== BIOLUMINESCENT WAVES ==============
// Ocean waves with glowing plankton effect
void lgpBioluminescentWaves() {
    static uint16_t wavePhase = 0;
    static uint8_t glowPoints[20];
    static uint8_t glowLife[20];
    
    wavePhase += paletteSpeed;
    
    // Wave parameters
    uint8_t waveCount = 2 + (visualParams.complexity >> 6);
    
    // Base ocean color
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Multiple wave superposition
        uint16_t wave = 0;
        for (uint8_t w = 0; w < waveCount; w++) {
            wave += sin8(((i << 2) + (wavePhase >> (4 - w))) >> w);
        }
        wave /= waveCount;
        
        // Deep blue base
        uint8_t blue = scale8(wave, 60);
        uint8_t green = scale8(wave, 20);
        
        strip1[i] = CRGB(0, green, blue);
        strip2[i] = CRGB(0, green >> 1, blue);
    }
    
    // Add bioluminescent sparkles
    EVERY_N_MILLISECONDS(100) {
        // Find dead glow point
        for (uint8_t g = 0; g < 20; g++) {
            if (glowLife[g] == 0) {
                // Spawn new glow
                glowPoints[g] = random8(HardwareConfig::STRIP_LENGTH);
                glowLife[g] = 255;
                break;
            }
        }
    }
    
    // Update and render glow points
    for (uint8_t g = 0; g < 20; g++) {
        if (glowLife[g] > 0) {
            // Fade out
            glowLife[g] = scale8(glowLife[g], 240);
            
            uint8_t pos = glowPoints[g];
            uint8_t intensity = scale8(glowLife[g], visualParams.intensity);
            
            // Bioluminescent blue-green
            CRGB glow = CRGB(0, intensity >> 1, intensity);
            
            // Apply with spreading
            for (int8_t spread = -3; spread <= 3; spread++) {
                int16_t p = pos + spread;
                if (p >= 0 && p < HardwareConfig::STRIP_LENGTH) {
                    uint8_t spreadIntensity = scale8(intensity, 255 - abs(spread) * 60);
                    strip1[p] += CRGB(0, spreadIntensity >> 1, spreadIntensity);
                    strip2[p] += CRGB(0, spreadIntensity >> 2, spreadIntensity);
                }
            }
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== PLASMA MEMBRANE ==============
// Organic cellular membrane with lipid bilayer dynamics
void lgpPlasmaMembrane() {
    static uint16_t time = 0;
    time += paletteSpeed >> 1;
    
    // Membrane undulation parameters
    uint8_t undulationFreq = 3 + (visualParams.complexity >> 5);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Membrane shape using multiple octaves
        uint16_t membrane = 0;
        membrane += inoise8(i * 3, time >> 2) << 1;
        membrane += inoise8(i * 7, time >> 1) >> 1;
        membrane += inoise8(i * 13, time);
        membrane >>= 2;
        
        // Lipid bilayer coloring
        uint8_t hue = 20 + (membrane >> 3);  // Yellow-orange range
        uint8_t sat = 200 + (membrane >> 2);
        uint8_t brightness = scale8(membrane, visualParams.intensity);
        
        // Inner and outer membrane layers
        CRGB inner = CHSV(hue, sat, brightness);
        CRGB outer = CHSV(hue + 10, sat - 50, scale8(brightness, 200));
        
        // Protein channels - NO WHITE STROBES
        if (random8() < 1 && visualParams.variation > 128) {  // Much rarer and only with high variation
            inner = blend(inner, CHSV(hue, 100, 255), 128);  // Blend instead of hard white
            outer = blend(outer, CHSV(hue + 20, 150, 200), 128);
        }
        
        strip1[i] = inner;
        strip2[i] = outer;
    }
    
    // Add membrane potential waves
    uint16_t potentialWave = beatsin16(5, 0, HardwareConfig::STRIP_LENGTH - 1);
    for (int8_t w = -10; w <= 10; w++) {
        int16_t pos = potentialWave + w;
        if (pos >= 0 && pos < HardwareConfig::STRIP_LENGTH) {
            uint8_t waveIntensity = 255 - abs(w) * 20;
            strip1[pos] = blend(strip1[pos], CRGB::Yellow, waveIntensity);
            strip2[pos] = blend(strip2[pos], CRGB::Gold, waveIntensity);
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== NEURAL NETWORK ==============
// Synaptic firing patterns with signal propagation
void lgpNeuralNetwork() {
    static uint16_t time = 0;
    static uint8_t neurons[20];
    static uint8_t neuronState[20];  // 0=resting, 1-255=firing
    static int8_t signalPos[10];
    static uint8_t signalStrength[10];
    
    time += paletteSpeed >> 2;
    
    // Initialize neurons on first run
    static bool initialized = false;
    if (!initialized) {
        for (uint8_t n = 0; n < 20; n++) {
            neurons[n] = random8(HardwareConfig::STRIP_LENGTH);
        }
        initialized = true;
    }
    
    // Background neural tissue
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t tissue = inoise8(i * 5, time >> 3) >> 2;
        strip1[i] = CRGB(tissue >> 1, 0, tissue);
        strip2[i] = CRGB(tissue >> 2, 0, tissue >> 1);
    }
    
    // Update neurons
    for (uint8_t n = 0; n < 20; n++) {
        if (neuronState[n] > 0) {
            // Decay firing state
            neuronState[n] = scale8(neuronState[n], 230);
        } else {
            // Random firing based on complexity
            if (random8() < visualParams.complexity >> 3) {
                neuronState[n] = 255;
                
                // Spawn signal
                for (uint8_t s = 0; s < 10; s++) {
                    if (signalStrength[s] == 0) {
                        signalPos[s] = neurons[n];
                        signalStrength[s] = 255;
                        break;
                    }
                }
            }
        }
        
        // Render neuron
        uint8_t pos = neurons[n];
        uint8_t intensity = scale8(neuronState[n], visualParams.intensity);
        CRGB neuronColor = CRGB(intensity, intensity >> 3, intensity >> 1);
        
        // Neuron body
        strip1[pos] = neuronColor;
        strip2[pos] = neuronColor;
        
        // Dendrites
        for (int8_t d = -2; d <= 2; d++) {
            if (d != 0) {
                int16_t dendPos = pos + d;
                if (dendPos >= 0 && dendPos < HardwareConfig::STRIP_LENGTH) {
                    uint8_t dendIntensity = intensity >> (1 + abs(d));
                    strip1[dendPos] += CRGB(dendIntensity >> 2, 0, dendIntensity >> 3);
                    strip2[dendPos] += CRGB(dendIntensity >> 3, 0, dendIntensity >> 2);
                }
            }
        }
    }
    
    // Update and render signals
    for (uint8_t s = 0; s < 10; s++) {
        if (signalStrength[s] > 0) {
            // Propagate signal
            signalPos[s] += (random8(2) == 0) ? 1 : -1;
            signalStrength[s] = scale8(signalStrength[s], 240);
            
            // Render signal
            if (signalPos[s] >= 0 && signalPos[s] < HardwareConfig::STRIP_LENGTH) {
                uint8_t sigIntensity = scale8(signalStrength[s], visualParams.intensity);
                CRGB sigColor = CRGB(sigIntensity >> 1, sigIntensity >> 2, sigIntensity);
                
                strip1[signalPos[s]] += sigColor;
                strip2[signalPos[s]] += sigColor;
            }
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== CRYSTALLINE GROWTH ==============
// Crystal formation with light refraction
void lgpCrystallineGrowth() {
    static uint16_t time = 0;
    static uint8_t crystalSeeds[10];
    static uint8_t crystalSize[10];
    static uint8_t crystalHue[10];
    
    time += paletteSpeed >> 3;
    
    // Initialize crystal seeds
    static bool initialized = false;
    if (!initialized) {
        for (uint8_t c = 0; c < 10; c++) {
            crystalSeeds[c] = random8(HardwareConfig::STRIP_LENGTH);
            crystalHue[c] = random8();
        }
        initialized = true;
    }
    
    // Background substrate
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        uint8_t substrate = 20 + inoise8(i * 10, time) >> 4;
        strip1[i] = CRGB(substrate >> 2, substrate >> 2, substrate);
        strip2[i] = CRGB(substrate >> 3, substrate >> 3, substrate >> 1);
    }
    
    // Update crystals
    for (uint8_t c = 0; c < 10; c++) {
        // Grow crystals based on complexity
        if (crystalSize[c] < 20 && random8() < visualParams.complexity >> 2) {
            crystalSize[c]++;
        }
        
        // Reset fully grown crystals occasionally
        if (crystalSize[c] >= 20 && random8() < 5) {
            crystalSize[c] = 0;
            crystalSeeds[c] = random8(HardwareConfig::STRIP_LENGTH);
            crystalHue[c] = random8();
        }
        
        // Render crystal
        uint8_t pos = crystalSeeds[c];
        
        for (int8_t facet = -crystalSize[c]; facet <= crystalSize[c]; facet++) {
            int16_t facetPos = pos + facet;
            if (facetPos >= 0 && facetPos < HardwareConfig::STRIP_LENGTH) {
                // Crystal structure with internal reflections
                uint8_t facetBrightness = 255 - (abs(facet) * 255 / (crystalSize[c] + 1));
                facetBrightness = scale8(facetBrightness, visualParams.intensity);
                
                // Prismatic dispersion
                uint8_t hue = crystalHue[c] + (facet * 5);
                
                // Different refraction on each strip
                CRGB color1 = CHSV(hue, 200 - abs(facet) * 10, facetBrightness);
                CRGB color2 = CHSV(hue + 30, 180 - abs(facet) * 8, scale8(facetBrightness, 200));
                
                strip1[facetPos] = blend(strip1[facetPos], color1, 128);
                strip2[facetPos] = blend(strip2[facetPos], color2, 128);
            }
        }
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== FLUID DYNAMICS ==============
// Laminar and turbulent flow visualization
void lgpFluidDynamics() {
    static uint16_t time = 0;
    static float velocity[HardwareConfig::STRIP_LENGTH];
    static float pressure[HardwareConfig::STRIP_LENGTH];
    
    time += paletteSpeed >> 2;
    
    // Reynolds number (complexity controls turbulence)
    float reynolds = visualParams.getComplexityNorm();
    
    // Update fluid simulation
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Pressure gradient
        float gradientForce = 0;
        if (i > 0 && i < HardwareConfig::STRIP_LENGTH - 1) {
            gradientForce = (pressure[i-1] - pressure[i+1]) * 0.1f;
        }
        
        // Turbulence
        float turbulence = (inoise8(i * 5, time) - 128) / 128.0f * reynolds;
        
        // Update velocity
        velocity[i] += gradientForce + turbulence * 0.1f;
        velocity[i] *= 0.95f;  // Viscous damping
        
        // Update pressure
        pressure[i] += velocity[i] * 0.1f;
        pressure[i] *= 0.98f;
        
        // Add source/sink at center
        if (abs(i - HardwareConfig::STRIP_CENTER_POINT) < 5) {
            pressure[i] += sin8(time >> 2) / 255.0f;
        }
    }
    
    // Render flow
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Velocity magnitude to color
        uint8_t speed = abs(velocity[i]) * 255;
        speed = constrain(speed, 0, 255);
        
        // Pressure to brightness
        uint8_t brightness = (pressure[i] + 1.0f) * 127;
        brightness = scale8(brightness, visualParams.intensity);
        
        // Laminar flow: smooth gradients
        // Turbulent flow: rapid color changes
        uint8_t hue = gHue + (uint8_t)(velocity[i] * 100) + i/2;
        
        // Different visualization on each strip
        strip1[i] = CHSV(hue, 255 - speed/2, brightness);
        strip2[i] = CHSV(hue + 60, 200, scale8(brightness, 200 + speed/4));
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
} 