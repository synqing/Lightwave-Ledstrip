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

    // Ocean waves with glowing plankton effect — centre-origin addressing
    m_wavePhase = (uint16_t)(m_wavePhase + ctx.speed);

    const uint8_t waveCount = 4;

    // Base ocean color — keyed to distance from center
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        uint16_t wave = 0;
        for (uint8_t w = 0; w < waveCount; w++) {
            wave += sin8(((dist << 2) + (m_wavePhase >> (4 - w))) >> w);
        }
        wave /= waveCount;

        uint8_t blue = scale8((uint8_t)wave, 60);
        uint8_t green = scale8((uint8_t)wave, 20);

        CRGB color = CRGB(0, green, blue);
        SET_CENTER_PAIR(ctx, dist, color);
    }

    // Spawn new glow points occasionally (distance from center)
    if ((ctx.frameNumber % 12) == 0) {
        for (uint8_t g = 0; g < 20; g++) {
            if (m_glowLife[g] == 0) {
                m_glowPoints[g] = random8(HALF_LENGTH);
                m_glowLife[g] = 255;
                break;
            }
        }
    }

    // Update and render glow points
    for (uint8_t g = 0; g < 20; g++) {
        if (m_glowLife[g] > 0) {
            m_glowLife[g] = scale8(m_glowLife[g], 240);

            uint8_t glowDist = m_glowPoints[g];
            uint8_t intensity = scale8(m_glowLife[g], ctx.brightness);

            for (int8_t spread = -3; spread <= 3; spread++) {
                int16_t dist = (int16_t)glowDist + spread;
                if (dist >= 0 && dist < HALF_LENGTH) {
                    uint8_t spreadIntensity = scale8(intensity, (uint8_t)(255 - abs(spread) * 60));
                    CRGB color = CRGB(0, spreadIntensity >> 1, spreadIntensity);
                    SET_CENTER_PAIR(ctx, (uint16_t)dist, color);
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
