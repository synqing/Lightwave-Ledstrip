/**
 * @file LGPPlasmaMembraneEffect.cpp
 * @brief LGP Plasma Membrane effect implementation
 */

#include "LGPPlasmaMembraneEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.5f;
constexpr float kPotentialRate = 5.0f;
constexpr float kOuterScale = 0.784f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.2f, 1.5f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"potential_rate", "Potential Rate", 1.0f, 12.0f, kPotentialRate, plugins::EffectParameterType::FLOAT, 0.5f, "wave", "Hz", false},
    {"outer_scale", "Outer Scale", 0.3f, 1.0f, kOuterScale, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "", false},
};
}

LGPPlasmaMembraneEffect::LGPPlasmaMembraneEffect()
    : m_time(0)
    , m_phaseRate(kPhaseRate)
    , m_potentialRate(kPotentialRate)
    , m_outerScale(kOuterScale)
{
}

bool LGPPlasmaMembraneEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_phaseRate = kPhaseRate;
    m_potentialRate = kPotentialRate;
    m_outerScale = kOuterScale;
    return true;
}

void LGPPlasmaMembraneEffect::render(plugins::EffectContext& ctx) {
    // Organic cellular membrane with lipid bilayer dynamics
    const uint16_t phaseStep = (uint16_t)fmaxf(1.0f, ctx.speed * m_phaseRate);
    m_time = (uint16_t)(m_time + phaseStep);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Membrane shape using multiple octaves
        uint16_t membrane = 0;
        membrane += (uint16_t)(inoise8(i * 3, m_time >> 2) << 1);
        membrane += (uint16_t)(inoise8(i * 7, m_time >> 1) >> 1);
        membrane += inoise8(i * 13, m_time);
        membrane >>= 2;

        uint8_t hue = (uint8_t)(20 + (membrane >> 3));
        uint8_t sat = (uint8_t)(200 + (membrane >> 2));
        uint8_t brightness = scale8((uint8_t)membrane, ctx.brightness);

        CRGB inner = ctx.palette.getColor(hue, brightness);
        uint8_t outerBright = scale8(brightness, (uint8_t)(m_outerScale * 255.0f));
        outerBright = (uint8_t)((outerBright * ctx.brightness) / 255);
        CRGB outer = ctx.palette.getColor((uint8_t)(hue + 10), outerBright);

        ctx.leds[i] = inner;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = outer;
        }
    }

    // Add membrane potential waves
    uint16_t potentialWave = beatsin16((uint16_t)m_potentialRate, 0, STRIP_LENGTH - 1);
    for (int8_t w = -10; w <= 10; w++) {
        int16_t pos = (int16_t)potentialWave + w;
        if (pos >= 0 && pos < STRIP_LENGTH) {
            uint8_t waveIntensity = (uint8_t)(255 - abs(w) * 20);
            ctx.leds[pos] = blend(ctx.leds[pos], CRGB::Yellow, waveIntensity);
            if (pos + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[pos + STRIP_LENGTH] = blend(ctx.leds[pos + STRIP_LENGTH], CRGB::Gold, waveIntensity);
            }
        }
    }
}

void LGPPlasmaMembraneEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPlasmaMembraneEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Plasma Membrane",
        "Cellular membrane fluctuations",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

uint8_t LGPPlasmaMembraneEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPPlasmaMembraneEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPPlasmaMembraneEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.2f, 1.5f);
        return true;
    }
    if (strcmp(name, "potential_rate") == 0) {
        m_potentialRate = constrain(value, 1.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "outer_scale") == 0) {
        m_outerScale = constrain(value, 0.3f, 1.0f);
        return true;
    }
    return false;
}

float LGPPlasmaMembraneEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "potential_rate") == 0) return m_potentialRate;
    if (strcmp(name, "outer_scale") == 0) return m_outerScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
