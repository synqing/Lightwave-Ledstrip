// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPBoxWaveEffect.cpp
 * @brief LGP Box Wave effect implementation
 */

#include "LGPBoxWaveEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPBoxWaveEffect::LGPBoxWaveEffect()
    : m_boxMotionPhase(0.0f)
{
}

bool LGPBoxWaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_boxMotionPhase = 0.0f;
    return true;
}

void LGPBoxWaveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN BOX WAVE - Creates controllable standing wave boxes from center
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // Box count: 3-12 boxes
    const float boxesPerSide = 6.0f;
    const float spatialFreq = boxesPerSide * PI / (float)HALF_LENGTH;

    m_boxMotionPhase += speedNorm * 0.05f;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // Base box pattern from center
        float boxPhase = distFromCenter * spatialFreq;
        float boxPattern = sinf(boxPhase + m_boxMotionPhase);

        // Sharpness control
        boxPattern = tanhf(boxPattern * 2.0f);

        // Convert to brightness
        uint8_t brightness = (uint8_t)(128.0f + 127.0f * boxPattern * intensityNorm);

        // Color wave overlay
        uint8_t colorIndex = (uint8_t)(ctx.gHue + (uint8_t)(distFromCenter * 2.0f));

        // Apply to both strips
        ctx.leds[i] = ctx.palette.getColor(colorIndex, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(colorIndex + 128), brightness);
        }
    }
}

void LGPBoxWaveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBoxWaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Box Wave",
        "Square wave standing patterns",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
