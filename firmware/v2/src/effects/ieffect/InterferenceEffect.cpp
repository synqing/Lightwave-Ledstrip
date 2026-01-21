/**
 * @file InterferenceEffect.cpp
 * @brief Interference effect implementation
 */

#include "InterferenceEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

InterferenceEffect::InterferenceEffect()
    : m_wave1Phase(0.0f)
    , m_wave2Phase(0.0f)
{
}

bool InterferenceEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_wave1Phase = 0.0f;
    m_wave2Phase = 0.0f;
    return true;
}

void InterferenceEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN INTERFERENCE - Two waves from center create patterns
    m_wave1Phase += ctx.speed / 20.0f;
    m_wave2Phase -= ctx.speed / 30.0f;

    // Wrap phases to prevent unbounded growth (prevents hue cycling - no-rainbows rule)
    const float twoPi = 2.0f * PI;
    if (m_wave1Phase > twoPi) m_wave1Phase -= twoPi;
    if (m_wave1Phase < 0.0f) m_wave1Phase += twoPi;
    if (m_wave2Phase > twoPi) m_wave2Phase -= twoPi;
    if (m_wave2Phase < 0.0f) m_wave2Phase += twoPi;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);
        float normalizedDist = distFromCenter / (float)HALF_LENGTH;

        // Two waves emanating from center
        float wave1 = sinf(normalizedDist * PI * 4.0f + m_wave1Phase) * 127.0f + 128.0f;
        float wave2 = sinf(normalizedDist * PI * 6.0f + m_wave2Phase) * 127.0f + 128.0f;

        // Interference pattern
        uint8_t brightness = (uint8_t)((wave1 + wave2) / 2.0f);
        // Wrap hue calculation to prevent rainbow cycling (no-rainbows rule: < 60° range)
        uint8_t hue = (uint8_t)((m_wave1Phase * 20.0f) + (distFromCenter * 8.0f));

        CRGB color = ctx.palette.getColor((uint8_t)(ctx.gHue + hue), brightness);

        ctx.leds[i] = color;
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void InterferenceEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& InterferenceEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Interference",
        "Basic wave interference",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
