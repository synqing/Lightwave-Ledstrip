/**
 * @file LGPGeometricEffects.cpp
 * @brief LGP Geometric pattern effects implementation
 *
 * Advanced shapes and patterns leveraging Light Guide Plate optics.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPGeometricEffects.h"
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
static float diamondPhase = 0;
static float hexPhase = 0;
static float spiralPhase = 0;
static uint16_t sierpinskiIteration = 0;
static float chevronPos = 0;
static float ringPhase = 0;
static float starPhase = 0;
static float networkPhase = 0;

// ==================== DIAMOND LATTICE ====================
void effectDiamondLattice(RenderContext& ctx) {
    // CENTER ORIGIN - Angled wave fronts create diamond patterns from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    diamondPhase += speedNorm * 0.02f;

    float diamondFreq = 6.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Create crossing diagonal waves from center
        float wave1 = sin((normalizedDist + diamondPhase) * diamondFreq * TWO_PI);
        float wave2 = sin((normalizedDist - diamondPhase) * diamondFreq * TWO_PI);

        // Interference creates diamond nodes
        float diamond = abs(wave1 * wave2);
        diamond = pow(diamond, 0.5f);

        uint8_t brightness = (uint8_t)(diamond * 255 * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, brightness);
        }
    }
}

// ==================== HEXAGONAL GRID ====================
void effectHexagonalGrid(RenderContext& ctx) {
    // Three waves at 120 degrees create hexagonal patterns

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    hexPhase += speedNorm * 0.01f;

    float hexSize = 10.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float pos = (float)i / STRIP_LENGTH;

        // Three waves at 120 degree angles
        float wave1 = sin(pos * hexSize * TWO_PI + hexPhase);
        float wave2 = sin(pos * hexSize * TWO_PI + hexPhase + TWO_PI / 3);
        float wave3 = sin(pos * hexSize * TWO_PI + hexPhase + 2 * TWO_PI / 3);

        // Multiplicative creates cells
        float pattern = abs(wave1 * wave2 * wave3);
        pattern = pow(pattern, 0.3f);

        uint8_t brightness = (uint8_t)(pattern * 255 * intensityNorm);
        uint8_t hue = ctx.hue + (uint8_t)(pattern * 60) + (i >> 1);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + 60, brightness);
        }
    }
}

// ==================== SPIRAL VORTEX ====================
void effectSpiralVortex(RenderContext& ctx) {
    // CENTER ORIGIN - Creates rotating spiral patterns from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    spiralPhase += speedNorm * 0.05f;

    int spiralArms = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Spiral equation
        float spiralAngle = normalizedDist * spiralArms * TWO_PI + spiralPhase;
        float spiral = sin(spiralAngle);

        // Radial fade
        spiral *= (1 - normalizedDist * 0.5f);

        uint8_t brightness = (uint8_t)(128 + 127 * spiral * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(spiralAngle * 255 / TWO_PI);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, brightness);
        }
    }
}

// ==================== SIERPINSKI TRIANGLES ====================
void effectSierpinskiTriangles(RenderContext& ctx) {
    // CENTER ORIGIN - Fractal triangle patterns through recursive interference

    float intensityNorm = ctx.brightness / 255.0f;

    sierpinskiIteration += ctx.speed >> 2;

    int maxDepth = 5;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        uint16_t x = i;
        uint16_t y = sierpinskiIteration >> 4;

        // XOR creates Sierpinski triangle
        uint16_t pattern = x ^ y;

        // Count bits for fractal depth
        uint8_t bitCount = 0;
        for (int d = 0; d < maxDepth; d++) {
            if (pattern & (1 << d)) bitCount++;
        }

        float smooth = sin(bitCount * PI / maxDepth);

        uint8_t brightness = (uint8_t)(smooth * 255 * intensityNorm);
        uint8_t hue = ctx.hue + (bitCount * 30);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + 128, brightness);
        }
    }
}

// ==================== CHEVRON WAVES ====================
void effectChevronWaves(RenderContext& ctx) {
    // CENTER ORIGIN - V-shaped patterns from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    chevronPos += speedNorm * 2;

    float chevronCount = 6.0f;
    float chevronAngle = 1.5f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 40);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);

        // Create V-shape from center
        float chevronPhase = distFromCenter * chevronAngle + chevronPos;
        float chevron = sin(chevronPhase * chevronCount * 0.1f);

        // Sharp edges
        chevron = tanh(chevron * 3) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(chevron * 255 * intensityNorm);
        uint8_t hue = ctx.hue + (uint8_t)(distFromCenter * 2) + (uint8_t)(chevronPos * 0.5f);

        ctx.leds[i] += CHSV(hue, 255, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] += CHSV(hue + 90, 255, brightness);
        }
    }
}

// ==================== CONCENTRIC RINGS ====================
void effectConcentricRings(RenderContext& ctx) {
    // CENTER ORIGIN - Radial standing waves create ring patterns

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    ringPhase += speedNorm * 0.1f;

    float ringCount = 10.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Bessel function-like
        float bessel = sin(distFromCenter * ringCount * 0.2f + ringPhase);
        bessel *= 1.0f / sqrt(normalizedDist + 0.1f);

        // Sharp ring edges
        float rings = tanh(bessel * 2);

        uint8_t brightness = (uint8_t)(128 + 127 * rings * intensityNorm);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + 128, brightness);
        }
    }
}

// ==================== STAR BURST ====================
void effectStarBurst(RenderContext& ctx) {
    // CENTER ORIGIN - Star-like patterns radiating from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    starPhase += speedNorm * 0.03f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs(i - CENTER_LEFT);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Star equation - radially symmetric from center
        float star = sin(distFromCenter * 0.3f + starPhase) * exp(-normalizedDist * 2);

        // Pulsing
        star *= 0.5f + 0.5f * sin(starPhase * 3);

        uint8_t brightness = (uint8_t)(128 + 127 * star * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter + star * 50);

        ctx.leds[i] += ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] += ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 85, brightness);
        }
    }
}

// ==================== MESH NETWORK ====================
void effectMeshNetwork(RenderContext& ctx) {
    // CENTER ORIGIN - Interconnected node patterns like neural networks

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    networkPhase += speedNorm * 0.02f;

    int nodeCount = 12;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 50);

    for (int n = 0; n < nodeCount; n++) {
        float nodePos = (float)n / nodeCount * STRIP_LENGTH;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distToNode = abs(i - nodePos);

            if (distToNode < 3) {
                // Node core
                uint8_t nodeBright = (uint8_t)(255 * intensityNorm);
                ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + (n * 20), nodeBright);
                if (i + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + (n * 20) + 128, nodeBright);
                }
            } else if (distToNode < 20) {
                // Connections to nearby nodes
                float connection = sin(distToNode * 0.5f + networkPhase + n);
                connection *= exp(-distToNode * 0.1f);

                uint8_t connBright = (uint8_t)(abs(connection) * 128 * intensityNorm);
                ctx.leds[i] += ColorFromPalette(*ctx.palette, ctx.hue + (n * 20), connBright);
                if (i + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[i + STRIP_LENGTH] += ColorFromPalette(*ctx.palette, ctx.hue + (n * 20) + 128, connBright);
                }
            }
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPGeometricEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Diamond Lattice", effectDiamondLattice)) count++;
    if (renderer->registerEffect(startId + count, "LGP Hexagonal Grid", effectHexagonalGrid)) count++;
    if (renderer->registerEffect(startId + count, "LGP Spiral Vortex", effectSpiralVortex)) count++;
    if (renderer->registerEffect(startId + count, "LGP Sierpinski", effectSierpinskiTriangles)) count++;
    if (renderer->registerEffect(startId + count, "LGP Chevron Waves", effectChevronWaves)) count++;
    if (renderer->registerEffect(startId + count, "LGP Concentric Rings", effectConcentricRings)) count++;
    if (renderer->registerEffect(startId + count, "LGP Star Burst", effectStarBurst)) count++;
    if (renderer->registerEffect(startId + count, "LGP Mesh Network", effectMeshNetwork)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
