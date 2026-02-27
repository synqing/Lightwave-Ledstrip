/**
 * @file LGPConcentricRingsEffect.cpp
 * @brief LGP Concentric Rings effect implementation
 */

#include "LGPConcentricRingsEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPConcentricRingsEffect::LGPConcentricRingsEffect()
    : m_phase(0.0f)
{
}

bool LGPConcentricRingsEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    return true;
}

void LGPConcentricRingsEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Radial standing waves create ring patterns
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase += speedNorm * 0.1f * 60.0f * dt;  // dt-corrected

    const float ringCount = 10.0f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Bessel function-like (sin(k*dist + phase) = INWARD motion - intentional design)
        float bessel = sinf(distFromCenter * ringCount * 0.2f + m_phase);
        bessel *= 1.0f / sqrtf(normalizedDist + 0.1f);

        // Sharp ring edges
        float rings = tanhf(bessel * 2.0f);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * rings * intensityNorm);

        ctx.leds[i] = ctx.palette.getColor(ctx.gHue, brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + 128), brightness);
        }
    }
}

void LGPConcentricRingsEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPConcentricRingsEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Concentric Rings",
        "Expanding circular rings",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
