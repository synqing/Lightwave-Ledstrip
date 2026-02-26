/**
 * @file LGPMetamaterialCloakEffect.cpp
 * @brief LGP Metamaterial Cloak effect implementation
 */

#include "LGPMetamaterialCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kCloakRadius = 15.0f;
constexpr float kRefractiveIndex = -1.5f;
constexpr float kPhaseStep = 0.25f;

const plugins::EffectParameter kParameters[] = {
    {"cloak_radius", "Cloak Radius", 8.0f, 30.0f, kCloakRadius, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "", false},
    {"refractive_index", "Refractive Index", -2.5f, -0.3f, kRefractiveIndex, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"phase_step", "Phase Step", 0.1f, 1.5f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
};
}

LGPMetamaterialCloakEffect::LGPMetamaterialCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.5f)
    , m_cloakRadius(kCloakRadius)
    , m_refractiveIndex(kRefractiveIndex)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPMetamaterialCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.5f;
    m_cloakRadius = kCloakRadius;
    m_refractiveIndex = kRefractiveIndex;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPMetamaterialCloakEffect::render(plugins::EffectContext& ctx) {
    // Negative refractive index creates invisibility effects
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float speedNorm = ctx.speed / 50.0f;
    const float cloakRadius = m_cloakRadius;
    const float refractiveIndex = m_refractiveIndex;

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

uint8_t LGPMetamaterialCloakEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPMetamaterialCloakEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPMetamaterialCloakEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "cloak_radius") == 0) {
        m_cloakRadius = constrain(value, 8.0f, 30.0f);
        return true;
    }
    if (strcmp(name, "refractive_index") == 0) {
        m_refractiveIndex = constrain(value, -2.5f, -0.3f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 1.5f);
        return true;
    }
    return false;
}

float LGPMetamaterialCloakEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "cloak_radius") == 0) return m_cloakRadius;
    if (strcmp(name, "refractive_index") == 0) return m_refractiveIndex;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
