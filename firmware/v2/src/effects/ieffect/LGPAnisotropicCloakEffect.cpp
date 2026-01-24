/**
 * @file LGPAnisotropicCloakEffect.cpp
 * @brief LGP Anisotropic Cloak effect implementation
 */

#include "LGPAnisotropicCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPAnisotropicCloakEffect::LGPAnisotropicCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.45f)
{
}

bool LGPAnisotropicCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.45f;
    return true;
}

void LGPAnisotropicCloakEffect::render(plugins::EffectContext& ctx) {
    // Directionally biased refractive shell
    m_time = (uint16_t)(m_time + (ctx.speed >> 2));

    float speedNorm = ctx.speed / 50.0f;
    const float cloakRadius = 20.0f;
    const float baseIndex = 1.0f;
    const float anisotropy = 0.5f;

    m_pos += m_vel * speedNorm;
    if (m_pos < cloakRadius || m_pos > (float)STRIP_LENGTH - cloakRadius) {
        m_vel = -m_vel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = fabsf((float)i - m_pos);
        float norm = (cloakRadius > 0.001f) ? (dist / cloakRadius) : 0.0f;
        norm = constrain(norm, 0.0f, 1.0f);

        float sideBias = (i < m_pos) ? (1.0f + anisotropy) : (1.0f - anisotropy);
        sideBias = constrain(sideBias, -2.0f, 2.0f);

        float offset = baseIndex * powf(norm, 1.5f) * sideBias * cloakRadius * 0.5f;
        float sample = (float)i + ((i < m_pos) ? -offset : offset);
        sample = constrain(sample, 0.0f, (float)(STRIP_LENGTH - 1));

        uint8_t wave = sin8((int16_t)(sample * 4.0f) + (m_time >> 2));
        float brightnessF = wave;

        if (norm < 0.25f) {
            brightnessF *= norm / 0.25f;
        }
        if (fabsf(norm - 1.0f) < 0.06f) {
            brightnessF = 255.0f;
        }

        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(sample) + (uint8_t)(sideBias * 20.0f));

        uint8_t brightU8 = (uint8_t)constrain(brightnessF, 0.0f, 255.0f);
        brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightU8);
        }
    }
}

void LGPAnisotropicCloakEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPAnisotropicCloakEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Anisotropic Cloak",
        "Direction-dependent visibility",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
