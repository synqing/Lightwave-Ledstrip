/**
 * @file LGPCausticFanEffect.cpp
 * @brief LGP Caustic Fan effect implementation
 */

#include "LGPCausticFanEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPCausticFanEffect::LGPCausticFanEffect()
    : m_time(0)
{
}

bool LGPCausticFanEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPCausticFanEffect::render(plugins::EffectContext& ctx) {
    // Two virtual focusing fans creating drifting caustic envelopes
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float intensityNorm = ctx.brightness / 255.0f;
    const float curvature = 1.5f;
    const float separation = 1.5f;
    const float gain = 12.0f;
    float animPhase = m_time / 256.0f;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)i - (float)CENTER_LEFT;

        float def1 = curvature * (x - separation) + sinf(animPhase);
        float def2 = -curvature * (x + separation) + sinf(animPhase * 1.21f);
        float diff = fabsf(def1 - def2);

        float caustic = 1.0f / (1.0f + diff * diff * gain);
        float envelope = 1.0f / (1.0f + fabsf(x) * 0.08f);
        float brightnessF = caustic * envelope * 255.0f * intensityNorm;

        brightnessF = constrain(brightnessF + (sin8((uint8_t)(i * 3 + (m_time >> 2))) >> 2), 0.0f, 255.0f);

        uint8_t brightness = (uint8_t)brightnessF;
        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(x * 1.5f) + (m_time >> 4));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 96), brightU8);
        }
    }
}

void LGPCausticFanEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPCausticFanEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Caustic Fan",
        "Focused light caustics",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
