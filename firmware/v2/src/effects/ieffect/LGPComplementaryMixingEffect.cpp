/**
 * @file LGPComplementaryMixingEffect.cpp
 * @brief LGP Complementary Mixing effect implementation
 */

#include "LGPComplementaryMixingEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.01f;
constexpr float kVariation = 0.5f;
constexpr float kCentreBrightness = 0.5f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.003f, 0.05f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.002f, "timing", "x", false},
    {"variation", "Variation", 0.1f, 1.0f, kVariation, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"centre_brightness", "Centre Brightness", 0.2f, 1.0f, kCentreBrightness, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
};
}

LGPComplementaryMixingEffect::LGPComplementaryMixingEffect()
    : m_phase(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_variation(kVariation)
    , m_centreBrightness(kCentreBrightness)
{
}

bool LGPComplementaryMixingEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_phaseRate = kPhaseRate;
    m_variation = kVariation;
    m_centreBrightness = kCentreBrightness;
    return true;
}

void LGPComplementaryMixingEffect::render(plugins::EffectContext& ctx) {
    // Dynamic complementary pairs create neutral zones
    float dt = ctx.getSafeDeltaSeconds();
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    m_phase += speed * m_phaseRate * 60.0f * dt;  // dt-corrected

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        uint8_t baseHue = (uint8_t)(ctx.gHue + (uint8_t)(m_phase * 255.0f));
        // Complementary color pairing: Strip 2 uses hue + 128 (180deg offset) for
        // intentional complementary color contrast, not rainbow cycling.
        uint8_t complementHue = (uint8_t)(baseHue + 128);

        uint8_t edgeIntensity = (uint8_t)(255.0f * (1.0f - normalizedDist * m_variation));

        if (normalizedDist > 0.5f) {
            uint8_t brightU8 = (uint8_t)(edgeIntensity * intensity);
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            ctx.leds[i] = ctx.palette.getColor(baseHue, brightU8);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(complementHue, brightU8);
            }
        } else {
            uint8_t brightU8 = (uint8_t)(255.0f * m_centreBrightness * intensity);
            brightU8 = (uint8_t)((brightU8 * ctx.brightness) / 255);
            ctx.leds[i] = ctx.palette.getColor(baseHue, brightU8);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(complementHue, brightU8);
            }
        }
    }
}

void LGPComplementaryMixingEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPComplementaryMixingEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Complementary Mixing",
        "Complementary color gradients",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPComplementaryMixingEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPComplementaryMixingEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPComplementaryMixingEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.003f, 0.05f);
        return true;
    }
    if (strcmp(name, "variation") == 0) {
        m_variation = constrain(value, 0.1f, 1.0f);
        return true;
    }
    if (strcmp(name, "centre_brightness") == 0) {
        m_centreBrightness = constrain(value, 0.2f, 1.0f);
        return true;
    }
    return false;
}

float LGPComplementaryMixingEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "variation") == 0) return m_variation;
    if (strcmp(name, "centre_brightness") == 0) return m_centreBrightness;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
