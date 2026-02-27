// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPChromaticAberrationEffect.cpp
 * @brief LGP Chromatic Aberration effect implementation
 */

#include "LGPChromaticAberrationEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPChromaticAberrationEffect::LGPChromaticAberrationEffect()
    : m_lensPosition(0.0f)
{
}

bool LGPChromaticAberrationEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_lensPosition = 0.0f;
    return true;
}

void LGPChromaticAberrationEffect::render(plugins::EffectContext& ctx) {
    // Different wavelengths refract at different angles
    float intensity = ctx.brightness / 255.0f;
    const float aberration = 1.5f;

    m_lensPosition += ctx.speed * 0.01f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redFocus = sinf((normalizedDist - 0.1f * aberration) * PI + m_lensPosition);
        float greenFocus = sinf(normalizedDist * PI + m_lensPosition);
        float blueFocus = sinf((normalizedDist + 0.1f * aberration) * PI + m_lensPosition);

        CRGB aberratedColor;
        aberratedColor.r = (uint8_t)(constrain(128.0f + 127.0f * redFocus, 0.0f, 255.0f) * intensity);
        aberratedColor.g = (uint8_t)(constrain(128.0f + 127.0f * greenFocus, 0.0f, 255.0f) * intensity);
        aberratedColor.b = (uint8_t)(constrain(128.0f + 127.0f * blueFocus, 0.0f, 255.0f) * intensity);

        ctx.leds[i] = aberratedColor;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            CRGB aberratedColor2;
            aberratedColor2.r = (uint8_t)(constrain(128.0f + 127.0f * blueFocus, 0.0f, 255.0f) * intensity);
            aberratedColor2.g = (uint8_t)(constrain(128.0f + 127.0f * greenFocus, 0.0f, 255.0f) * intensity);
            aberratedColor2.b = (uint8_t)(constrain(128.0f + 127.0f * redFocus, 0.0f, 255.0f) * intensity);
            ctx.leds[i + STRIP_LENGTH] = aberratedColor2;
        }
    }
}

void LGPChromaticAberrationEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChromaticAberrationEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Aberration",
        "Lens dispersion edge effects",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
