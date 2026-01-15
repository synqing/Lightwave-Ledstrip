/**
 * @file LGPPhotonicCrystalAmbientEffect.cpp
 * @brief LGP Photonic Crystal (Ambient) - Time-driven bandgap structure
 * 
 * Restored from commit cd67598~5 - original clean implementation.
 */

#include "LGPPhotonicCrystalAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPhotonicCrystalAmbientEffect::LGPPhotonicCrystalAmbientEffect()
    : m_phase(0)
{
}

bool LGPPhotonicCrystalAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0;
    return true;
}

void LGPPhotonicCrystalAmbientEffect::render(plugins::EffectContext& ctx) {
    using namespace lightwaveos::effects;
    
    // CENTER ORIGIN - Periodic refractive index modulation
    m_phase = (uint16_t)(m_phase + ctx.speed);

    const uint8_t latticeSize = 8;

    for (uint16_t i = 0; i < STRIP_LENGTH && i < ctx.ledCount; i++) {
        // CENTER ORIGIN: Calculate distance from center
        uint16_t distFromCenter = centerPairDistance(i);

        // Periodic structure
        uint8_t cellPosition = (uint8_t)(distFromCenter % latticeSize);
        bool inBandgap = cellPosition < (latticeSize >> 1);

        // Photonic band structure - CENTER ORIGIN PUSH
        uint8_t brightness = 0;
        if (inBandgap) {
            // Allowed modes - push outward from center
            brightness = sin8((distFromCenter << 2) - (m_phase >> 7));
        } else {
            // Forbidden gap - evanescent decay
            uint8_t decay = (uint8_t)(255 - (cellPosition * 50));
            brightness = scale8(sin8((distFromCenter << 1) - (m_phase >> 8)), decay);
        }

        brightness = scale8(brightness, ctx.brightness);

        uint8_t hue = inBandgap ? ctx.gHue : (uint8_t)(ctx.gHue + 128);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(hue + distFromCenter / 4), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(hue + distFromCenter / 4 + 64), brightness);
        }
    }
}

void LGPPhotonicCrystalAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPhotonicCrystalAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Photonic Crystal (Ambient)",
        "Time-driven bandgap structure simulation",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
