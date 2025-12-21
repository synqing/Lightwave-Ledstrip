// LGP Advanced Pattern Effects
// Implementation of advanced optical phenomena for Light Guide Plate displays
// Based on wave optics and interference theory

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

// Helper functions
// Note: scale16 is already defined in FastLED library

// ============== MOIRÉ CURTAINS ==============
// Two slightly mismatched spatial frequencies create slow-moving beat patterns
void lgpMoireCurtains() {
    static uint16_t phase = 0;
    
    // Base frequency + delta controlled by variation
    float baseFreq = 4.0f + visualParams.getComplexityNorm() * 8.0f;  // 4-12 cycles
    float delta = visualParams.getVariationNorm() * 0.5f;  // 0-0.5 frequency difference
    
    float leftFreq = baseFreq + delta / 2;
    float rightFreq = baseFreq - delta / 2;
    
    // Phase advance based on speed
    phase += paletteSpeed;
    
    // Render strips with different frequencies - CENTER ORIGIN
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);

        // Use distance for palette index (no gHue rainbow)
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2);

        // Left strip
        uint16_t leftPhase = (uint16_t)(sin16(distFromCenter * leftFreq * 410 + phase) + 32768) >> 8;
        uint8_t leftBright = scale8(leftPhase, visualParams.intensity);
        // Get color at FULL brightness first, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        color1.nscale8(leftBright);
        strip1[i] = color1;

        // Right strip - slightly different frequency
        uint16_t rightPhase = (uint16_t)(sin16(distFromCenter * rightFreq * 410 + phase) + 32768) >> 8;
        uint8_t rightBright = scale8(rightPhase, visualParams.intensity);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color2.nscale8(rightBright);
        strip2[i] = color2;
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== RADIAL RIPPLE ==============
// Concentric rings that appear to expand from virtual center
void lgpRadialRipple() {
    static uint16_t time = 0;
    
    // Ring parameters
    uint8_t ringCount = 2 + (visualParams.complexity >> 5);  // 2-10 rings
    uint16_t ringSpeed = paletteSpeed << 2;
    uint8_t ringSharpness = 255 - visualParams.getIntensityNorm() * 127;  // Duty cycle
    
    time += ringSpeed;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Calculate virtual radial distance from center
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT) / HardwareConfig::STRIP_CENTER_POINT;
        
        // Square the distance for circular appearance
        uint16_t distSquared = (uint16_t)(distFromCenter * distFromCenter * 65535);
        
        // Create expanding rings using sin16
        int16_t wave = sin16((distSquared >> 1) * ringCount - time);
        
        // Square wave thresholding for sharp rings
        uint8_t brightness = (wave > (int16_t)ringSharpness - 32768) ? 255 : 0;
        brightness = scale8(brightness, visualParams.intensity);

        // Use distance-based palette index (no gHue rainbow)
        uint8_t paletteIndex = distFromCenter * 255;

        // Get color at FULL brightness first, then scale - preserves saturation
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 64, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== HOLOGRAPHIC VORTEX ==============
// Spiral interference pattern with depth illusion
void lgpHolographicVortex() {
    static uint16_t time = 0;
    time += paletteSpeed << 1;
    
    uint8_t spiralCount = 1 + (visualParams.complexity >> 6);  // 1-4 spirals
    uint8_t tightness = visualParams.intensity >> 2;  // Radial chirp factor
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Distance from center for radial coordinate
        float distFromCenter = abs((float)i - HardwareConfig::STRIP_CENTER_POINT);
        float r = distFromCenter / HardwareConfig::STRIP_CENTER_POINT;

        // Symmetric azimuthal angle: same angle magnitude on both sides
        uint16_t theta = distFromCenter * 410;

        // Spiral phase: k*theta + m*r - omega*t
        uint16_t phase = spiralCount * theta + (uint16_t)(tightness * r * 65535) - time;

        // Create vortex pattern
        uint8_t brightness = sin8(phase >> 8);
        uint8_t paletteIndex = (phase >> 10);

        // Add depth via brightness modulation (reduce triple scaling to single)
        brightness = scale8(brightness, 255 - (uint8_t)(r * 64));  // Gentler radial decay
        brightness = max(brightness, (uint8_t)64);  // Floor to prevent total darkness

        // Get color at FULL brightness first, then scale - preserves saturation
        // Use paletteIndex only (no gHue rainbow)
        CRGB color1 = ColorFromPalette(currentPalette, paletteIndex, 255);
        CRGB color2 = ColorFromPalette(currentPalette, paletteIndex + 128, 255);
        color1.nscale8(brightness);
        color2.nscale8(brightness);
        strip1[i] = color1;
        strip2[i] = color2;
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== EVANESCENT DRIFT ==============
// Exponentially fading waves from edges - subtle ambient effect
void lgpEvanescentDrift() {
    static uint16_t phase1 = 0;
    static uint16_t phase2 = 32768;  // Anti-phase
    
    // Phase speeds
    phase1 += paletteSpeed;
    phase2 -= paletteSpeed;
    
    // Decay constant (alpha)
    uint8_t alpha = 255 - visualParams.intensity;  // Higher intensity = less decay
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Distance from nearest edge
        uint8_t distFromLeftEdge = i;
        uint8_t distFromRightEdge = HardwareConfig::STRIP_LENGTH - 1 - i;
        uint8_t distFromEdge = min(distFromLeftEdge, distFromRightEdge);
        
        // Exponential decay approximation using FastLED scale8
        uint8_t decay = 255;
        for (uint8_t j = 0; j < distFromEdge && j < 8; j++) {
            decay = scale8(decay, alpha);
        }
        
        // Wave patterns
        uint8_t wave1 = sin8((i << 2) + (phase1 >> 8));
        uint8_t wave2 = sin8((i << 2) + (phase2 >> 8));
        
        // Apply decay
        wave1 = scale8(wave1, decay);
        wave2 = scale8(wave2, decay);
        
        // Color mapping
        strip1[i] = ColorFromPalette(currentPalette, gHue + i, wave1);
        strip2[i] = ColorFromPalette(currentPalette, gHue + i + 85, wave2);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== CHROMATIC SHEAR ==============
// Color planes sliding past each other with velocity shear
void lgpChromaticShear() {
    static uint16_t shearPhase = 0;
    static uint8_t paletteOffset = 0;
    
    // Shear velocity
    shearPhase += paletteSpeed;
    
    // Palette rotation speed
    EVERY_N_MILLISECONDS(50) {
        paletteOffset += visualParams.getVariationNorm() * 10;
    }
    
    // Shear amount based on complexity
    uint8_t shearAmount = visualParams.complexity;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Use distance from center instead of linear position
        float distFromCenter = abs((int)i - HardwareConfig::STRIP_CENTER_POINT);
        uint8_t distPos = (distFromCenter * 255) / HardwareConfig::STRIP_HALF_LENGTH;

        // Apply shear transformation
        uint16_t shearOffset = ((uint16_t)distPos * shearAmount) >> 3;

        // Left strip: base hue + shear
        uint8_t leftHue = paletteOffset + distPos + (shearPhase >> 8);
        uint8_t leftBright = visualParams.intensity;

        // Right strip: complementary hue + inverse shear
        uint8_t rightHue = paletteOffset + distPos + 120 - (shearPhase >> 8);
        uint8_t rightBright = visualParams.intensity;
        
        // Add interference at center
        if (abs(i - HardwareConfig::STRIP_CENTER_POINT) < 20) {
            uint8_t centerBlend = 255 - abs(i - HardwareConfig::STRIP_CENTER_POINT) * 12;
            leftBright = scale8(leftBright, 255 - (centerBlend >> 1));
            rightBright = scale8(rightBright, 255 - (centerBlend >> 1));
        }
        
        strip1[i] = ColorFromPalette(currentPalette, leftHue, leftBright);
        strip2[i] = ColorFromPalette(currentPalette, rightHue, rightBright);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== MODAL CAVITY RESONANCE ==============
// Excite specific waveguide modes
void lgpModalCavity() {
    static uint16_t time = 0;
    time += paletteSpeed;
    
    // Mode number (1-20)
    uint8_t modeNumber = 1 + (visualParams.complexity >> 4);
    
    // Mode beating for dynamics
    uint8_t beatMode = modeNumber + (visualParams.variation >> 6);
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Normalized position
        float x = (float)i / HardwareConfig::STRIP_LENGTH;
        
        // Primary mode: sin(n*pi*x)
        int16_t mode1 = sin16((uint16_t)(x * modeNumber * 32768));
        
        // Beat mode for interference
        int16_t mode2 = sin16((uint16_t)(x * beatMode * 32768) + time);
        
        // Combine modes
        int16_t combined = (mode1 >> 1) + (mode2 >> 2);
        uint8_t brightness = (combined + 32768) >> 8;
        
        // Apply cosine taper for non-equidistant spacing
        uint8_t taper = cos8(x * 255) >> 1;
        brightness = scale8(brightness, 128 + taper);
        brightness = scale8(brightness, visualParams.intensity);
        
        // Color based on mode energy
        uint8_t hue = gHue + (modeNumber * 12);
        
        // Apply to both strips
        strip1[i] = ColorFromPalette(currentPalette, hue, brightness);
        strip2[i] = ColorFromPalette(currentPalette, hue + 64, brightness);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== FRESNEL ZONES ==============
// Optical zone plates creating focusing effects
void lgpFresnelZones() {
    // CENTER ORIGIN: Fixed focal point at center LED 79/80
    // (Removed moving focal point - not CENTER ORIGIN compliant)

    // Zone count
    uint8_t zoneCount = 3 + (visualParams.complexity >> 5);

    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Distance from fixed center focal point
        int16_t dist = abs((int16_t)i - HardwareConfig::STRIP_CENTER_POINT);
        
        // Fresnel zone calculation: sqrt(n * lambda * f)
        // Approximated for LED spacing
        uint16_t zoneRadius = sqrt16(dist << 8) * zoneCount;
        
        // Binary zone plate
        bool inZone = (zoneRadius & 0x100) != 0;
        uint8_t brightness = inZone ? 255 : 0;
        
        // Smooth edges based on intensity
        if (visualParams.intensity < 200) {
            uint8_t edge = zoneRadius & 0xFF;
            brightness = inZone ? edge : 255 - edge;
        }
        
        brightness = scale8(brightness, visualParams.intensity);
        
        // Chromatic aberration effect
        uint8_t hue = gHue + (dist >> 2);
        
        strip1[i] = ColorFromPalette(currentPalette, hue, brightness);
        strip2[i] = ColorFromPalette(currentPalette, hue + 30, scale8(brightness, 200));
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
}

// ============== PHOTONIC CRYSTAL ==============
// Periodic refractive index modulation
void lgpPhotonicCrystal() {
    static uint16_t phase = 0;
    phase += paletteSpeed;
    
    // Lattice constant
    uint8_t latticeSize = 4 + (visualParams.complexity >> 6);
    
    // Defect density for interesting patterns
    uint8_t defectProbability = visualParams.variation;
    
    for (uint16_t i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Calculate distance from center
        uint16_t distFromCenter = abs((int16_t)i - HardwareConfig::STRIP_CENTER_POINT);
        // Periodic structure - use distance, not raw i
        uint8_t cellPosition = distFromCenter % latticeSize;
        bool inBandgap = cellPosition < (latticeSize >> 1);
        
        // Add defects
        if (random8() < defectProbability) {
            inBandgap = !inBandgap;
        }
        
        // Photonic band structure - CENTER ORIGIN PUSH
        uint8_t brightness = 0;
        if (inBandgap) {
            // Allowed modes - push outward from center
            brightness = sin8((distFromCenter << 2) - (phase >> 7));
        } else {
            // Forbidden gap - evanescent decay
            uint8_t decay = 255 - (cellPosition * 50);
            brightness = scale8(sin8((distFromCenter << 1) - (phase >> 8)), decay);
        }
        
        brightness = scale8(brightness, visualParams.intensity);

        // Color based on band structure
        uint8_t hue = inBandgap ? gHue : gHue + 128;

        // CENTER ORIGIN: Use distance from center for symmetric color gradient
        strip1[i] = ColorFromPalette(currentPalette, hue + distFromCenter/4, brightness);
        strip2[i] = ColorFromPalette(currentPalette, hue + distFromCenter/4 + 64, brightness);
    }
    
    // Sync to unified buffer
    memcpy(leds, strip1, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
    memcpy(&leds[HardwareConfig::STRIP_LENGTH], strip2, sizeof(CRGB) * HardwareConfig::STRIP_LENGTH);
} 