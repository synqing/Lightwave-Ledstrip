/**
 * @file LGPBassBreathEffect.cpp
 * @brief Organic breathing effect implementation
 */

#include "LGPBassBreathEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPBassBreathEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_breathLevel = 0.0f;
    m_hueShift = 0.0f;
    return true;
}

void LGPBassBreathEffect::render(plugins::EffectContext& ctx) {
    float bass, mid, treble;

    if (ctx.audio.available) {
        bass = ctx.audio.bass();
        mid = ctx.audio.mid();
        treble = ctx.audio.treble();
    } else {
        // Fallback: slow sine wave breathing
        float phase = (float)(ctx.totalTimeMs % 3000) / 3000.0f;
        bass = 0.5f + 0.3f * sinf(phase * 6.283f);
        mid = 0.3f;
        treble = 0.2f;
    }

    // Breath dynamics: fast attack, slow decay
    float targetBreath = bass * 0.8f + mid * 0.2f;
    if (targetBreath > m_breathLevel) {
        m_breathLevel = targetBreath;  // Instant attack
    } else {
        m_breathLevel *= 0.97f;  // Slow exhale (~500ms)
    }

    // Hue shifts with treble activity
    m_hueShift += treble * ctx.deltaTimeMs * 0.1f;
    if (m_hueShift > 255.0f) m_hueShift -= 255.0f;

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // Render CENTER PAIR breathing
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // Breath expands from center
        float breathRadius = m_breathLevel;
        float brightness;

        if (normalizedDist < breathRadius) {
            // Inside breath: bright, fading toward edge of breath
            brightness = 0.25f + 0.75f * (1.0f - normalizedDist / breathRadius);
        } else {
            // Outside breath: dim ambient
            brightness = 0.03f;
        }

        // Apply master brightness and breath level
        uint8_t bright = (uint8_t)(brightness * m_breathLevel * ctx.brightness);

        // Color: base hue + accumulated shift + distance variation
        uint8_t hue = ctx.gHue + (uint8_t)m_hueShift + (uint8_t)(normalizedDist * 32);
        CRGB color = ctx.palette.getColor(hue, bright);

        SET_CENTER_PAIR(ctx, dist, color);
    }
}

void LGPBassBreathEffect::cleanup() {}

const plugins::EffectMetadata& LGPBassBreathEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Bass Breath",
        "Organic breathing driven by bass",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
