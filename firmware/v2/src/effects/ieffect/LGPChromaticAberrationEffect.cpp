/**
 * @file LGPChromaticAberrationEffect.cpp
 * @brief LGP Chromatic Aberration effect implementation
 */

#include "LGPChromaticAberrationEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.01f;
constexpr float kAberration = 1.5f;
constexpr float kFocusSpread = 0.1f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.003f, 0.05f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.002f, "timing", "x", false},
    {"aberration", "Aberration", 0.6f, 3.0f, kAberration, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"focus_spread", "Focus Spread", 0.02f, 0.25f, kFocusSpread, plugins::EffectParameterType::FLOAT, 0.01f, "blend", "x", false},
};
}

LGPChromaticAberrationEffect::LGPChromaticAberrationEffect()
    : m_lensPosition(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_aberration(kAberration)
    , m_focusSpread(kFocusSpread)
{
}

bool LGPChromaticAberrationEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_lensPosition = 0.0f;
    m_phaseRate = kPhaseRate;
    m_aberration = kAberration;
    m_focusSpread = kFocusSpread;
    return true;
}

void LGPChromaticAberrationEffect::render(plugins::EffectContext& ctx) {
    // Different wavelengths refract at different angles
    float dt = ctx.getSafeDeltaSeconds();
    float intensity = ctx.brightness / 255.0f;
    m_lensPosition += ctx.speed * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redFocus = sinf((normalizedDist - m_focusSpread * m_aberration) * PI + m_lensPosition);
        float greenFocus = sinf(normalizedDist * PI + m_lensPosition);
        float blueFocus = sinf((normalizedDist + m_focusSpread * m_aberration) * PI + m_lensPosition);

        CRGB aberratedColor;
        aberratedColor.r = (uint8_t)(constrain(128.0f + 127.0f * redFocus, 0.0f, 255.0f) * intensity);
        aberratedColor.g = (uint8_t)(constrain(128.0f + 127.0f * greenFocus, 0.0f, 255.0f) * intensity);
        aberratedColor.b = (uint8_t)(constrain(128.0f + 127.0f * blueFocus, 0.0f, 255.0f) * intensity);

        ctx.leds[i] = aberratedColor;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            CRGB aberratedColor2;
            aberratedColor2.r = (uint8_t)(constrain(128.0f + 127.0f * blueFocus, 0.0f, 255.0f) * intensity);
            aberratedColor2.g = (uint8_t)(constrain(128.0f + 127.0f * greenFocus, 0.0f, 255.0f) * intensity);
            aberratedColor2.b = (uint8_t)(constrain(128.0f + 127.0f * redFocus, 0.0f, 255.0f) * intensity);
            ctx.leds[i + STRIP_LENGTH] = aberratedColor2;
        }
    }
}

void LGPChromaticAberrationEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChromaticAberrationEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Aberration",
        "Lens dispersion edge effects",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPChromaticAberrationEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPChromaticAberrationEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPChromaticAberrationEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.003f, 0.05f);
        return true;
    }
    if (strcmp(name, "aberration") == 0) {
        m_aberration = constrain(value, 0.6f, 3.0f);
        return true;
    }
    if (strcmp(name, "focus_spread") == 0) {
        m_focusSpread = constrain(value, 0.02f, 0.25f);
        return true;
    }
    return false;
}

float LGPChromaticAberrationEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "aberration") == 0) return m_aberration;
    if (strcmp(name, "focus_spread") == 0) return m_focusSpread;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
