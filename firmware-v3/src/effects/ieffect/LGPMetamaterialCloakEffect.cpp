/**
 * @file LGPMetamaterialCloakEffect.cpp
 * @brief LGP Metamaterial Cloak effect implementation
 */

#include "LGPMetamaterialCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPMetamaterialCloakEffect::LGPMetamaterialCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.5f)
{
}

bool LGPMetamaterialCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.5f;
    return true;
}

void LGPMetamaterialCloakEffect::render(plugins::EffectContext& ctx) {
    // Negative refractive index creates invisibility effects
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float speedNorm = ctx.speed / 50.0f;
    const float cloakRadius = 15.0f;
    const float refractiveIndex = -1.5f;

    m_pos += m_vel * speedNorm;
    if (m_pos < cloakRadius || m_pos > (float)STRIP_LENGTH - cloakRadius) {
        m_vel = -m_vel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint8_t wave = sin8((uint16_t)(i * 4 + (m_time >> 2)));
        uint8_t hue = (uint8_t)(ctx.gHue + (i >> 2));

        float distFromCloak = fabsf((float)i - m_pos);

        if (distFromCloak < cloakRadius) {
            float bendAngle = (distFromCloak / cloakRadius) * PI;
            wave = sin8((int)(i * 4 * refractiveIndex) + (m_time >> 2) + (int)(bendAngle * 128.0f));

            if (distFromCloak < cloakRadius * 0.5f) {
                wave = scale8(wave, (uint8_t)(255.0f * (distFromCloak / (cloakRadius * 0.5f))));
            }

            if (fabsf(distFromCloak - cloakRadius) < 2.0f) {
                wave = 255;
                hue = 160;
            }
        }

        uint8_t brightU8 = (uint8_t)((wave * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightU8);
        }
    }
}

void LGPMetamaterialCloakEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPMetamaterialCloakEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Metamaterial Cloak",
        "Invisibility cloak simulation",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
