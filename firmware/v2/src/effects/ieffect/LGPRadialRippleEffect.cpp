/**
 * @file LGPRadialRippleEffect.cpp
 * @brief LGP Radial Ripple effect implementation
 */

#include "LGPRadialRippleEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kRingCount = 6;
constexpr float kRingSpeedScale = 4.0f;
constexpr float kDistScale = 1.0f;

const plugins::EffectParameter kParameters[] = {
    {"ring_count", "Ring Count", 3.0f, 12.0f, (float)kRingCount, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"ring_speed_scale", "Ring Speed", 1.0f, 8.0f, kRingSpeedScale, plugins::EffectParameterType::FLOAT, 0.1f, "timing", "x", false},
    {"dist_scale", "Distance Scale", 0.5f, 2.0f, kDistScale, plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
}

LGPRadialRippleEffect::LGPRadialRippleEffect()
    : m_time(0)
    , m_ringCount(kRingCount)
    , m_ringSpeedScale(kRingSpeedScale)
    , m_distScale(kDistScale)
{
}

bool LGPRadialRippleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    m_ringCount = kRingCount;
    m_ringSpeedScale = kRingSpeedScale;
    m_distScale = kDistScale;
    return true;
}

void LGPRadialRippleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Concentric rings that expand from center
    uint16_t ringSpeed = (uint16_t)(ctx.speed * m_ringSpeedScale);
    m_time = (uint16_t)(m_time + ringSpeed);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i) / (float)HALF_LENGTH;

        // Square the distance for circular appearance
        uint16_t distSquared = (uint16_t)(distFromCenter * distFromCenter * m_distScale * 65535.0f);

        // Create expanding rings
        int16_t wave = sin16((distSquared >> 1) * m_ringCount - m_time);

        // Convert to brightness
        uint8_t brightness = (uint8_t)((wave + 32768) >> 8);
        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (distSquared >> 10));
        ctx.leds[i] = ctx.palette.getColor(hue, brightness);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 64), brightness);
        }
    }
}

void LGPRadialRippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPRadialRippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Radial Ripple",
        "Complex radial wave interference",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPRadialRippleEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPRadialRippleEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPRadialRippleEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "ring_count") == 0) {
        int clamped = (int)constrain(value, 3.0f, 12.0f);
        m_ringCount = clamped;
        return true;
    }
    if (strcmp(name, "ring_speed_scale") == 0) {
        m_ringSpeedScale = constrain(value, 1.0f, 8.0f);
        return true;
    }
    if (strcmp(name, "dist_scale") == 0) {
        m_distScale = constrain(value, 0.5f, 2.0f);
        return true;
    }
    return false;
}

float LGPRadialRippleEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "ring_count") == 0) return (float)m_ringCount;
    if (strcmp(name, "ring_speed_scale") == 0) return m_ringSpeedScale;
    if (strcmp(name, "dist_scale") == 0) return m_distScale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
