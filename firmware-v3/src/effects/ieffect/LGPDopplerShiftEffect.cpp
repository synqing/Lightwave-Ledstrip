/**
 * @file LGPDopplerShiftEffect.cpp
 * @brief LGP Doppler Shift effect implementation
 */

#include "LGPDopplerShiftEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPDopplerShiftEffect::LGPDopplerShiftEffect()
    : m_sourcePosition(0.0f)
{
}

bool LGPDopplerShiftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_sourcePosition = 0.0f;
    return true;
}

void LGPDopplerShiftEffect::render(plugins::EffectContext& ctx) {
    // Moving colors shift frequency based on velocity
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_sourcePosition += speed * 5.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        float relativePos = i - fmodf(m_sourcePosition, (float)STRIP_LENGTH);
        float velocity = speed * 10.0f;

        float dopplerFactor;
        if (relativePos > 0.0f) {
            dopplerFactor = 1.0f - (velocity / 100.0f);
        } else {
            dopplerFactor = 1.0f + (velocity / 100.0f);
        }

        uint8_t shiftedHue = ctx.gHue;
        if (dopplerFactor > 1.0f) {
            shiftedHue = (uint8_t)(ctx.gHue - (uint8_t)(30.0f * (dopplerFactor - 1.0f)));
        } else {
            shiftedHue = (uint8_t)(ctx.gHue + (uint8_t)(30.0f * (1.0f - dopplerFactor)));
        }

        uint8_t brightness = (uint8_t)(255.0f * intensity * (1.0f - distFromCenter / HALF_LENGTH));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(shiftedHue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(shiftedHue + 90), brightU8);
        }
    }
}

void LGPDopplerShiftEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPDopplerShiftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Doppler Shift",
        "Red/Blue shift based on velocity",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
