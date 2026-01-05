/**
 * @file LGPGrinCloakEffect.cpp
 * @brief LGP GRIN Cloak effect implementation
 */

#include "LGPGrinCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPGrinCloakEffect::LGPGrinCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.35f)
{
}

bool LGPGrinCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.35f;
    return true;
}

void LGPGrinCloakEffect::render(plugins::EffectContext& ctx) {
    // Smooth gradient refractive profile emulating GRIN optics
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    float speedNorm = ctx.speed / 50.0f;

    const float cloakRadius = 20.0f;
    const float exponent = 2.0f;
    const float gradient = 1.5f;

    m_pos += m_vel * speedNorm;
    if (m_pos < cloakRadius || m_pos > (float)STRIP_LENGTH - cloakRadius) {
        m_vel = -m_vel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = fabsf((float)i - m_pos);
        float norm = (cloakRadius > 0.001f) ? (dist / cloakRadius) : 0.0f;
        norm = constrain(norm, 0.0f, 1.0f);

        float lensStrength = gradient * powf(norm, exponent);
        float direction = (i < m_pos) ? -1.0f : 1.0f;
        float sample = (float)i + direction * lensStrength * cloakRadius * 0.6f;
        sample = constrain(sample, 0.0f, (float)(STRIP_LENGTH - 1));

        uint8_t wave = sin8((int16_t)(sample * 4.0f) + (m_time >> 2));

        float focusGain = 1.0f + (1.0f - norm) * gradient * 0.3f;
        float brightnessF = wave * focusGain;

        if (norm < 0.3f) {
            brightnessF *= norm / 0.3f;
        }

        if (fabsf(norm - 1.0f) < 0.08f) {
            brightnessF = 255.0f;
        }

        uint8_t brightness = (uint8_t)constrain(brightnessF, 0.0f, 255.0f);
        uint8_t hue = (uint8_t)(ctx.gHue + (uint8_t)(sample * 1.5f));

        // Use palette system - apply brightness scaling
        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 128), brightU8);
        }
    }
}

void LGPGrinCloakEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPGrinCloakEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP GRIN Cloak",
        "Gradient index optics",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
