/**
 * @file TrailModifier.cpp
 * @brief Temporal persistence modifier implementation
 */

#include "TrailModifier.h"
#include <cstring>
#include <cmath>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#else
#include "../../../test/unit/mocks/fastled_mock.h"
#endif

namespace lightwaveos {
namespace effects {
namespace modifiers {

TrailModifier::TrailModifier(TrailMode mode, uint8_t fadeRate, uint8_t minFade, uint8_t maxFade)
    : m_mode(mode)
    , m_fadeRate(fadeRate)
    , m_minFade(minFade)
    , m_maxFade(maxFade)
    , m_enabled(true)
    , m_hasHistory(false)
{
}

bool TrailModifier::init(const plugins::EffectContext& ctx) {
    // Initialize history buffer to black
    memset(m_previousFrame, 0, sizeof(m_previousFrame));
    m_hasHistory = false;
    return true;
}

void TrailModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    const uint16_t count = ctx.ledCount > MAX_LEDS ? MAX_LEDS : ctx.ledCount;
    const uint8_t fade = calculateFadeRate(ctx);

    if (m_hasHistory) {
        // Blend current frame with faded previous frame
        for (uint16_t i = 0; i < count; i++) {
            // Fade the previous frame
            CRGB faded = m_previousFrame[i];
            faded.fadeToBlackBy(fade);

            // Take the maximum of current and faded previous
            // This creates "trails" - bright pixels persist longer
            ctx.leds[i].r = (ctx.leds[i].r > faded.r) ? ctx.leds[i].r : faded.r;
            ctx.leds[i].g = (ctx.leds[i].g > faded.g) ? ctx.leds[i].g : faded.g;
            ctx.leds[i].b = (ctx.leds[i].b > faded.b) ? ctx.leds[i].b : faded.b;
        }
    }

    // Store current frame for next iteration
    memcpy(m_previousFrame, ctx.leds, count * sizeof(CRGB));
    m_hasHistory = true;
}

uint8_t TrailModifier::calculateFadeRate(const plugins::EffectContext& ctx) const {
    switch (m_mode) {
        case TrailMode::CONSTANT:
            return m_fadeRate;

        case TrailMode::BEAT_REACTIVE: {
#if FEATURE_AUDIO_SYNC
            if (ctx.audio.available) {
                // Get beat phase (0.0 at beat, 1.0 just before next beat)
                float phase = ctx.audio.beatPhase();

                // Invert: faster fade right after beat, slower between beats
                // This makes trails "snap" short on beats and stretch between
                float t = 1.0f - phase;  // 1 at beat, 0 between

                // Interpolate between min and max fade
                return m_minFade + static_cast<uint8_t>((m_maxFade - m_minFade) * t);
            }
#else
            (void)ctx;
#endif
            return m_fadeRate;
        }

        case TrailMode::VELOCITY:
            // Future: calculate fade based on how much the LED buffer changed
            // For now, fall back to constant
            return m_fadeRate;

        default:
            return m_fadeRate;
    }
}

void TrailModifier::unapply() {
    m_hasHistory = false;
    memset(m_previousFrame, 0, sizeof(m_previousFrame));
}

const ModifierMetadata& TrailModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Trail",
        "Temporal persistence with configurable fade trails",
        ModifierType::TRAIL,
        1
    );
    return metadata;
}

bool TrailModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<TrailMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "fadeRate") == 0) {
        setFadeRate(static_cast<uint8_t>(value));
        return true;
    }
    if (strcmp(name, "minFade") == 0) {
        setMinFade(static_cast<uint8_t>(value));
        return true;
    }
    if (strcmp(name, "maxFade") == 0) {
        setMaxFade(static_cast<uint8_t>(value));
        return true;
    }
    return false;
}

float TrailModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "fadeRate") == 0) {
        return static_cast<float>(m_fadeRate);
    }
    if (strcmp(name, "minFade") == 0) {
        return static_cast<float>(m_minFade);
    }
    if (strcmp(name, "maxFade") == 0) {
        return static_cast<float>(m_maxFade);
    }
    return 0.0f;
}

void TrailModifier::setMode(TrailMode mode) {
    m_mode = mode;
}

void TrailModifier::setFadeRate(uint8_t rate) {
    m_fadeRate = rate;
}

void TrailModifier::setMinFade(uint8_t min) {
    m_minFade = min;
}

void TrailModifier::setMaxFade(uint8_t max) {
    m_maxFade = max;
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
