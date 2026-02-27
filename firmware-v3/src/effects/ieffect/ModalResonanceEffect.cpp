/**
 * @file ModalResonanceEffect.cpp
 * @brief LGP Modal Resonance implementation
 */

#include "ModalResonanceEffect.h"
#include "../CoreEffects.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

ModalResonanceEffect::ModalResonanceEffect()
    : m_modalModePhase(0.0f)
{
}

bool ModalResonanceEffect::init(plugins::EffectContext& ctx) {
    m_modalModePhase = 0.0f;
    return true;
}

void ModalResonanceEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN MODAL RESONANCE - Explores different optical cavity modes

    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_modalModePhase += speedNorm * 0.01f;

    // Mode number varies with time
    float baseMode = BASE_MODE_MIN + sinf(m_modalModePhase) * BASE_MODE_RANGE;

    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; i++) {
        float distFromCenter = ctx.getDistanceFromCenter(i) * (float)HALF_LENGTH;
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Mode pattern
        float modalPattern = sinf(normalizedDist * baseMode * TWO_PI);

        // Add harmonic
        modalPattern += sinf(normalizedDist * baseMode * 2.0f * TWO_PI) * HARMONIC_WEIGHT;
        modalPattern /= 1.5f;

        // Apply window function
        float window = sinf(normalizedDist * PI);
        modalPattern *= window;

        uint8_t brightness = (uint8_t)(128 + 127 * modalPattern * intensityNorm);

        uint8_t paletteIndex = (uint8_t)(baseMode * 10.0f) + (uint8_t)(normalizedDist * 50.0f);

        CRGB color = ctx.palette.getColor(ctx.gHue + paletteIndex, brightness);
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor(ctx.gHue + paletteIndex + 128, brightness);
        }
    }
}

void ModalResonanceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& ModalResonanceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Modal Resonance",
        "Explores different optical cavity resonance modes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

