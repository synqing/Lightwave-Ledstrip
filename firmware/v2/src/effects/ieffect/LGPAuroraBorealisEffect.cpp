/**
 * @file LGPAuroraBorealisEffect.cpp
 * @brief LGP Aurora Borealis effect implementation
 */

#include "LGPAuroraBorealisEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.0625f;
constexpr float kCurtainDrift = 1.0f;
constexpr float kShimmerAmount = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.02f, 0.20f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"curtain_drift", "Curtain Drift", 0.5f, 2.0f, kCurtainDrift, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"shimmer_amount", "Shimmer Amount", 0.0f, 1.0f, kShimmerAmount, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "", false},
};
}

LGPAuroraBorealisEffect::LGPAuroraBorealisEffect()
    : m_time(0)
    , m_curtainPhase{0, 51, 102, 153, 204}
    , m_phaseRate(kPhaseRate)
    , m_curtainDrift(kCurtainDrift)
    , m_shimmerAmount(kShimmerAmount)
{
}

bool LGPAuroraBorealisEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    const uint8_t initPhases[5] = {0, 51, 102, 153, 204};
    memcpy(m_curtainPhase, initPhases, sizeof(m_curtainPhase));
    m_phaseRate = kPhaseRate;
    m_curtainDrift = kCurtainDrift;
    m_shimmerAmount = kShimmerAmount;
    return true;
}

void LGPAuroraBorealisEffect::render(plugins::EffectContext& ctx) {
    // Northern lights simulation with waveguide color mixing
    const uint16_t phaseStep = (uint16_t)fmaxf(1.0f, ctx.speed * m_phaseRate);
    m_time = (uint16_t)(m_time + phaseStep);

    const uint8_t curtainCount = 4;

    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint8_t c = 0; c < curtainCount; c++) {
        const uint8_t driftStep = (uint8_t)fmaxf(1.0f, (c + 1) * m_curtainDrift);
        m_curtainPhase[c] = (uint8_t)(m_curtainPhase[c] + driftStep);
        uint16_t curtainCenter = beatsin16(1, 20, STRIP_LENGTH - 20, 0, (uint16_t)(m_curtainPhase[c] << 8));
        uint8_t curtainWidth = beatsin8(1, 20, 35, 0, m_curtainPhase[c]);

        // Aurora colors - greens, blues, purples
        uint8_t hue = (uint8_t)(96 + (c * 32));

        for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
            int16_t dist = abs((int16_t)i - (int16_t)curtainCenter);
            if (dist < curtainWidth) {
                uint8_t brightness = qsub8(255, (uint8_t)((dist * 255) / curtainWidth));
                brightness = scale8(brightness, ctx.brightness);

                // Shimmer effect
                uint8_t shimmerMod = (inoise8(i * 5, m_time >> 3) >> 3);
                uint8_t shimmerScale = (uint8_t)(220 + (shimmerMod * m_shimmerAmount));
                brightness = scale8(brightness, shimmerScale);

                CRGB color1 = ctx.palette.getColor(hue, brightness);
                CRGB color2 = ctx.palette.getColor((uint8_t)(hue + 20), brightness);

                ctx.leds[i] = color1;
                if (i + STRIP_LENGTH < ctx.ledCount) {
                    ctx.leds[i + STRIP_LENGTH] = color2;
                }
            }
        }
    }

    // Add corona at edges
    for (uint8_t i = 0; i < 20; i++) {
        uint8_t corona = scale8((uint8_t)(255 - i * 12), ctx.brightness >> 1);
        // Corona color for strip 1: (0, corona >> 2, corona >> 1)
        CRGB coronaColor1 = CRGB(0, corona >> 2, corona >> 1);
        ctx.leds[i] = coronaColor1;
        ctx.leds[STRIP_LENGTH - 1 - i] = coronaColor1;
        if (STRIP_LENGTH < ctx.ledCount) {
            // Corona color for strip 2: (0, corona >> 3, corona)
            CRGB coronaColor2 = CRGB(0, corona >> 3, corona);
            ctx.leds[STRIP_LENGTH + i] = coronaColor2;
            ctx.leds[ctx.ledCount - 1 - i] = coronaColor2;
        }
    }
}

void LGPAuroraBorealisEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPAuroraBorealisEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Aurora Borealis",
        "Shimmering curtain lights",
        plugins::EffectCategory::NATURE,
        1
    };
    return meta;
}

uint8_t LGPAuroraBorealisEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPAuroraBorealisEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPAuroraBorealisEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.02f, 0.20f);
        return true;
    }
    if (strcmp(name, "curtain_drift") == 0) {
        m_curtainDrift = constrain(value, 0.5f, 2.0f);
        return true;
    }
    if (strcmp(name, "shimmer_amount") == 0) {
        m_shimmerAmount = constrain(value, 0.0f, 1.0f);
        return true;
    }
    return false;
}

float LGPAuroraBorealisEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "curtain_drift") == 0) return m_curtainDrift;
    if (strcmp(name, "shimmer_amount") == 0) return m_shimmerAmount;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
