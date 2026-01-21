/**
 * @file LGPChromaticShearEffect.cpp
 * @brief LGP Chromatic Shear effect implementation
 */

#include "LGPChromaticShearEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPChromaticShearEffect::LGPChromaticShearEffect()
    : m_phase(0)
    , m_paletteOffset(0)
    , m_lastUpdateFrame(0)
{
}

bool LGPChromaticShearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0;
    m_paletteOffset = 0;
    m_lastUpdateFrame = 0;
    return true;
}

void LGPChromaticShearEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Color planes sliding with velocity shear
    m_phase = (uint16_t)(m_phase + ctx.speed);
    float complexityNorm = ctx.complexity / 255.0f;
    float variationNorm = ctx.variation / 255.0f;

    // Palette rotation
    if (ctx.frameNumber - m_lastUpdateFrame > 5) {
        uint8_t step = (uint8_t)(2 + (ctx.variation >> 7));
        m_paletteOffset = (uint8_t)(m_paletteOffset + step);
        m_lastUpdateFrame = ctx.frameNumber;
    }

    const uint8_t shearAmount = (uint8_t)(96 + complexityNorm * 96.0f);
    uint8_t hueSpread = (uint8_t)(96 + variationNorm * 80.0f);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance(i);
        uint8_t distPos = (uint8_t)((distFromCenter * 255.0f) / (float)HALF_LENGTH);

        // Apply shear transformation
        uint16_t shearOffset = ((uint16_t)distPos * shearAmount) >> 3;
        (void)shearOffset;

        // Left strip: base hue + shear
        uint8_t leftHue = (uint8_t)(m_paletteOffset + distPos + (m_phase >> 8));
        uint8_t leftBright = ctx.brightness;

        // Right strip: complementary hue + inverse shear
        uint8_t rightHue = (uint8_t)(m_paletteOffset + distPos + hueSpread - (m_phase >> 8));
        uint8_t rightBright = ctx.brightness;

        // Add interference at center
        if (centerPairDistance(i) < 20) {
            uint8_t centerBlend = (uint8_t)(255 - centerPairDistance(i) * 12);
            leftBright = scale8(leftBright, (uint8_t)(255 - (centerBlend >> 1)));
            rightBright = scale8(rightBright, (uint8_t)(255 - (centerBlend >> 1)));
        }

        ctx.leds[i] = ctx.palette.getColor(leftHue, leftBright);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(rightHue, rightBright);
        }
    }
}

void LGPChromaticShearEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPChromaticShearEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Chromatic Shear",
        "Color-splitting shear effect",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
