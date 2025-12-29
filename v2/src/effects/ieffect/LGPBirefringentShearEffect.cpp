/**
 * @file LGPBirefringentShearEffect.cpp
 * @brief LGP Birefringent Shear effect implementation
 */

#include "LGPBirefringentShearEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPBirefringentShearEffect::LGPBirefringentShearEffect()
    : m_time(0)
{
}

bool LGPBirefringentShearEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_time = 0;
    return true;
}

void LGPBirefringentShearEffect::render(plugins::EffectContext& ctx) {
    // Dual spatial modes slipping past one another
    m_time = (uint16_t)(m_time + (ctx.speed >> 1));

    float intensityNorm = ctx.brightness / 255.0f;
    const float baseFrequency = 3.5f;
    const float deltaK = 1.5f;
    const float drift = 0.3f;

    uint8_t mixWave = (uint8_t)(intensityNorm * 255.0f);
    uint8_t mixCarrier = (uint8_t)(255 - mixWave);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        float idx = (float)i;

        float phaseBase = m_time / 128.0f;
        float phase1 = idx * (baseFrequency + deltaK) + phaseBase;
        float phase2 = idx * (baseFrequency - deltaK) - phaseBase + drift * idx * 0.05f;

        uint8_t wave1 = sin8((int16_t)(phase1 * 16.0f));
        uint8_t wave2 = sin8((int16_t)(phase2 * 16.0f));

        uint8_t combined = qadd8(scale8(wave1, mixCarrier), scale8(wave2, mixWave));
        uint8_t beat = (uint8_t)abs((int)wave1 - (int)wave2);
        uint8_t brightness = qadd8(combined, scale8(beat, 96));

        uint8_t hue1 = (uint8_t)(ctx.gHue + (uint8_t)(idx) + (m_time >> 4));
        uint8_t hue2 = (uint8_t)(hue1 + 128);

        ctx.leds[i] = CHSV(hue1, ctx.saturation, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = CHSV(hue2, ctx.saturation, brightness);
        }
    }
}

void LGPBirefringentShearEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPBirefringentShearEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Birefringent Shear",
        "Polarization splitting",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
