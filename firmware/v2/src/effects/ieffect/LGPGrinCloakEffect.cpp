/**
 * @file LGPGrinCloakEffect.cpp
 * @brief LGP GRIN Cloak effect implementation
 */

#include "LGPGrinCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kCloakRadius = 20.0f;
constexpr float kGradient = 1.5f;
constexpr float kPhaseStep = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"cloak_radius", "Cloak Radius", 10.0f, 32.0f, kCloakRadius, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "", false},
    {"gradient", "Gradient", 0.5f, 2.5f, kGradient, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"phase_step", "Phase Step", 0.1f, 2.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
};
}

LGPGrinCloakEffect::LGPGrinCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.35f)
    , m_cloakRadius(kCloakRadius)
    , m_gradient(kGradient)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPGrinCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.35f;
    m_cloakRadius = kCloakRadius;
    m_gradient = kGradient;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPGrinCloakEffect::render(plugins::EffectContext& ctx) {
    // Smooth gradient refractive profile emulating GRIN optics
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float speedNorm = ctx.speed / 50.0f;

    const float cloakRadius = m_cloakRadius;
    const float gradient = m_gradient;

    m_pos += m_vel * speedNorm;
    if (m_pos < cloakRadius || m_pos > (float)STRIP_LENGTH - cloakRadius) {
        m_vel = -m_vel;
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float dist = fabsf((float)i - m_pos);
        float norm = (cloakRadius > 0.001f) ? (dist / cloakRadius) : 0.0f;
        norm = constrain(norm, 0.0f, 1.0f);

        float lensStrength = gradient * (norm * norm);  // Optimized: exponent=2.0 → x*x
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

uint8_t LGPGrinCloakEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPGrinCloakEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPGrinCloakEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "cloak_radius") == 0) {
        m_cloakRadius = constrain(value, 10.0f, 32.0f);
        return true;
    }
    if (strcmp(name, "gradient") == 0) {
        m_gradient = constrain(value, 0.5f, 2.5f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 2.0f);
        return true;
    }
    return false;
}

float LGPGrinCloakEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "cloak_radius") == 0) return m_cloakRadius;
    if (strcmp(name, "gradient") == 0) return m_gradient;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
