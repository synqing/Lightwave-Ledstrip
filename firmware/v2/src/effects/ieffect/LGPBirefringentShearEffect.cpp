/**
 * @file LGPBirefringentShearEffect.cpp
 * @brief LGP Birefringent Shear effect implementation
 */

#include "LGPBirefringentShearEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kBaseFrequency = 3.5f;
constexpr float kDeltaK = 1.5f;
constexpr float kDrift = 0.3f;
constexpr float kPhaseStep = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"base_frequency", "Base Frequency", 1.0f, 8.0f, kBaseFrequency, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"delta_k", "Delta K", 0.1f, 3.0f, kDeltaK, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "", false},
    {"drift", "Drift", 0.0f, 1.5f, kDrift, plugins::EffectParameterType::FLOAT, 0.02f, "timing", "x", false},
    {"phase_step", "Phase Step", 0.1f, 2.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", true},
};
}

LGPBirefringentShearEffect::LGPBirefringentShearEffect()
    : m_time(0)
    , m_baseFrequency(kBaseFrequency)
    , m_deltaK(kDeltaK)
    , m_drift(kDrift)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPBirefringentShearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_baseFrequency = kBaseFrequency;
    m_deltaK = kDeltaK;
    m_drift = kDrift;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPBirefringentShearEffect::render(plugins::EffectContext& ctx) {
    // Dual spatial modes slipping past one another
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float intensityNorm = ctx.brightness / 255.0f;
    const float baseFrequency = m_baseFrequency;
    const float deltaK = m_deltaK;
    const float drift = m_drift;

    uint8_t mixWave = (uint8_t)(intensityNorm * 255.0f);
    uint8_t mixCarrier = (uint8_t)(255 - mixWave);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float idx = (float)i;

        float phaseBase = m_time / 128.0f;
        float phase1 = idx * (baseFrequency + deltaK) + phaseBase;
        float phase2 = idx * (baseFrequency - deltaK) - phaseBase + drift * idx * 0.05f;

        uint8_t wave1 = sin8((int16_t)(phase1 * 16.0f));
        uint8_t wave2 = sin8((int16_t)(phase2 * 16.0f));

        uint8_t combined = qadd8(scale8(wave1, mixCarrier), scale8(wave2, mixWave));
        uint8_t beat = (uint8_t)abs((int)wave1 - (int)wave2);
        uint8_t brightness = qadd8(combined, scale8(beat, 96));

        uint8_t hue1 = (uint8_t)(ctx.gHue + (uint8_t)(idx) + (m_time >> 4));
        uint8_t hue2 = (uint8_t)(hue1 + 128);

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue1, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, brightU8);
        }
    }
}

void LGPBirefringentShearEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBirefringentShearEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Birefringent Shear",
        "Polarization splitting",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPBirefringentShearEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPBirefringentShearEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPBirefringentShearEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "base_frequency") == 0) {
        m_baseFrequency = constrain(value, 1.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "delta_k") == 0) {
        m_deltaK = constrain(value, 0.1f, 3.0f);
        return true;
    }
    if (strcmp(name, "drift") == 0) {
        m_drift = constrain(value, 0.0f, 1.5f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 2.0f);
        return true;
    }
    return false;
}

float LGPBirefringentShearEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "base_frequency") == 0) return m_baseFrequency;
    if (strcmp(name, "delta_k") == 0) return m_deltaK;
    if (strcmp(name, "drift") == 0) return m_drift;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
