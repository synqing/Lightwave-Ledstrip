/**
 * @file LGPRGBPrismEffect.cpp
 * @brief LGP RGB Prism effect implementation
 */

#include "LGPRGBPrismEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kPhaseRate = 0.02f;
constexpr float kDispersion = 1.5f;
constexpr float kCentreBoost = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"phase_rate", "Phase Rate", 0.005f, 0.08f, kPhaseRate, plugins::EffectParameterType::FLOAT, 0.005f, "timing", "x", false},
    {"dispersion", "Dispersion", 0.8f, 2.5f, kDispersion, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
    {"centre_boost", "Centre Boost", 0.2f, 2.0f, kCentreBoost, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
};
}

LGPRGBPrismEffect::LGPRGBPrismEffect()
    : m_prismAngle(0.0f)
    , m_phaseRate(kPhaseRate)
    , m_dispersion(kDispersion)
    , m_centreBoost(kCentreBoost)
{
}

bool LGPRGBPrismEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_prismAngle = 0.0f;
    m_phaseRate = kPhaseRate;
    m_dispersion = kDispersion;
    m_centreBoost = kCentreBoost;
    return true;
}

void LGPRGBPrismEffect::render(plugins::EffectContext& ctx) {
    // Simulates light passing through a prism
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;
    m_prismAngle += speed * m_phaseRate;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        float redAngle = sinf(normalizedDist * m_dispersion + m_prismAngle);
        float greenAngle = sinf(normalizedDist * m_dispersion * 1.1f + m_prismAngle);
        float blueAngle = sinf(normalizedDist * m_dispersion * 1.2f + m_prismAngle);

        // Strip 1: Red channel dominant
        ctx.leds[i].r = (uint8_t)((128.0f + 127.0f * redAngle) * intensity);
        ctx.leds[i].g = (uint8_t)(64.0f * fabsf(greenAngle) * intensity);
        ctx.leds[i].b = 0;

        // Strip 2: Blue channel dominant
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH].r = 0;
            ctx.leds[i + STRIP_LENGTH].g = (uint8_t)(64.0f * fabsf(greenAngle) * intensity);
            ctx.leds[i + STRIP_LENGTH].b = (uint8_t)((128.0f + 127.0f * blueAngle) * intensity);
        }

        // Green emerges at center
        if (distFromCenter < 10.0f) {
            ctx.leds[i].g += (uint8_t)(128.0f * intensity * m_centreBoost);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH].g += (uint8_t)(128.0f * intensity * m_centreBoost);
            }
        }
    }
}

void LGPRGBPrismEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRGBPrismEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP RGB Prism",
        "RGB component splitting",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPRGBPrismEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPRGBPrismEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPRGBPrismEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "phase_rate") == 0) {
        m_phaseRate = constrain(value, 0.005f, 0.08f);
        return true;
    }
    if (strcmp(name, "dispersion") == 0) {
        m_dispersion = constrain(value, 0.8f, 2.5f);
        return true;
    }
    if (strcmp(name, "centre_boost") == 0) {
        m_centreBoost = constrain(value, 0.2f, 2.0f);
        return true;
    }
    return false;
}

float LGPRGBPrismEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "phase_rate") == 0) return m_phaseRate;
    if (strcmp(name, "dispersion") == 0) return m_dispersion;
    if (strcmp(name, "centre_boost") == 0) return m_centreBoost;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
