// LGP Interference Pattern Effects
// Specifically designed for Light Guide Plate edge-lit configurations
// These effects exploit optical waveguide properties to create unique visuals

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

// ============== LGP BOX WAVE CONTROLLER ==============
// Creates controllable standing wave boxes
void lgpBoxWave() {
    // ENCODER MAPPING:
    // Speed (3): Box oscillation speed
    // Intensity (4): Box contrast/sharpness
    // Saturation (5): Color saturation
    // Complexity (6): Number of boxes (3-12)
    // Variation (7): Motion type (standing/traveling/rotating)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // Box count: 3-12 boxes based on complexity
    float boxesPerSide = 3 + (complexity * 9);  // 3-12 boxes
    float spatialFreq = boxesPerSide * PI / HardwareConfig::STRIP_HALF_LENGTH;
    
    // Motion phase
    static float motionPhase = 0;
    motionPhase += speed * 0.05f;
    
    // Clear previous frame
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        
        // Base box pattern
        float boxPhase = distFromCenter * spatialFreq;
        
        // Motion type based on variation
        float boxPattern;
        if (variation < 0.33f) {
            // Standing waves (original box effect)
            boxPattern = sin(boxPhase + motionPhase);
        } else if (variation < 0.66f) {
            // Traveling waves
            float travelPhase = (i / (float)HardwareConfig::STRIP_LENGTH) * TWO_PI * boxesPerSide;
            boxPattern = sin(travelPhase - motionPhase * 10);
        } else {
            // Rotating/spiral pattern
            float spiralPhase = boxPhase + (i * 0.02f);
            boxPattern = sin(spiralPhase + motionPhase) * cos(spiralPhase - motionPhase * 0.5f);
        }
        
        // Sharpness control via intensity
        if (intensity > 0.5f) {
            // Square wave shaping for sharper boxes
            float sharpness = (intensity - 0.5f) * 4;  // 0-2 range
            boxPattern = tanh(boxPattern * (1 + sharpness)) / tanh(1 + sharpness);
        }
        
        // Convert to brightness
        uint8_t brightness = 128 + (127 * boxPattern * intensity);
        
        // Color wave overlay
        uint8_t colorIndex = gHue + (distFromCenter * 2);
        
        // Apply to strips with phase relationship
        CRGB color1 = CHSV(colorIndex, saturation * 255, brightness);
        CRGB color2 = CHSV(colorIndex + 128, saturation * 255, brightness);  // Anti-phase
        
        strip1[i] = color1;
        strip2[i] = color2;
    }
}

// ============== LGP HOLOGRAPHIC SHIMMER ==============
// Creates depth illusion through multi-layer interference
void lgpHolographic() {
    // ENCODER MAPPING:
    // Speed (3): Shimmer animation speed
    // Intensity (4): Effect brightness/visibility
    // Saturation (5): Color richness
    // Complexity (6): Number of depth layers (2-5)
    // Variation (7): Layer interaction mode
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float phase1 = 0, phase2 = 0, phase3 = 0;
    phase1 += speed * 0.02f;
    phase2 += speed * 0.03f;
    phase3 += speed * 0.05f;
    
    int numLayers = 2 + (complexity * 3);  // 2-5 layers
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalized = dist / HardwareConfig::STRIP_HALF_LENGTH;
        
        float layerSum = 0;
        
        // Layer 1 - Slow, wide pattern
        layerSum += sin(dist * 0.05f + phase1) * (numLayers >= 1 ? 1.0f : 0);
        
        // Layer 2 - Medium pattern
        layerSum += sin(dist * 0.15f + phase2) * 0.7f * (numLayers >= 2 ? 1.0f : 0);
        
        // Layer 3 - Fast, tight pattern
        layerSum += sin(dist * 0.3f + phase3) * 0.5f * (numLayers >= 3 ? 1.0f : 0);
        
        // Layer 4 - Very fast shimmer
        if (numLayers >= 4) {
            layerSum += sin(dist * 0.6f - phase1 * 3) * 0.3f;
        }
        
        // Layer 5 - Chaos layer
        if (numLayers >= 5) {
            layerSum += sin(dist * 1.2f + phase2 * 5) * sin(phase3) * 0.2f;
        }
        
        // Normalize and apply intensity
        layerSum = layerSum / numLayers;
        
        // Variation controls layer interaction
        if (variation < 0.33f) {
            // Additive (bright)
            layerSum = tanh(layerSum);
        } else if (variation < 0.66f) {
            // Multiplicative (moiré-like)
            layerSum = layerSum * sin(normalized * PI);
        } else {
            // Differential (edge enhance)
            float nextSum = sin((dist + 1) * 0.15f + phase2);
            layerSum = (layerSum - nextSum) * 5;
        }
        
        uint8_t brightness = 128 + (127 * layerSum * intensity);
        
        // Chromatic dispersion effect
        uint8_t hue1 = gHue + (dist * 0.5f) + (layerSum * 20);
        uint8_t hue2 = gHue - (dist * 0.5f) - (layerSum * 20);
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP MODAL RESONANCE ==============
// Explores different optical cavity modes
void lgpModalResonance() {
    // ENCODER MAPPING:
    // Speed (3): Mode sweep speed
    // Intensity (4): Mode amplitude
    // Saturation (5): Color saturation
    // Complexity (6): Mode number (1-20)
    // Variation (7): Mode blend type
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    // Mode selection
    static float modePhase = 0;
    modePhase += speed * 0.01f;
    
    float baseMode;
    if (complexity < 0.5f) {
        // Low modes (1-10)
        baseMode = 1 + (complexity * 18);  // 1-10
    } else {
        // High modes (10-20) with sweep
        baseMode = 10 + sin(modePhase) * 10 * (complexity - 0.5f) * 2;
    }
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        float modalPattern;
        
        if (variation < 0.25f) {
            // Pure mode
            modalPattern = sin(position * baseMode * TWO_PI);
        } else if (variation < 0.5f) {
            // Mode beating (two close modes)
            float mode1 = sin(position * baseMode * TWO_PI);
            float mode2 = sin(position * (baseMode + 0.5f) * TWO_PI);
            modalPattern = (mode1 + mode2) / 2;
        } else if (variation < 0.75f) {
            // Harmonic series
            modalPattern = sin(position * baseMode * TWO_PI) +
                          sin(position * baseMode * 2 * TWO_PI) * 0.5f +
                          sin(position * baseMode * 3 * TWO_PI) * 0.25f;
            modalPattern /= 1.75f;
        } else {
            // Chaotic mode mixing
            modalPattern = sin(position * baseMode * TWO_PI) *
                          cos(position * (baseMode * 1.618f) * TWO_PI) *
                          sin(modePhase * 5);
        }
        
        // Apply window function for smoother edges
        float window = sin(position * PI);
        modalPattern *= window;
        
        uint8_t brightness = 128 + (127 * modalPattern * intensity);
        
        // Color based on mode number and position
        uint8_t hue = gHue + (baseMode * 10) + (position * 50);
        
        // Opposing strips get complementary phase
        strip1[i] = CHSV(hue, saturation * 255, brightness);
        strip2[i] = CHSV(hue + 128, saturation * 255, brightness);
    }
}

// ============== LGP INTERFERENCE SCANNER ==============
// Creates scanning interference patterns
void lgpInterferenceScanner() {
    // ENCODER MAPPING:
    // Speed (3): Scan speed
    // Intensity (4): Pattern contrast
    // Saturation (5): Color depth
    // Complexity (6): Interference complexity
    // Variation (7): Scan pattern type
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float scanPhase = 0;
    static float scanPhase2 = 0;
    scanPhase += speed * 0.05f;
    scanPhase2 += speed * 0.03f;
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float position = (float)i / HardwareConfig::STRIP_LENGTH;
        
        float pattern = 0;
        
        if (variation < 0.25f) {
            // Linear scan
            float scanPos = fmod(scanPhase, TWO_PI);
            float scanWindow = 0.2f + (complexity * 0.3f);
            float distFromScan = abs(position - (scanPos / TWO_PI));
            if (distFromScan < scanWindow) {
                pattern = cos(distFromScan / scanWindow * PI/2);
            }
        } else if (variation < 0.5f) {
            // Radial scan from center
            float ringRadius = fmod(scanPhase * 30, HardwareConfig::STRIP_HALF_LENGTH);
            float ringWidth = 5 + (complexity * 20);
            if (abs(dist - ringRadius) < ringWidth) {
                pattern = cos((dist - ringRadius) / ringWidth * PI/2);
            }
        } else if (variation < 0.75f) {
            // Dual sweep interference
            float wave1 = sin(dist * 0.1f + scanPhase);
            float wave2 = sin(dist * 0.1f - scanPhase2);
            pattern = (wave1 + wave2) / 2;
            
            // Add complexity via harmonics
            if (complexity > 0.5f) {
                pattern += sin(dist * 0.3f + scanPhase * 2) * 0.3f;
                pattern += sin(dist * 0.5f - scanPhase2 * 3) * 0.2f;
            }
        } else {
            // Moiré pattern scanner
            float grid1 = sin(position * 20 * (1 + complexity) + scanPhase);
            float grid2 = sin(position * 21 * (1 + complexity) - scanPhase);
            pattern = (grid1 * grid2 + 1) / 2;
        }
        
        // Intensity shaping
        pattern = pow(abs(pattern), 2 - intensity);
        
        uint8_t brightness = pattern * 255 * intensity;
        
        // Color mapping
        uint8_t hue1 = gHue + (dist * 2) + (pattern * 50);
        uint8_t hue2 = gHue - (dist * 2) + (pattern * 50);
        
        // Create opposition for interference
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, 255 - brightness);  // Inverted
    }
}

// ============== LGP WAVE COLLISION ==============
// Simulates wave packets colliding in the light guide
void lgpWaveCollision() {
    static float wave1Pos = 0;
    static float wave2Pos = HardwareConfig::STRIP_LENGTH;
    static float wave1Vel = 2.0f;
    static float wave2Vel = -2.0f;
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    
    // Update wave positions
    wave1Pos += wave1Vel * speed;
    wave2Pos += wave2Vel * speed;
    
    // Bounce at edges
    if (wave1Pos < 0 || wave1Pos > HardwareConfig::STRIP_LENGTH) {
        wave1Vel = -wave1Vel;
        wave1Pos = constrain(wave1Pos, 0, HardwareConfig::STRIP_LENGTH);
    }
    if (wave2Pos < 0 || wave2Pos > HardwareConfig::STRIP_LENGTH) {
        wave2Vel = -wave2Vel;
        wave2Pos = constrain(wave2Pos, 0, HardwareConfig::STRIP_LENGTH);
    }
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 30);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 30);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        // Wave packet 1
        float dist1 = abs(i - wave1Pos);
        float packet1 = exp(-dist1 * 0.05f) * cos(dist1 * 0.5f);
        
        // Wave packet 2
        float dist2 = abs(i - wave2Pos);
        float packet2 = exp(-dist2 * 0.05f) * cos(dist2 * 0.5f);
        
        // Interference
        float interference = packet1 + packet2;
        
        uint8_t brightness = 128 + (127 * interference * intensity);
        uint8_t hue = gHue + (i * 2) + (interference * 50);
        
        strip1[i] = CHSV(hue, 255, brightness);
        strip2[i] = CHSV(hue + 128, 255, brightness);
    }
}