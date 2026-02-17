/**
 * @file LGPAudioTestEffect.cpp
 * @brief Audio-reactive test effect implementation
 *
 * Demonstrates Phase 2 audio pipeline integration:
 * - ControlBus: RMS, flux, 8-band spectrum
 * - MusicalGrid: beat phase, beat tick, BPM
 * - Graceful degradation when audio unavailable
 */

#include "LGPAudioTestEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPAudioTestEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatDecay = 0.0f;
    m_lastBeatPhase = 0.0f;
    m_fallbackPhase = 0.0f;
    return true;
}

void LGPAudioTestEffect::render(plugins::EffectContext& ctx) {
    // ========================================================================
    // Determine audio source (real vs fallback)
    // ========================================================================

    float rms = 0.5f;
    float beatPhase = 0.0f;
    bool onBeat = false;
    float bands[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    if (ctx.audio.available) {
        // Real audio data from AudioActor
        rms = ctx.audio.rms();
        beatPhase = ctx.audio.beatPhase();
        onBeat = ctx.audio.isOnBeat();

        // Get all 8 bands
        for (int i = 0; i < 8; ++i) {
            bands[i] = ctx.audio.getBand(i);
        }
    } else {
        // Fallback: fake 120 BPM beat
        m_fallbackPhase += ((ctx.deltaTimeSeconds * 1000.0f) / 500.0f);  // 500ms per beat = 120 BPM
        if (m_fallbackPhase >= 1.0f) {
            m_fallbackPhase -= 1.0f;
        }
        beatPhase = m_fallbackPhase;

        // Fake RMS from sine wave
        rms = 0.5f + 0.3f * sinf(beatPhase * 2.0f * 3.14159f);

        // Fake bands with phase offset
        for (int i = 0; i < 8; ++i) {
            float phaseOffset = (float)i * 0.125f;
            bands[i] = 0.5f + 0.4f * sinf((beatPhase + phaseOffset) * 2.0f * 3.14159f);
        }

        // Fake beat detection at phase crossing
        if (beatPhase < 0.05f && m_lastBeatPhase > 0.95f) {
            onBeat = true;
        }
    }

    m_lastBeatPhase = beatPhase;

    // Get dt for frame-rate independent decay
    float dt = ctx.getSafeRawDeltaSeconds();

    // ========================================================================
    // Beat pulse animation (decays over time)
    // ========================================================================

    if (onBeat) {
        m_beatDecay = 1.0f;  // Reset to full intensity on beat
    } else {
        // Exponential decay (~200ms, dt-corrected)
        m_beatDecay *= powf(0.92f, dt * 60.0f);
        if (m_beatDecay < 0.01f) m_beatDecay = 0.0f;
    }

    // ========================================================================
    // Master brightness from RMS + beat pulse
    // ========================================================================

    float masterIntensity = 0.3f + (rms * 0.5f) + (m_beatDecay * 0.2f);
    masterIntensity = fminf(1.0f, masterIntensity);
    uint8_t masterBright = (uint8_t)(masterIntensity * 255.0f);

    // ========================================================================
    // Clear buffer
    // ========================================================================

    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // ========================================================================
    // CENTER PAIR rendering: bands map from center (bass) to edges (treble)
    // ========================================================================

    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        // Map distance from center to band index
        // dist 0-9: band 0 (bass)
        // dist 10-19: band 1
        // ...
        // dist 70-79: band 7 (treble)
        uint8_t bandIdx = (uint8_t)(dist / 10);
        if (bandIdx > 7) bandIdx = 7;

        float bandEnergy = bands[bandIdx];

        // Color: hue shifts with distance + gHue for animation
        uint8_t hue = ctx.gHue + (uint8_t)(dist * 2);

        // Brightness: band energy × master intensity
        uint8_t bright = (uint8_t)(bandEnergy * masterBright);

        // Beat pulse adds extra brightness at center
        if (dist < 20 && m_beatDecay > 0.1f) {
            float centerBoost = (1.0f - (float)dist / 20.0f) * m_beatDecay;
            bright = (uint8_t)fminf(255.0f, (float)bright + centerBoost * 100.0f);
        }

        CRGB color = ctx.palette.getColor(hue, bright);

        // Set center pair (both strips)
        SET_CENTER_PAIR(ctx, dist, color);
    }

    // ========================================================================
    // Beat indicator: bright flash at center on beat
    // Uses palette color with boosted brightness (scales toward BLACK)
    // instead of additive white (which washes colors toward WHITE)
    // ========================================================================

    if (m_beatDecay > 0.5f) {
        // Boost brightness based on beat decay (0.7 base + 0.3 from decay)
        uint8_t beatBright = (uint8_t)((0.7f + m_beatDecay * 0.3f) * ctx.brightness);
        CRGB beatColor = ctx.palette.getColor(ctx.gHue, beatBright);

        // Center pair flash (just the center 4 LEDs) - assignment preserves saturation
        ctx.leds[CENTER_LEFT] = beatColor;
        ctx.leds[CENTER_RIGHT] = beatColor;
        ctx.leds[STRIP_LENGTH + CENTER_LEFT] = beatColor;
        ctx.leds[STRIP_LENGTH + CENTER_RIGHT] = beatColor;
    }
}

void LGPAudioTestEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPAudioTestEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Audio Test",
        "Audio-reactive spectrum + beat visualization",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
