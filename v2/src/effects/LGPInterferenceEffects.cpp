/**
 * @file LGPInterferenceEffects.cpp
 * @brief LGP Interference pattern effects implementation
 *
 * These effects exploit optical waveguide properties to create interference patterns.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPInterferenceEffects.h"
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
static float boxMotionPhase = 0;
static float holoPhase1 = 0, holoPhase2 = 0, holoPhase3 = 0;
static float modalModePhase = 0;
static float scanPhase = 0, scanPhase2 = 0;
static float wave1Pos = 0, wave2Pos = 0;

// ==================== BOX WAVE ====================
void effectBoxWave(RenderContext& ctx) {
    // CENTER ORIGIN BOX WAVE - Creates controllable standing wave boxes from center

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // Box count: 3-12 boxes
    float boxesPerSide = 6.0f;
    float spatialFreq = boxesPerSide * PI / HALF_LENGTH;

    boxMotionPhase += speedNorm * 0.05f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Base box pattern from center
        float boxPhase = distFromCenter * spatialFreq;
        float boxPattern = sin(boxPhase + boxMotionPhase);

        // Sharpness control
        boxPattern = tanh(boxPattern * 2.0f);

        // Convert to brightness
        uint8_t brightness = (uint8_t)(128 + 127 * boxPattern * intensityNorm);

        // Color wave overlay
        uint8_t colorIndex = ctx.hue + (uint8_t)(distFromCenter * 2);

        // Apply to both strips
        CRGB color = ColorFromPalette(*ctx.palette, colorIndex, brightness);
        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, colorIndex + 128, brightness);
        }
    }
}

// ==================== HOLOGRAPHIC ====================
void effectHolographic(RenderContext& ctx) {
    // CENTER ORIGIN HOLOGRAPHIC - Creates depth illusion through multi-layer interference

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    holoPhase1 += speedNorm * 0.02f;
    holoPhase2 += speedNorm * 0.03f;
    holoPhase3 += speedNorm * 0.05f;

    int numLayers = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);
        float normalized = dist / (float)HALF_LENGTH;

        float layerSum = 0;

        // Layer 1 - Slow, wide pattern
        layerSum += sin(dist * 0.05f + holoPhase1);

        // Layer 2 - Medium pattern
        layerSum += sin(dist * 0.15f + holoPhase2) * 0.7f;

        // Layer 3 - Fast, tight pattern
        layerSum += sin(dist * 0.3f + holoPhase3) * 0.5f;

        // Layer 4 - Very fast shimmer
        layerSum += sin(dist * 0.6f - holoPhase1 * 3) * 0.3f;

        // Normalize
        layerSum = layerSum / numLayers;
        layerSum = tanh(layerSum);

        uint8_t brightness = (uint8_t)(128 + 127 * layerSum * intensityNorm);

        // Chromatic dispersion effect
        uint8_t paletteIndex1 = (uint8_t)(dist * 0.5f) + (uint8_t)(layerSum * 20);
        uint8_t paletteIndex2 = 128 - (uint8_t)(dist * 0.5f) - (uint8_t)(layerSum * 20);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex1, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex2, brightness);
        }
    }
}

// ==================== MODAL RESONANCE ====================
void effectModalResonance(RenderContext& ctx) {
    // CENTER ORIGIN MODAL RESONANCE - Explores different optical cavity modes

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    modalModePhase += speedNorm * 0.01f;

    // Mode number varies with time
    float baseMode = 5 + sin(modalModePhase) * 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Mode pattern
        float modalPattern = sin(normalizedDist * baseMode * TWO_PI);

        // Add harmonic
        modalPattern += sin(normalizedDist * baseMode * 2 * TWO_PI) * 0.5f;
        modalPattern /= 1.5f;

        // Apply window function
        float window = sin(normalizedDist * PI);
        modalPattern *= window;

        uint8_t brightness = (uint8_t)(128 + 127 * modalPattern * intensityNorm);

        uint8_t paletteIndex = (uint8_t)(baseMode * 10) + (uint8_t)(normalizedDist * 50);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, brightness);
        }
    }
}

// ==================== INTERFERENCE SCANNER ====================
void effectInterferenceScanner(RenderContext& ctx) {
    // CENTER ORIGIN INTERFERENCE SCANNER - Creates scanning interference patterns

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    scanPhase += speedNorm * 0.05f;
    scanPhase2 += speedNorm * 0.03f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = dist / (float)HALF_LENGTH;

        // Radial scan from center
        float ringRadius = fmod(scanPhase * 30, (float)HALF_LENGTH);
        float ringWidth = 15.0f;

        float pattern = 0;
        if (abs(dist - ringRadius) < ringWidth) {
            pattern = cos((dist - ringRadius) / ringWidth * PI / 2);
        }

        // Add dual sweep interference
        float wave1 = sin(dist * 0.1f + scanPhase);
        float wave2 = sin(dist * 0.1f - scanPhase2);
        pattern += (wave1 + wave2) / 4.0f;

        // Clamp
        pattern = fmax(-1.0f, fmin(1.0f, pattern));

        uint8_t brightness = (uint8_t)((pattern * 0.5f + 0.5f) * 255 * intensityNorm);

        uint8_t paletteIndex = (uint8_t)(dist * 2) + (uint8_t)(pattern * 50);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, 255 - brightness);
        }
    }
}

// ==================== WAVE COLLISION ====================
void effectWaveCollision(RenderContext& ctx) {
    // CENTER ORIGIN WAVE COLLISION - Simulates wave packets colliding in light guide

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // Update wave positions - bounce between edges
    wave1Pos += speedNorm * 2.0f;
    wave2Pos -= speedNorm * 2.0f;

    // Bounce at edges
    if (wave1Pos > STRIP_LENGTH) {
        wave1Pos = STRIP_LENGTH;
        // Reverse direction by swapping
        float temp = wave1Pos;
        wave1Pos = wave2Pos;
        wave2Pos = temp;
    }
    if (wave2Pos < 0) {
        wave2Pos = 0;
    }

    // Keep waves in bounds using modulo
    float effectiveWave1 = fmod(ctx.frameCount * speedNorm * 0.5f, (float)STRIP_LENGTH);
    float effectiveWave2 = STRIP_LENGTH - fmod(ctx.frameCount * speedNorm * 0.5f, (float)STRIP_LENGTH);

    fadeToBlackBy(ctx.leds, ctx.numLeds, 30);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        // Wave packet 1
        float dist1 = abs(i - effectiveWave1);
        float packet1 = exp(-dist1 * 0.05f) * cos(dist1 * 0.5f);

        // Wave packet 2
        float dist2 = abs(i - effectiveWave2);
        float packet2 = exp(-dist2 * 0.05f) * cos(dist2 * 0.5f);

        // Interference
        float interference = packet1 + packet2;

        uint8_t brightness = (uint8_t)(128 + 127 * interference * intensityNorm);

        // CENTER ORIGIN color mapping
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2) + (uint8_t)(interference * 50);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, brightness);
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPInterferenceEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Box Wave", effectBoxWave)) count++;
    if (renderer->registerEffect(startId + count, "LGP Holographic", effectHolographic)) count++;
    if (renderer->registerEffect(startId + count, "LGP Modal Resonance", effectModalResonance)) count++;
    if (renderer->registerEffect(startId + count, "LGP Interference Scanner", effectInterferenceScanner)) count++;
    if (renderer->registerEffect(startId + count, "LGP Wave Collision", effectWaveCollision)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
