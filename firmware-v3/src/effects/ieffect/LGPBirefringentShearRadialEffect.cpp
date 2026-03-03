/**
 * @file LGPBirefringentShearRadialEffect.cpp
 * @brief LGP Birefringent Shear (Radial) - Concentric polarization splitting
 *
 * Radial variant of LGPBirefringentShearEffect.
 * Two concentric wave modes expand from center, creating standing wave
 * interference at radial distances from the center pair.
 */

#include "LGPBirefringentShearRadialEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPBirefringentShearRadialEffect::LGPBirefringentShearRadialEffect()
    : m_time(0)
{
}

bool LGPBirefringentShearRadialEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPBirefringentShearRadialEffect::render(plugins::EffectContext& ctx) {
    // Dual concentric modes slipping past one another — radial standing wave
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    float intensityNorm = ctx.brightness / 255.0f;
    const float baseFrequency = 3.5f;
    const float deltaK = 1.5f;
    const float drift = 0.3f;

    uint8_t mixWave = (uint8_t)(intensityNorm * 255.0f);
    uint8_t mixCarrier = (uint8_t)(255 - mixWave);

    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float r = (float)dist;

        float phaseBase = m_time / 128.0f;
        float phase1 = r * (baseFrequency + deltaK) + phaseBase;
        float phase2 = r * (baseFrequency - deltaK) - phaseBase + drift * r * 0.05f;

        uint8_t wave1 = sin8((int16_t)(phase1 * 16.0f));
        uint8_t wave2 = sin8((int16_t)(phase2 * 16.0f));

        uint8_t combined = qadd8(scale8(wave1, mixCarrier), scale8(wave2, mixWave));
        uint8_t beat = (uint8_t)abs((int)wave1 - (int)wave2);
        uint8_t brightness = qadd8(combined, scale8(beat, 96));

        uint8_t hue1 = (uint8_t)(ctx.gHue + (uint8_t)(dist) + (m_time >> 4));
        uint8_t hue2 = (uint8_t)(hue1 + 128);

        uint8_t brightU8 = (uint8_t)((brightness * ctx.brightness) / 255);
        CRGB color1 = ctx.palette.getColor(hue1, brightU8);
        CRGB color2 = ctx.palette.getColor(hue2, brightU8);

        // Strip 1: symmetric from center
        uint16_t left1 = CENTER_LEFT - dist;
        uint16_t right1 = CENTER_RIGHT + dist;
        if (left1 < STRIP_LENGTH) ctx.leds[left1] = color1;
        if (right1 < STRIP_LENGTH) ctx.leds[right1] = color1;

        // Strip 2: complementary hue at same radial distance
        uint16_t left2 = STRIP_LENGTH + CENTER_LEFT - dist;
        uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
        if (left2 < ctx.ledCount) ctx.leds[left2] = color2;
        if (right2 < ctx.ledCount) ctx.leds[right2] = color2;
    }
}

void LGPBirefringentShearRadialEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBirefringentShearRadialEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Birefringent Shear R",
        "Radial polarization split",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
