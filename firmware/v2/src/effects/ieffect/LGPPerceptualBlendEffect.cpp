/**
 * @file LGPPerceptualBlendEffect.cpp
 * @brief LGP Perceptual Blend effect implementation
 */

#include "LGPPerceptualBlendEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.01f;
constexpr float kLumaScale = 1.0f;
constexpr float kChromaScale = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.003f, 0.05f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.002f, "timing", "x", false},
    {"luma_scale", "Luma Scale", 0.5f, 1.5f, kLumaScale, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "x", false},
    {"chroma_scale", "Chroma Scale", 0.5f, 2.0f, kChromaScale, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "x", false},
};
}

LGPPerceptualBlendEffect::LGPPerceptualBlendEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_lumaScale(kLumaScale)
    , m_chromaScale(kChromaScale)
{
}

bool LGPPerceptualBlendEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_lumaScale = kLumaScale;
    m_chromaScale = kChromaScale;
    return true;
}

void LGPPerceptualBlendEffect::render(plugins::EffectContext& ctx) {
    // Uses perceptually uniform color space for natural mixing
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_phase += speed * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float L = (50.0f + 50.0f * sinf(m_phase)) * m_lumaScale;
        float a = 50.0f * m_chromaScale * cosf(m_phase + normalizedDist * PI);
        float b = 50.0f * m_chromaScale * sinf(m_phase - normalizedDist * PI);

        CRGB color;
        color.r = (uint8_t)(constrain(L + a * 2.0f, 0.0f, 255.0f) * intensity);
        color.g = (uint8_t)(constrain(L - a - b, 0.0f, 255.0f) * intensity);
        color.b = (uint8_t)(constrain(L + b * 2.0f, 0.0f, 255.0f) * intensity);

        ctx.leds[i] = color;

        if (i + STRIP_LENGTH < ctx.ledCount) {
            CRGB color2;
            color2.r = (uint8_t)(constrain(L - a * 2.0f, 0.0f, 255.0f) * intensity);
            color2.g = (uint8_t)(constrain(L + a + b, 0.0f, 255.0f) * intensity);
            color2.b = (uint8_t)(constrain(L - b * 2.0f, 0.0f, 255.0f) * intensity);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerceptualBlendEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerceptualBlendEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perceptual Blend",
        "Lab color space mixing",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPPerceptualBlendEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPPerceptualBlendEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPPerceptualBlendEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.003f, 0.05f);
        return true;
    }
    if (strcmp(name, "luma_scale") == 0) {
        m_lumaScale = constrain(value, 0.5f, 1.5f);
        return true;
    }
    if (strcmp(name, "chroma_scale") == 0) {
        m_chromaScale = constrain(value, 0.5f, 2.0f);
        return true;
    }
    return false;
}

float LGPPerceptualBlendEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "luma_scale") == 0) return m_lumaScale;
    if (strcmp(name, "chroma_scale") == 0) return m_chromaScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
