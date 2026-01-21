/**
 * @file FireEffect.cpp
 * @brief Fire effect implementation
 */

#include "FireEffect.h"
#include "../CoreEffects.h"
#include "../../core/narrative/NarrativeEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

FireEffect::FireEffect()
{
    // Initialize heat array to zero
    memset(m_fireHeat, 0, sizeof(m_fireHeat));
}

bool FireEffect::init(plugins::EffectContext& ctx) {
    // Reset heat array
    memset(m_fireHeat, 0, sizeof(m_fireHeat));
    return true;
}

void FireEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN FIRE - Sparks ignite at center 79/80 and spread outward
    // Narrative integration: spark frequency modulated by tension
    using namespace lightwaveos::narrative;

    // Fade for trail persistence
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Cool down every cell
    for (int i = 0; i < STRIP_LENGTH; i++) {
        m_fireHeat[i] = qsub8(m_fireHeat[i], random8(0, ((55 * 10) / STRIP_LENGTH) + 2));
    }

    // Heat diffuses
    for (int k = 1; k < STRIP_LENGTH - 1; k++) {
        m_fireHeat[k] = (m_fireHeat[k - 1] + m_fireHeat[k] + m_fireHeat[k + 1]) / 3;
    }

    // Ignite new sparks at CENTER PAIR (79/80)
    // Apply narrative tension to spark frequency (opt-in, backward compatible)
    float narrativeTension = 1.0f;
    if (NARRATIVE.isEnabled()) {
        narrativeTension = NARRATIVE.getTension();  // 0.0-1.0
    }
    uint8_t sparkChance = (uint8_t)((80 + ctx.speed) * (0.5f + narrativeTension * 0.5f));
    if (random8() < sparkChance) {
        int center = CENTER_LEFT + random8(2);  // 79 or 80
        m_fireHeat[center] = qadd8(m_fireHeat[center], random8(160, 255));
    }

    // Map heat to LEDs with CENTER PAIR pattern
    for (int i = 0; i < STRIP_LENGTH; i++) {
        CRGB color = HeatColor(m_fireHeat[i]);

        // Strip 1
        ctx.leds[i] = color;

        // Strip 2 (mirrored)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color;
        }
    }
}

void FireEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& FireEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Fire",
        "Realistic fire simulation radiating from centre",
        plugins::EffectCategory::FIRE,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

