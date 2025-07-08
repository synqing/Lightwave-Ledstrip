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

// ============== LGP SOLITON EXPLORER ==============
// Self-maintaining wave packets that preserve shape while traveling
void lgpSolitonExplorer() {
    // ENCODER MAPPING:
    // Speed (3): Soliton velocity
    // Intensity (4): Soliton amplitude/nonlinearity
    // Saturation (5): Color saturation
    // Complexity (6): Number of solitons (1-4)
    // Variation (7): Soliton type (bright/dark/breather)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float soliton1Pos = 0;
    static float soliton2Pos = HardwareConfig::STRIP_LENGTH * 0.33f;
    static float soliton3Pos = HardwareConfig::STRIP_LENGTH * 0.66f;
    static float soliton4Pos = HardwareConfig::STRIP_LENGTH;
    static float breathePhase = 0;
    
    int numSolitons = 1 + (complexity * 3);  // 1-4 solitons
    float velocity = speed * 1.5f;
    
    // Update positions
    soliton1Pos += velocity;
    soliton2Pos += velocity * 0.8f;
    soliton3Pos += velocity * 1.2f;
    soliton4Pos += velocity * 0.6f;
    breathePhase += speed * 0.1f;
    
    // Wrap around
    if (soliton1Pos > HardwareConfig::STRIP_LENGTH) soliton1Pos = 0;
    if (soliton2Pos > HardwareConfig::STRIP_LENGTH) soliton2Pos = 0;
    if (soliton3Pos > HardwareConfig::STRIP_LENGTH) soliton3Pos = 0;
    if (soliton4Pos > HardwareConfig::STRIP_LENGTH) soliton4Pos = 0;
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 15);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float totalWave = 0;
        
        // Soliton 1 - Always present
        float dist1 = abs(i - soliton1Pos);
        float width1 = 15 + (intensity * 20);
        float soliton1;
        
        if (variation < 0.33f) {
            // Bright soliton (hyperbolic secant)
            soliton1 = 1.0f / cosh(dist1 / width1);
        } else if (variation < 0.66f) {
            // Dark soliton (tanh profile)
            soliton1 = tanh(dist1 / width1);
        } else {
            // Breather soliton (oscillating)
            float breatheAmp = 1 + 0.3f * sin(breathePhase + dist1 * 0.1f);
            soliton1 = breatheAmp / cosh(dist1 / width1);
        }
        
        totalWave += soliton1 * intensity;
        
        // Additional solitons
        if (numSolitons >= 2) {
            float dist2 = abs(i - soliton2Pos);
            float soliton2 = 0.7f / cosh(dist2 / (width1 * 0.8f));
            totalWave += soliton2 * intensity;
        }
        
        if (numSolitons >= 3) {
            float dist3 = abs(i - soliton3Pos);
            float soliton3 = 0.5f / cosh(dist3 / (width1 * 1.2f));
            totalWave += soliton3 * intensity;
        }
        
        if (numSolitons >= 4) {
            float dist4 = abs(i - soliton4Pos);
            float soliton4 = 0.4f / cosh(dist4 / (width1 * 0.6f));
            totalWave += soliton4 * intensity;
        }
        
        // Nonlinear self-interaction (Kerr effect simulation)
        totalWave = totalWave / (1 + totalWave * intensity);
        
        uint8_t brightness = 128 + (127 * totalWave);
        
        // Color based on wave amplitude and position
        uint8_t hue1 = gHue + (i * 0.5f) + (totalWave * 30);
        uint8_t hue2 = gHue + (i * 0.5f) - (totalWave * 30);
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP QUANTUM TUNNELING ==============
// Wave packets that tunnel through barrier regions
void lgpQuantumTunneling() {
    // ENCODER MAPPING:
    // Speed (3): Packet velocity
    // Intensity (4): Barrier height/tunneling probability
    // Saturation (5): Color saturation
    // Complexity (6): Number of barriers (1-3)
    // Variation (7): Barrier type (rectangular/gaussian/periodic)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float packetPos = 0;
    static float packetVel = 1.0f;
    static float tunnelingProb = 1.0f;
    
    packetPos += packetVel * speed;
    
    // Reset at edges
    if (packetPos > HardwareConfig::STRIP_LENGTH) {
        packetPos = 0;
        tunnelingProb = 1.0f;
    }
    
    int numBarriers = 1 + (complexity * 2);  // 1-3 barriers
    
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 25);
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float dist = abs(i - packetPos);
        
        // Gaussian wave packet
        float packetWidth = 20;
        float wavePacket = exp(-dist * dist / (2 * packetWidth * packetWidth));
        
        // Calculate transmission probability through barriers
        float barrierEffect = 1.0f;
        
        for(int b = 0; b < numBarriers; b++) {
            float barrierCenter = HardwareConfig::STRIP_LENGTH * (0.3f + b * 0.3f);
            float barrierWidth = 15 + (intensity * 30);
            float distToBarrier = abs(i - barrierCenter);
            
            float barrier = 0;
            if (variation < 0.33f) {
                // Rectangular barrier
                barrier = (distToBarrier < barrierWidth) ? intensity : 0;
            } else if (variation < 0.66f) {
                // Gaussian barrier
                barrier = intensity * exp(-distToBarrier * distToBarrier / (barrierWidth * barrierWidth));
            } else {
                // Periodic barrier (multiple thin barriers)
                barrier = (sin(distToBarrier * 0.5f) > 0) ? intensity * 0.5f : 0;
            }
            
            // Quantum tunneling probability (simplified)
            float transmission = exp(-2 * barrier * sqrt(2 * intensity));
            barrierEffect *= transmission;
        }
        
        // Apply tunneling effect
        wavePacket *= barrierEffect;
        
        // Add quantum interference fringes
        float interference = 1 + 0.2f * sin(i * 0.3f + packetPos * 0.1f);
        wavePacket *= interference;
        
        uint8_t brightness = wavePacket * 255;
        
        // Color shifts based on tunneling probability
        uint8_t hue1 = gHue + (barrierEffect * 60) + (i * 0.2f);
        uint8_t hue2 = gHue - (barrierEffect * 60) + (i * 0.2f);
        
        // Visualize barriers as dimmer regions
        for(int b = 0; b < numBarriers; b++) {
            float barrierCenter = HardwareConfig::STRIP_LENGTH * (0.3f + b * 0.3f);
            float barrierWidth = 15 + (intensity * 30);
            if (abs(i - barrierCenter) < barrierWidth) {
                brightness = brightness * (1 - intensity * 0.7f);
            }
        }
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP ROGUE WAVE GENERATOR ==============
// Extreme wave events emerging from background noise
void lgpRogueWaveGenerator() {
    // ENCODER MAPPING:
    // Speed (3): Background wave frequency
    // Intensity (4): Rogue wave amplitude multiplier
    // Saturation (5): Color saturation
    // Complexity (6): Number of background modes (3-12)
    // Variation (7): Rogue wave trigger probability
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float rogueWavePos = -50;
    static float rogueWaveAmp = 0;
    static float rogueWavePhase = 0;
    static bool rogueActive = false;
    static float backgroundPhase = 0;
    static float rogueTimer = 0;
    
    backgroundPhase += speed * 0.05f;
    rogueTimer += speed * 0.01f;
    
    // Rogue wave trigger probability based on variation
    float triggerProb = variation * 0.001f;  // Very low probability
    if (!rogueActive && random(10000) < triggerProb * 10000) {
        rogueActive = true;
        rogueWavePos = -50;
        rogueWaveAmp = 2.0f + (intensity * 3.0f);  // 2-5x normal amplitude
        rogueWavePhase = 0;
        rogueTimer = 0;
    }
    
    // Move rogue wave
    if (rogueActive) {
        rogueWavePos += speed * 2.0f;
        rogueWavePhase += speed * 0.1f;
        
        // Rogue wave decay over time
        rogueWaveAmp *= 0.998f;
        
        if (rogueWavePos > HardwareConfig::STRIP_LENGTH + 50) {
            rogueActive = false;
            rogueWaveAmp = 0;
        }
    }
    
    int numModes = 3 + (complexity * 9);  // 3-12 background modes
    
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float totalWave = 0;
        
        // Background sea state - superposition of multiple modes
        for(int m = 1; m <= numModes; m++) {
            float freq = m * 0.1f;
            float amplitude = 1.0f / (m * m);  // 1/f² spectrum
            float phase = backgroundPhase * m + (i * freq);
            
            // Add random phase noise for each mode
            float noise = sin(phase * 7.13f) * 0.1f;  // Pseudo-random
            totalWave += amplitude * sin(phase + noise);
        }
        
        // Normalize background to reasonable amplitude
        totalWave *= 0.3f;
        
        // Add rogue wave if active
        if (rogueActive) {
            float distToRogue = abs(i - rogueWavePos);
            float rogueWidth = 25 + (intensity * 25);
            
            // Peregrine soliton profile (approximate)
            float rogueProfile = 1.0f / cosh(distToRogue / rogueWidth);
            
            // Modulation instability growth
            float modulation = 1 + 0.5f * sin(rogueWavePhase * 3);
            
            // Focusing effect - wave packet compression
            float focusEffect = exp(-distToRogue * distToRogue / (rogueWidth * rogueWidth * 4));
            
            float rogueContribution = rogueWaveAmp * rogueProfile * modulation * focusEffect;
            totalWave += rogueContribution;
        }
        
        // Nonlinear wave steepening (simplified)
        if (totalWave > 0.5f) {
            totalWave = 0.5f + (totalWave - 0.5f) * 0.7f;  // Soft saturation
        }
        
        uint8_t brightness = 128 + (127 * totalWave * intensity);
        
        // Color based on wave height and danger level
        uint8_t baseHue = gHue + (i * 0.3f);
        if (rogueActive && abs(i - rogueWavePos) < 40) {
            // Rogue wave area gets warning colors
            baseHue += 60;  // Shift toward red/orange
        }
        
        uint8_t hue1 = baseHue + (totalWave * 20);
        uint8_t hue2 = baseHue - (totalWave * 20);
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}

// ============== LGP TURING PATTERN ENGINE ==============
// Biological pattern formation via reaction-diffusion
void lgpTuringPatternEngine() {
    // ENCODER MAPPING:
    // Speed (3): Reaction rate
    // Intensity (4): Pattern contrast
    // Saturation (5): Color saturation
    // Complexity (6): Pattern scale (spots to stripes)
    // Variation (7): Pattern type (spots/stripes/maze/spiral)
    
    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();
    
    static float concentrationA[320];  // Activator
    static float concentrationB[320];  // Inhibitor
    static bool initialized = false;
    
    // Initialize with random noise
    if (!initialized) {
        for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
            concentrationA[i] = 0.5f + (random(100) - 50) * 0.001f;
            concentrationB[i] = 0.5f + (random(100) - 50) * 0.001f;
        }
        initialized = true;
    }
    
    // Reaction-diffusion parameters
    float Da = 0.1f;  // Activator diffusion rate
    float Db = 0.5f;  // Inhibitor diffusion rate (higher)
    float dt = speed * 0.1f;
    
    // Pattern scale based on complexity
    float reactionScale = 1.0f + (complexity * 3.0f);
    
    // Temporary arrays for calculations
    static float newA[320];
    static float newB[320];
    
    // Reaction-diffusion step
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float A = concentrationA[i];
        float B = concentrationB[i];
        
        // Neighbor indices (with wrapping)
        int left = (i - 1 + HardwareConfig::STRIP_LENGTH) % HardwareConfig::STRIP_LENGTH;
        int right = (i + 1) % HardwareConfig::STRIP_LENGTH;
        
        // Laplacian (discrete diffusion)
        float laplacianA = concentrationA[left] - 2*A + concentrationA[right];
        float laplacianB = concentrationB[left] - 2*B + concentrationB[right];
        
        // Reaction terms based on variation (different Turing patterns)
        float reactionA, reactionB;
        
        if (variation < 0.25f) {
            // Spots pattern (Gierer-Meinhardt)
            reactionA = A*A/B - A;
            reactionB = A*A - B;
        } else if (variation < 0.5f) {
            // Stripes pattern (FitzHugh-Nagumo)
            reactionA = A - A*A*A - B;
            reactionB = A - B;
        } else if (variation < 0.75f) {
            // Maze pattern (Gray-Scott)
            float feed = 0.04f;
            float kill = 0.06f;
            reactionA = -A*B*B + feed*(1-A);
            reactionB = A*B*B - (kill + feed)*B;
        } else {
            // Spiral pattern (Brusselator)
            float a = 1.0f;
            float b = 3.0f;
            reactionA = a + A*A*B - (b+1)*A;
            reactionB = b*A - A*A*B;
        }
        
        // Scale reactions
        reactionA *= reactionScale;
        reactionB *= reactionScale;
        
        // Update concentrations
        newA[i] = A + dt * (Da * laplacianA + reactionA);
        newB[i] = B + dt * (Db * laplacianB + reactionB);
        
        // Clamp to reasonable bounds
        newA[i] = constrain(newA[i], 0, 2);
        newB[i] = constrain(newB[i], 0, 2);
    }
    
    // Copy back
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        concentrationA[i] = newA[i];
        concentrationB[i] = newB[i];
    }
    
    // Visualize the pattern
    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float A = concentrationA[i];
        float B = concentrationB[i];
        
        // Pattern visualization
        float pattern = A - B;  // Activator minus inhibitor
        
        // Enhance contrast
        pattern = tanh(pattern * (1 + intensity * 3));
        
        uint8_t brightness = 128 + (127 * pattern * intensity);
        
        // Color based on concentration levels
        uint8_t hue1 = gHue + (A * 100) + (i * 0.2f);
        uint8_t hue2 = gHue + (B * 100) + (i * 0.2f);
        
        strip1[i] = CHSV(hue1, saturation * 255, brightness);
        strip2[i] = CHSV(hue2, saturation * 255, brightness);
    }
}