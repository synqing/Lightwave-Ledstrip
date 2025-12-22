/**
 * @file LGPColorMixingEffects.cpp
 * @brief LGP Color Mixing effects implementation
 *
 * Exploiting opposing light channels for unprecedented color phenomena.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPColorMixingEffects.h"
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
static float prismAngle = 0;
static float colorPhase = 0;
static float waveFunction = 0;
static float sourcePosition = 0;

// Color Accelerator
static float redParticle = 0;
static float blueParticle = 159;
static bool collision = false;
static float debrisRadius = 0;

// DNA Helix
static float helixRotation = 0;

// Phase Transition
static float phaseAnimation = 0;

// Perceptual Blend
static float blendPhase = 0;

// Chromatic Aberration
static float lensPosition = 0;

// ==================== COLOR TEMPERATURE ====================
void effectColorTemperature(RenderContext& ctx) {
    // Warm colors from edges meet cool colors at center, creating white

    float intensity = ctx.brightness / 255.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        CRGB warm, cool;

        // Warm side (reds/oranges)
        warm.r = 255;
        warm.g = (uint8_t)(180 - normalizedDist * 100);
        warm.b = (uint8_t)(50 + normalizedDist * 50);

        // Cool side (blues/cyans)
        cool.r = (uint8_t)(150 + normalizedDist * 50);
        cool.g = (uint8_t)(200 + normalizedDist * 55);
        cool.b = 255;

        ctx.leds[i] = warm.scale8((uint8_t)(intensity * 255));
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = cool.scale8((uint8_t)(intensity * 255));
        }
    }
}

// ==================== RGB PRISM ====================
void effectRGBPrism(RenderContext& ctx) {
    // Simulates light passing through a prism

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    float dispersion = 1.5f;

    prismAngle += speed * 0.02f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redAngle = sin(normalizedDist * dispersion + prismAngle);
        float greenAngle = sin(normalizedDist * dispersion * 1.1f + prismAngle);
        float blueAngle = sin(normalizedDist * dispersion * 1.2f + prismAngle);

        // Strip 1: Red channel dominant
        ctx.leds[i].r = (uint8_t)((128 + 127 * redAngle) * intensity);
        ctx.leds[i].g = (uint8_t)(64 * abs(greenAngle) * intensity);
        ctx.leds[i].b = 0;

        // Strip 2: Blue channel dominant
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH].r = 0;
            ctx.leds[i + STRIP_LENGTH].g = (uint8_t)(64 * abs(greenAngle) * intensity);
            ctx.leds[i + STRIP_LENGTH].b = (uint8_t)((128 + 127 * blueAngle) * intensity);
        }

        // Green emerges at center
        if (distFromCenter < 10) {
            ctx.leds[i].g += (uint8_t)(128 * intensity);
            if (i + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[i + STRIP_LENGTH].g += (uint8_t)(128 * intensity);
            }
        }
    }
}

// ==================== COMPLEMENTARY MIXING ====================
void effectComplementaryMixing(RenderContext& ctx) {
    // Dynamic complementary pairs create neutral zones

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    float variation = 0.5f;

    colorPhase += speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        uint8_t baseHue = ctx.hue + (uint8_t)(colorPhase * 255);
        uint8_t complementHue = baseHue + 128;

        uint8_t edgeIntensity = (uint8_t)(255 * (1 - normalizedDist * variation));

        if (normalizedDist > 0.5f) {
            ctx.leds[i] = CHSV(baseHue, 255, (uint8_t)(edgeIntensity * intensity));
            if (i + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[i + STRIP_LENGTH] = CHSV(complementHue, 255, (uint8_t)(edgeIntensity * intensity));
            }
        } else {
            uint8_t saturation = (uint8_t)(255 * (normalizedDist * 2));
            ctx.leds[i] = CHSV(baseHue, saturation, (uint8_t)(128 * intensity));
            if (i + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[i + STRIP_LENGTH] = CHSV(complementHue, saturation, (uint8_t)(128 * intensity));
            }
        }
    }
}

// ==================== QUANTUM COLORS ====================
void effectQuantumColors(RenderContext& ctx) {
    // Colors exist in quantum states until "observed"

    float intensity = ctx.brightness / 255.0f;
    float complexity = 0.5f;

    waveFunction += ctx.speed * 0.001f;

    int numStates = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float probability = sin(waveFunction + normalizedDist * TWO_PI * numStates);
        probability = probability * probability;

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

        uint8_t uncertainty = (uint8_t)(255 * (0.5f + 0.5f * sin(distFromCenter * 0.2f)));

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset, (uint8_t)(uncertainty * intensity));
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset + 128, (uint8_t)((255 - uncertainty) * intensity));
        }
    }
}

// ==================== DOPPLER SHIFT ====================
void effectDopplerShift(RenderContext& ctx) {
    // Moving colors shift frequency based on velocity

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    sourcePosition += speed * 5;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        float relativePos = i - fmod(sourcePosition, (float)STRIP_LENGTH);
        float velocity = speed * 10;

        float dopplerFactor;
        if (relativePos > 0) {
            dopplerFactor = 1.0f - (velocity / 100.0f);
        } else {
            dopplerFactor = 1.0f + (velocity / 100.0f);
        }

        uint8_t shiftedHue = ctx.hue;
        if (dopplerFactor > 1.0f) {
            shiftedHue = ctx.hue - (uint8_t)(30 * (dopplerFactor - 1.0f));
        } else {
            shiftedHue = ctx.hue + (uint8_t)(30 * (1.0f - dopplerFactor));
        }

        uint8_t brightness = (uint8_t)(255 * intensity * (1 - distFromCenter / HALF_LENGTH));

        ctx.leds[i] = CHSV(shiftedHue, 255, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(shiftedHue + 90, 255, brightness);
        }
    }
}

// ==================== COLOR ACCELERATOR ====================
void effectColorAccelerator(RenderContext& ctx) {
    // RGB particles accelerate from edges and collide at center

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    fadeToBlackBy(ctx.leds, ctx.numLeds, 20);

    if (!collision) {
        redParticle += speed * 10 * (1 + (redParticle / STRIP_LENGTH));
        blueParticle -= speed * 10 * (1 + ((STRIP_LENGTH - blueParticle) / STRIP_LENGTH));

        // Draw particle trails
        for (int t = 0; t < 20; t++) {
            int redPos = (int)redParticle - t;
            int bluePos = (int)blueParticle + t;

            if (redPos >= 0 && redPos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                ctx.leds[redPos] = CRGB(trailBright, 0, 0);
            }

            if (bluePos >= 0 && bluePos < STRIP_LENGTH) {
                uint8_t trailBright = (uint8_t)((255 - t * 12) * intensity);
                if (bluePos + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[bluePos + STRIP_LENGTH] = CRGB(0, 0, trailBright);
                }
            }
        }

        // Check for collision
        if (redParticle >= CENTER_LEFT - 5 && blueParticle <= CENTER_LEFT + 5) {
            collision = true;
            debrisRadius = 0;
        }
    } else {
        debrisRadius += speed * 8;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            float distFromCenter = (float)centerPairDistance((uint16_t)i);

            if (distFromCenter <= debrisRadius) {
                uint8_t debrisHue = random8();
                uint8_t debrisBright = (uint8_t)(255 * (1 - distFromCenter / debrisRadius) * intensity);

                if (random8(2) == 0) {
                    ctx.leds[i] += CHSV(debrisHue, 255, debrisBright);
                } else if (i + STRIP_LENGTH < ctx.numLeds) {
                    ctx.leds[i + STRIP_LENGTH] += CHSV(debrisHue, 255, debrisBright);
                }
            }
        }

        if (debrisRadius > HALF_LENGTH) {
            collision = false;
            redParticle = 0;
            blueParticle = STRIP_LENGTH - 1;
        }
    }
}

// ==================== DNA HELIX ====================
void effectDNAHelix(RenderContext& ctx) {
    // Double helix with color base pairing

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    helixRotation += speed * 0.05f;

    float helixPitch = 20.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float angle1 = (distFromCenter / helixPitch) * TWO_PI + helixRotation;
        float angle2 = angle1 + PI;

        uint8_t paletteOffset1, paletteOffset2;
        if (sin(angle1 * 2) > 0) {
            paletteOffset1 = 0;
            paletteOffset2 = 15;
        } else {
            paletteOffset1 = 10;
            paletteOffset2 = 25;
        }

        float strand1Intensity = (sin(angle1) + 1) * 0.5f;
        float strand2Intensity = (sin(angle2) + 1) * 0.5f;

        float connectionIntensity = 0;
        if (fmod(distFromCenter, helixPitch / 4) < 2) {
            connectionIntensity = 1;
        }

        uint8_t brightness = (uint8_t)(255 * intensity);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset1, (uint8_t)(brightness * strand1Intensity));
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset2, (uint8_t)(brightness * strand2Intensity));
        }

        if (connectionIntensity > 0) {
            CRGB conn2 = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset2, brightness);
            CRGB conn1 = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset1, brightness);
            ctx.leds[i] = blend(ctx.leds[i], conn2, 128);
            if (i + STRIP_LENGTH < ctx.numLeds) {
                ctx.leds[i + STRIP_LENGTH] = blend(ctx.leds[i + STRIP_LENGTH], conn1, 128);
            }
        }
    }
}

// ==================== PHASE TRANSITION ====================
void effectPhaseTransition(RenderContext& ctx) {
    // Colors undergo state changes like matter

    float temperature = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    float pressure = 0.5f;

    phaseAnimation += temperature * 0.1f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float localTemp = temperature + (normalizedDist * pressure);

        CRGB color;
        uint8_t paletteOffset;
        uint8_t brightness;

        if (localTemp < 0.25f) {
            // Solid phase
            float crystal = sin(distFromCenter * 0.3f) * 0.5f + 0.5f;
            paletteOffset = (uint8_t)(crystal * 5);
            brightness = (uint8_t)(255 * intensity);
            color = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset, brightness);
        } else if (localTemp < 0.5f) {
            // Liquid phase
            float flow = sin(distFromCenter * 0.1f + phaseAnimation);
            paletteOffset = 10 + (uint8_t)(flow * 5);
            brightness = (uint8_t)(200 * intensity);
            color = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset, brightness);
        } else if (localTemp < 0.75f) {
            // Gas phase
            float dispersion = random8() / 255.0f;
            if (dispersion < 0.3f) {
                paletteOffset = 20;
                brightness = (uint8_t)(150 * intensity);
                color = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset, brightness);
            } else {
                color = CRGB::Black;
            }
        } else {
            // Plasma phase
            float plasma = sin(distFromCenter * 0.5f + phaseAnimation * 10);
            paletteOffset = 30 + (uint8_t)(plasma * 10);
            brightness = (uint8_t)(255 * intensity);
            color = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset, brightness);
        }

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteOffset + 60, brightness);
        }
    }
}

// ==================== CHROMATIC ABERRATION ====================
void effectChromaticAberration(RenderContext& ctx) {
    // Different wavelengths refract at different angles

    float intensity = ctx.brightness / 255.0f;
    float aberration = 1.5f;

    lensPosition += ctx.speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redFocus = sin((normalizedDist - 0.1f * aberration) * PI + lensPosition);
        float greenFocus = sin(normalizedDist * PI + lensPosition);
        float blueFocus = sin((normalizedDist + 0.1f * aberration) * PI + lensPosition);

        CRGB aberratedColor;
        aberratedColor.r = (uint8_t)(constrain(128 + 127 * redFocus, 0, 255) * intensity);
        aberratedColor.g = (uint8_t)(constrain(128 + 127 * greenFocus, 0, 255) * intensity);
        aberratedColor.b = (uint8_t)(constrain(128 + 127 * blueFocus, 0, 255) * intensity);

        ctx.leds[i] = aberratedColor;

        if (i + STRIP_LENGTH < ctx.numLeds) {
            CRGB aberratedColor2;
            aberratedColor2.r = (uint8_t)(constrain(128 + 127 * blueFocus, 0, 255) * intensity);
            aberratedColor2.g = (uint8_t)(constrain(128 + 127 * greenFocus, 0, 255) * intensity);
            aberratedColor2.b = (uint8_t)(constrain(128 + 127 * redFocus, 0, 255) * intensity);
            ctx.leds[i + STRIP_LENGTH] = aberratedColor2;
        }
    }
}

// ==================== PERCEPTUAL BLEND ====================
void effectPerceptualBlend(RenderContext& ctx) {
    // Uses perceptually uniform color space for natural mixing

    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    blendPhase += speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float L = 50 + 50 * sin(blendPhase);
        float a = 50 * cos(blendPhase + normalizedDist * PI);
        float b = 50 * sin(blendPhase - normalizedDist * PI);

        CRGB color;
        color.r = (uint8_t)(constrain(L + a * 2, 0, 255) * intensity);
        color.g = (uint8_t)(constrain(L - a - b, 0, 255) * intensity);
        color.b = (uint8_t)(constrain(L + b * 2, 0, 255) * intensity);

        ctx.leds[i] = color;

        if (i + STRIP_LENGTH < ctx.numLeds) {
            CRGB color2;
            color2.r = (uint8_t)(constrain(L - a * 2, 0, 255) * intensity);
            color2.g = (uint8_t)(constrain(L + a + b, 0, 255) * intensity);
            color2.b = (uint8_t)(constrain(L - b * 2, 0, 255) * intensity);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPColorMixingEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Color Temperature", effectColorTemperature)) count++;
    if (renderer->registerEffect(startId + count, "LGP RGB Prism", effectRGBPrism)) count++;
    if (renderer->registerEffect(startId + count, "LGP Complementary Mixing", effectComplementaryMixing)) count++;
    if (renderer->registerEffect(startId + count, "LGP Quantum Colors", effectQuantumColors)) count++;
    if (renderer->registerEffect(startId + count, "LGP Doppler Shift", effectDopplerShift)) count++;
    if (renderer->registerEffect(startId + count, "LGP Color Accelerator", effectColorAccelerator)) count++;
    if (renderer->registerEffect(startId + count, "LGP DNA Helix", effectDNAHelix)) count++;
    if (renderer->registerEffect(startId + count, "LGP Phase Transition", effectPhaseTransition)) count++;
    if (renderer->registerEffect(startId + count, "LGP Chromatic Aberration", effectChromaticAberration)) count++;
    if (renderer->registerEffect(startId + count, "LGP Perceptual Blend", effectPerceptualBlend)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
