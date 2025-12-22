// LGP Advanced Color Mixing Effects
// Exploiting opposing light channels for unprecedented color phenomena

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "../../utils/TrigLookup.h"
#include "../utils/FastLEDOptim.h"
#include <math.h>

#ifndef TWO_PI
#define TWO_PI 6.28318530718f
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== COLOR TEMPERATURE GRADIENT ==============
// Warm colors from one edge meet cool colors from the other
// Creates perfect white at intersection
void lgpColorTemperature() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Color temperature calculation (2000K to 9000K)
        float temp1 = 2000 + normalizedDist * 1000;  // Warm edge
        float temp2 = 9000 - normalizedDist * 2000;  // Cool edge
        
        // Convert temperature to RGB (simplified)
        CRGB warm, cool;
        if (i < HardwareConfig::STRIP_CENTER_POINT) {
            // Warm side (reds/oranges)
            warm.r = 255;
            warm.g = 180 - (normalizedDist * 100);
            warm.b = 50 + (normalizedDist * 50);
            // Optimized: Use FastLEDOptim::fastScale8 instead of scale8(intensity * 255)
            uint8_t intensity8 = visualParams.intensity;
            strip1[i] = FastLEDOptim::fastScaleRGB(warm, intensity8);
            
            // Cool side (blues/cyans)
            cool.r = 150 + (normalizedDist * 50);
            cool.g = 200 + (normalizedDist * 55);
            cool.b = 255;
            strip2[i] = FastLEDOptim::fastScaleRGB(cool, intensity8);
        } else {
            // Mirror for other half
            warm.r = 255;
            warm.g = 180 - (normalizedDist * 100);
            warm.b = 50 + (normalizedDist * 50);
            uint8_t intensity8 = visualParams.intensity;
            strip1[i] = FastLEDOptim::fastScaleRGB(warm, intensity8);
            
            cool.r = 150 + (normalizedDist * 50);
            cool.g = 200 + (normalizedDist * 55);
            cool.b = 255;
            strip2[i] = FastLEDOptim::fastScaleRGB(cool, intensity8);
        }
    }
}

// ============== RGB PRISM SEPARATION ==============
// Simulates light passing through a prism
void lgpRGBPrism() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float prismAngle = 0;
    prismAngle += speed * 0.02f;
    
    // Dispersion amount
    float dispersion = 0.5f + complexity * 2.0f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Different wavelengths refract differently
        // Optimized: Use FastLED sin16 for better performance
        float redPhase = (normalizedDist * dispersion + prismAngle) * TWO_PI;
        float greenPhase = (normalizedDist * dispersion * 1.1f + prismAngle) * TWO_PI;
        float bluePhase = (normalizedDist * dispersion * 1.2f + prismAngle) * TWO_PI;
        
        uint16_t redPhase16 = FastLEDOptim::radiansToPhase16(redPhase);
        uint16_t greenPhase16 = FastLEDOptim::radiansToPhase16(greenPhase);
        uint16_t bluePhase16 = FastLEDOptim::radiansToPhase16(bluePhase);
        
        int16_t redWave16 = FastLEDOptim::fastSin16(redPhase16);
        int16_t greenWave16 = FastLEDOptim::fastSin16(greenPhase16);
        int16_t blueWave16 = FastLEDOptim::fastSin16(bluePhase16);
        
        // Convert to 8-bit brightness (0-255)
        uint8_t redBrightness = (redWave16 >> 8) + 128;
        uint8_t greenBrightness = (greenWave16 >> 8) + 128;
        uint8_t blueBrightness = (blueWave16 >> 8) + 128;
        
        // Strip 1: Red channel dominant
        uint8_t intensity8 = visualParams.intensity;
        strip1[i].r = FastLEDOptim::fastScale8(redBrightness, intensity8);
        strip1[i].g = FastLEDOptim::fastScale8(abs(greenBrightness - 128) + 64, intensity8);
        strip1[i].b = 0;
        
        // Strip 2: Blue channel dominant
        strip2[i].r = 0;
        strip2[i].g = FastLEDOptim::fastScale8(abs(greenBrightness - 128) + 64, intensity8);
        strip2[i].b = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        
        // Green emerges at intersection
        if (distFromCenter < 10) {
            strip1[i].g = FastLEDOptim::fastQAdd8(strip1[i].g, FastLEDOptim::fastScale8(128, intensity8));
            strip2[i].g = FastLEDOptim::fastQAdd8(strip2[i].g, FastLEDOptim::fastScale8(128, intensity8));
        }
    }
}

// ============== COMPLEMENTARY COLOR MIXING ==============
// Dynamic complementary pairs create neutral zones
void lgpComplementaryMixing() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float colorPhase = 0;
    colorPhase += speed * 0.01f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Use palette colors instead of rainbow (gHue is FORBIDDEN)
        // Phase-based palette index for slow color drift
        uint8_t paletteIndex1 = (uint8_t)(colorPhase * 255) + (uint8_t)(distFromCenter * 2);
        uint8_t paletteIndex2 = paletteIndex1 + 128;  // Complementary in palette

        // Intensity falls off from edges
        uint8_t edgeIntensity = 255 * (1 - normalizedDist * variation);

        // Mix complementary colors
        if (normalizedDist > 0.5f) {
            // Strong colors at edges - get at full brightness, then scale
            CRGB color1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
            CRGB color2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
            color1.nscale8(edgeIntensity * intensity);
            color2.nscale8(edgeIntensity * intensity);
            strip1[i] = color1;
            strip2[i] = color2;
        } else {
            // Mixing zone - reduced brightness toward center
            uint8_t brightness = 128 * intensity;
            CRGB color1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
            CRGB color2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
            color1.nscale8(brightness);
            color2.nscale8(brightness);
            strip1[i] = color1;
            strip2[i] = color2;
        }
    }
}

// ============== QUANTUM COLOR SUPERPOSITION ==============
// Colors exist in quantum states until "observed"
void lgpQuantumColors() {
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float waveFunction = 0;
    waveFunction += paletteSpeed * 0.001f;
    
    // Quantum states
    int numStates = 2 + (complexity * 4);  // 2-6 possible states
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Wave function probability
        float probability = TrigLookup::sinf_lookup(waveFunction + normalizedDist * TWO_PI * numStates);
        probability = probability * probability;  // Square for probability density
        
        // Collapse to specific palette state (not full spectrum discrete colors)
        uint8_t paletteOffset;
        if (probability < 0.25f) {
            paletteOffset = 0;
        } else if (probability < 0.5f) {
            paletteOffset = 10;
        } else if (probability < 0.75f) {
            paletteOffset = 20;
        } else {
            paletteOffset = 30;
        }

        // Uncertainty principle - fuzzy at observation boundary
        uint8_t uncertainty = 255 * (0.5f + 0.5f * TrigLookup::sinf_lookup(distFromCenter * 20));

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 3) + paletteOffset;

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(uncertainty * intensity);
        color2.nscale8((255 - uncertainty) * intensity);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== COLOR DOPPLER SHIFT ==============
// Moving colors shift frequency based on velocity
void lgpDopplerShift() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    
    static float sourcePosition = 0;
    sourcePosition += speed * 5;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Calculate relative velocity
        float relativePos = i - fmod(sourcePosition, HardwareConfig::STRIP_LENGTH);
        float velocity = speed * 10;
        
        // Doppler shift calculation
        float dopplerFactor;
        if (relativePos > 0) {
            // Moving away - red shift
            dopplerFactor = 1.0f - (velocity / 100.0f);
        } else {
            // Moving toward - blue shift
            dopplerFactor = 1.0f + (velocity / 100.0f);
        }
        
        // Position-based palette index with Doppler shift (no gHue - rainbow forbidden)
        uint8_t baseIndex = (uint8_t)(distFromCenter * 2);
        uint8_t shiftedIndex;
        if (dopplerFactor > 1.0f) {
            shiftedIndex = baseIndex - (uint8_t)(30 * (dopplerFactor - 1.0f));  // Blue shift
        } else {
            shiftedIndex = baseIndex + (uint8_t)(30 * (1.0f - dopplerFactor));  // Red shift
        }

        uint8_t brightness = 255 * intensity * (1 - distFromCenter / HardwareConfig::STRIP_HALF_LENGTH);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, shiftedIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, shiftedIndex + 64, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== COLOR PARTICLE ACCELERATOR ==============
// RGB particles accelerate from edges and collide at center
void lgpColorAccelerator() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    
    static float redParticle = 0;
    static float blueParticle = HardwareConfig::STRIP_LENGTH - 1;
    static bool collision = false;
    static float debrisRadius = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    if (!collision) {
        // Accelerate particles
        redParticle += speed * 10 * (1 + (redParticle / HardwareConfig::STRIP_LENGTH));
        blueParticle -= speed * 10 * (1 + ((HardwareConfig::STRIP_LENGTH - blueParticle) / HardwareConfig::STRIP_LENGTH));
        
        // Draw particle trails
        for(int t = 0; t < 20; t++) {
            int redPos = redParticle - t;
            int bluePos = blueParticle + t;
            
            if (redPos >= 0 && redPos < HardwareConfig::STRIP_LENGTH) {
                uint8_t trailBright = 255 - (t * 12);
                strip1[redPos] = CRGB(trailBright * intensity, 0, 0);
            }
            
            if (bluePos >= 0 && bluePos < HardwareConfig::STRIP_LENGTH) {
                uint8_t trailBright = 255 - (t * 12);
                strip2[bluePos] = CRGB(0, 0, trailBright * intensity);
            }
        }
        
        // Check for collision
        if (redParticle >= HardwareConfig::STRIP_CENTER_POINT - 5 &&
            blueParticle <= HardwareConfig::STRIP_CENTER_POINT + 5) {
            collision = true;
            debrisRadius = 0;
        }
    } else {
        // Collision creates new colors
        debrisRadius += speed * 8;
        
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
            
            if (distFromCenter <= debrisRadius) {
                // Random color debris
                uint8_t debrisHue = random8();
                uint8_t debrisBright = 255 * (1 - distFromCenter / debrisRadius) * intensity;
                
                if (random8(2) == 0) {
                    strip1[i] += CHSV(debrisHue, 255, debrisBright);
                } else {
                    strip2[i] += CHSV(debrisHue, 255, debrisBright);
                }
            }
        }
        
        // Reset when debris reaches edges
        if (debrisRadius > HardwareConfig::STRIP_HALF_LENGTH) {
            collision = false;
            redParticle = 0;
            blueParticle = HardwareConfig::STRIP_LENGTH - 1;
        }
    }
}

// ============== CHROMATIC DNA HELIX ==============
// Double helix with color base pairing
void lgpDNAHelix() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float helixRotation = 0;
    helixRotation += speed * 0.05f;
    
    // Helix parameters
    float helixPitch = 10 + complexity * 20;  // Distance per full rotation
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Calculate helix position
        float angle1 = (distFromCenter / helixPitch) * TWO_PI + helixRotation;
        float angle2 = angle1 + PI;  // Opposite strand
        
        // DNA base pairs - use palette offsets instead of discrete spectrum
        uint8_t paletteOffset1, paletteOffset2;
        if (TrigLookup::sinf_lookup(angle1 * 2) > 0) {
            paletteOffset1 = 0;   // Base pair type A
            paletteOffset2 = 15;  // Base pair type T
        } else {
            paletteOffset1 = 10;  // Base pair type G
            paletteOffset2 = 25;  // Base pair type C
        }

        // Helix structure
        float strand1Intensity = (TrigLookup::sinf_lookup(angle1) + 1) * 0.5f;
        float strand2Intensity = (TrigLookup::sinf_lookup(angle2) + 1) * 0.5f;

        // Base pair connections at specific points
        float connectionIntensity = 0;
        if (fmod(distFromCenter, helixPitch/4) < 2) {
            connectionIntensity = 1;
        }

        // Position-based palette indices (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex1 = (uint8_t)(distFromCenter * 2) + paletteOffset1;
        uint8_t paletteIndex2 = (uint8_t)(distFromCenter * 2) + paletteOffset2;

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
        color1.nscale8(255 * strand1Intensity * intensity);
        color2.nscale8(255 * strand2Intensity * intensity);

        // Draw strands
        strip1[i] = color1;
        strip2[i] = color2;

        // Add connections
        if (connectionIntensity > 0) {
            CRGB conn2 = ColorFromPalette(currentPalette, paletteIndex2, 255);
            CRGB conn1 = ColorFromPalette(currentPalette, paletteIndex1, 255);
            conn2.nscale8(255 * intensity);
            conn1.nscale8(255 * intensity);
            strip1[i] = blend(strip1[i], conn2, 128);
            strip2[i] = blend(strip2[i], conn1, 128);
        }
    }
}

// ============== COLOR PHASE TRANSITION ==============
// Colors undergo state changes like matter
void lgpPhaseTransition() {
    float temperature = paletteSpeed / 255.0f;  // "Temperature" parameter
    float intensity = visualParams.getIntensityNorm();
    float pressure = visualParams.getComplexityNorm();  // "Pressure" parameter
    
    static float phaseAnimation = 0;
    phaseAnimation += temperature * 0.1f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Determine phase based on temperature and pressure
        float localTemp = temperature + (normalizedDist * pressure);
        
        CRGB color;
        uint8_t paletteOffset;
        uint8_t brightness;

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t baseIndex = (uint8_t)(distFromCenter * 2);

        if (localTemp < 0.25f) {
            // Solid phase - crystalline structure
            float crystal = TrigLookup::sinf_lookup(distFromCenter * 10) * 0.5f + 0.5f;
            paletteOffset = 0 + crystal * 5;  // Small range for solid
            brightness = 255;
            color = ColorFromPalette(currentPalette, baseIndex + paletteOffset, 255);
            color.nscale8(brightness * intensity);
        } else if (localTemp < 0.5f) {
            // Liquid phase - flowing motion
            float flow = TrigLookup::sinf_lookup(distFromCenter * 0.5f + phaseAnimation);
            paletteOffset = 10 + flow * 5;  // Small range for liquid
            brightness = 200;
            color = ColorFromPalette(currentPalette, baseIndex + paletteOffset, 255);
            color.nscale8(brightness * intensity);
        } else if (localTemp < 0.75f) {
            // Gas phase - dispersed particles
            float dispersion = random8() / 255.0f;
            if (dispersion < 0.3f) {
                paletteOffset = 20;
                brightness = 150;
                color = ColorFromPalette(currentPalette, baseIndex + paletteOffset, 255);
                color.nscale8(brightness * intensity);
            } else {
                color = CRGB::Black;
                brightness = 0;
            }
        } else {
            // Plasma phase - ionized, energetic
            float plasma = TrigLookup::sinf_lookup(distFromCenter * 20 + phaseAnimation * 10);
            paletteOffset = 30 + plasma * 10;  // Reduced from 40
            brightness = 255;
            color = ColorFromPalette(currentPalette, baseIndex + paletteOffset, 255);
            color.nscale8(brightness * intensity);
        }

        // Different phases on each strip create phase boundary effects
        strip1[i] = color;
        CRGB color2 = ColorFromPalette(currentPalette, baseIndex + paletteOffset + 60, 255);
        color2.nscale8(brightness * intensity);
        strip2[i] = color2;
    }
}

// ============== HSV CYLINDER MIXING ==============
// Explore saturation/value space with palette colors (no rainbow)
void lgpHSVCylinder() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();

    static float cylinderRotation = 0;
    cylinderRotation += speed * 0.02f;

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(cylinderRotation * 10) + (uint8_t)(normalizedDist * complexity * 30) + (uint8_t)(distFromCenter * 2);

        // Strip 2: Travels through SATURATION (radius from center)
        uint8_t sat2 = 255 * (1 - normalizedDist);

        // Value (height) oscillates
        uint8_t val = 128 + 127 * TrigLookup::sinf_lookup(cylinderRotation + distFromCenter * 0.1f);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(val * intensity);
        color2.nscale8(sat2 * (val * intensity) / 255);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== PERCEPTUAL COLOR BLENDING ==============
// Uses perceptually uniform color space for natural mixing
void lgpPerceptualBlend() {
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    
    static float blendPhase = 0;
    blendPhase += speed * 0.01f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Define colors in a perceptually uniform way
        // Simplified LAB-like mixing
        float L = 50 + 50 * TrigLookup::sinf_lookup(blendPhase);  // Lightness
        float a = 50 * TrigLookup::cosf_lookup(blendPhase + normalizedDist * PI);  // Green-Red
        float b = 50 * TrigLookup::sinf_lookup(blendPhase - normalizedDist * PI);  // Blue-Yellow
        
        // Convert to RGB (simplified)
        CRGB color;
        color.r = constrain(L + a * 2, 0, 255) * intensity;
        color.g = constrain(L - a - b, 0, 255) * intensity;
        color.b = constrain(L + b * 2, 0, 255) * intensity;
        
        // Opposite mixing on strip2
        CRGB color2;
        color2.r = constrain(L - a * 2, 0, 255) * intensity;
        color2.g = constrain(L + a + b, 0, 255) * intensity;
        color2.b = constrain(L - b * 2, 0, 255) * intensity;
        
        strip1[i] = color;
        strip2[i] = color2;
    }
}

// ============== CHROMATIC ABERRATION ==============
// Different wavelengths refract at different angles
void lgpChromaticAberration() {
    float intensity = visualParams.getIntensityNorm();
    float aberration = visualParams.getComplexityNorm() * 3;
    
    static float lensPosition = 0;
    lensPosition += paletteSpeed * 0.01f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Lens equation with chromatic aberration
        // Optimized: Use FastLED sin16 for better performance
        float redPhase = ((normalizedDist - 0.1f * aberration) * PI + lensPosition);
        float greenPhase = (normalizedDist * PI + lensPosition);
        float bluePhase = ((normalizedDist + 0.1f * aberration) * PI + lensPosition);
        
        uint16_t redPhase16 = FastLEDOptim::radiansToPhase16(redPhase);
        uint16_t greenPhase16 = FastLEDOptim::radiansToPhase16(greenPhase);
        uint16_t bluePhase16 = FastLEDOptim::radiansToPhase16(bluePhase);
        
        int16_t redWave16 = FastLEDOptim::fastSin16(redPhase16);
        int16_t greenWave16 = FastLEDOptim::fastSin16(greenPhase16);
        int16_t blueWave16 = FastLEDOptim::fastSin16(bluePhase16);
        
        // Convert to 8-bit brightness (0-255)
        uint8_t redBrightness = constrain((redWave16 >> 8) + 128, 0, 255);
        uint8_t greenBrightness = constrain((greenWave16 >> 8) + 128, 0, 255);
        uint8_t blueBrightness = constrain((blueWave16 >> 8) + 128, 0, 255);
        
        uint8_t intensity8 = visualParams.intensity;
        
        // Create chromatic aberration effect
        CRGB aberratedColor;
        aberratedColor.r = FastLEDOptim::fastScale8(redBrightness, intensity8);
        aberratedColor.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        aberratedColor.b = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        
        strip1[i] = aberratedColor;
        
        // Opposite aberration on strip2
        CRGB aberratedColor2;
        aberratedColor2.r = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        aberratedColor2.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        aberratedColor2.b = FastLEDOptim::fastScale8(redBrightness, intensity8);
        
        strip2[i] = aberratedColor2;
    }
}

// ============== ADDITIVE VS SUBTRACTIVE MIXING ==============
// Demonstrates the difference between light and pigment mixing
void lgpAdditiveSubtractive() {
    float intensity = visualParams.getIntensityNorm();
    float mixZone = visualParams.getVariationNorm();
    
    static uint8_t color1 = 0;
    static uint8_t color2 = 120;
    color1 += paletteSpeed / 10;
    color2 += paletteSpeed / 10;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        CRGB finalColor;
        
        if (normalizedDist > mixZone) {
            // Edges: Pure colors
            if (i < HardwareConfig::STRIP_CENTER_POINT) {
                strip1[i] = CHSV(color1, 255, 255 * intensity);
                strip2[i] = CHSV(color2, 255, 255 * intensity);
            } else {
                strip1[i] = CHSV(color2, 255, 255 * intensity);
                strip2[i] = CHSV(color1, 255, 255 * intensity);
            }
        } else {
            // Center: Mixing zone
            float mixRatio = normalizedDist / mixZone;
            
            // Additive mixing (light) - gets brighter
            CRGB additive1 = CHSV(color1, 255, 128);
            CRGB additive2 = CHSV(color2, 255, 128);
            CRGB additiveMix = additive1 + additive2;
            
            // Subtractive mixing (pigment) - gets darker
            CRGB subtractive1 = CHSV(color1, 255, 255);
            CRGB subtractive2 = CHSV(color2, 255, 255);
            CRGB subtractiveMix;
            subtractiveMix.r = (subtractive1.r * subtractive2.r) / 255;
            subtractiveMix.g = (subtractive1.g * subtractive2.g) / 255;
            subtractiveMix.b = (subtractive1.b * subtractive2.b) / 255;
            
            // Transition between mixing modes
            strip1[i] = blend(additiveMix, subtractiveMix, mixRatio * 255).scale8(intensity * 255);
            strip2[i] = blend(subtractiveMix, additiveMix, mixRatio * 255).scale8(intensity * 255);
        }
    }
}

// ============== METAMERIC COLOR MATCHING ==============
// Different spectral distributions that appear as the same color
void lgpMetamericColors() {
    float intensity = visualParams.getIntensityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float spectralShift = 0;
    spectralShift += paletteSpeed * 0.001f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Target color from palette (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2) + (uint8_t)(spectralShift * 10);
        CRGB targetColor = ColorFromPalette(currentPalette, paletteIndex, 200);
        
        if (normalizedDist > 0.5f) {
            // Edges: Different spectral distributions
            
            // Strip1: Narrow band spectrum
            float narrow1 = TrigLookup::sinf_lookup(spectralShift * 10) * variation;
            float narrow2 = TrigLookup::cosf_lookup(spectralShift * 10) * variation;
            CRGB spectrum1;
            spectrum1.r = targetColor.r + 50 * narrow1;
            spectrum1.g = targetColor.g - 30 * narrow1;
            spectrum1.b = targetColor.b + 20 * narrow2;
            
            // Strip2: Broad band spectrum
            float broad1 = TrigLookup::sinf_lookup(spectralShift) * variation;
            float broad2 = TrigLookup::cosf_lookup(spectralShift) * variation;
            CRGB spectrum2;
            spectrum2.r = targetColor.r - 30 * broad1;
            spectrum2.g = targetColor.g + 40 * broad2;
            spectrum2.b = targetColor.b - 10 * broad1;
            
            strip1[i] = spectrum1.scale8(intensity * 255);
            strip2[i] = spectrum2.scale8(intensity * 255);
        } else {
            // Center: Colors converge to same appearance
            strip1[i] = targetColor.scale8(intensity * 255);
            strip2[i] = targetColor.scale8(intensity * 255);
        }
    }
}

// ============== CHROMATIC LENS (Static Aberration) ==============
// Static chromatic aberration effect simulating a lens with fixed dispersion
// Based on physics equations from LGP_OPTICAL_PHYSICS_REFERENCE.md
void lgpChromaticLens() {
    float intensity = visualParams.getIntensityNorm();
    float aberration = visualParams.getComplexityNorm() * 0.3f;  // 0.0-0.3 aberration amount
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Chromatic dispersion: different wavelengths focus at different positions
        // Using FastLED optimization utilities for performance
        float redPhase = (normalizedDist - 0.1f * aberration) * PI;
        float greenPhase = normalizedDist * PI;
        float bluePhase = (normalizedDist + 0.1f * aberration) * PI;
        
        uint16_t redPhase16 = FastLEDOptim::radiansToPhase16(redPhase);
        uint16_t greenPhase16 = FastLEDOptim::radiansToPhase16(greenPhase);
        uint16_t bluePhase16 = FastLEDOptim::radiansToPhase16(bluePhase);
        
        int16_t redWave16 = FastLEDOptim::fastSin16(redPhase16);
        int16_t greenWave16 = FastLEDOptim::fastSin16(greenPhase16);
        int16_t blueWave16 = FastLEDOptim::fastSin16(bluePhase16);
        
        // Convert to RGB brightness (0-255)
        uint8_t redBrightness = constrain((redWave16 >> 8) + 128, 0, 255);
        uint8_t greenBrightness = constrain((greenWave16 >> 8) + 128, 0, 255);
        uint8_t blueBrightness = constrain((blueWave16 >> 8) + 128, 0, 255);
        
        uint8_t intensity8 = visualParams.intensity;
        
        // Apply chromatic separation
        CRGB color1;
        color1.r = FastLEDOptim::fastScale8(redBrightness, intensity8);
        color1.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        color1.b = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        
        // Strip 2: Complementary aberration (opposite direction)
        CRGB color2;
        color2.r = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        color2.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        color2.b = FastLEDOptim::fastScale8(redBrightness, intensity8);
        
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== CHROMATIC PULSE (Aberration Sweeps from Centre) ==============
// Dynamic chromatic aberration that pulses outward from centre
// Creates a "breathing" lens effect
void lgpChromaticPulse() {
    float intensity = visualParams.getIntensityNorm();
    float speed = paletteSpeed / 255.0f;
    float aberration = visualParams.getComplexityNorm() * 0.3f;
    
    static float pulsePhase = 0;
    pulsePhase += speed * 0.02f;
    
    // Pulse amplitude: 0.0-1.0
    float pulseAmplitude = (FastLEDOptim::fastSin8((uint8_t)(pulsePhase * 255)) / 255.0f) * 0.5f + 0.5f;
    float currentAberration = aberration * pulseAmplitude;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Chromatic dispersion with pulsing aberration
        float redPhase = (normalizedDist - 0.1f * currentAberration) * PI + pulsePhase;
        float greenPhase = normalizedDist * PI + pulsePhase;
        float bluePhase = (normalizedDist + 0.1f * currentAberration) * PI + pulsePhase;
        
        uint16_t redPhase16 = FastLEDOptim::radiansToPhase16(redPhase);
        uint16_t greenPhase16 = FastLEDOptim::radiansToPhase16(greenPhase);
        uint16_t bluePhase16 = FastLEDOptim::radiansToPhase16(bluePhase);
        
        int16_t redWave16 = FastLEDOptim::fastSin16(redPhase16);
        int16_t greenWave16 = FastLEDOptim::fastSin16(greenPhase16);
        int16_t blueWave16 = FastLEDOptim::fastSin16(bluePhase16);
        
        uint8_t redBrightness = constrain((redWave16 >> 8) + 128, 0, 255);
        uint8_t greenBrightness = constrain((greenWave16 >> 8) + 128, 0, 255);
        uint8_t blueBrightness = constrain((blueWave16 >> 8) + 128, 0, 255);
        
        uint8_t intensity8 = visualParams.intensity;
        
        // Create pulsing chromatic effect
        CRGB color1;
        color1.r = FastLEDOptim::fastScale8(redBrightness, intensity8);
        color1.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        color1.b = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        
        // Strip 2: Phase-shifted pulse
        CRGB color2;
        float phase2 = pulsePhase + PI;
        uint16_t redPhase16_2 = FastLEDOptim::radiansToPhase16((normalizedDist - 0.1f * currentAberration) * PI + phase2);
        uint16_t greenPhase16_2 = FastLEDOptim::radiansToPhase16(normalizedDist * PI + phase2);
        uint16_t bluePhase16_2 = FastLEDOptim::radiansToPhase16((normalizedDist + 0.1f * currentAberration) * PI + phase2);
        
        int16_t redWave16_2 = FastLEDOptim::fastSin16(redPhase16_2);
        int16_t greenWave16_2 = FastLEDOptim::fastSin16(greenPhase16_2);
        int16_t blueWave16_2 = FastLEDOptim::fastSin16(bluePhase16_2);
        
        uint8_t redBrightness2 = constrain((redWave16_2 >> 8) + 128, 0, 255);
        uint8_t greenBrightness2 = constrain((greenWave16_2 >> 8) + 128, 0, 255);
        uint8_t blueBrightness2 = constrain((blueWave16_2 >> 8) + 128, 0, 255);
        
        color2.r = FastLEDOptim::fastScale8(redBrightness2, intensity8);
        color2.g = FastLEDOptim::fastScale8(greenBrightness2, intensity8);
        color2.b = FastLEDOptim::fastScale8(blueBrightness2, intensity8);
        
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== CHROMATIC INTERFERENCE (Dual-Edge with Dispersion) ==============
// Combines dual-edge interference with chromatic dispersion
// Creates complex interference patterns with wavelength-dependent effects
void lgpChromaticInterference() {
    float intensity = visualParams.getIntensityNorm();
    float speed = paletteSpeed / 255.0f;
    float aberration = visualParams.getComplexityNorm() * 0.3f;
    float variation = visualParams.getVariationNorm();
    
    static float phase1 = 0, phase2 = 0;
    phase1 += speed * 0.01f;
    phase2 += speed * 0.015f;  // Slightly different speed for interference
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Edge 1 contribution (left edge)
        float edge1Dist = (float)i / HardwareConfig::STRIP_LENGTH;
        float edge1RedPhase = (edge1Dist - 0.1f * aberration) * TWO_PI + phase1;
        float edge1GreenPhase = edge1Dist * TWO_PI + phase1;
        float edge1BluePhase = (edge1Dist + 0.1f * aberration) * TWO_PI + phase1;
        
        // Edge 2 contribution (right edge)
        float edge2Dist = 1.0f - edge1Dist;
        float edge2RedPhase = (edge2Dist - 0.1f * aberration) * TWO_PI + phase2;
        float edge2GreenPhase = edge2Dist * TWO_PI + phase2;
        float edge2BluePhase = (edge2Dist + 0.1f * aberration) * TWO_PI + phase2;
        
        // Calculate interference for each wavelength
        uint16_t edge1RedPhase16 = FastLEDOptim::radiansToPhase16(edge1RedPhase);
        uint16_t edge1GreenPhase16 = FastLEDOptim::radiansToPhase16(edge1GreenPhase);
        uint16_t edge1BluePhase16 = FastLEDOptim::radiansToPhase16(edge1BluePhase);
        
        uint16_t edge2RedPhase16 = FastLEDOptim::radiansToPhase16(edge2RedPhase);
        uint16_t edge2GreenPhase16 = FastLEDOptim::radiansToPhase16(edge2GreenPhase);
        uint16_t edge2BluePhase16 = FastLEDOptim::radiansToPhase16(edge2BluePhase);
        
        // Interference calculation: I = I1 + I2 + 2√(I1×I2) × cos(Δφ)
        int16_t edge1RedWave = FastLEDOptim::fastSin16(edge1RedPhase16);
        int16_t edge1GreenWave = FastLEDOptim::fastSin16(edge1GreenPhase16);
        int16_t edge1BlueWave = FastLEDOptim::fastSin16(edge1BluePhase16);
        
        int16_t edge2RedWave = FastLEDOptim::fastSin16(edge2RedPhase16);
        int16_t edge2GreenWave = FastLEDOptim::fastSin16(edge2GreenPhase16);
        int16_t edge2BlueWave = FastLEDOptim::fastSin16(edge2BluePhase16);
        
        // Convert to amplitude (0-255)
        uint8_t edge1RedAmp = (edge1RedWave >> 8) + 128;
        uint8_t edge1GreenAmp = (edge1GreenWave >> 8) + 128;
        uint8_t edge1BlueAmp = (edge1BlueWave >> 8) + 128;
        
        uint8_t edge2RedAmp = (edge2RedWave >> 8) + 128;
        uint8_t edge2GreenAmp = (edge2GreenWave >> 8) + 128;
        uint8_t edge2BlueAmp = (edge2BlueWave >> 8) + 128;
        
        // Calculate phase difference for interference
        uint16_t redPhaseDiff = abs((int16_t)edge1RedPhase16 - (int16_t)edge2RedPhase16);
        uint16_t greenPhaseDiff = abs((int16_t)edge1GreenPhase16 - (int16_t)edge2GreenPhase16);
        uint16_t bluePhaseDiff = abs((int16_t)edge1BluePhase16 - (int16_t)edge2BluePhase16);
        
        // Interference term: cos(phase_diff)
        int16_t redInterference = FastLEDOptim::fastCos16(redPhaseDiff);
        int16_t greenInterference = FastLEDOptim::fastCos16(greenPhaseDiff);
        int16_t blueInterference = FastLEDOptim::fastCos16(bluePhaseDiff);
        
        // Simplified interference: I = I1 + I2 + interference_term
        uint8_t redBrightness = FastLEDOptim::fastQAdd8(
            FastLEDOptim::fastQAdd8(edge1RedAmp, edge2RedAmp),
            FastLEDOptim::fastScale8((redInterference >> 8) + 128, 128)
        );
        uint8_t greenBrightness = FastLEDOptim::fastQAdd8(
            FastLEDOptim::fastQAdd8(edge1GreenAmp, edge2GreenAmp),
            FastLEDOptim::fastScale8((greenInterference >> 8) + 128, 128)
        );
        uint8_t blueBrightness = FastLEDOptim::fastQAdd8(
            FastLEDOptim::fastQAdd8(edge1BlueAmp, edge2BlueAmp),
            FastLEDOptim::fastScale8((blueInterference >> 8) + 128, 128)
        );
        
        uint8_t intensity8 = visualParams.intensity;
        
        CRGB color1;
        color1.r = FastLEDOptim::fastScale8(redBrightness, intensity8);
        color1.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        color1.b = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        
        // Strip 2: Phase-shifted interference
        CRGB color2;
        color2.r = FastLEDOptim::fastScale8(blueBrightness, intensity8);
        color2.g = FastLEDOptim::fastScale8(greenBrightness, intensity8);
        color2.b = FastLEDOptim::fastScale8(redBrightness, intensity8);
        
        strip1[i] = color1;
        strip2[i] = color2;
    }
}