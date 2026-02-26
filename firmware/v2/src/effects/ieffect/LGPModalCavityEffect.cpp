/**
 * @file LGPModalCavityEffect.cpp
 * @brief LGP Modal Cavity effect implementation
 */

#include "LGPModalCavityEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kModeNumber = 8;
constexpr int kBeatOffset = 2;
constexpr float kPhaseStep = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"mode_number", "Mode Number", 3.0f, 14.0f, (float)kModeNumber, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"beat_offset", "Beat Offset", 1.0f, 6.0f, (float)kBeatOffset, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"phase_step", "Phase Step", 0.25f, 4.0f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
};
}

LGPModalCavityEffect::LGPModalCavityEffect()
    : m_time(0)
    , m_modeNumber(kModeNumber)
    , m_beatOffset(kBeatOffset)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPModalCavityEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_modeNumber = kModeNumber;
    m_beatOffset = kBeatOffset;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPModalCavityEffect::render(plugins::EffectContext& ctx) {
    // Excite specific waveguide modes
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));
    const uint8_t modeNumber = (uint8_t)m_modeNumber;
    const uint8_t beatMode = (uint8_t)(m_modeNumber + m_beatOffset);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float x = (float)centerPairDistance(i) / (float)HALF_LENGTH;

        // Primary mode
        int16_t mode1 = sin16((uint16_t)(x * modeNumber * 32768.0f));

        // Beat mode
        int16_t mode2 = sin16((uint16_t)(x * beatMode * 32768.0f) + m_time);

        // Combine modes
        int16_t combined = (int16_t)((mode1 >> 1) + (mode2 >> 2));
        uint8_t brightness = (uint8_t)((combined + 32768) >> 8);

        // Apply cosine taper
        uint8_t taper = (uint8_t)(cos8((uint8_t)(x * 255.0f)) >> 1);
        brightness = scale8(brightness, (uint8_t)(128 + taper));
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (modeNumber * 12));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 64), brightness);
        }
    }
}

void LGPModalCavityEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPModalCavityEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Modal Cavity",
        "Resonant optical cavity modes",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPModalCavityEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPModalCavityEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPModalCavityEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "mode_number") == 0) {
        m_modeNumber = (int)constrain(value, 3.0f, 14.0f);
        return true;
    }
    if (strcmp(name, "beat_offset") == 0) {
        m_beatOffset = (int)constrain(value, 1.0f, 6.0f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.25f, 4.0f);
        return true;
    }
    return false;
}

float LGPModalCavityEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "mode_number") == 0) return (float)m_modeNumber;
    if (strcmp(name, "beat_offset") == 0) return (float)m_beatOffset;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
