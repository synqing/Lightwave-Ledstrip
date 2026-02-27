// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPChromaticPulseEffect.cpp
 * @brief LGP Chromatic Pulse effect implementation
 */

#include "LGPChromaticPulseEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

CRGB chromaticDispersion(float position, float aberration, float phase, float intensity) {
    float distFromCenter = (float)centerPairDistance((uint16_t)position);
    float normalizedDist = distFromCenter / (float)HALF_LENGTH;

    const float n0 = 1.49f;
    const float B = 0.0045f;
    const float C = 0.0001f;

    const float lambdaRed = 0.70f;
    const float lambdaGreen = 0.55f;
    const float lambdaBlue = 0.45f;

    const float nRed = n0 + (B / ((lambdaRed * lambdaRed) - C));
    const float nGreen = n0 + (B / ((lambdaGreen * lambdaGreen) - C));
    const float nBlue = n0 + (B / ((lambdaBlue * lambdaBlue) - C));

    const float deltaR = (nGreen - nRed);
    const float deltaB = (nBlue - nGreen);
    const float dispersionScale = 20.0f;

    if (aberration < 0.0f) aberration = 0.0f;
    if (aberration > 3.0f) aberration = 3.0f;

    const float redOffset = -0.1f * aberration * deltaR * dispersionScale;
    const float blueOffset = 0.1f * aberration * deltaB * dispersionScale;

    float redFocus = sinf((normalizedDist + redOffset) * PI + phase);
    float greenFocus = sinf(normalizedDist * PI + phase);
    float blueFocus = sinf((normalizedDist + blueOffset) * PI + phase);

    uint8_t r = (uint8_t)constrain((int)(128 + 127 * redFocus), 0, 255);
    uint8_t g = (uint8_t)constrain((int)(128 + 127 * greenFocus), 0, 255);
    uint8_t b = (uint8_t)constrain((int)(128 + 127 * blueFocus), 0, 255);

    r = (uint8_t)(r * intensity);
    g = (uint8_t)(g * intensity);
    b = (uint8_t)(b * intensity);

    return CRGB(r, g, b);
}

CRGB chromaticDispersionPalette(float position,
                                float aberration,
                                float phase,
                                float intensity,
                                const plugins::PaletteRef& palette,
                                uint8_t baseHue)
{
    if (!palette.isValid()) {
        return chromaticDispersion(position, aberration, phase, intensity);
    }

    float distFromCenter = (float)centerPairDistance((uint16_t)position);
    float normalizedDist = distFromCenter / (float)HALF_LENGTH;
    if (normalizedDist < 0.0f) normalizedDist = 0.0f;
    if (normalizedDist > 1.0f) normalizedDist = 1.0f;

    uint8_t phaseScroll = (uint8_t)((phase * 255.0f) / TWO_PI);
    uint8_t idx = (uint8_t)(baseHue + (uint8_t)(normalizedDist * 255.0f) + phaseScroll);

    int16_t sep = (int16_t)(8 + (aberration * 24.0f));

    CRGB cR = palette.getColor((uint8_t)(idx - sep), 255);
    CRGB cG = palette.getColor(idx, 255);
    CRGB cB = palette.getColor((uint8_t)(idx + sep), 255);

    float aberr = aberration;
    if (aberr < 0.0f) aberr = 0.0f;
    if (aberr > 3.0f) aberr = 3.0f;

    float redOffset = -0.04f * aberr;
    float blueOffset = 0.05f * aberr;

    float redFocus = 0.5f + 0.5f * sinf((normalizedDist + redOffset) * PI + phase);
    float greenFocus = 0.5f + 0.5f * sinf((normalizedDist) * PI + phase);
    float blueFocus = 0.5f + 0.5f * sinf((normalizedDist + blueOffset) * PI + phase);

    float weightSum = redFocus + greenFocus + blueFocus;
    if (weightSum < 0.001f) weightSum = 1.0f;

    float rF = (cR.r * redFocus + cG.r * greenFocus + cB.r * blueFocus) / weightSum;
    float gF = (cR.g * redFocus + cG.g * greenFocus + cB.g * blueFocus) / weightSum;
    float bF = (cR.b * redFocus + cG.b * greenFocus + cB.b * blueFocus) / weightSum;

    CRGB out((uint8_t)constrain((int)rF, 0, 255),
             (uint8_t)constrain((int)gF, 0, 255),
             (uint8_t)constrain((int)bF, 0, 255));

    out.nscale8_video((uint8_t)(constrain(intensity * 255.0f, 0.0f, 255.0f)));
    return out;
}

} // namespace

LGPChromaticPulseEffect::LGPChromaticPulseEffect()
    : m_phase(0.0f)
{
}

bool LGPChromaticPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPChromaticPulseEffect::render(plugins::EffectContext& ctx) {
    // Aberration sweeps from centre outward with intensity pulse
    float baseIntensity = ctx.brightness / 255.0f;
    float baseAberration = (ctx.complexity / 255.0f) * 3.0f;

    m_phase += ctx.speed * 0.015f;
    if (m_phase > TWO_PI) m_phase -= TWO_PI;

    float aberration = baseAberration * (0.5f + 0.5f * sinf(m_phase));
    float intensity = baseIntensity * (0.7f + 0.3f * sinf(m_phase * 1.5f));
    float phase = m_phase * 0.5f;

    for (int i = 0; i < STRIP_LENGTH && i < (int)ctx.ledCount; i++) {
        CRGB color = chromaticDispersionPalette((float)i, aberration, phase, intensity, ctx.palette, ctx.gHue);
        ctx.leds[i] = color;
    }

    if (ctx.ledCount >= STRIP_LENGTH * 2) {
        for (int i = 0; i < STRIP_LENGTH; i++) {
            int mirrorIdx = STRIP_LENGTH * 2 - 1 - i;
            if (mirrorIdx < (int)ctx.ledCount) {
                CRGB color = chromaticDispersionPalette((float)i, aberration, phase + PI * 0.5f, intensity, ctx.palette, ctx.gHue);
                ctx.leds[mirrorIdx] = color;
            }
        }
    }
}

void LGPChromaticPulseEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChromaticPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Pulse",
        "Pulsing dispersion wave",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
