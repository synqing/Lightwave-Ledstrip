// LGP Novel Physics Effects
// Advanced effects exploiting dual-edge optical interference properties
// These effects are IMPOSSIBLE on single LED strips - they require two
// coherent light sources creating real interference patterns

#include <FastLED.h>
#include <math.h>
#include "../../config/hardware_config.h"
#include "../../core/EffectTypes.h"
#include "LGPNovelPhysics.h"

// Math constants
#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef TWO_PI
#define TWO_PI (2.0 * PI)
#endif

// External references
extern CRGB strip1[];
extern CRGB strip2[];
extern CRGBPalette16 currentPalette;
extern uint8_t gHue;
extern uint8_t paletteSpeed;
extern VisualParams visualParams;

// ============== CHLADNI PLATE HARMONICS ==============
// Visualizes acoustic resonance patterns on vibrating plates
// "Sand particles" migrate from antinodes (high vibration) to nodes (stillness)
// Dual strips show TOP and BOTTOM plate surface with 180-degree phase offset
void lgpChladniHarmonics() {
    // ENCODER MAPPING:
    // Speed (3): Vibration frequency - plate oscillation rate
    // Intensity (4): Drive amplitude - antinode brightness
    // Saturation (5): Particle glow - brightness at nodes
    // Complexity (6): Mode number - harmonic (1=fundamental, 12=complex)
    // Variation (7): Damping/chaos - pure modes vs mixed

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();

    // Mode number: 1-12 based on complexity
    int modeNumber = 1 + (int)(complexity * 11);

    // Vibration phase
    static float vibrationPhase = 0;
    vibrationPhase += speed * 0.08f;

    // Secondary mode for mixed harmonics
    static float mixPhase = 0;
    mixPhase += speed * 0.05f;

    // Fade previous frame
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 15);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 15);

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedPos = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Mode shape: standing wave pattern
        // sin(n * pi * x / L) gives n antinodes
        float modeShape = sin(modeNumber * PI * normalizedPos);

        // Add mixing with nearby modes for variation control
        float mixedMode = modeShape;
        if (variation > 0.3f) {
            float mix1 = sin((modeNumber + 1) * PI * normalizedPos) * sin(mixPhase);
            float mix2 = sin((modeNumber - 1) * PI * normalizedPos) * cos(mixPhase * 1.3f);
            float mixAmount = (variation - 0.3f) / 0.7f;
            mixedMode = modeShape * (1 - mixAmount * 0.5f) + (mix1 + mix2) * mixAmount * 0.25f;
        }

        // Temporal oscillation: plate vibrates up and down
        float temporalOscillation = cos(vibrationPhase);
        float plateDisplacement = mixedMode * temporalOscillation;

        // Sand particle visualization:
        // Particles accumulate at NODES (where displacement is always zero)
        // Particles flee from ANTINODES (max displacement)
        float nodeStrength = 1.0f / (abs(modeShape) + 0.1f);
        nodeStrength = constrain(nodeStrength, 0, 3.0f);

        // Antinode visualization: shows displacement magnitude
        float antinodeStrength = abs(plateDisplacement) * intensity;

        // Blend: low intensity shows particles at nodes, high shows plate motion
        float particleBrightness = nodeStrength * (1 - intensity) * 0.3f;
        float motionBrightness = antinodeStrength * intensity;
        float totalBrightness = (particleBrightness + motionBrightness) * 255;

        uint8_t brightness = constrain(totalBrightness, 20, 255);

        // Color: warm tones for plate, cool for particles
        uint8_t hue1 = gHue + (uint8_t)(plateDisplacement * 30);
        uint8_t hue2 = gHue + 128 + (uint8_t)(plateDisplacement * 30);  // 180-degree phase offset

        uint8_t sat = saturation * 255;

        // Strip1 = TOP surface (phase 0)
        // Strip2 = BOTTOM surface (phase 180) - inverted displacement
        strip1[i] = CHSV(hue1, sat, brightness);

        // Bottom surface has opposite phase
        float bottomDisplacement = -plateDisplacement;
        float bottomBrightness = (particleBrightness + abs(bottomDisplacement) * intensity) * 255;
        strip2[i] = CHSV(hue2, sat, constrain(bottomBrightness, 20, 255));
    }
}

// ============== GRAVITATIONAL WAVE CHIRP ==============
// Binary black hole inspiral with LIGO-accurate frequency evolution
// Two spiral waves accelerate exponentially (chirp), merge at center, ringdown
// Strip1 = h+ polarization, Strip2 = hx polarization (orthogonal)
void lgpGravitationalWaveChirp() {
    // ENCODER MAPPING:
    // Speed (3): Inspiral duration - 2-10 seconds to merger
    // Intensity (4): Strain amplitude - wave visibility
    // Saturation (5): Color saturation
    // Complexity (6): System mass - heavier = faster chirp
    // Variation (7): Ringdown frequency - final BH oscillation pitch

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();

    // Inspiral state machine
    static float inspiralProgress = 0;  // 0 to 1 during inspiral
    static float ringdownPhase = 0;
    static bool merging = false;
    static bool ringdown = false;
    static float mergeFlash = 0;

    // Chirp parameters based on system mass
    float chirpRate = 0.002f + speed * 0.008f;
    float massRatio = 0.5f + complexity * 1.5f;  // Affects chirp steepness

    if (!merging && !ringdown) {
        // Inspiral phase: frequency increases as 1/(t_merge - t)^(3/8)
        inspiralProgress += chirpRate;

        if (inspiralProgress >= 1.0f) {
            merging = true;
            mergeFlash = 1.0f;
        }
    } else if (merging) {
        // Merger: brief bright flash
        mergeFlash *= 0.92f;
        if (mergeFlash < 0.05f) {
            merging = false;
            ringdown = true;
            ringdownPhase = 0;
        }
    } else if (ringdown) {
        // Ringdown: damped sinusoid
        ringdownPhase += 0.15f + variation * 0.2f;
        float ringdownDecay = exp(-ringdownPhase * 0.05f);

        if (ringdownDecay < 0.01f) {
            // Reset cycle
            ringdown = false;
            inspiralProgress = 0;
        }
    }

    // Fade previous frame
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 25);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 25);

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        float wave1 = 0, wave2 = 0;

        if (!merging && !ringdown) {
            // Chirp frequency: f = f0 * (1 - progress)^(-3/8)
            float t_remaining = max(0.01f, 1.0f - inspiralProgress);
            float chirpFreq = pow(t_remaining, -3.0f/8.0f * massRatio);
            chirpFreq = constrain(chirpFreq, 1.0f, 20.0f);

            // Amplitude grows as frequency increases
            float amplitude = intensity * (1 + inspiralProgress * 2);

            // Two spiraling wavefronts approaching center
            static float phase1 = 0, phase2 = 0;
            phase1 += chirpFreq * 0.1f;
            phase2 = phase1 + PI/2;  // 90-degree offset for hx polarization

            // Waves compress toward center as inspiral progresses
            float compressionFactor = 1 + inspiralProgress * 3;
            float spatialPhase = normalizedDist * chirpFreq * compressionFactor;

            // h+ polarization (Strip1)
            wave1 = sin(spatialPhase - phase1) * amplitude * (1 - normalizedDist);

            // hx polarization (Strip2) - 90-degree phase offset
            wave2 = sin(spatialPhase - phase2) * amplitude * (1 - normalizedDist);

        } else if (merging) {
            // Merger flash at center
            float flashRadius = 0.3f + (1 - mergeFlash) * 0.5f;
            if (normalizedDist < flashRadius) {
                float flashIntensity = mergeFlash * (1 - normalizedDist / flashRadius);
                wave1 = flashIntensity * intensity * 2;
                wave2 = flashIntensity * intensity * 2;
            }

        } else if (ringdown) {
            // Ringdown: expanding damped ring
            float ringdownFreq = 5.0f + variation * 10.0f;
            float ringdownDecay = exp(-ringdownPhase * 0.05f);
            float ringRadius = ringdownPhase * 0.1f;

            float distToRing = abs(normalizedDist - fmod(ringRadius, 1.0f));
            if (distToRing < 0.2f) {
                float ringShape = cos(distToRing / 0.2f * PI/2);
                wave1 = sin(ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
                wave2 = cos(ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
            }
        }

        // Convert to brightness
        uint8_t brightness1 = 128 + constrain((int)(wave1 * 127), -127, 127);
        uint8_t brightness2 = 128 + constrain((int)(wave2 * 127), -127, 127);

        // Color: deep purple for gravitational waves, shifts during merger
        uint8_t baseHue = 200;  // Purple
        if (merging) baseHue = 40;  // Yellow during merger
        else if (ringdown) baseHue = 160;  // Cyan during ringdown

        uint8_t hue1 = baseHue + gHue;
        uint8_t hue2 = baseHue + gHue + 30;

        strip1[i] = CHSV(hue1, saturation * 255, brightness1);
        strip2[i] = CHSV(hue2, saturation * 255, brightness2);
    }
}

// ============== QUANTUM ENTANGLEMENT COLLAPSE ==============
// EPR paradox visualization with superposition and measurement
// Strips start in quantum foam (chaotic), collapse wavefront from center
// reveals perfect anti-correlation (complementary colors)
void lgpQuantumEntanglementCollapse() {
    // ENCODER MAPPING:
    // Speed (3): Collapse speed - wavefront expansion rate
    // Intensity (4): Superposition chaos - pre-collapse fluctuation
    // Saturation (5): Color purity
    // Complexity (6): Quantum mode n - wave function nodes (1-8)
    // Variation (7): Decoherence rate - edge noise accumulation

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();

    // Collapse state
    static float collapseRadius = 0;  // 0 to 1
    static bool collapsing = false;
    static bool collapsed = false;
    static float holdTime = 0;
    static uint8_t collapsedHue = 0;  // The "measured" value
    static float quantumPhase = 0;

    // Quantum mode number for wave function
    int quantumN = 1 + (int)(complexity * 7);  // 1-8 nodes

    quantumPhase += speed * 0.1f;

    // State machine
    if (!collapsing && !collapsed) {
        // Superposition state: wait for "measurement"
        static float measurementTimer = 0;
        measurementTimer += speed * 0.01f;

        if (measurementTimer > 1.0f + random8() / 255.0f) {
            // Measurement triggered!
            collapsing = true;
            collapseRadius = 0;
            collapsedHue = gHue + random8();  // Random measurement outcome
            measurementTimer = 0;
        }
    } else if (collapsing) {
        // Collapse wavefront expanding from center
        collapseRadius += speed * 0.02f;

        if (collapseRadius >= 1.0f) {
            collapsing = false;
            collapsed = true;
            holdTime = 0;
        }
    } else if (collapsed) {
        // Hold collapsed state, then reset
        holdTime += speed * 0.02f;

        if (holdTime > 1.5f) {
            collapsed = false;
            collapseRadius = 0;
        }
    }

    // Fade previous frame
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 20);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 20);

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        uint8_t hue1, hue2, brightness1, brightness2;

        if (!collapsing && !collapsed) {
            // SUPERPOSITION: quantum foam with wave function nodes
            // Wave function: psi = sin(n*pi*x) * e^(i*omega*t)
            float waveFunction = sin(quantumN * PI * normalizedDist);

            // Probability density |psi|^2
            float probability = waveFunction * waveFunction;

            // Quantum fluctuations (uncertainty)
            float fluctuation = sin(quantumPhase * 3 + i * 0.2f) *
                               cos(quantumPhase * 5 - i * 0.15f) *
                               intensity;

            // Superposition: strips show random, uncorrelated colors
            hue1 = gHue + (uint8_t)(sin(quantumPhase + i * 0.1f) * 60);
            hue2 = gHue + (uint8_t)(cos(quantumPhase * 1.3f - i * 0.12f) * 60);

            brightness1 = 80 + (probability * 100) + (abs(fluctuation) * 75);
            brightness2 = 80 + (probability * 100) + (abs(fluctuation) * 75);

        } else if (collapsing) {
            // COLLAPSE: wavefront expanding from center
            if (normalizedDist < collapseRadius) {
                // Collapsed region: perfect anti-correlation
                hue1 = collapsedHue;
                hue2 = collapsedHue + 128;  // Complementary = anti-correlated

                // Wave function collapse visualization
                float collapseEdge = collapseRadius - normalizedDist;
                float edgeFactor = constrain(collapseEdge * 10, 0, 1);

                brightness1 = 180 * edgeFactor + 50;
                brightness2 = 180 * edgeFactor + 50;

            } else {
                // Pre-collapse region: still in superposition
                float chaos = sin(quantumPhase * 5 + i * 0.3f) * intensity;
                hue1 = gHue + (uint8_t)(chaos * 40);
                hue2 = gHue + (uint8_t)(chaos * 40) + random8(30);
                brightness1 = 60 + abs(chaos) * 50;
                brightness2 = 60 + abs(chaos) * 50;

                // Decoherence: noise accumulates at edges
                if (variation > 0.5f && normalizedDist > 0.7f) {
                    float decoherence = (variation - 0.5f) * 2 * (normalizedDist - 0.7f) / 0.3f;
                    brightness1 = brightness1 * (1 - decoherence * 0.5f);
                    brightness2 = brightness2 * (1 - decoherence * 0.5f);
                }
            }

        } else {
            // COLLAPSED: stable entangled state
            hue1 = collapsedHue;
            hue2 = collapsedHue + 128;  // Perfect anti-correlation

            // Gentle pulsing to show quantum coherence
            float pulse = sin(quantumPhase) * 0.1f + 0.9f;
            brightness1 = 200 * pulse;
            brightness2 = 200 * pulse;
        }

        strip1[i] = CHSV(hue1, saturation * 255, brightness1);
        strip2[i] = CHSV(hue2, saturation * 255, brightness2);
    }
}

// ============== MYCELIAL NETWORK PROPAGATION ==============
// Fungal hyphal growth with fractal branching and nutrient flow
// Dual strips create depth layers (upper/lower mycelial mats)
// Interference zones form glowing "fruiting bodies"
void lgpMycelialNetwork() {
    // ENCODER MAPPING:
    // Speed (3): Growth rate - hyphal extension speed
    // Intensity (4): Network density - number of growth tips
    // Saturation (5): Nutrient visibility
    // Complexity (6): Branching frequency - fractal depth (1-10)
    // Variation (7): Flow direction bias - inward vs outward nutrients

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();

    // Hyphal growth tips (up to 16)
    static float tipPositions[16];
    static float tipVelocities[16];
    static bool tipActive[16];
    static float tipAge[16];
    static uint8_t numTips = 4;
    static bool initialized = false;

    // Nutrient flow phase
    static float nutrientPhase = 0;
    nutrientPhase += speed * 0.05f;

    // Initialize tips from center
    if (!initialized) {
        for(int t = 0; t < 16; t++) {
            tipPositions[t] = HardwareConfig::STRIP_CENTER_POINT;
            tipVelocities[t] = 0;
            tipActive[t] = false;
            tipAge[t] = 0;
        }
        // Start with a few active tips
        tipActive[0] = true;
        tipVelocities[0] = 0.5f;
        tipActive[1] = true;
        tipVelocities[1] = -0.5f;
        initialized = true;
    }

    // Branching frequency based on complexity
    float branchProbability = 0.001f + complexity * 0.01f;
    numTips = 4 + (int)(intensity * 12);  // 4-16 tips

    // Update tips
    for(int t = 0; t < 16; t++) {
        if (tipActive[t]) {
            // Grow tip
            tipPositions[t] += tipVelocities[t] * speed;
            tipAge[t] += speed * 0.01f;

            // Boundary check
            if (tipPositions[t] < 0 || tipPositions[t] >= HardwareConfig::STRIP_LENGTH) {
                tipActive[t] = false;
            }

            // Branching chance
            if (random8() < branchProbability * 255) {
                // Find inactive tip slot
                for(int newTip = 0; newTip < numTips; newTip++) {
                    if (!tipActive[newTip]) {
                        tipActive[newTip] = true;
                        tipPositions[newTip] = tipPositions[t];
                        tipVelocities[newTip] = -tipVelocities[t] * (0.5f + random8() / 255.0f * 0.5f);
                        tipAge[newTip] = 0;
                        break;
                    }
                }
            }
        } else if (random8() < 5) {
            // Occasionally spawn new tip from center
            tipActive[t] = true;
            tipPositions[t] = HardwareConfig::STRIP_CENTER_POINT;
            tipVelocities[t] = (random8() > 127 ? 1 : -1) * (0.3f + random8() / 255.0f * 0.4f);
            tipAge[t] = 0;
        }
    }

    // Fade previous frame (slow fade for trail persistence)
    fadeToBlackBy(strip1, HardwareConfig::STRIP_LENGTH, 8);
    fadeToBlackBy(strip2, HardwareConfig::STRIP_LENGTH, 8);

    // Network density map
    static float networkDensity[160];

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;

        // Decay network density slowly
        networkDensity[i] *= 0.998f;

        // Check for nearby tips
        float tipGlow = 0;
        for(int t = 0; t < 16; t++) {
            if (tipActive[t]) {
                float distToTip = abs(i - tipPositions[t]);
                if (distToTip < 5) {
                    // Growing tip glow
                    tipGlow += (5 - distToTip) / 5.0f * intensity;
                    // Build network density
                    networkDensity[i] = min(1.0f, networkDensity[i] + 0.02f);
                }
            }
        }

        // Nutrient flow visualization
        float flowDirection = (variation - 0.5f) * 2;  // -1 to 1
        float nutrientWave = sin(normalizedDist * 10 - nutrientPhase * flowDirection * 3);
        float nutrientBrightness = networkDensity[i] * (0.5f + nutrientWave * 0.5f) * saturation;

        // Fruiting body detection: where both strips have high density
        float fruitingIntensity = 0;

        // Base hyphae color: bioluminescent blue-green
        uint8_t hue1 = 140 + gHue * 0.3f;  // Cyan-ish
        uint8_t hue2 = 160 + gHue * 0.3f;  // More green

        // Combine tip glow, network, and nutrient flow
        float brightness1 = tipGlow * 200 + networkDensity[i] * 80 + nutrientBrightness * 60;
        float brightness2 = tipGlow * 150 + networkDensity[i] * 90 + nutrientBrightness * 70;

        // Fruiting bodies where strips overlap with high intensity
        if (brightness1 > 100 && brightness2 > 100) {
            fruitingIntensity = min(brightness1, brightness2) / 255.0f;
            // Golden glow for fruiting bodies
            hue1 = 40 + gHue * 0.2f;
            hue2 = 50 + gHue * 0.2f;
            brightness1 = min(255.0f, brightness1 * 1.3f);
            brightness2 = min(255.0f, brightness2 * 1.3f);
        }

        strip1[i] = CHSV(hue1, saturation * 255, constrain(brightness1, 0, 255));
        strip2[i] = CHSV(hue2, saturation * 255, constrain(brightness2, 0, 255));
    }
}

// ============== RILEY DISSONANCE ==============
// Op Art perceptual instability inspired by Bridget Riley
// High-contrast patterns with frequency mismatch create binocular rivalry
// Static patterns appear to shimmer and breathe due to optical interference
void lgpRileyDissonance() {
    // ENCODER MAPPING:
    // Speed (3): Pattern drift - slow rotation of conflict zones
    // Intensity (4): Contrast/aggression - perceptual discomfort level
    // Saturation (5): Color vs B&W
    // Complexity (6): Pattern type - 0-0.25 circles, 0.25-0.5 stripes, 0.5-0.75 checkerboard, 0.75-1 spirals
    // Variation (7): Frequency mismatch - beat envelope width

    float speed = paletteSpeed / 255.0f;
    float intensity = visualParams.getIntensityNorm();
    float saturation = visualParams.getSaturationNorm();
    float complexity = visualParams.getComplexityNorm();
    float variation = visualParams.getVariationNorm();

    // Pattern phase
    static float patternPhase = 0;
    patternPhase += speed * 0.02f;

    // Frequency mismatch creates beat pattern
    float baseFreq = 8 + complexity * 12;  // 8-20 cycles
    float freqMismatch = 0.02f + variation * 0.15f;  // 2-17% mismatch
    float freq1 = baseFreq * (1 + freqMismatch / 2);
    float freq2 = baseFreq * (1 - freqMismatch / 2);

    // Beat frequency = |f1 - f2|
    float beatFreq = baseFreq * freqMismatch;

    for(int i = 0; i < HardwareConfig::STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - HardwareConfig::STRIP_CENTER_POINT);
        float normalizedDist = distFromCenter / HardwareConfig::STRIP_HALF_LENGTH;
        float position = (float)i / HardwareConfig::STRIP_LENGTH;

        float pattern1 = 0, pattern2 = 0;

        if (complexity < 0.25f) {
            // CONCENTRIC CIRCLES (radial pattern from center)
            pattern1 = sin(normalizedDist * freq1 * TWO_PI + patternPhase);
            pattern2 = sin(normalizedDist * freq2 * TWO_PI - patternPhase);

        } else if (complexity < 0.5f) {
            // STRIPES (linear pattern)
            pattern1 = sin(position * freq1 * TWO_PI + patternPhase);
            pattern2 = sin(position * freq2 * TWO_PI - patternPhase * 0.7f);

        } else if (complexity < 0.75f) {
            // CHECKERBOARD (product of two patterns)
            float check1 = sin(position * freq1 * TWO_PI);
            float check2 = sin(normalizedDist * freq1 * TWO_PI + patternPhase);
            pattern1 = check1 * check2;

            float check3 = sin(position * freq2 * TWO_PI);
            float check4 = sin(normalizedDist * freq2 * TWO_PI - patternPhase);
            pattern2 = check3 * check4;

        } else {
            // SPIRALS (combination of radial and angular)
            float spiralAngle = position * TWO_PI + normalizedDist * 3;
            pattern1 = sin(spiralAngle * freq1 / 4 + patternPhase * 2);
            pattern2 = sin(spiralAngle * freq2 / 4 - patternPhase * 1.5f);
        }

        // Apply contrast enhancement (makes patterns more aggressive)
        float contrast = 1 + intensity * 4;  // 1-5x contrast
        pattern1 = tanh(pattern1 * contrast) / tanh(contrast);
        pattern2 = tanh(pattern2 * contrast) / tanh(contrast);

        // Beat envelope: interference between the two patterns
        float beatEnvelope = (pattern1 + pattern2) / 2;
        float rivalryZone = abs(pattern1 - pattern2);  // Where patterns conflict

        // Brightness: high contrast between on/off
        uint8_t brightness1 = 128 + (int)(pattern1 * 127 * intensity);
        uint8_t brightness2 = 128 + (int)(pattern2 * 127 * intensity);

        // Color: can be B&W or colored based on saturation
        uint8_t hue1, hue2;
        uint8_t sat;

        if (saturation < 0.3f) {
            // Near B&W: classic Riley
            hue1 = 0;
            hue2 = 0;
            sat = 0;
        } else {
            // Colored: complementary colors increase dissonance
            hue1 = gHue;
            hue2 = gHue + 128;  // Complementary
            sat = saturation * 255;

            // Shift hue in rivalry zones
            if (rivalryZone > 0.5f) {
                hue1 += (uint8_t)(rivalryZone * 30);
                hue2 -= (uint8_t)(rivalryZone * 30);
            }
        }

        // Apply to strips: different patterns create binocular rivalry
        strip1[i] = CHSV(hue1, sat, brightness1);
        strip2[i] = CHSV(hue2, sat, brightness2);
    }
}
