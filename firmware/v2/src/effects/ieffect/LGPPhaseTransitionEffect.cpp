/**
 * @file LGPPhaseTransitionEffect.cpp
 * @brief LGP Phase Transition effect implementation
 */

#include "LGPPhaseTransitionEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.1f;
constexpr float kPressure = 0.5f;
constexpr float kPlasmaScale = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.03f, 0.30f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.01f, "timing", "x", false},
    {"pressure", "Pressure", 0.1f, 1.2f, kPressure, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"plasma_scale", "Plasma Scale", 0.5f, 2.0f, kPlasmaScale, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
};
}

LGPPhaseTransitionEffect::LGPPhaseTransitionEffect()
    : m_phaseAnimation(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_pressure(kPressure)
    , m_plasmaScale(kPlasmaScale)
{
}

bool LGPPhaseTransitionEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phaseAnimation = 0.0f;
    m_phaseRate = kPhaseRate;
    m_pressure = kPressure;
    m_plasmaScale = kPlasmaScale;
    return true;
}

void LGPPhaseTransitionEffect::render(plugins::EffectContext& ctx) {
    // Colors undergo state changes like matter
    float temperature = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    m_phaseAnimation += temperature * m_phaseRate;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float localTemp = temperature + (normalizedDist * m_pressure);

        CRGB color;
        uint8_t paletteOffset = 0;
        uint8_t brightness = 0;

        if (localTemp < 0.25f) {
            // Solid phase
            float crystal = sinf(distFromCenter * 0.3f) * 0.5f + 0.5f;
            paletteOffset = (uint8_t)(crystal * 5.0f);
            brightness = (uint8_t)(255.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        } else if (localTemp < 0.5f) {
            // Liquid phase (sin(k*dist + phase) = INWARD flow - intentional design)
            float flow = sinf(distFromCenter * 0.1f + m_phaseAnimation);
            paletteOffset = (uint8_t)(10 + flow * 5.0f);
            brightness = (uint8_t)(200.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        } else if (localTemp < 0.75f) {
            // Gas phase
            float dispersion = random8() / 255.0f;
            if (dispersion < 0.3f) {
                paletteOffset = 20;
                brightness = (uint8_t)(150.0f * intensity);
                color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
            } else {
                color = CRGB::Black;
            }
        } else {
            // Plasma phase (sin(k*dist + phase) = INWARD, 10x for sharp plasma bands - intentional design)
            float plasma = sinf(distFromCenter * 0.5f + m_phaseAnimation * 10.0f * m_plasmaScale);
            paletteOffset = (uint8_t)(30 + plasma * 10.0f);
            brightness = (uint8_t)(255.0f * intensity);
            color = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset), brightness);
        }

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset + 60), brightness);
        }
    }
}

void LGPPhaseTransitionEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhaseTransitionEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Phase Transition",
        "State change simulation",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPPhaseTransitionEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPPhaseTransitionEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPPhaseTransitionEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.03f, 0.30f);
        return true;
    }
    if (strcmp(name, "pressure") == 0) {
        m_pressure = constrain(value, 0.1f, 1.2f);
        return true;
    }
    if (strcmp(name, "plasma_scale") == 0) {
        m_plasmaScale = constrain(value, 0.5f, 2.0f);
        return true;
    }
    return false;
}

float LGPPhaseTransitionEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "pressure") == 0) return m_pressure;
    if (strcmp(name, "plasma_scale") == 0) return m_plasmaScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
