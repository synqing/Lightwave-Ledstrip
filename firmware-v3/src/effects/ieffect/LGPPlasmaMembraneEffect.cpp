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
    // Organic cellular membrane with concentric lipid bilayer dynamics
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    for (uint16_t d = 0; d < HALF_LENGTH; d++) {
        // Membrane shape using multiple octaves keyed to distance from center
        uint16_t membrane = 0;
        membrane += (uint16_t)(inoise8(d * 3, m_time >> 2) << 1);
        membrane += (uint16_t)(inoise8(d * 7, m_time >> 1) >> 1);
        membrane += inoise8(d * 13, m_time);
        membrane >>= 2;

        uint8_t hue = (uint8_t)(20 + (membrane >> 3));
        uint8_t brightness = scale8((uint8_t)membrane, ctx.brightness);

        CRGB color = ctx.palette.getColor(hue, brightness);
        SET_CENTER_PAIR(ctx, d, color);
    }

    // Add membrane potential waves radiating from center
    uint16_t potentialWave = beatsin16(5, 0, HALF_LENGTH - 1);
    for (int8_t w = -10; w <= 10; w++) {
        int16_t waveDist = (int16_t)potentialWave + w;
        if (waveDist >= 0 && waveDist < HALF_LENGTH) {
            uint8_t waveIntensity = (uint8_t)(255 - abs(w) * 20);
            // Blend potential wave symmetrically from center on both strips
            uint16_t left1  = CENTER_LEFT - (uint16_t)waveDist;
            uint16_t right1 = CENTER_RIGHT + (uint16_t)waveDist;
            uint16_t left2  = STRIP_LENGTH + CENTER_LEFT - (uint16_t)waveDist;
            uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + (uint16_t)waveDist;
            if (left1 < ctx.ledCount) {
                ctx.leds[left1] = blend(ctx.leds[left1], CRGB::Yellow, waveIntensity);
            }
            if (right1 < ctx.ledCount) {
                ctx.leds[right1] = blend(ctx.leds[right1], CRGB::Yellow, waveIntensity);
            }
            if (left2 < ctx.ledCount) {
                ctx.leds[left2] = blend(ctx.leds[left2], CRGB::Gold, waveIntensity);
            }
            if (right2 < ctx.ledCount) {
                ctx.leds[right2] = blend(ctx.leds[right2], CRGB::Gold, waveIntensity);
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
