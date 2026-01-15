/**
 * @file IntensityModifier.cpp
 * @brief Intensity modifier implementation
 */

#include "IntensityModifier.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace modifiers {

IntensityModifier::IntensityModifier(IntensitySource source, float baseIntensity, float depth)
    : m_source(source)
    , m_baseIntensity(std::clamp(baseIntensity, 0.0f, 1.0f))
    , m_depth(std::clamp(depth, 0.0f, 1.0f))
    , m_frequency(1.0f)
    , m_enabled(true) {
}

bool IntensityModifier::init(const plugins::EffectContext& ctx) {
    (void)ctx;
    Serial.printf("[IntensityModifier] Initialized (source: %d, base: %.2f, depth: %.2f)\n",
                  static_cast<int>(m_source), m_baseIntensity, m_depth);
    return true;
}

float IntensityModifier::calculateScaling(const plugins::EffectContext& ctx) {
    float modulation = 0.0f;

    switch (m_source) {
        case IntensitySource::CONSTANT:
            return m_baseIntensity;

        case IntensitySource::AUDIO_RMS:
#if FEATURE_AUDIO_SYNC
            if (ctx.audio.available) {
                modulation = ctx.audio.rms();
            }
#endif
            break;

        case IntensitySource::AUDIO_BEAT_PHASE:
#if FEATURE_AUDIO_SYNC
            if (ctx.audio.available) {
                // Pulse on beat: 1.0 at beat (phase=0), 0.0 at midpoint (phase=0.5)
                float phase = ctx.audio.beatPhase();
                modulation = 1.0f - phase;  // Linear decay from 1 to 0
            }
#endif
            break;

        case IntensitySource::SINE_WAVE: {
            float phase = ctx.getPhase(m_frequency);
            modulation = 0.5f + 0.5f * sinf(phase * 2.0f * M_PI);
            break;
        }

        case IntensitySource::TRIANGLE_WAVE: {
            float phase = ctx.getPhase(m_frequency);
            modulation = 1.0f - fabsf(2.0f * phase - 1.0f);
            break;
        }
    }

    // Combine base intensity with modulation
    return m_baseIntensity * (1.0f - m_depth + m_depth * modulation);
}

void IntensityModifier::apply(plugins::EffectContext& ctx) {
    if (!m_enabled) return;

    float scaling = calculateScaling(ctx);

    // Scale all LEDs by intensity
    for (uint16_t i = 0; i < ctx.ledCount; i++) {
        ctx.leds[i].nscale8_video(static_cast<uint8_t>(scaling * 255.0f));
    }
}

void IntensityModifier::unapply() {
    Serial.println("[IntensityModifier] Unapplied");
}

const ModifierMetadata& IntensityModifier::getMetadata() const {
    static ModifierMetadata metadata{
        "Intensity",
        "Brightness envelope (audio-reactive)",
        ModifierType::INTENSITY,
        1
    };
    return metadata;
}

bool IntensityModifier::setParameter(const char* name, float value) {
    if (strcmp(name, "source") == 0) {
        setSource(static_cast<IntensitySource>(static_cast<uint8_t>(value)));
        return true;
    } else if (strcmp(name, "baseIntensity") == 0) {
        setBaseIntensity(value);
        return true;
    } else if (strcmp(name, "depth") == 0) {
        setDepth(value);
        return true;
    } else if (strcmp(name, "frequency") == 0) {
        setFrequency(value);
        return true;
    } else if (strcmp(name, "enabled") == 0) {
        setEnabled(value > 0.5f);
        return true;
    }
    return false;
}

float IntensityModifier::getParameter(const char* name) const {
    if (strcmp(name, "source") == 0) {
        return static_cast<float>(static_cast<uint8_t>(m_source));
    } else if (strcmp(name, "baseIntensity") == 0) {
        return m_baseIntensity;
    } else if (strcmp(name, "depth") == 0) {
        return m_depth;
    } else if (strcmp(name, "frequency") == 0) {
        return m_frequency;
    } else if (strcmp(name, "enabled") == 0) {
        return m_enabled ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void IntensityModifier::setSource(IntensitySource source) {
    m_source = source;
    Serial.printf("[IntensityModifier] Set source: %d\n", static_cast<int>(m_source));
}

void IntensityModifier::setBaseIntensity(float intensity) {
    m_baseIntensity = std::clamp(intensity, 0.0f, 1.0f);
    Serial.printf("[IntensityModifier] Set base intensity: %.2f\n", m_baseIntensity);
}

void IntensityModifier::setDepth(float depth) {
    m_depth = std::clamp(depth, 0.0f, 1.0f);
    Serial.printf("[IntensityModifier] Set depth: %.2f\n", m_depth);
}

void IntensityModifier::setFrequency(float hz) {
    m_frequency = std::max(0.1f, hz);
    Serial.printf("[IntensityModifier] Set frequency: %.2f Hz\n", m_frequency);
}

} // namespace modifiers
} // namespace effects
} // namespace lightwaveos
