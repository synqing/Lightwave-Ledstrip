/**
 * @file LGPFresnelZonesEffect.cpp
 * @brief LGP Fresnel Zones effect implementation
 */

#include "LGPFresnelZonesEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {
constexpr int kZoneCount = 6;
constexpr int kEdgeThreshold = 200;
constexpr float kStrip2Scale = 200.0f / 255.0f;

const plugins::EffectParameter kParameters[] = {
    {"zone_count", "Zone Count", 2.0f, 12.0f, (float)kZoneCount, plugins::EffectParameterType::INT, 1.0f, "wave", "", false},
    {"edge_threshold", "Edge Threshold", 64.0f, 255.0f, (float)kEdgeThreshold, plugins::EffectParameterType::INT, 1.0f, "blend", "", true},
    {"strip2_scale", "Strip2 Scale", 0.2f, 1.0f, kStrip2Scale, plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", true},
};
}

LGPFresnelZonesEffect::LGPFresnelZonesEffect()
    : m_zoneCount(kZoneCount)
    , m_edgeThreshold(kEdgeThreshold)
    , m_strip2Scale(kStrip2Scale)
{
}

bool LGPFresnelZonesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_zoneCount = kZoneCount;
    m_edgeThreshold = kEdgeThreshold;
    m_strip2Scale = kStrip2Scale;
    return true;
}

void LGPFresnelZonesEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Optical zone plates creating focusing effects
    const uint8_t zoneCount = (uint8_t)m_zoneCount;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Distance from center
        uint16_t dist = centerPairDistance(i);

        // Fresnel zone calculation
        uint16_t zoneRadius = (uint16_t)(sqrt16(dist << 8) * zoneCount);

        // Binary zone plate
        bool inZone = (zoneRadius & 0x100) != 0;
        uint8_t brightness = inZone ? 255 : 0;

        // Smooth edges based on intensity
        if (ctx.brightness < m_edgeThreshold) {
            uint8_t edge = (uint8_t)(zoneRadius & 0xFF);
            brightness = inZone ? edge : (uint8_t)(255 - edge);
        }

        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (dist >> 2));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 30), scale8(brightness, (uint8_t)(m_strip2Scale * 255.0f)));
        }
    }
}

void LGPFresnelZonesEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPFresnelZonesEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Fresnel Zones",
        "Fresnel lens zone plate pattern",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

uint8_t LGPFresnelZonesEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kParameters) / sizeof(kParameters[0]));
}

const plugins::EffectParameter* LGPFresnelZonesEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kParameters[index];
}

bool LGPFresnelZonesEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "zone_count") == 0) {
        m_zoneCount = (int)constrain(value, 2.0f, 12.0f);
        return true;
    }
    if (strcmp(name, "edge_threshold") == 0) {
        m_edgeThreshold = (int)constrain(value, 64.0f, 255.0f);
        return true;
    }
    if (strcmp(name, "strip2_scale") == 0) {
        m_strip2Scale = constrain(value, 0.2f, 1.0f);
        return true;
    }
    return false;
}

float LGPFresnelZonesEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "zone_count") == 0) return (float)m_zoneCount;
    if (strcmp(name, "edge_threshold") == 0) return (float)m_edgeThreshold;
    if (strcmp(name, "strip2_scale") == 0) return m_strip2Scale;
    return 0.0f;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
