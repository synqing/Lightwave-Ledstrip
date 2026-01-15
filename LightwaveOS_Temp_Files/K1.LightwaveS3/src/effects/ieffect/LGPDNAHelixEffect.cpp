/**
 * @file LGPDNAHelixEffect.cpp
 * @brief LGP DNA Helix effect implementation
 */

#include "LGPDNAHelixEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPDNAHelixEffect::LGPDNAHelixEffect()
    : m_rotation(0.0f)
{
}

bool LGPDNAHelixEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_rotation = 0.0f;
    return true;
}

void LGPDNAHelixEffect::render(plugins::EffectContext& ctx) {
    // Double helix with color base pairing
    float speed = ctx.speed / 255.0f;
    float intensity = ctx.brightness / 255.0f;

    m_rotation += speed * 0.05f;

    const float helixPitch = 20.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        float angle1 = (distFromCenter / helixPitch) * TWO_PI + m_rotation;
        float angle2 = angle1 + PI;

        uint8_t paletteOffset1;
        uint8_t paletteOffset2;
        if (sinf(angle1 * 2.0f) > 0.0f) {
            paletteOffset1 = 0;
            paletteOffset2 = 15;
        } else {
            paletteOffset1 = 10;
            paletteOffset2 = 25;
        }

        float strand1Intensity = (sinf(angle1) + 1.0f) * 0.5f;
        float strand2Intensity = (sinf(angle2) + 1.0f) * 0.5f;

        float connectionIntensity = 0.0f;
        if (fmodf(distFromCenter, helixPitch / 4.0f) < 2.0f) {
            connectionIntensity = 1.0f;
        }

        uint8_t brightness = (uint8_t)(255.0f * intensity);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset1),
                                           (uint8_t)(brightness * strand1Intensity));
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset2),
                                                             (uint8_t)(brightness * strand2Intensity));
        }

        if (connectionIntensity > 0.0f) {
            CRGB conn2 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset2), brightness);
            CRGB conn1 = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteOffset1), brightness);
            ctx.leds[i] = blend(ctx.leds[i], conn2, 128);
            if (i + STRIP_LENGTH < ctx.ledCount) {
                ctx.leds[i + STRIP_LENGTH] = blend(ctx.leds[i + STRIP_LENGTH], conn1, 128);
            }
        }
    }
}

void LGPDNAHelixEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPDNAHelixEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP DNA Helix",
        "Double helix structure",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
