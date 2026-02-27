/**
 * @file LGPMoireCurtainsEffect.cpp
 * @brief LGP Moire Curtains effect implementation
 */

#include "LGPMoireCurtainsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPMoireCurtainsEffect::LGPMoireCurtainsEffect()
    : m_phase(0)
{
}

bool LGPMoireCurtainsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0;
    return true;
}

void LGPMoireCurtainsEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Two slightly mismatched frequencies create beat patterns
    const float baseFreq = 8.0f;
    const float delta = 0.2f;

    const float leftFreq = baseFreq + delta / 2.0f;
    const float rightFreq = baseFreq - delta / 2.0f;

    m_phase = (uint16_t)(m_phase + ctx.speed);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);

        // Left strip
        uint16_t leftPhaseVal = (uint16_t)(sin16(distFromCenter * leftFreq * 410.0f + m_phase) + 32768) >> 8;
        uint8_t leftBright = scale8(leftPhaseVal, ctx.brightness);
        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + (uint8_t)(distFromCenter / 2.0f)), leftBright);

        // Right strip - slightly different frequency
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint16_t rightPhaseVal = (uint16_t)(sin16(distFromCenter * rightFreq * 410.0f + m_phase) + 32768) >> 8;
            uint8_t rightBright = scale8(rightPhaseVal, ctx.brightness);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(
                (uint8_t)(ctx.gHue + (uint8_t)(distFromCenter / 2.0f) + 128),
                rightBright);
        }
    }
}

void LGPMoireCurtainsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMoireCurtainsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Moire Curtains",
        "Shifting moire interference layers",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
