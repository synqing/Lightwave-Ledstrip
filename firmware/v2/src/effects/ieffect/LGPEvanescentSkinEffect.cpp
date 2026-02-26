/**
 * @file LGPEvanescentSkinEffect.cpp
 * @brief LGP Evanescent Skin effect implementation
 */

#include "LGPEvanescentSkinEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kLambda = 4.0f;
constexpr float kSkinFreq = 7.0f;
constexpr float kRingRadiusScale = 0.6f;
constexpr float kPhaseStep = 0.25f;

const plugins::EffectParameter kParameters[] = {
    {"lambda", "Lambda", 1.0f, 10.0f, kLambda, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"skin_freq", "Skin Frequency", 2.0f, 12.0f, kSkinFreq, plugins::EffectParameterType::FLOAT, 0.1f, "wave", "", false},
    {"ring_radius_scale", "Ring Radius", 0.2f, 1.0f, kRingRadiusScale, plugins::EffectParameterType::FLOAT, 0.02f, "wave", "x", false},
    {"phase_step", "Phase Step", 0.1f, 1.2f, kPhaseStep, plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", true},
};
}

LGPEvanescentSkinEffect::LGPEvanescentSkinEffect()
    : m_time(0)
    , m_lambda(kLambda)
    , m_skinFreq(kSkinFreq)
    , m_ringRadiusScale(kRingRadiusScale)
    , m_phaseStep(kPhaseStep)
{
}

bool LGPEvanescentSkinEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_lambda = kLambda;
    m_skinFreq = kSkinFreq;
    m_ringRadiusScale = kRingRadiusScale;
    m_phaseStep = kPhaseStep;
    return true;
}

void LGPEvanescentSkinEffect::render(plugins::EffectContext& ctx) {
    // Thin shimmering layers hugging rims or edges
    m_time = (uint16_t)(m_time + (uint16_t)(ctx.speed * m_phaseStep));

    float intensityNorm = ctx.brightness / 255.0f;
    (void)intensityNorm;
    const bool rimMode = true;
    const float lambda = m_lambda;
    const float skinFreq = m_skinFreq;
    float anim = m_time / 256.0f;

    float ringRadius = HALF_LENGTH * m_ringRadiusScale;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float brightnessF;
        float hue = (float)ctx.gHue + (i >> 1);

        if (rimMode) {
            float distFromCenter = (float)centerPairDistance(i);
            float skinDistance = fabsf(distFromCenter - ringRadius);
            float envelope = 1.0f / (1.0f + lambda * skinDistance);
            float carrier = sinf(distFromCenter * skinFreq * 0.05f + anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        } else {
            uint16_t edgeDistance = std::min((uint16_t)i, (uint16_t)(STRIP_LENGTH - 1 - i));
            float distToEdge = (float)edgeDistance;
            float envelope = 1.0f / (1.0f + lambda * distToEdge * 0.4f);
            float carrier = sinf(((float)STRIP_LENGTH - distToEdge) * skinFreq * 0.04f - anim * TWO_PI);
            brightnessF = envelope * (carrier * 0.5f + 0.5f) * 255.0f;
        }

        brightnessF = constrain(brightnessF, 0.0f, 255.0f);
        uint8_t brightness = (uint8_t)brightnessF;

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor((uint8_t)hue, brightU8);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)hue + 128, brightU8);
        }
    }
}

void LGPEvanescentSkinEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPEvanescentSkinEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Evanescent Skin",
        "Surface wave propagation",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

uint8_t LGPEvanescentSkinEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPEvanescentSkinEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPEvanescentSkinEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lambda") == 0) {
        m_lambda = constrain(value, 1.0f, 10.0f);
        return true;
    }
    if (strcmp(name, "skin_freq") == 0) {
        m_skinFreq = constrain(value, 2.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "ring_radius_scale") == 0) {
        m_ringRadiusScale = constrain(value, 0.2f, 1.0f);
        return true;
    }
    if (strcmp(name, "phase_step") == 0) {
        m_phaseStep = constrain(value, 0.1f, 1.2f);
        return true;
    }
    return false;
}

float LGPEvanescentSkinEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lambda") == 0) return m_lambda;
    if (strcmp(name, "skin_freq") == 0) return m_skinFreq;
    if (strcmp(name, "ring_radius_scale") == 0) return m_ringRadiusScale;
    if (strcmp(name, "phase_step") == 0) return m_phaseStep;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
