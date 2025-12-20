/**
 * @file LGPNovelPhysicsEffects.cpp
 * @brief LGP Novel Physics effects implementation
 *
 * Advanced effects exploiting dual-edge optical interference properties.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPNovelPhysicsEffects.h"
#include "CoreEffects.h"
#include <FastLED.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {

using namespace lightwaveos::actors;

// ==================== Static State ====================

// Chladni Harmonics
static float vibrationPhase = 0;
static float mixPhase = 0;

// Gravitational Wave Chirp
static float inspiralProgress = 0;
static float ringdownPhase = 0;
static bool gwMerging = false;
static bool gwRingdown = false;
static float mergeFlash = 0;
static float gwPhase1 = 0, gwPhase2 = 0;

// Quantum Entanglement
static float collapseRadius = 0;
static bool qeCollapsing = false;
static bool qeCollapsed = false;
static float holdTime = 0;
static uint8_t collapsedHue = 0;
static float quantumPhase = 0;
static float measurementTimer = 0;

// Mycelial Network
static float tipPositions[16];
static float tipVelocities[16];
static bool tipActive[16];
static float tipAge[16];
static bool myceliumInitialized = false;
static float nutrientPhase = 0;
static float networkDensity[160];

// Riley Dissonance
static float patternPhase = 0;

// ==================== CHLADNI HARMONICS ====================
void effectChladniHarmonics(RenderContext& ctx) {
    // Visualizes acoustic resonance patterns on vibrating plates

    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    int modeNumber = 4;

    vibrationPhase += speed * 0.08f;
    mixPhase += speed * 0.05f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 15);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedPos = distFromCenter / (float)HALF_LENGTH;

        // Mode shape: standing wave pattern
        float modeShape = sin(modeNumber * PI * normalizedPos);

        // Add mixing with nearby modes
        float mix1 = sin((modeNumber + 1) * PI * normalizedPos) * sin(mixPhase);
        float mix2 = sin((modeNumber - 1) * PI * normalizedPos) * cos(mixPhase * 1.3f);
        float mixedMode = modeShape * 0.75f + (mix1 + mix2) * 0.25f * 0.5f;

        // Temporal oscillation
        float temporalOscillation = cos(vibrationPhase);
        float plateDisplacement = mixedMode * temporalOscillation;

        // Sand particle visualization
        float nodeStrength = 1.0f / (abs(modeShape) + 0.1f);
        nodeStrength = constrain(nodeStrength, 0.0f, 3.0f);

        float antinodeStrength = abs(plateDisplacement) * intensity;

        float particleBrightness = nodeStrength * (1 - intensity) * 0.3f;
        float motionBrightness = antinodeStrength * intensity;
        float totalBrightness = (particleBrightness + motionBrightness) * 255;

        uint8_t brightness = (uint8_t)constrain(totalBrightness, 20.0f, 255.0f);

        uint8_t hue1 = ctx.hue + (uint8_t)(plateDisplacement * 30);
        uint8_t hue2 = ctx.hue + 128 + (uint8_t)(plateDisplacement * 30);

        ctx.leds[i] = CHSV(hue1, 200, brightness);

        if (i + STRIP_LENGTH < ctx.numLeds) {
            float bottomDisplacement = -plateDisplacement;
            float bottomBrightness = (particleBrightness + abs(bottomDisplacement) * intensity) * 255;
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, 200, (uint8_t)constrain(bottomBrightness, 20.0f, 255.0f));
        }
    }
}

// ==================== GRAVITATIONAL WAVE CHIRP ====================
void effectGravitationalWaveChirp(RenderContext& ctx) {
    // Binary black hole inspiral with LIGO-accurate frequency evolution

    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    float chirpRate = 0.002f + speed * 0.008f;
    float massRatio = 1.0f;

    if (!gwMerging && !gwRingdown) {
        inspiralProgress += chirpRate;

        if (inspiralProgress >= 1.0f) {
            gwMerging = true;
            mergeFlash = 1.0f;
        }
    } else if (gwMerging) {
        mergeFlash *= 0.92f;
        if (mergeFlash < 0.05f) {
            gwMerging = false;
            gwRingdown = true;
            ringdownPhase = 0;
        }
    } else if (gwRingdown) {
        ringdownPhase += 0.15f + 0.1f;
        float ringdownDecay = exp(-ringdownPhase * 0.05f);

        if (ringdownDecay < 0.01f) {
            gwRingdown = false;
            inspiralProgress = 0;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.numLeds, 25);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float wave1 = 0, wave2 = 0;

        if (!gwMerging && !gwRingdown) {
            float t_remaining = fmax(0.01f, 1.0f - inspiralProgress);
            float chirpFreq = pow(t_remaining, -3.0f / 8.0f * massRatio);
            chirpFreq = constrain(chirpFreq, 1.0f, 20.0f);

            float amplitude = intensity * (1 + inspiralProgress * 2);

            gwPhase1 += chirpFreq * 0.1f;
            gwPhase2 = gwPhase1 + PI / 2;

            float compressionFactor = 1 + inspiralProgress * 3;
            float spatialPhase = normalizedDist * chirpFreq * compressionFactor;

            wave1 = sin(spatialPhase - gwPhase1) * amplitude * (1 - normalizedDist);
            wave2 = sin(spatialPhase - gwPhase2) * amplitude * (1 - normalizedDist);

        } else if (gwMerging) {
            float flashRadius = 0.3f + (1 - mergeFlash) * 0.5f;
            if (normalizedDist < flashRadius) {
                float flashIntensity = mergeFlash * (1 - normalizedDist / flashRadius);
                wave1 = flashIntensity * intensity * 2;
                wave2 = flashIntensity * intensity * 2;
            }

        } else if (gwRingdown) {
            float ringdownFreq = 10.0f;
            float ringdownDecay = exp(-ringdownPhase * 0.05f);
            float ringRadius = ringdownPhase * 0.1f;

            float distToRing = abs(normalizedDist - fmod(ringRadius, 1.0f));
            if (distToRing < 0.2f) {
                float ringShape = cos(distToRing / 0.2f * PI / 2);
                wave1 = sin(ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
                wave2 = cos(ringdownPhase * ringdownFreq) * ringShape * ringdownDecay * intensity;
            }
        }

        uint8_t brightness1 = (uint8_t)(128 + constrain((int)(wave1 * 127), -127, 127));
        uint8_t brightness2 = (uint8_t)(128 + constrain((int)(wave2 * 127), -127, 127));

        uint8_t baseHue = 200;
        if (gwMerging) baseHue = 40;
        else if (gwRingdown) baseHue = 160;

        ctx.leds[i] = CHSV(baseHue + ctx.hue, 200, brightness1);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(baseHue + ctx.hue + 30, 200, brightness2);
        }
    }
}

// ==================== QUANTUM ENTANGLEMENT COLLAPSE ====================
void effectQuantumEntanglementCollapse(RenderContext& ctx) {
    // EPR paradox visualization with superposition and measurement

    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    int quantumN = 4;

    quantumPhase += speed * 0.1f;

    if (!qeCollapsing && !qeCollapsed) {
        measurementTimer += speed * 0.01f;

        if (measurementTimer > 1.0f + random8() / 255.0f) {
            qeCollapsing = true;
            collapseRadius = 0;
            collapsedHue = ctx.hue + random8();
            measurementTimer = 0;
        }
    } else if (qeCollapsing) {
        collapseRadius += speed * 0.02f;

        if (collapseRadius >= 1.0f) {
            qeCollapsing = false;
            qeCollapsed = true;
            holdTime = 0;
        }
    } else if (qeCollapsed) {
        holdTime += speed * 0.02f;

        if (holdTime > 1.5f) {
            qeCollapsed = false;
            collapseRadius = 0;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        uint8_t hue1, hue2, brightness1, brightness2;

        if (!qeCollapsing && !qeCollapsed) {
            float waveFunc = sin(quantumN * PI * normalizedDist);
            float probability = waveFunc * waveFunc;

            float fluctuation = sin(quantumPhase * 3 + i * 0.2f) *
                               cos(quantumPhase * 5 - i * 0.15f) * intensity;

            hue1 = ctx.hue + (uint8_t)(sin(quantumPhase + i * 0.1f) * 15);
            hue2 = ctx.hue + (uint8_t)(cos(quantumPhase * 1.3f - i * 0.12f) * 15);

            brightness1 = (uint8_t)(80 + probability * 100 + abs(fluctuation) * 75);
            brightness2 = (uint8_t)(80 + probability * 100 + abs(fluctuation) * 75);

        } else if (qeCollapsing) {
            if (normalizedDist < collapseRadius) {
                hue1 = collapsedHue;
                hue2 = collapsedHue + 128;

                float collapseEdge = collapseRadius - normalizedDist;
                float edgeFactor = constrain(collapseEdge * 10, 0.0f, 1.0f);

                brightness1 = (uint8_t)(180 * edgeFactor + 50);
                brightness2 = (uint8_t)(180 * edgeFactor + 50);

            } else {
                float chaos = sin(quantumPhase * 5 + i * 0.3f) * intensity;
                hue1 = ctx.hue + (uint8_t)(chaos * 40);
                hue2 = ctx.hue + (uint8_t)(chaos * 40) + random8(30);
                brightness1 = (uint8_t)(60 + abs(chaos) * 50);
                brightness2 = (uint8_t)(60 + abs(chaos) * 50);
            }

        } else {
            hue1 = collapsedHue;
            hue2 = collapsedHue + 128;

            float pulse = sin(quantumPhase) * 0.1f + 0.9f;
            brightness1 = (uint8_t)(200 * pulse);
            brightness2 = (uint8_t)(200 * pulse);
        }

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue1, brightness1);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue2, brightness2);
        }
    }
}

// ==================== MYCELIAL NETWORK ====================
void effectMycelialNetwork(RenderContext& ctx) {
    // Fungal hyphal growth with fractal branching and nutrient flow

    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    nutrientPhase += speed * 0.05f;

    // Initialize tips from center
    if (!myceliumInitialized) {
        for (int t = 0; t < 16; t++) {
            tipPositions[t] = CENTER_LEFT;
            tipVelocities[t] = 0;
            tipActive[t] = false;
            tipAge[t] = 0;
        }
        tipActive[0] = true;
        tipVelocities[0] = 0.5f;
        tipActive[1] = true;
        tipVelocities[1] = -0.5f;
        myceliumInitialized = true;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            networkDensity[i] = 0;
        }
    }

    float branchProbability = 0.005f;
    uint8_t numTips = 8;

    // Update tips
    for (int t = 0; t < 16; t++) {
        if (tipActive[t]) {
            tipPositions[t] += tipVelocities[t] * speed;
            tipAge[t] += speed * 0.01f;

            if (tipPositions[t] < 0 || tipPositions[t] >= STRIP_LENGTH) {
                tipActive[t] = false;
            }

            if (random8() < (uint8_t)(branchProbability * 255)) {
                for (int newTip = 0; newTip < numTips; newTip++) {
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
            tipActive[t] = true;
            tipPositions[t] = CENTER_LEFT;
            tipVelocities[t] = (random8() > 127 ? 1 : -1) * (0.3f + random8() / 255.0f * 0.4f);
            tipAge[t] = 0;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.numLeds, 8);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        networkDensity[i] *= 0.998f;

        float tipGlow = 0;
        for (int t = 0; t < 16; t++) {
            if (tipActive[t]) {
                float distToTip = abs(i - tipPositions[t]);
                if (distToTip < 5) {
                    tipGlow += (5 - distToTip) / 5.0f * intensity;
                    networkDensity[i] = fmin(1.0f, networkDensity[i] + 0.02f);
                }
            }
        }

        float flowDirection = 0.5f;
        float nutrientWave = sin(normalizedDist * 10 - nutrientPhase * flowDirection * 3);
        float nutrientBrightness = networkDensity[i] * (0.5f + nutrientWave * 0.5f);

        uint8_t hue1 = 140 + (uint8_t)(ctx.hue * 0.3f);
        uint8_t hue2 = 160 + (uint8_t)(ctx.hue * 0.3f);

        float brightness1 = tipGlow * 200 + networkDensity[i] * 80 + nutrientBrightness * 60;
        float brightness2 = tipGlow * 150 + networkDensity[i] * 90 + nutrientBrightness * 70;

        if (brightness1 > 100 && brightness2 > 100) {
            hue1 = 40 + (uint8_t)(ctx.hue * 0.2f);
            hue2 = 50 + (uint8_t)(ctx.hue * 0.2f);
            brightness1 = fmin(255.0f, brightness1 * 1.3f);
            brightness2 = fmin(255.0f, brightness2 * 1.3f);
        }

        ctx.leds[i] = CHSV(hue1, 200, (uint8_t)constrain(brightness1, 0.0f, 255.0f));
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, 200, (uint8_t)constrain(brightness2, 0.0f, 255.0f));
        }
    }
}

// ==================== RILEY DISSONANCE ====================
void effectRileyDissonance(RenderContext& ctx) {
    // Op Art perceptual instability inspired by Bridget Riley

    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;
    float complexity = 0.5f;

    patternPhase += speed * 0.02f;

    float baseFreq = 12.0f;
    float freqMismatch = 0.08f;
    float freq1 = baseFreq * (1 + freqMismatch / 2);
    float freq2 = baseFreq * (1 - freqMismatch / 2);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;
        float position = (float)i / STRIP_LENGTH;

        float pattern1 = 0, pattern2 = 0;

        // CONCENTRIC CIRCLES
        pattern1 = sin(normalizedDist * freq1 * TWO_PI + patternPhase);
        pattern2 = sin(normalizedDist * freq2 * TWO_PI - patternPhase);

        // Apply contrast enhancement
        float contrast = 1 + intensity * 4;
        pattern1 = tanh(pattern1 * contrast) / tanh(contrast);
        pattern2 = tanh(pattern2 * contrast) / tanh(contrast);

        float rivalryZone = abs(pattern1 - pattern2);

        uint8_t brightness1 = (uint8_t)(128 + (int)(pattern1 * 127 * intensity));
        uint8_t brightness2 = (uint8_t)(128 + (int)(pattern2 * 127 * intensity));

        // Complementary colors increase dissonance
        uint8_t hue1 = ctx.hue;
        uint8_t hue2 = ctx.hue + 128;
        uint8_t sat = 200;

        if (rivalryZone > 0.5f) {
            hue1 += (uint8_t)(rivalryZone * 30);
            hue2 -= (uint8_t)(rivalryZone * 30);
        }

        ctx.leds[i] = CHSV(hue1, sat, brightness1);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, sat, brightness2);
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPNovelPhysicsEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Chladni Harmonics", effectChladniHarmonics)) count++;
    if (renderer->registerEffect(startId + count, "LGP Gravitational Wave Chirp", effectGravitationalWaveChirp)) count++;
    if (renderer->registerEffect(startId + count, "LGP Quantum Entanglement", effectQuantumEntanglementCollapse)) count++;
    if (renderer->registerEffect(startId + count, "LGP Mycelial Network", effectMycelialNetwork)) count++;
    if (renderer->registerEffect(startId + count, "LGP Riley Dissonance", effectRileyDissonance)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
