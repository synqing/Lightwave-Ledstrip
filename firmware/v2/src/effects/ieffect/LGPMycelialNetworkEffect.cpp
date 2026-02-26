/**
 * @file LGPMycelialNetworkEffect.cpp
 * @brief LGP Mycelial Network effect implementation
 */

#include "LGPMycelialNetworkEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kNutrientPhaseRate = 0.05f;
constexpr float kBranchProbability = 0.005f;
constexpr float kNutrientFreq = 10.0f;

const plugins::EffectParameter kParameters[] = {
    {"nutrient_phase_rate", "Nutrient Phase Rate", 0.01f, 0.20f, kNutrientPhaseRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"branch_probability", "Branch Probability", 0.001f, 0.03f, kBranchProbability, plugins::EffectParameterType::FLOAT, 0.001f, "wave", "", false},
    {"nutrient_freq", "Nutrient Frequency", 2.0f, 20.0f, kNutrientFreq, plugins::EffectParameterType::FLOAT, 0.2f, "wave", "", false},
};
}

LGPMycelialNetworkEffect::LGPMycelialNetworkEffect()
    : m_ps(nullptr)
    , m_nutrientPhase(0.0f)
    , m_initialized(false)
    , m_nutrientPhaseRate(kNutrientPhaseRate)
    , m_branchProbability(kBranchProbability)
    , m_nutrientFreq(kNutrientFreq)
{
}

bool LGPMycelialNetworkEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_nutrientPhase = 0.0f;
    m_initialized = false;
    m_nutrientPhaseRate = kNutrientPhaseRate;
    m_branchProbability = kBranchProbability;
    m_nutrientFreq = kNutrientFreq;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<MycelialPsram*>(
            heap_caps_malloc(sizeof(MycelialPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(MycelialPsram));
#endif
    return true;
}

void LGPMycelialNetworkEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    // Fungal hyphal growth with fractal branching and nutrient flow
    float speed = ctx.speed / 50.0f;
    float intensity = ctx.brightness / 255.0f;

    m_nutrientPhase += speed * m_nutrientPhaseRate;

    if (!m_initialized) {
        for (int t = 0; t < 16; t++) {
            m_ps->tipPositions[t] = (float)CENTER_LEFT;
            m_ps->tipVelocities[t] = 0.0f;
            m_ps->tipActive[t] = false;
            m_ps->tipAge[t] = 0.0f;
        }
        m_ps->tipActive[0] = true;
        m_ps->tipVelocities[0] = 0.5f;
        m_ps->tipActive[1] = true;
        m_ps->tipVelocities[1] = -0.5f;

        for (int i = 0; i < STRIP_LENGTH; i++) {
            m_ps->networkDensity[i] = 0.0f;
        }

        m_initialized = true;
    }

    // Get dt for frame-rate independent decay
    float dt = ctx.getSafeDeltaSeconds();

    const float branchProbability = m_branchProbability;
    const uint8_t numTips = 8;

    // Update tips
    for (int t = 0; t < 16; t++) {
        if (m_ps->tipActive[t]) {
            m_ps->tipPositions[t] += m_ps->tipVelocities[t] * speed;
            m_ps->tipAge[t] += speed * 0.01f;

            if (m_ps->tipPositions[t] < 0.0f || m_ps->tipPositions[t] >= STRIP_LENGTH) {
                m_ps->tipActive[t] = false;
            }

            if (random8() < (uint8_t)(branchProbability * 255.0f)) {
                for (int newTip = 0; newTip < numTips; newTip++) {
                    if (!m_ps->tipActive[newTip]) {
                        m_ps->tipActive[newTip] = true;
                        m_ps->tipPositions[newTip] = m_ps->tipPositions[t];
                        m_ps->tipVelocities[newTip] = -m_ps->tipVelocities[t] * (0.5f + random8() / 255.0f * 0.5f);
                        m_ps->tipAge[newTip] = 0.0f;
                        break;
                    }
                }
            }
        } else if (random8() < 5) {
            m_ps->tipActive[t] = true;
            m_ps->tipPositions[t] = (float)CENTER_LEFT;
            m_ps->tipVelocities[t] = (random8() > 127 ? 1.0f : -1.0f) * (0.3f + random8() / 255.0f * 0.4f);
            m_ps->tipAge[t] = 0.0f;
        }
    }

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        m_ps->networkDensity[i] *= powf(0.998f, dt * 60.0f);  // dt-corrected decay

        float tipGlow = 0.0f;
        for (int t = 0; t < 16; t++) {
            if (m_ps->tipActive[t]) {
                float distToTip = fabsf((float)i - m_ps->tipPositions[t]);
                if (distToTip < 5.0f) {
                    tipGlow += (5.0f - distToTip) / 5.0f * intensity;
                    m_ps->networkDensity[i] = fminf(1.0f, m_ps->networkDensity[i] + 0.02f);
                }
            }
        }

        const float flowDirection = 0.5f;
        float nutrientWave = sinf(normalizedDist * m_nutrientFreq - m_nutrientPhase * flowDirection * 3.0f);
        float nutrientBrightness = m_ps->networkDensity[i] * (0.5f + nutrientWave * 0.5f);

        uint8_t hue1 = (uint8_t)(140 + (uint8_t)(ctx.gHue * 0.3f));
        uint8_t hue2 = (uint8_t)(160 + (uint8_t)(ctx.gHue * 0.3f));

        float brightness1 = tipGlow * 200.0f + m_ps->networkDensity[i] * 80.0f + nutrientBrightness * 60.0f;
        float brightness2 = tipGlow * 150.0f + m_ps->networkDensity[i] * 90.0f + nutrientBrightness * 70.0f;

        if (brightness1 > 100.0f && brightness2 > 100.0f) {
            hue1 = (uint8_t)(40 + (uint8_t)(ctx.gHue * 0.2f));
            hue2 = (uint8_t)(50 + (uint8_t)(ctx.gHue * 0.2f));
            brightness1 = fminf(255.0f, brightness1 * 1.3f);
            brightness2 = fminf(255.0f, brightness2 * 1.3f);
        }

        uint8_t brightU8_1 = (uint8_t)constrain(brightness1, 0.0f, 255.0f);
        brightU8_1 = (uint8_t)((brightU8_1 * ctx.brightness) / 255);
        ctx.leds[i] = ctx.palette.getColor(hue1, brightU8_1);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t brightU8_2 = (uint8_t)constrain(brightness2, 0.0f, 255.0f);
            brightU8_2 = (uint8_t)((brightU8_2 * ctx.brightness) / 255);
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(hue2, brightU8_2);
        }
    }
}

void LGPMycelialNetworkEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPMycelialNetworkEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Mycelial Network",
        "Fungal network expansion",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPMycelialNetworkEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPMycelialNetworkEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPMycelialNetworkEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "nutrient_phase_rate") == 0) {
        m_nutrientPhaseRate = constrain(value, 0.01f, 0.20f);
        return true;
    }
    if (strcmp(name, "branch_probability") == 0) {
        m_branchProbability = constrain(value, 0.001f, 0.03f);
        return true;
    }
    if (strcmp(name, "nutrient_freq") == 0) {
        m_nutrientFreq = constrain(value, 2.0f, 20.0f);
        return true;
    }
    return false;
}

float LGPMycelialNetworkEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "nutrient_phase_rate") == 0) return m_nutrientPhaseRate;
    if (strcmp(name, "branch_probability") == 0) return m_branchProbability;
    if (strcmp(name, "nutrient_freq") == 0) return m_nutrientFreq;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
