/**
 * @file LGPColorTemperatureEffect.cpp
 * @brief LGP Color Temperature effect implementation
 */

#include "LGPColorTemperatureEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr float kTempBias = 1.0f;
constexpr float kEdgeFalloff = 1.0f;
constexpr float kCoolBoost = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"temp_bias", "Temp Bias", 0.5f, 1.5f, kTempBias, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "x", false},
    {"edge_falloff", "Edge Falloff", 0.5f, 1.5f, kEdgeFalloff, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"cool_boost", "Cool Boost", 0.5f, 1.5f, kCoolBoost, plugins::EffectParameterType::FLOAT, 0.05f, "colour", "x", false},
};
}

LGPColorTemperatureEffect::LGPColorTemperatureEffect()
    : m_tempBias(kTempBias)
    , m_edgeFalloff(kEdgeFalloff)
    , m_coolBoost(kCoolBoost)
{
}

bool LGPColorTemperatureEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_tempBias = kTempBias;
    m_edgeFalloff = kEdgeFalloff;
    m_coolBoost = kCoolBoost;
    return true;
}

void LGPColorTemperatureEffect::render(plugins::EffectContext& ctx) {
    // Warm colors from edges meet cool colors at center, creating white
    float intensity = ctx.brightness / 255.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        CRGB warm, cool;

        // Warm side (reds/oranges)
        warm.r = 255;
        warm.g = (uint8_t)(180.0f - normalizedDist * 100.0f * m_tempBias);
        warm.b = (uint8_t)(50.0f + normalizedDist * 50.0f);

        // Cool side (blues/cyans)
        cool.r = (uint8_t)(150.0f + normalizedDist * 50.0f * m_tempBias);
        cool.g = (uint8_t)(200.0f + normalizedDist * 55.0f * m_coolBoost);
        cool.b = 255;

        uint8_t edgeScale = (uint8_t)(255.0f * (1.0f - normalizedDist * 0.25f * m_edgeFalloff));
        ctx.leds[i] = warm.scale8((uint8_t)(intensity * edgeScale));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = cool.scale8((uint8_t)(intensity * edgeScale));
        }
    }
}

void LGPColorTemperatureEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPColorTemperatureEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Color Temperature",
        "Blackbody radiation gradients",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPColorTemperatureEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPColorTemperatureEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPColorTemperatureEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "temp_bias") == 0) {
        m_tempBias = constrain(value, 0.5f, 1.5f);
        return true;
    }
    if (strcmp(name, "edge_falloff") == 0) {
        m_edgeFalloff = constrain(value, 0.5f, 1.5f);
        return true;
    }
    if (strcmp(name, "cool_boost") == 0) {
        m_coolBoost = constrain(value, 0.5f, 1.5f);
        return true;
    }
    return false;
}

float LGPColorTemperatureEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "temp_bias") == 0) return m_tempBias;
    if (strcmp(name, "edge_falloff") == 0) return m_edgeFalloff;
    if (strcmp(name, "cool_boost") == 0) return m_coolBoost;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
