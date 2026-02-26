/**
 * @file LGPAnisotropicCloakEffect.cpp
 * @brief LGP Anisotropic Cloak effect implementation
 */

#include "LGPAnisotropicCloakEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kCloakRadius = 20.0f;
constexpr float kBaseIndex = 1.0f;
constexpr float kAnisotropy = 0.5f;
constexpr float kPhaseStep = 0.25f;

const plugins::EffectParameter kParameters[] = {
    {"cloak_radius", "Cloak Radius", 10.0f, 32.0f, kCloakRadius, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "", false},
    {"base_index", "Base Index", 0.2f, 2.0f, kBaseIndex, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"anisotropy", "Anisotropy", 0.0f, 0.95f, kAnisotropy, plugins::EffectParameterType::FLOAT, 0.02f, "wave", "", false},
    {"phase_step", "Phase Step", 0.1f, 1.2f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", true},
};
}

LGPAnisotropicCloakEffect::LGPAnisotropicCloakEffect()
    : m_time(0)
    , m_pos(80.0f)
    , m_vel(0.45f)
    , m_cloakRadius(kCloakRadius)
    , m_baseIndex(kBaseIndex)
    , m_anisotropy(kAnisotropy)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPAnisotropicCloakEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_pos = 80.0f;
    m_vel = 0.45f;
    m_cloakRadius = kCloakRadius;
    m_baseIndex = kBaseIndex;
    m_anisotropy = kAnisotropy;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPAnisotropicCloakEffect::render(plugins::EffectContext& ctx) {
    // Directionally biased refractive shell
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float speedNorm = ctx.speed / 50.0f;
    const float cloakRadius = m_cloakRadius;
    const float baseIndex = m_baseIndex;
    const float anisotropy = m_anisotropy;

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

        float offset = baseIndex * (norm * sqrtf(norm)) * sideBias * cloakRadius * 0.5f;  // Optimized: x^1.5 = x*sqrt(x)
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

uint8_t LGPAnisotropicCloakEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPAnisotropicCloakEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPAnisotropicCloakEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "cloak_radius") == 0) {
        m_cloakRadius = constrain(value, 10.0f, 32.0f);
        return true;
    }
    if (strcmp(name, "base_index") == 0) {
        m_baseIndex = constrain(value, 0.2f, 2.0f);
        return true;
    }
    if (strcmp(name, "anisotropy") == 0) {
        m_anisotropy = constrain(value, 0.0f, 0.95f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 1.2f);
        return true;
    }
    return false;
}

float LGPAnisotropicCloakEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "cloak_radius") == 0) return m_cloakRadius;
    if (strcmp(name, "base_index") == 0) return m_baseIndex;
    if (strcmp(name, "anisotropy") == 0) return m_anisotropy;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
