/**
 * @file LGPAuroraBorealisEffect.cpp
 * @brief LGP Aurora Borealis effect implementation
 */

#include "LGPAuroraBorealisEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPAuroraBorealisEffect::LGPAuroraBorealisEffect()
    : m_time(0)
    , m_curtainPhase{0, 51, 102, 153, 204}
{
}

bool LGPAuroraBorealisEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    const uint8_t initPhases[5] = {0, 51, 102, 153, 204};
    memcpy(m_curtainPhase, initPhases, sizeof(m_curtainPhase));
    return true;
}

void LGPAuroraBorealisEffect::render(plugins::EffectContext& ctx) {
    // Northern lights simulation with concentric curtains radiating from center
    m_time = (uint16_t)(m_time + (ctx.speed >> 4));

    const uint8_t curtainCount = 4;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint8_t c = 0; c < curtainCount; c++) {
        m_curtainPhase[c] = (uint8_t)(m_curtainPhase[c] + (c + 1));
        // Curtain center is now a radial distance from center [0, HALF_LENGTH)
        uint16_t curtainDist = beatsin16(1, 0, HALF_LENGTH - 1, 0, (uint16_t)(m_curtainPhase[c] << 8));
        uint8_t curtainWidth = beatsin8(1, 20, 35, 0, m_curtainPhase[c]);

        // Aurora colors - greens, blues, purples
        uint8_t hue = (uint8_t)(96 + (c * 32));

        for (uint16_t d = 0; d < HALF_LENGTH; d++) {
            int16_t radialDist = abs((int16_t)d - (int16_t)curtainDist);
            if (radialDist < curtainWidth) {
                uint8_t brightness = qsub8(255, (uint8_t)((radialDist * 255) / curtainWidth));
                brightness = scale8(brightness, ctx.brightness);

                // Shimmer effect keyed to distance from center
                brightness = scale8(brightness, (uint8_t)(220 + (inoise8(d * 5, m_time >> 3) >> 3)));

                CRGB color = ctx.palette.getColor(hue, brightness);
                SET_CENTER_PAIR(ctx, d, color);
            }
        }
    }

    // Add corona at outer edges (high distances from center)
    for (uint8_t i = 0; i < 20; i++) {
        uint16_t outerDist = HALF_LENGTH - 1 - i;
        uint8_t corona = scale8((uint8_t)(255 - i * 12), ctx.brightness >> 1);
        CRGB coronaColor = CRGB(0, corona >> 2, corona >> 1);
        SET_CENTER_PAIR(ctx, outerDist, coronaColor);
    }
}

void LGPAuroraBorealisEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPAuroraBorealisEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Aurora Borealis",
        "Shimmering curtain lights",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
