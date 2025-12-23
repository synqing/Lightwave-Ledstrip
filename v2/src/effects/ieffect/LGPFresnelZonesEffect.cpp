/**
 * @file LGPFresnelZonesEffect.cpp
 * @brief LGP Fresnel Zones effect implementation
 */

#include "LGPFresnelZonesEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPFresnelZonesEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void LGPFresnelZonesEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Optical zone plates creating focusing effects
    const uint8_t zoneCount = 6;

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Distance from center
        uint16_t dist = centerPairDistance(i);

        // Fresnel zone calculation
        uint16_t zoneRadius = (uint16_t)(sqrt16(dist << 8) * zoneCount);

        // Binary zone plate
        bool inZone = (zoneRadius & 0x100) != 0;
        uint8_t brightness = inZone ? 255 : 0;

        // Smooth edges based on intensity
        if (ctx.brightness < 200) {
            uint8_t edge = (uint8_t)(zoneRadius & 0xFF);
            brightness = inZone ? edge : (uint8_t)(255 - edge);
        }

        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = (uint8_t)(ctx.gHue + (dist >> 2));

        ctx.leds[i] = ctx.palette.getColor(hue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + 30), scale8(brightness, 200));
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

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
