/**
 * @file LGPChladniHarmonicsEffect.cpp
 * @brief LGP Chladni Harmonics effect implementation
 */

#include "LGPChladniHarmonicsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kModeNumber = 4;
constexpr float kVibrationRate = 0.08f;
constexpr float kMixRate = 0.05f;

const plugins::EffectParameter kParameters[] = {
    {"mode_number", "Mode Number", 2.0f, 8.0f, (float)kModeNumber, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"vibration_rate", "Vibration Rate", 0.02f, 0.2f, kVibrationRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"mix_rate", "Mix Rate", 0.01f, 0.2f, kMixRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
};
}

LGPChladniHarmonicsEffect::LGPChladniHarmonicsEffect()
    : m_vibrationPhase(0.0f)
    , m_mixPhase(0.0f)
    , m_modeNumber(kModeNumber)
    , m_vibrationRate(kVibrationRate)
    , m_mixRate(kMixRate)
{
}

bool LGPChladniHarmonicsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_vibrationPhase = 0.0f;
    m_mixPhase = 0.0f;
    m_modeNumber = kModeNumber;
    m_vibrationRate = kVibrationRate;
    m_mixRate = kMixRate;
    return true;
}

void LGPChladniHarmonicsEffect::render(plugins::EffectContext& ctx) {
    // Visualizes acoustic resonance patterns on vibrating plates
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    const int modeNumber = m_modeNumber;
    m_vibrationPhase += speed * m_vibrationRate;
    m_mixPhase += speed * m_mixRate;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        float normalizedPos = distFromCenter / (float)HALF_LENGTH;

        // Mode shape: standing wave pattern
        float modeShape = sinf(modeNumber * PI * normalizedPos);

        // Add mixing with nearby modes
        float mix1 = sinf((modeNumber + 1) * PI * normalizedPos) * sinf(m_mixPhase);
        float mix2 = sinf((modeNumber - 1) * PI * normalizedPos) * cosf(m_mixPhase * 1.3f);
        float mixedMode = modeShape * 0.75f + (mix1 + mix2) * 0.25f * 0.5f;

        // Temporal oscillation
        float temporalOscillation = cosf(m_vibrationPhase);
        float plateDisplacement = mixedMode * temporalOscillation;

        // Sand particle visualization
        float nodeStrength = 1.0f / (fabsf(modeShape) + 0.1f);
        nodeStrength = constrain(nodeStrength, 0.0f, 3.0f);

        float antinodeStrength = fabsf(plateDisplacement) * intensity;

        float particleBrightness = nodeStrength * (1.0f - intensity) * 0.3f;
        float motionBrightness = antinodeStrength * intensity;
        float totalBrightness = (particleBrightness + motionBrightness) * 255.0f;

        uint8_t brightness = (uint8_t)constrain(totalBrightness, 20.0f, 255.0f);

        uint8_t hue1 = (uint8_t)(ctx.gHue + (uint8_t)(plateDisplacement * 30.0f));
        uint8_t hue2 = (uint8_t)(ctx.gHue + 128 + (uint8_t)(plateDisplacement * 30.0f));

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue1, brightU8);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            float bottomDisplacement = -plateDisplacement;
            float bottomBrightness = (particleBrightness + fabsf(bottomDisplacement) * intensity) * 255.0f;
            uint8_t bottomBrightU8 = (uint8_t)constrain(bottomBrightness, 20.0f, 255.0f);
            bottomBrightU8 = (uint8_t)((bottomBrightU8 * ctx.brightness) / 255);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, bottomBrightU8);
        }
    }
}

void LGPChladniHarmonicsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChladniHarmonicsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chladni Harmonics",
        "Resonant nodal patterns",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPChladniHarmonicsEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPChladniHarmonicsEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPChladniHarmonicsEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "mode_number") == 0) {
        m_modeNumber = (int)constrain(value, 2.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "vibration_rate") == 0) {
        m_vibrationRate = constrain(value, 0.02f, 0.2f);
        return true;
    }
    if (strcmp(name, "mix_rate") == 0) {
        m_mixRate = constrain(value, 0.01f, 0.2f);
        return true;
    }
    return false;
}

float LGPChladniHarmonicsEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "mode_number") == 0) return (float)m_modeNumber;
    if (strcmp(name, "vibration_rate") == 0) return m_vibrationRate;
    if (strcmp(name, "mix_rate") == 0) return m_mixRate;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
