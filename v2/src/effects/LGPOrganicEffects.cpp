/**
 * @file LGPOrganicEffects.cpp
 * @brief LGP Organic pattern effects implementation
 *
 * Natural and fluid patterns leveraging Light Guide Plate diffusion.
 * These effects create organic, living visuals through optical blending.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPOrganicEffects.h"
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
static uint16_t auroraTime = 0;
static uint8_t curtainPhase[5] = {0, 51, 102, 153, 204};

static uint16_t bioWavePhase = 0;
static uint8_t glowPoints[20];
static uint8_t glowLife[20];
static bool bioInitialized = false;

static uint16_t membraneTime = 0;

static uint16_t neuralTime = 0;
static uint8_t neurons[20];
static uint8_t neuronState[20];
static int8_t signalPos[10];
static uint8_t signalStrength[10];
static bool neuralInitialized = false;

static uint16_t crystalTime = 0;
static uint8_t crystalSeeds[10];
static uint8_t crystalSize[10];
static uint8_t crystalHue[10];
static bool crystalInitialized = false;

static uint16_t fluidTime = 0;
static float velocity[160];
static float pressure[160];

// ==================== AURORA BOREALIS ====================
void effectAuroraBorealis(RenderContext& ctx) {
    // Northern lights simulation with waveguide color mixing

    auroraTime += ctx.speed >> 4;

    uint8_t curtainCount = 4;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (uint8_t c = 0; c < curtainCount; c++) {
        curtainPhase[c] += (c + 1);
        uint16_t curtainCenter = beatsin16(1, 20, STRIP_LENGTH - 20, 0, curtainPhase[c] << 8);
        uint8_t curtainWidth = beatsin8(1, 20, 35, 0, curtainPhase[c]);

        // Aurora colors - greens, blues, purples
        uint8_t hue = 96 + (c * 32);

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            int16_t dist = abs((int16_t)i - curtainCenter);
            if (dist < curtainWidth) {
                uint8_t brightness = qsub8(255, (dist * 255) / curtainWidth);
                brightness = scale8(brightness, ctx.brightness);

                // Shimmer effect
                brightness = scale8(brightness, 220 + (inoise8(i * 5, auroraTime >> 3) >> 3));

                CRGB color1 = ColorFromPalette(*ctx.palette, hue, brightness);
                CRGB color2 = ColorFromPalette(*ctx.palette, hue + 20, brightness);

                ctx.leds[i] += color1;
                if (i + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[i + STRIP_LENGTH] += color2;
                }
            }
        }
    }

    // Add corona at edges
    for (uint8_t i = 0; i < 20; i++) {
        uint8_t corona = scale8(255 - i * 12, ctx.brightness >> 1);
        ctx.leds[i] += CRGB(0, corona >> 2, corona >> 1);
        ctx.leds[STRIP_LENGTH - 1 - i] += CRGB(0, corona >> 2, corona >> 1);
        if (STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[STRIP_LENGTH + i] += CRGB(0, corona >> 3, corona);
            ctx.leds[ctx.numLeds - 1 - i] += CRGB(0, corona >> 3, corona);
        }
    }
}

// ==================== BIOLUMINESCENT WAVES ====================
void effectBioluminescentWaves(RenderContext& ctx) {
    // Ocean waves with glowing plankton effect

    bioWavePhase += ctx.speed;

    uint8_t waveCount = 4;

    // Base ocean color
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t wave = 0;
        for (uint8_t w = 0; w < waveCount; w++) {
            wave += sin8(((i << 2) + (bioWavePhase >> (4 - w))) >> w);
        }
        wave /= waveCount;

        uint8_t blue = scale8(wave, 60);
        uint8_t green = scale8(wave, 20);

        ctx.leds[i] = CRGB(0, green, blue);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(0, green >> 1, blue);
        }
    }

    // Spawn new glow points occasionally
    if (ctx.frameCount % 12 == 0) {
        for (uint8_t g = 0; g < 20; g++) {
            if (glowLife[g] == 0) {
                glowPoints[g] = random8(STRIP_LENGTH);
                glowLife[g] = 255;
                break;
            }
        }
    }

    // Update and render glow points
    for (uint8_t g = 0; g < 20; g++) {
        if (glowLife[g] > 0) {
            glowLife[g] = scale8(glowLife[g], 240);

            uint8_t pos = glowPoints[g];
            uint8_t intensity = scale8(glowLife[g], ctx.brightness);

            for (int8_t spread = -3; spread <= 3; spread++) {
                int16_t p = pos + spread;
                if (p >= 0 && p < STRIP_LENGTH) {
                    uint8_t spreadIntensity = scale8(intensity, 255 - abs(spread) * 60);
                    ctx.leds[p] += CRGB(0, spreadIntensity >> 1, spreadIntensity);
                    if (p + STRIP_LENGTH < ctx.numLeds) {
                        ctx.leds[p + STRIP_LENGTH] += CRGB(0, spreadIntensity >> 2, spreadIntensity);
                    }
                }
            }
        }
    }
}

// ==================== PLASMA MEMBRANE ====================
void effectPlasmaMembrane(RenderContext& ctx) {
    // Organic cellular membrane with lipid bilayer dynamics

    membraneTime += ctx.speed >> 1;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Membrane shape using multiple octaves
        uint16_t membrane = 0;
        membrane += inoise8(i * 3, membraneTime >> 2) << 1;
        membrane += inoise8(i * 7, membraneTime >> 1) >> 1;
        membrane += inoise8(i * 13, membraneTime);
        membrane >>= 2;

        uint8_t hue = 20 + (membrane >> 3);
        uint8_t sat = 200 + (membrane >> 2);
        uint8_t brightness = scale8(membrane, ctx.brightness);

        CRGB inner = CHSV(hue, sat, brightness);
        CRGB outer = CHSV(hue + 10, sat - 50, scale8(brightness, 200));

        ctx.leds[i] = inner;
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = outer;
        }
    }

    // Add membrane potential waves
    uint16_t potentialWave = beatsin16(5, 0, STRIP_LENGTH - 1);
    for (int8_t w = -10; w <= 10; w++) {
        int16_t pos = potentialWave + w;
        if (pos >= 0 && pos < STRIP_LENGTH) {
            uint8_t waveIntensity = 255 - abs(w) * 20;
            ctx.leds[pos] = blend(ctx.leds[pos], CRGB::Yellow, waveIntensity);
            if (pos + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[pos + STRIP_LENGTH] = blend(ctx.leds[pos + STRIP_LENGTH], CRGB::Gold, waveIntensity);
            }
        }
    }
}

// ==================== NEURAL NETWORK ====================
void effectNeuralNetwork(RenderContext& ctx) {
    // Synaptic firing patterns with signal propagation

    neuralTime += ctx.speed >> 2;

    // Initialize neurons on first run
    if (!neuralInitialized) {
        for (uint8_t n = 0; n < 20; n++) {
            neurons[n] = random8(STRIP_LENGTH);
            neuronState[n] = 0;
        }
        for (uint8_t s = 0; s < 10; s++) {
            signalStrength[s] = 0;
        }
        neuralInitialized = true;
    }

    // Background neural tissue
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t tissue = inoise8(i * 5, neuralTime >> 3) >> 2;
        ctx.leds[i] = CRGB(tissue >> 1, 0, tissue);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(tissue >> 2, 0, tissue >> 1);
        }
    }

    // Update neurons
    for (uint8_t n = 0; n < 20; n++) {
        if (neuronState[n] > 0) {
            neuronState[n] = scale8(neuronState[n], 230);
        } else {
            if (random8() < 16) {
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
        uint8_t intensity = scale8(neuronState[n], ctx.brightness);
        CRGB neuronColor = CRGB(intensity, intensity >> 3, intensity >> 1);

        if (pos < STRIP_LENGTH) {
            ctx.leds[pos] = neuronColor;
            if (pos + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[pos + STRIP_LENGTH] = neuronColor;
            }
        }

        // Dendrites
        for (int8_t d = -2; d <= 2; d++) {
            if (d != 0) {
                int16_t dendPos = pos + d;
                if (dendPos >= 0 && dendPos < STRIP_LENGTH) {
                    uint8_t dendIntensity = intensity >> (1 + abs(d));
                    ctx.leds[dendPos] += CRGB(dendIntensity >> 2, 0, dendIntensity >> 3);
                    if (dendPos + STRIP_LENGTH < ctx.numLeds) {
                        ctx.leds[dendPos + STRIP_LENGTH] += CRGB(dendIntensity >> 3, 0, dendIntensity >> 2);
                    }
                }
            }
        }
    }

    // Update and render signals
    for (uint8_t s = 0; s < 10; s++) {
        if (signalStrength[s] > 0) {
            signalPos[s] += (random8(2) == 0) ? 1 : -1;
            signalStrength[s] = scale8(signalStrength[s], 240);

            if (signalPos[s] >= 0 && signalPos[s] < STRIP_LENGTH) {
                uint8_t sigIntensity = scale8(signalStrength[s], ctx.brightness);
                CRGB sigColor = CRGB(sigIntensity >> 1, sigIntensity >> 2, sigIntensity);

                ctx.leds[signalPos[s]] += sigColor;
                if (signalPos[s] + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[signalPos[s] + STRIP_LENGTH] += sigColor;
                }
            }
        }
    }
}

// ==================== CRYSTALLINE GROWTH ====================
void effectCrystallineGrowth(RenderContext& ctx) {
    // Crystal formation with light refraction

    crystalTime += ctx.speed >> 3;

    // Initialize crystal seeds
    if (!crystalInitialized) {
        for (uint8_t c = 0; c < 10; c++) {
            crystalSeeds[c] = random8(STRIP_LENGTH);
            crystalSize[c] = 0;
            crystalHue[c] = random8();
        }
        crystalInitialized = true;
    }

    // Background substrate
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t substrate = 20 + (inoise8(i * 10, crystalTime) >> 4);
        ctx.leds[i] = CRGB(substrate >> 2, substrate >> 2, substrate);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(substrate >> 3, substrate >> 3, substrate >> 1);
        }
    }

    // Update crystals
    for (uint8_t c = 0; c < 10; c++) {
        // Grow crystals
        if (crystalSize[c] < 20 && random8() < 32) {
            crystalSize[c]++;
        }

        // Reset fully grown crystals occasionally
        if (crystalSize[c] >= 20 && random8() < 5) {
            crystalSize[c] = 0;
            crystalSeeds[c] = random8(STRIP_LENGTH);
            crystalHue[c] = random8(30);
        }

        // Render crystal
        uint8_t pos = crystalSeeds[c];

        for (int8_t facet = -crystalSize[c]; facet <= crystalSize[c]; facet++) {
            int16_t facetPos = pos + facet;
            if (facetPos >= 0 && facetPos < STRIP_LENGTH) {
                uint8_t facetBrightness = 255 - (abs(facet) * 255 / (crystalSize[c] + 1));
                facetBrightness = scale8(facetBrightness, ctx.brightness);

                uint8_t paletteIndex = crystalHue[c] + abs(facet);

                CRGB color1 = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, facetBrightness);
                CRGB color2 = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 30, scale8(facetBrightness, 200));

                ctx.leds[facetPos] = blend(ctx.leds[facetPos], color1, 128);
                if (facetPos + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[facetPos + STRIP_LENGTH] = blend(ctx.leds[facetPos + STRIP_LENGTH], color2, 128);
                }
            }
        }
    }
}

// ==================== FLUID DYNAMICS ====================
void effectFluidDynamics(RenderContext& ctx) {
    // Laminar and turbulent flow visualization

    fluidTime += ctx.speed >> 2;

    float speedNorm = ctx.speed / 50.0f;
    float reynolds = speedNorm;

    // Update fluid simulation
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Pressure gradient
        float gradientForce = 0;
        if (i > 0 && i < STRIP_LENGTH - 1) {
            gradientForce = (pressure[i - 1] - pressure[i + 1]) * 0.1f;
        }

        // Turbulence
        float turbulence = (inoise8(i * 5, fluidTime) - 128) / 128.0f * reynolds;

        // Update velocity
        velocity[i] += gradientForce + turbulence * 0.1f;
        velocity[i] *= 0.95f;

        // Update pressure
        pressure[i] += velocity[i] * 0.1f;
        pressure[i] *= 0.98f;

        // Add source/sink at center
        if (abs(i - CENTER_LEFT) < 5) {
            pressure[i] += sin8(fluidTime >> 2) / 255.0f;
        }
    }

    // Render flow
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t speed8 = (uint8_t)constrain(abs(velocity[i]) * 255, 0, 255);

        uint8_t brightness = (uint8_t)constrain((pressure[i] + 1.0f) * 127, 0, 255);
        brightness = scale8(brightness, ctx.brightness);

        float distFromCenter = abs((int)i - CENTER_LEFT);
        uint8_t paletteIndex = (uint8_t)(velocity[i] * 20) + (uint8_t)(distFromCenter / 4);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 60, scale8(brightness, 200 + speed8 / 4));
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPOrganicEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Aurora Borealis", effectAuroraBorealis)) count++;
    if (renderer->registerEffect(startId + count, "LGP Bioluminescent Waves", effectBioluminescentWaves)) count++;
    if (renderer->registerEffect(startId + count, "LGP Plasma Membrane", effectPlasmaMembrane)) count++;
    if (renderer->registerEffect(startId + count, "LGP Neural Network", effectNeuralNetwork)) count++;
    if (renderer->registerEffect(startId + count, "LGP Crystalline Growth", effectCrystallineGrowth)) count++;
    if (renderer->registerEffect(startId + count, "LGP Fluid Dynamics", effectFluidDynamics)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
