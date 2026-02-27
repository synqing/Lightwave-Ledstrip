// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPPlasmaMembraneEffect.cpp
 * @brief LGP Plasma Membrane effect implementation
 */

#include "LGPPlasmaMembraneEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPlasmaMembraneEffect::LGPPlasmaMembraneEffect()
    : m_time(0)
{
}

bool LGPPlasmaMembraneEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPPlasmaMembraneEffect::render(plugins::EffectContext& ctx) {
    // Organic cellular membrane with lipid bilayer dynamics
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Membrane shape using multiple octaves
        uint16_t membrane = 0;
        membrane += (uint16_t)(inoise8(i * 3, m_time >> 2) << 1);
        membrane += (uint16_t)(inoise8(i * 7, m_time >> 1) >> 1);
        membrane += inoise8(i * 13, m_time);
        membrane >>= 2;

        uint8_t hue = (uint8_t)(20 + (membrane >> 3));
        uint8_t sat = (uint8_t)(200 + (membrane >> 2));
        uint8_t brightness = scale8((uint8_t)membrane, ctx.brightness);

        CRGB inner = ctx.palette.getColor(hue, brightness);
        uint8_t outerBright = scale8(brightness, 200);
        outerBright = (uint8_t)((outerBright * ctx.brightness) / 255);
        CRGB outer = ctx.palette.getColor((uint8_t)(hue + 10), outerBright);

        ctx.leds[i] = inner;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = outer;
        }
    }

    // Add membrane potential waves
    uint16_t potentialWave = beatsin16(5, 0, STRIP_LENGTH - 1);
    for (int8_t w = -10; w <= 10; w++) {
        int16_t pos = (int16_t)potentialWave + w;
        if (pos >= 0 && pos < STRIP_LENGTH) {
            uint8_t waveIntensity = (uint8_t)(255 - abs(w) * 20);
            ctx.leds[pos] = blend(ctx.leds[pos], CRGB::Yellow, waveIntensity);
            if (pos + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[pos + STRIP_LENGTH] = blend(ctx.leds[pos + STRIP_LENGTH], CRGB::Gold, waveIntensity);
            }
        }
    }
}

void LGPPlasmaMembraneEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPlasmaMembraneEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Plasma Membrane",
        "Cellular membrane fluctuations",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
