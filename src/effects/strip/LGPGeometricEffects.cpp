// LGP Geometric Pattern Effects
// Advanced shapes and patterns leveraging Light Guide Plate optics
// Based on optical waveguide theory and interference phenomena

#include <FastLED.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "../../utils/TrigLookup.h"

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== LGP DIAMOND LATTICE ==============
// Creates diamond/rhombus patterns through angular interference
void lgpDiamondLattice() {
    // Theory: Angled wave fronts create diamond patterns when they intersect
    // Similar to X-ray crystallography patterns
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float phase = 0;
    phase += speed * 0.02f;
    
    // Diamond size based on complexity
    float diamondFreq = 2 + (complexity * 8);  // 2-10 diamonds
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Use distance from center, not linear position
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Create crossing diagonal waves from center
        float wave1 = TrigLookup::sinf_lookup((normalizedDist + phase) * diamondFreq * TWO_PI);
        float wave2 = TrigLookup::sinf_lookup((normalizedDist - phase) * diamondFreq * TWO_PI);

        // Interference creates diamond nodes
        float diamond = abs(wave1 * wave2);

        // Edge sharpening
        diamond = pow(diamond, 0.5f);  // Sharpen peaks

        uint8_t brightness = diamond * 255 * intensity;

        // Use palette instead of rainbow - map distance to palette index
        uint8_t paletteIndex = distFromCenter * 2;

        // Get color at FULL brightness first, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP HEXAGONAL GRID ==============
// Creates honeycomb-like patterns using 3-wave interference
void lgpHexagonalGrid() {
    // Theory: Three waves at 120° create hexagonal interference patterns
    // Like acoustic cymatics patterns
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float phase = 0;
    phase += speed * 0.01f;
    
    // Hexagon size
    float hexSize = 3 + (complexity * 12);  // 3-15 hexagons
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float pos = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Three waves at 120 degree angles
        float wave1 = TrigLookup::sinf_lookup(pos * hexSize * TWO_PI + phase);
        float wave2 = TrigLookup::sinf_lookup(pos * hexSize * TWO_PI + phase + TWO_PI/3);
        float wave3 = TrigLookup::sinf_lookup(pos * hexSize * TWO_PI + phase + 2*TWO_PI/3);
        
        float pattern;
        if (variation < 0.5f) {
            // Additive - creates nodes
            pattern = (wave1 + wave2 + wave3) / 3;
            pattern = abs(pattern);
        } else {
            // Multiplicative - creates cells
            pattern = abs(wave1 * wave2 * wave3);
            pattern = pow(pattern, 0.3f);  // Adjust contrast
        }
        
        uint8_t brightness = pattern * 255 * intensity;

        // Position-based palette index for iridescence (no gHue - rainbow forbidden)
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        uint8_t paletteIndex = (uint8_t)(pattern * 60) + (uint8_t)(distFromCenter * 2);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 40, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP SPIRAL VORTEX ==============
// Creates rotating spiral patterns using phase-shifted waves
void lgpSpiralVortex() {
    // Theory: Helical phase fronts create spiral interference
    // Like optical vortex beams
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float vortexPhase = 0;
    vortexPhase += speed * 0.05f;
    
    // Number of spiral arms
    int spiralArms = 2 + (complexity * 6);  // 2-8 arms
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Spiral equation: r * theta
        float spiralAngle = normalizedDist * spiralArms * TWO_PI + vortexPhase;
        
        // Create spiral with different profiles
        float spiral;
        if (variation < 0.33f) {
            // Archimedean spiral
            spiral = TrigLookup::sinf_lookup(spiralAngle);
        } else if (variation < 0.66f) {
            // Logarithmic spiral
            spiral = TrigLookup::sinf_lookup(spiralAngle * (1 + normalizedDist));
        } else {
            // Fermat's spiral
            spiral = TrigLookup::sinf_lookup(spiralAngle * sqrt(normalizedDist + 0.1f));
        }
        
        // Radial fade
        spiral *= (1 - normalizedDist * 0.5f);
        
        uint8_t brightness = 128 + (127 * spiral * intensity);

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(spiralAngle * 255 / TWO_PI) + (uint8_t)(distFromCenter * 2);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP SIERPINSKI TRIANGLES ==============
// Creates fractal triangle patterns through recursive interference
void lgpSierpinskiTriangles() {
    // Theory: Self-similar interference at multiple scales
    // Creates fractal-like patterns
    
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static uint16_t iteration = 0;
    iteration += paletteSpeed >> 2;
    
    // Fractal depth
    int maxDepth = 3 + (complexity * 4);  // 3-7 levels
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float pos = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Binary representation determines Sierpinski pattern
        uint16_t x = i;
        uint16_t y = iteration >> 4;
        
        // XOR creates Sierpinski triangle
        uint16_t pattern = x ^ y;
        
        // Count bits for fractal depth
        uint8_t bitCount = 0;
        for(int d = 0; d < maxDepth; d++) {
            if (pattern & (1 << d)) bitCount++;
        }
        
        // Create smooth transitions
        float smooth = TrigLookup::sinf_lookup(bitCount * PI / maxDepth);

        uint8_t brightness = smooth * 255 * intensity;

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        uint8_t paletteIndex = (uint8_t)(bitCount * 30) + (uint8_t)(distFromCenter * 2);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP CHEVRON WAVES ==============
// Creates V-shaped patterns moving through the light guide
void lgpChevronWaves() {
    // Theory: Counter-propagating waves create chevron patterns
    // Like wake patterns in water
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float wavePos = 0;
    wavePos += speed * 2;
    
    // Chevron angle and count
    float chevronCount = 2 + (complexity * 8);  // 2-10 chevrons
    float chevronAngle = 0.5f + (variation * 2);  // Angle steepness
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 40);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 40);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Create V-shape from center
        float chevronPhase = distFromCenter * chevronAngle + wavePos;
        float chevron = TrigLookup::sinf_lookup(chevronPhase * chevronCount * 0.1f);
        
        // Sharp edges
        chevron = tanh(chevron * 3) * 0.5f + 0.5f;
        
        uint8_t brightness = chevron * 255 * intensity;

        // Position-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2) + (uint8_t)(wavePos * 0.5f);

        // Get colors at full brightness, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 64, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] += color1;
        strip2[i] += color2;
    }
}

// ============== LGP CIRCULAR RINGS ==============
// Creates concentric ring patterns through radial waves
void lgpConcentricRings() {
    // Theory: Radial standing waves create ring patterns
    // Like Bessel functions in cylindrical waveguides
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float ringPhase = 0;
    ringPhase += speed * 0.1f;
    
    // Ring density
    float ringCount = 3 + (complexity * 12);  // 3-15 rings
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        float rings;
        if (variation < 0.33f) {
            // Simple concentric rings
            rings = TrigLookup::sinf_lookup(distFromCenter * ringCount * 0.2f + ringPhase);
        } else if (variation < 0.66f) {
            // Bessel function-like (more realistic)
            float bessel = TrigLookup::sinf_lookup(distFromCenter * ringCount * 0.2f + ringPhase);
            bessel *= 1.0f / sqrt(normalizedDist + 0.1f);  // J0 approximation
            rings = bessel;
        } else {
            // Fresnel zones
            float fresnel = TrigLookup::sinf_lookup(sqrt(distFromCenter) * ringCount + ringPhase);
            rings = fresnel;
        }
        
        // Sharp ring edges
        rings = tanh(rings * 2);

        uint8_t brightness = 128 + (127 * rings * intensity);

        // Get color at FULL brightness first, then scale - preserves saturation
        // Use distance-based palette index for color variation (not gHue rainbow)
        uint8_t paletteIndex = distFromCenter * 3;
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP STAR BURST ==============
// Creates star-like patterns radiating from center
void lgpStarBurst() {
    // Theory: Multiple radial waves with angular modulation
    // Creates star-like interference patterns
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float starPhase = 0;
    starPhase += speed * 0.03f;
    
    // Number of star points
    int starPoints = 3 + (complexity * 9);  // 3-12 points
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Star equation - radially symmetric from center
        // Rotating star pattern using phase only (no positional angle)
        float star = TrigLookup::sinf_lookup(distFromCenter * 0.3f + starPhase) *
                    exp(-normalizedDist * 2);  // Radial decay

        // Pulsing
        star *= 0.5f + 0.5f * TrigLookup::sinf_lookup(starPhase * 3);

        uint8_t brightness = 128 + (127 * star * intensity);

        // Color varies with distance - symmetric from center (no gHue rainbow)
        uint8_t paletteIndex = distFromCenter + (star * 50);

        // Get color at FULL brightness first, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 85, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] += color1;
        strip2[i] += color2;
    }
}

// ============== LGP MESH NETWORK ==============
// Creates interconnected node patterns like neural networks
void lgpMeshNetwork() {
    // Theory: Discrete nodes with connecting waves
    // Simulates network topology visualization
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float complexity = visualParams.getComplexityNorm();
    
    static float networkPhase = 0;
    networkPhase += speed * 0.02f;
    
    // Node density
    int nodeCount = 5 + (complexity * 15);  // 5-20 nodes
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 50);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 50);
    
    // Place nodes
    for(int n = 0; n < nodeCount; n++) {
        float nodePos = (float)n / nodeCount * HardwareConfig::STRIP_LENGTH;

        // Node-based palette index (no gHue - rainbow cycling forbidden)
        uint8_t nodePaletteIndex = (uint8_t)(n * 20);

        // Draw node
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            float distToNode = abs(i - nodePos);
            float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);

            // Add position variance to palette index
            uint8_t paletteIndex = nodePaletteIndex + (uint8_t)(distFromCenter * 0.5f);

            if (distToNode < 3) {
                // Node core - get at full brightness, then scale
                uint8_t nodeBright = 255 * intensity;
                CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
                CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
                color1.nscale8(nodeBright);
                color2.nscale8(nodeBright);
                strip1[i] = color1;
                strip2[i] = color2;
            } else if (distToNode < 20) {
                // Connections to nearby nodes
                float connection = TrigLookup::sinf_lookup(distToNode * 0.5f + networkPhase + n);
                connection *= exp(-distToNode * 0.1f);  // Decay

                uint8_t connBright = abs(connection) * 128 * intensity;
                CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
                CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
                color1.nscale8(connBright);
                color2.nscale8(connBright);
                strip1[i] += color1;
                strip2[i] += color2;
            }
        }
    }
}