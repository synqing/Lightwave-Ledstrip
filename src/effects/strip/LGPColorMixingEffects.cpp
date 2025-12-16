// LGP Advanced Color Mixing Effects
// Exploiting opposing light channels for unprecedented color phenomena

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
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
            strip1[i] = warm.scale8(intensity * 255);
            
            // Cool side (blues/cyans)
            cool.r = 150 + (normalizedDist * 50);
            cool.g = 200 + (normalizedDist * 55);
            cool.b = 255;
            strip2[i] = cool.scale8(intensity * 255);
        } else {
            // Mirror for other half
            warm.r = 255;
            warm.g = 180 - (normalizedDist * 100);
            warm.b = 50 + (normalizedDist * 50);
            strip1[i] = warm.scale8(intensity * 255);
            
            cool.r = 150 + (normalizedDist * 50);
            cool.g = 200 + (normalizedDist * 55);
            cool.b = 255;
            strip2[i] = cool.scale8(intensity * 255);
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
        float redAngle = sin(normalizedDist * dispersion + prismAngle);
        float greenAngle = sin(normalizedDist * dispersion * 1.1f + prismAngle);
        float blueAngle = sin(normalizedDist * dispersion * 1.2f + prismAngle);
        
        // Strip 1: Red channel dominant
        strip1[i].r = 128 + 127 * redAngle * intensity;
        strip1[i].g = 64 * abs(greenAngle) * intensity;
        strip1[i].b = 0;
        
        // Strip 2: Blue channel dominant
        strip2[i].r = 0;
        strip2[i].g = 64 * abs(greenAngle) * intensity;
        strip2[i].b = 128 + 127 * blueAngle * intensity;
        
        // Green emerges at intersection
        if (distFromCenter < 10) {
            strip1[i].g += 128 * intensity;
            strip2[i].g += 128 * intensity;
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
        
        // Base hue rotates over time
        uint8_t baseHue = gHue + (colorPhase * 255);
        uint8_t complementHue = baseHue + 128;  // Exact opposite
        
        // Intensity falls off from edges
        uint8_t edgeIntensity = 255 * (1 - normalizedDist * variation);
        
        // Mix complementary colors
        if (normalizedDist > 0.5f) {
            // Strong colors at edges
            strip1[i] = CHSV(baseHue, 255, edgeIntensity * intensity);
            strip2[i] = CHSV(complementHue, 255, edgeIntensity * intensity);
        } else {
            // Mixing zone - desaturate toward center
            uint8_t saturation = 255 * (normalizedDist * 2);
            strip1[i] = CHSV(baseHue, saturation, 128 * intensity);
            strip2[i] = CHSV(complementHue, saturation, 128 * intensity);
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
        float probability = sin(waveFunction + normalizedDist * TWO_PI * numStates);
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
        uint8_t uncertainty = 255 * (0.5f + 0.5f * sin(distFromCenter * 20));

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteOffset, uncertainty * intensity);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteOffset + 128, (255 - uncertainty) * intensity);
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
        
        // Apply frequency shift to hue
        uint8_t shiftedHue = gHue;
        if (dopplerFactor > 1.0f) {
            shiftedHue = gHue - (30 * (dopplerFactor - 1.0f));  // Blue shift
        } else {
            shiftedHue = gHue + (30 * (1.0f - dopplerFactor));  // Red shift
        }
        
        uint8_t brightness = 255 * intensity * (1 - distFromCenter / HardwareConfig::STRIP_HALF_LENGTH);
        
        strip1[i] = CHSV(shiftedHue, 255, brightness);
        strip2[i] = CHSV(shiftedHue + 90, 255, brightness);
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
        if (sin(angle1 * 2) > 0) {
            paletteOffset1 = 0;   // Base pair type A
            paletteOffset2 = 15;  // Base pair type T
        } else {
            paletteOffset1 = 10;  // Base pair type G
            paletteOffset2 = 25;  // Base pair type C
        }

        // Helix structure
        float strand1Intensity = (sin(angle1) + 1) * 0.5f;
        float strand2Intensity = (sin(angle2) + 1) * 0.5f;

        // Base pair connections at specific points
        float connectionIntensity = 0;
        if (fmod(distFromCenter, helixPitch/4) < 2) {
            connectionIntensity = 1;
        }

        uint8_t brightness = 255 * intensity;

        // Draw strands
        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteOffset1, brightness * strand1Intensity);
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteOffset2, brightness * strand2Intensity);

        // Add connections
        if (connectionIntensity > 0) {
            CRGB conn2 = ColorFromPalette(currentPalette, gHue + paletteOffset2, brightness);
            CRGB conn1 = ColorFromPalette(currentPalette, gHue + paletteOffset1, brightness);
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

        if (localTemp < 0.25f) {
            // Solid phase - crystalline structure
            float crystal = sin(distFromCenter * 10) * 0.5f + 0.5f;
            paletteOffset = 0 + crystal * 5;  // Small range for solid
            brightness = 255 * intensity;
            color = ColorFromPalette(currentPalette, gHue + paletteOffset, brightness);
        } else if (localTemp < 0.5f) {
            // Liquid phase - flowing motion
            float flow = sin(distFromCenter * 0.5f + phaseAnimation);
            paletteOffset = 10 + flow * 5;  // Small range for liquid
            brightness = 200 * intensity;
            color = ColorFromPalette(currentPalette, gHue + paletteOffset, brightness);
        } else if (localTemp < 0.75f) {
            // Gas phase - dispersed particles
            float dispersion = random8() / 255.0f;
            if (dispersion < 0.3f) {
                paletteOffset = 20;
                brightness = 150 * intensity;
                color = ColorFromPalette(currentPalette, gHue + paletteOffset, brightness);
            } else {
                color = CRGB::Black;
            }
        } else {
            // Plasma phase - ionized, energetic
            float plasma = sin(distFromCenter * 20 + phaseAnimation * 10);
            paletteOffset = 30 + plasma * 10;  // Reduced from 40
            brightness = 255 * intensity;
            color = ColorFromPalette(currentPalette, gHue + paletteOffset, brightness);
        }

        // Different phases on each strip create phase boundary effects
        strip1[i] = color;
        strip2[i] = ColorFromPalette(currentPalette, gHue + paletteOffset + 60, brightness);
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

        // Use palette with small offset instead of full hue rotation
        uint8_t paletteIndex = (cylinderRotation * 10) + (normalizedDist * complexity * 30);

        // Strip 2: Travels through SATURATION (radius from center)
        uint8_t sat2 = 255 * (1 - normalizedDist);

        // Value (height) oscillates
        uint8_t val = 128 + 127 * sin(cylinderRotation + distFromCenter * 0.1f);

        strip1[i] = ColorFromPalette(currentPalette, gHue + paletteIndex, val * intensity);
        strip2[i] = ColorFromPalette(currentPalette, gHue + 128, sat2 * (val * intensity) / 255);
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
        float L = 50 + 50 * sin(blendPhase);  // Lightness
        float a = 50 * cos(blendPhase + normalizedDist * PI);  // Green-Red
        float b = 50 * sin(blendPhase - normalizedDist * PI);  // Blue-Yellow
        
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
        float redFocus = sin((normalizedDist - 0.1f * aberration) * PI + lensPosition);
        float greenFocus = sin(normalizedDist * PI + lensPosition);
        float blueFocus = sin((normalizedDist + 0.1f * aberration) * PI + lensPosition);
        
        // Create rainbow halos at boundaries
        CRGB aberratedColor;
        aberratedColor.r = constrain(128 + 127 * redFocus, 0, 255) * intensity;
        aberratedColor.g = constrain(128 + 127 * greenFocus, 0, 255) * intensity;
        aberratedColor.b = constrain(128 + 127 * blueFocus, 0, 255) * intensity;
        
        strip1[i] = aberratedColor;
        
        // Opposite aberration on strip2
        CRGB aberratedColor2;
        aberratedColor2.r = constrain(128 + 127 * blueFocus, 0, 255) * intensity;
        aberratedColor2.g = constrain(128 + 127 * greenFocus, 0, 255) * intensity;
        aberratedColor2.b = constrain(128 + 127 * redFocus, 0, 255) * intensity;
        
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
        
        // Target color (what we want to match)
        CRGB targetColor = CHSV(gHue, 200, 200);
        
        if (normalizedDist > 0.5f) {
            // Edges: Different spectral distributions
            
            // Strip1: Narrow band spectrum
            float narrow1 = sin(spectralShift * 10) * variation;
            float narrow2 = cos(spectralShift * 10) * variation;
            CRGB spectrum1;
            spectrum1.r = targetColor.r + 50 * narrow1;
            spectrum1.g = targetColor.g - 30 * narrow1;
            spectrum1.b = targetColor.b + 20 * narrow2;
            
            // Strip2: Broad band spectrum  
            float broad1 = sin(spectralShift) * variation;
            float broad2 = cos(spectralShift) * variation;
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