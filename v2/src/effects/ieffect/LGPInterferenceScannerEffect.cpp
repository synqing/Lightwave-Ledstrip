/**
 * @file LGPInterferenceScannerEffect.cpp
 * @brief LGP Interference Scanner effect implementation
 */

#include "LGPInterferenceScannerEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPInterferenceScannerEffect::LGPInterferenceScannerEffect()
    : m_scanPhase(0.0f)
    , m_scanPhase2(0.0f)
{
}

bool LGPInterferenceScannerEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_scanPhase = 0.0f;
    m_scanPhase2 = 0.0f;
    return true;
}

void LGPInterferenceScannerEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE SCANNER - Creates scanning interference patterns
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_scanPhase += speedNorm * 0.05f;
    m_scanPhase2 += speedNorm * 0.03f;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        // Radial scan from center
        float ringRadius = fmodf(m_scanPhase * 30.0f, (float)HALF_LENGTH);
        const float ringWidth = 15.0f;

        float pattern = 0.0f;
        if (fabsf(dist - ringRadius) < ringWidth) {
            pattern = cosf((dist - ringRadius) / ringWidth * PI / 2.0f);
        }

        // Add dual sweep interference
        float wave1 = sinf(dist * 0.1f + m_scanPhase);
        float wave2 = sinf(dist * 0.1f - m_scanPhase2);
        pattern += (wave1 + wave2) / 4.0f;

        // Clamp
        pattern = fmaxf(-1.0f, fminf(1.0f, pattern));

        uint8_t brightness = (uint8_t)((pattern * 0.5f + 0.5f) * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(dist * 2.0f + pattern * 50.0f);

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(ctx.gHue + paletteIndex + 128),
                                                             (uint8_t)(255 - brightness));
        }
    }
}

void LGPInterferenceScannerEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPInterferenceScannerEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Interference Scanner",
        "Scanning beam with interference fringes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
