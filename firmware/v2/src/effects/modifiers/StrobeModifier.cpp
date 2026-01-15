/**
 * @file StrobeModifier.cpp
 * @brief Rhythmic pulsing modifier implementation
 */

#include "StrobeModifier.h"
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

StrobeModifier::StrobeModifier(
    StrobeMode mode,
    uint8_t subdivision,
    float dutyCycle,
    float intensity,
    float rateHz
)
    : m_mode(mode)
    , m_subdivision(subdivision > 16 ? 16 : (subdivision < 1 ? 1 : subdivision))
    , m_dutyCycle(dutyCycle < 0.0f ? 0.0f : (dutyCycle > 1.0f ? 1.0f : dutyCycle))
    , m_intensity(intensity < 0.0f ? 0.0f : (intensity > 1.0f ? 1.0f : intensity))
    , m_rateHz(rateHz < 1.0f ? 1.0f : (rateHz > 30.0f ? 30.0f : rateHz))
    , m_enabled(true)
{
}

bool StrobeModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    return true;
}

void StrobeModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    // Calculate current phase in strobe cycle (0.0 - 1.0)
    float phase = calculatePhase(ctx);

    // Determine if we're in "on" (visible) or "off" (blackout) phase
    bool isOn = phase < m_dutyCycle;

    if (!isOn) {
        // Apply blackout based on intensity
        uint8_t fadeAmount = static_cast<uint8_t>(m_intensity * 255.0f);
        fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);
    }
}

float StrobeModifier::calculatePhase(const plugins::EffectContext& ctx) const {
    switch (m_mode) {
        case StrobeMode::BEAT_SYNC: {
#if FEATURE_AUDIO_SYNC
            if (ctx.audio.available) {
                // Use beat phase directly (0.0 at beat, 1.0 just before next)
                return ctx.audio.beatPhase();
            }
#endif
            // Fallback to manual rate if no audio
            float period = 1000.0f / m_rateHz;
            return fmodf(static_cast<float>(ctx.totalTimeMs), period) / period;
        }

        case StrobeMode::SUBDIVISION: {
#if FEATURE_AUDIO_SYNC
            if (ctx.audio.available) {
                // Multiply beat phase by subdivision to get sub-beat phase
                // e.g., subdivision=4 gives 4 strobes per beat (sixteenth notes)
                float phase = ctx.audio.beatPhase() * static_cast<float>(m_subdivision);

                // Return fractional part (0.0 - 1.0 within each subdivision)
                return phase - floorf(phase);
            }
#endif
            // Fallback: use manual rate scaled by subdivision
            float period = 1000.0f / (m_rateHz * m_subdivision);
            return fmodf(static_cast<float>(ctx.totalTimeMs), period) / period;
        }

        case StrobeMode::MANUAL_RATE: {
            // Fixed Hz strobe rate (ignores beat)
            float period = 1000.0f / m_rateHz;
            return fmodf(static_cast<float>(ctx.totalTimeMs), period) / period;
        }

        default:
            return 0.0f;
    }
}

void StrobeModifier::unapply() {
    // No cleanup needed
}

const ModifierMetadata& StrobeModifier::getMetadata() const {
    static ModifierMetadata metadata(
        "Strobe",
        "Rhythmic pulsing with beat-sync and subdivision modes",
        ModifierType::STROBE,
        1
    );
    return metadata;
}

bool StrobeModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "mode") == 0) {
        setMode(static_cast<StrobeMode>(static_cast<uint8_t>(value)));
        return true;
    }
    if (strcmp(name, "subdivision") == 0) {
        setSubdivision(static_cast<uint8_t>(value));
        return true;
    }
    if (strcmp(name, "dutyCycle") == 0) {
        setDutyCycle(value);
        return true;
    }
    if (strcmp(name, "intensity") == 0) {
        setIntensity(value);
        return true;
    }
    if (strcmp(name, "rateHz") == 0) {
        setRateHz(value);
        return true;
    }
    return false;
}

float StrobeModifier::getParameter(const char* name) const {
    if (strcmp(name, "mode") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_mode));
    }
    if (strcmp(name, "subdivision") == 0) {
        return static_cast<float>(m_subdivision);
    }
    if (strcmp(name, "dutyCycle") == 0) {
        return m_dutyCycle;
    }
    if (strcmp(name, "intensity") == 0) {
        return m_intensity;
    }
    if (strcmp(name, "rateHz") == 0) {
        return m_rateHz;
    }
    return 0.0f;
}

void StrobeModifier::setMode(StrobeMode mode) {
    m_mode = mode;
}

void StrobeModifier::setSubdivision(uint8_t subdivision) {
    m_subdivision = subdivision > 16 ? 16 : (subdivision < 1 ? 1 : subdivision);
}

void StrobeModifier::setDutyCycle(float dutyCycle) {
    m_dutyCycle = dutyCycle < 0.0f ? 0.0f : (dutyCycle > 1.0f ? 1.0f : dutyCycle);
}

void StrobeModifier::setIntensity(float intensity) {
    m_intensity = intensity < 0.0f ? 0.0f : (intensity > 1.0f ? 1.0f : intensity);
}

void StrobeModifier::setRateHz(float rateHz) {
    m_rateHz = rateHz < 1.0f ? 1.0f : (rateHz > 30.0f ? 30.0f : rateHz);
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
