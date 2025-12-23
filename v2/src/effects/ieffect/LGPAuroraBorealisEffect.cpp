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
    // Northern lights simulation with waveguide color mixing
    m_time = (uint16_t)(m_time + (ctx.speed >> 4));

    const uint8_t curtainCount = 4;

    fadeToBlackBy(ctx.leds, ctx.ledCount, 20);

    for (uint8_t c = 0; c < curtainCount; c++) {
        m_curtainPhase[c] = (uint8_t)(m_curtainPhase[c] + (c + 1));
        uint16_t curtainCenter = beatsin16(1, 20, STRIP_LENGTH - 20, 0, (uint16_t)(m_curtainPhase[c] << 8));
        uint8_t curtainWidth = beatsin8(1, 20, 35, 0, m_curtainPhase[c]);

        // Aurora colors - greens, blues, purples
        uint8_t hue = (uint8_t)(96 + (c * 32));

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            int16_t dist = abs((int16_t)i - (int16_t)curtainCenter);
            if (dist < curtainWidth) {
                uint8_t brightness = qsub8(255, (uint8_t)((dist * 255) / curtainWidth));
                brightness = scale8(brightness, ctx.brightness);

                // Shimmer effect
                brightness = scale8(brightness, (uint8_t)(220 + (inoise8(i * 5, m_time >> 3) >> 3)));

                CRGB color1 = ctx.palette.getColor(hue, brightness);
                CRGB color2 = ctx.palette.getColor((uint8_t)(hue + 20), brightness);

                ctx.leds[i] += color1;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] += color2;
                }
            }
        }
    }

    // Add corona at edges
    for (uint8_t i = 0; i < 20; i++) {
        uint8_t corona = scale8((uint8_t)(255 - i * 12), ctx.brightness >> 1);
        ctx.leds[i] += CRGB(0, corona >> 2, corona >> 1);
        ctx.leds[STRIP_LENGTH - 1 - i] += CRGB(0, corona >> 2, corona >> 1);
        if (STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[STRIP_LENGTH + i] += CRGB(0, corona >> 3, corona);
            ctx.leds[ctx.ledCount - 1 - i] += CRGB(0, corona >> 3, corona);
        }
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
