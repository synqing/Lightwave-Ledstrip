/**
 * @file LGPAdvancedEffects.cpp
 * @brief LGP Advanced optical effects implementation
 *
 * Implementation of advanced optical phenomena including Moire patterns,
 * Fresnel zones, and photonic crystal effects.
 * All effects use CENTER ORIGIN pattern (LEDs 79/80 as center).
 */

#include "LGPAdvancedEffects.h"
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
static uint16_t moirePhase = 0;
static uint16_t radialTime = 0;
static uint16_t vortexTime = 0;
static uint16_t evanescentPhase1 = 0, evanescentPhase2 = 32768;
static uint16_t shearPhase = 0;
static uint8_t shearPaletteOffset = 0;
static uint16_t cavityTime = 0;
static uint16_t photonicPhase = 0;

// ==================== MOIRE CURTAINS ====================
void effectMoireCurtains(RenderContext& ctx) {
    // CENTER ORIGIN - Two slightly mismatched frequencies create beat patterns

    float baseFreq = 8.0f;
    float delta = 0.2f;

    float leftFreq = baseFreq + delta / 2;
    float rightFreq = baseFreq - delta / 2;

    moirePhase += ctx.speed;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((int)i - CENTER_LEFT);

        // Left strip
        uint16_t leftPhaseVal = (uint16_t)(sin16(distFromCenter * leftFreq * 410 + moirePhase) + 32768) >> 8;
        uint8_t leftBright = scale8(leftPhaseVal, ctx.brightness);
        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + (uint8_t)(distFromCenter / 2), leftBright);

        // Right strip - slightly different frequency
        if (i + STRIP_LENGTH < ctx.numLeds) {
            uint16_t rightPhaseVal = (uint16_t)(sin16(distFromCenter * rightFreq * 410 + moirePhase) + 32768) >> 8;
            uint8_t rightBright = scale8(rightPhaseVal, ctx.brightness);
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + (uint8_t)(distFromCenter / 2) + 128, rightBright);
        }
    }
}

// ==================== RADIAL RIPPLE ====================
void effectRadialRipple(RenderContext& ctx) {
    // CENTER ORIGIN - Concentric rings that expand from center

    uint8_t ringCount = 6;
    uint16_t ringSpeed = ctx.speed << 2;

    radialTime += ringSpeed;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT) / CENTER_LEFT;

        // Square the distance for circular appearance
        uint16_t distSquared = (uint16_t)(distFromCenter * distFromCenter * 65535);

        // Create expanding rings
        int16_t wave = sin16((distSquared >> 1) * ringCount - radialTime);

        // Convert to brightness
        uint8_t brightness = (wave + 32768) >> 8;
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = ctx.hue + (distSquared >> 10);
        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue, brightness);

        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + 64, brightness);
        }
    }
}

// ==================== HOLOGRAPHIC VORTEX ====================
void effectHolographicVortex(RenderContext& ctx) {
    // CENTER ORIGIN - Spiral interference pattern with depth illusion

    vortexTime += ctx.speed << 1;

    uint8_t spiralCount = 3;
    uint8_t tightness = ctx.brightness >> 2;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((float)i - CENTER_LEFT);
        float r = distFromCenter / CENTER_LEFT;

        // Symmetric azimuthal angle
        uint16_t theta = (uint16_t)(distFromCenter * 410);

        // Spiral phase
        uint16_t phase = spiralCount * theta + (uint16_t)(tightness * r * 65535) - vortexTime;

        uint8_t brightness = sin8(phase >> 8);
        uint8_t paletteIndex = phase >> 10;

        // Add depth via brightness modulation
        brightness = scale8(brightness, 255 - (uint8_t)(r * 127));
        brightness = scale8(brightness, ctx.brightness);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + paletteIndex + 128, brightness);
        }
    }
}

// ==================== EVANESCENT DRIFT ====================
void effectEvanescentDrift(RenderContext& ctx) {
    // Exponentially fading waves from edges - subtle ambient effect

    evanescentPhase1 += ctx.speed;
    evanescentPhase2 -= ctx.speed;

    uint8_t alpha = 255 - ctx.brightness;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t distFromLeftEdge = i;
        uint8_t distFromRightEdge = STRIP_LENGTH - 1 - i;
        uint8_t distFromEdge = min(distFromLeftEdge, distFromRightEdge);

        // Exponential decay approximation
        uint8_t decay = 255;
        for (uint8_t j = 0; j < distFromEdge && j < 8; j++) {
            decay = scale8(decay, alpha);
        }

        // Wave patterns
        uint8_t wave1 = sin8((i << 2) + (evanescentPhase1 >> 8));
        uint8_t wave2 = sin8((i << 2) + (evanescentPhase2 >> 8));

        // Apply decay
        wave1 = scale8(wave1, decay);
        wave2 = scale8(wave2, decay);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, ctx.hue + i, wave1);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, ctx.hue + i + 85, wave2);
        }
    }
}

// ==================== CHROMATIC SHEAR ====================
void effectChromaticShear(RenderContext& ctx) {
    // CENTER ORIGIN - Color planes sliding with velocity shear

    shearPhase += ctx.speed;

    // Palette rotation
    static uint32_t lastUpdate = 0;
    if (ctx.frameCount - lastUpdate > 5) {
        shearPaletteOffset += 2;
        lastUpdate = ctx.frameCount;
    }

    uint8_t shearAmount = 128;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = abs((int)i - CENTER_LEFT);
        uint8_t distPos = (distFromCenter * 255) / HALF_LENGTH;

        // Apply shear transformation
        uint16_t shearOffset = ((uint16_t)distPos * shearAmount) >> 3;

        // Left strip: base hue + shear
        uint8_t leftHue = shearPaletteOffset + distPos + (shearPhase >> 8);
        uint8_t leftBright = ctx.brightness;

        // Right strip: complementary hue + inverse shear
        uint8_t rightHue = shearPaletteOffset + distPos + 120 - (shearPhase >> 8);
        uint8_t rightBright = ctx.brightness;

        // Add interference at center
        if (abs(i - CENTER_LEFT) < 20) {
            uint8_t centerBlend = 255 - abs(i - CENTER_LEFT) * 12;
            leftBright = scale8(leftBright, 255 - (centerBlend >> 1));
            rightBright = scale8(rightBright, 255 - (centerBlend >> 1));
        }

        ctx.leds[i] = ColorFromPalette(*ctx.palette, leftHue, leftBright);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, rightHue, rightBright);
        }
    }
}

// ==================== MODAL CAVITY ====================
void effectModalCavity(RenderContext& ctx) {
    // Excite specific waveguide modes

    cavityTime += ctx.speed;

    uint8_t modeNumber = 8;
    uint8_t beatMode = modeNumber + 2;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i / STRIP_LENGTH;

        // Primary mode
        int16_t mode1 = sin16((uint16_t)(x * modeNumber * 32768));

        // Beat mode
        int16_t mode2 = sin16((uint16_t)(x * beatMode * 32768) + cavityTime);

        // Combine modes
        int16_t combined = (mode1 >> 1) + (mode2 >> 2);
        uint8_t brightness = (combined + 32768) >> 8;

        // Apply cosine taper
        uint8_t taper = cos8((uint8_t)(x * 255)) >> 1;
        brightness = scale8(brightness, 128 + taper);
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = ctx.hue + (modeNumber * 12);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + 64, brightness);
        }
    }
}

// ==================== FRESNEL ZONES ====================
void effectFresnelZones(RenderContext& ctx) {
    // CENTER ORIGIN - Optical zone plates creating focusing effects

    uint8_t zoneCount = 6;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Distance from center
        int16_t dist = abs((int16_t)i - CENTER_LEFT);

        // Fresnel zone calculation
        uint16_t zoneRadius = sqrt16(dist << 8) * zoneCount;

        // Binary zone plate
        bool inZone = (zoneRadius & 0x100) != 0;
        uint8_t brightness = inZone ? 255 : 0;

        // Smooth edges based on intensity
        if (ctx.brightness < 200) {
            uint8_t edge = zoneRadius & 0xFF;
            brightness = inZone ? edge : 255 - edge;
        }

        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = ctx.hue + (dist >> 2);

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + 30, scale8(brightness, 200));
        }
    }
}

// ==================== PHOTONIC CRYSTAL ====================
void effectPhotonicCrystal(RenderContext& ctx) {
    // CENTER ORIGIN - Periodic refractive index modulation

    photonicPhase += ctx.speed;

    uint8_t latticeSize = 8;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // CENTER ORIGIN: Calculate distance from center
        uint16_t distFromCenter = abs((int16_t)i - CENTER_LEFT);

        // Periodic structure
        uint8_t cellPosition = distFromCenter % latticeSize;
        bool inBandgap = cellPosition < (latticeSize >> 1);

        // Photonic band structure - CENTER ORIGIN PUSH
        uint8_t brightness = 0;
        if (inBandgap) {
            // Allowed modes - push outward from center
            brightness = sin8((distFromCenter << 2) - (photonicPhase >> 7));
        } else {
            // Forbidden gap - evanescent decay
            uint8_t decay = 255 - (cellPosition * 50);
            brightness = scale8(sin8((distFromCenter << 1) - (photonicPhase >> 8)), decay);
        }

        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = inBandgap ? ctx.hue : ctx.hue + 128;

        ctx.leds[i] = ColorFromPalette(*ctx.palette, hue + distFromCenter / 4, brightness);
        if (i + STRIP_LENGTH < ctx.numLeds) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(*ctx.palette, hue + distFromCenter / 4 + 64, brightness);
        }
    }
}

// ==================== EFFECT REGISTRATION ====================

uint8_t registerLGPAdvancedEffects(RendererActor* renderer, uint8_t startId) {
    if (!renderer) return 0;

    uint8_t count = 0;

    if (renderer->registerEffect(startId + count, "LGP Moire Curtains", effectMoireCurtains)) count++;
    if (renderer->registerEffect(startId + count, "LGP Radial Ripple", effectRadialRipple)) count++;
    if (renderer->registerEffect(startId + count, "LGP Holographic Vortex", effectHolographicVortex)) count++;
    if (renderer->registerEffect(startId + count, "LGP Evanescent Drift", effectEvanescentDrift)) count++;
    if (renderer->registerEffect(startId + count, "LGP Chromatic Shear", effectChromaticShear)) count++;
    if (renderer->registerEffect(startId + count, "LGP Modal Cavity", effectModalCavity)) count++;
    if (renderer->registerEffect(startId + count, "LGP Fresnel Zones", effectFresnelZones)) count++;
    if (renderer->registerEffect(startId + count, "LGP Photonic Crystal", effectPhotonicCrystal)) count++;

    return count;
}

} // namespace effects
} // namespace lightwaveos
