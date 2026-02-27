// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPBioluminescentWavesEffect.cpp
 * @brief LGP Bioluminescent Waves effect implementation
 */

#include "LGPBioluminescentWavesEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPBioluminescentWavesEffect::LGPBioluminescentWavesEffect()
    : m_wavePhase(0)
    , m_glowPoints{}
    , m_glowLife{}
{
}

bool LGPBioluminescentWavesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_wavePhase = 0;
    memset(m_glowPoints, 0, sizeof(m_glowPoints));
    memset(m_glowLife, 0, sizeof(m_glowLife));
    return true;
}

void LGPBioluminescentWavesEffect::render(plugins::EffectContext& ctx) {
    // Fade to prevent color accumulation from additive blending
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Ocean waves with glowing plankton effect
    m_wavePhase = (uint16_t)(m_wavePhase + ctx.speed);

    const uint8_t waveCount = 4;

    // Base ocean color
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t wave = 0;
        for (uint8_t w = 0; w < waveCount; w++) {
            wave += sin8(((i << 2) + (m_wavePhase >> (4 - w))) >> w);
        }
        wave /= waveCount;

        uint8_t blue = scale8((uint8_t)wave, 60);
        uint8_t green = scale8((uint8_t)wave, 20);

        ctx.leds[i] = CRGB(0, green, blue);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CRGB(0, green >> 1, blue);
        }
    }

    // Spawn new glow points occasionally
    if ((ctx.frameNumber % 12) == 0) {
        for (uint8_t g = 0; g < 20; g++) {
            if (m_glowLife[g] == 0) {
                m_glowPoints[g] = random8(STRIP_LENGTH);
                m_glowLife[g] = 255;
                break;
            }
        }
    }

    // Update and render glow points
    for (uint8_t g = 0; g < 20; g++) {
        if (m_glowLife[g] > 0) {
            m_glowLife[g] = scale8(m_glowLife[g], 240);

            uint8_t pos = m_glowPoints[g];
            uint8_t intensity = scale8(m_glowLife[g], ctx.brightness);

            for (int8_t spread = -3; spread <= 3; spread++) {
                int16_t p = (int16_t)pos + spread;
                if (p >= 0 && p < STRIP_LENGTH) {
                    uint8_t spreadIntensity = scale8(intensity, (uint8_t)(255 - abs(spread) * 60));
                    ctx.leds[p] = CRGB(0, spreadIntensity >> 1, spreadIntensity);
                    if (p + STRIP_LENGTH < ctx.ledCount) {
                        uint16_t idx = p + STRIP_LENGTH;
                        ctx.leds[idx] = CRGB(0, spreadIntensity >> 2, spreadIntensity);
                    }
                }
            }
        }
    }
}

void LGPBioluminescentWavesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBioluminescentWavesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Bioluminescent Waves",
        "Glowing plankton in waves",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
