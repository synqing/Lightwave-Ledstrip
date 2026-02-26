/**
 * @file LGPMoireCurtainsEffect.cpp
 * @brief LGP Moire Curtains effect implementation
 */

#include "LGPMoireCurtainsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kBaseFreq = 8.0f;
constexpr float kDeltaFreq = 0.2f;
constexpr float kPhaseStep = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"base_freq", "Base Frequency", 4.0f, 16.0f, kBaseFreq, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"delta_freq", "Delta Frequency", 0.05f, 1.0f, kDeltaFreq, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
    {"phase_step", "Phase Step", 0.25f, 4.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
};
}

LGPMoireCurtainsEffect::LGPMoireCurtainsEffect()
    : m_phase(0)
    , m_baseFreq(kBaseFreq)
    , m_deltaFreq(kDeltaFreq)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPMoireCurtainsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0;
    m_baseFreq = kBaseFreq;
    m_deltaFreq = kDeltaFreq;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPMoireCurtainsEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Two slightly mismatched frequencies create beat patterns
    const float leftFreq = m_baseFreq + m_deltaFreq / 2.0f;
    const float rightFreq = m_baseFreq - m_deltaFreq / 2.0f;
    m_phase = (uint16_t)(m_phase + (uint16_t)(ctx.speed * m_phaseStep));

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

uint8_t LGPMoireCurtainsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPMoireCurtainsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPMoireCurtainsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "base_freq") == 0) {
        m_baseFreq = constrain(value, 4.0f, 16.0f);
        return true;
    }
    if (strcmp(name, "delta_freq") == 0) {
        m_deltaFreq = constrain(value, 0.05f, 1.0f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.25f, 4.0f);
        return true;
    }
    return false;
}

float LGPMoireCurtainsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "base_freq") == 0) return m_baseFreq;
    if (strcmp(name, "delta_freq") == 0) return m_deltaFreq;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
