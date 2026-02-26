/**
 * @file ModalResonanceEffect.cpp
 * @brief LGP Modal Resonance implementation
 */

#include "ModalResonanceEffect.h"
#include "../CoreEffects.h"
#include <math.h>
#include <cstring>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kBaseModeMin = 5.0f;
constexpr float kBaseModeRange = 4.0f;
constexpr float kHarmonicWeight = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"base_mode_min", "Mode Min", 1.0f, 10.0f, kBaseModeMin, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"base_mode_range", "Mode Range", 0.5f, 8.0f, kBaseModeRange, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"harmonic_weight", "Harmonic", 0.0f, 1.0f, kHarmonicWeight, plugins::EffectParameterType::FLOAT, 0.01f, "wave", "", false},
};
}

ModalResonanceEffect::ModalResonanceEffect()
    : m_modalModePhase(0.0f)
    , m_baseModeMin(kBaseModeMin)
    , m_baseModeRange(kBaseModeRange)
    , m_harmonicWeight(kHarmonicWeight)
{
}

bool ModalResonanceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_modalModePhase = 0.0f;
    m_baseModeMin = kBaseModeMin;
    m_baseModeRange = kBaseModeRange;
    m_harmonicWeight = kHarmonicWeight;
    return true;
}

void ModalResonanceEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN MODAL RESONANCE - Explores different optical cavity modes

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_modalModePhase += speedNorm * 0.01f;

    // Mode number varies with time
    float baseMode = m_baseModeMin + sinf(m_modalModePhase) * m_baseModeRange;

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        float distFromCenter = ctx.getDistanceFromCenter(i) * (float)HALF_LENGTH;
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Mode pattern
        float modalPattern = sinf(normalizedDist * baseMode * TWO_PI);

        // Add harmonic
        modalPattern += sinf(normalizedDist * baseMode * 2.0f * TWO_PI) * m_harmonicWeight;
        modalPattern /= 1.5f;

        // Apply window function
        float window = sinf(normalizedDist * PI);
        modalPattern *= window;

        uint8_t brightness = (uint8_t)(128 + 127 * modalPattern * intensityNorm);

        uint8_t paletteIndex = (uint8_t)(baseMode * 10.0f) + (uint8_t)(normalizedDist * 50.0f);

        CRGB color = ctx.palette.getColor(ctx.gHue + paletteIndex, brightness);
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(ctx.gHue + paletteIndex + 128, brightness);
        }
    }
}

void ModalResonanceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ModalResonanceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Modal Resonance",
        "Explores different optical cavity resonance modes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t ModalResonanceEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* ModalResonanceEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool ModalResonanceEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "base_mode_min") == 0) {
        m_baseModeMin = constrain(value, 1.0f, 10.0f);
        return true;
    }
    if (strcmp(name, "base_mode_range") == 0) {
        m_baseModeRange = constrain(value, 0.5f, 8.0f);
        return true;
    }
    if (strcmp(name, "harmonic_weight") == 0) {
        m_harmonicWeight = constrain(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float ModalResonanceEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "base_mode_min") == 0) return m_baseModeMin;
    if (strcmp(name, "base_mode_range") == 0) return m_baseModeRange;
    if (strcmp(name, "harmonic_weight") == 0) return m_harmonicWeight;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
