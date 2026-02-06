/**
 * @file BeatPulseStackEffect.cpp
 * @brief Beat Pulse (Stack) - UI preview parity pulse (static palette gradient + white push)
 */

#include "BeatPulseStackEffect.h"

#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline uint8_t clampU8FromFloat(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

static inline void addWhiteSaturating(CRGB& c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r) + w;
    uint16_t g = static_cast<uint16_t>(c.g) + w;
    uint16_t b = static_cast<uint16_t>(c.b) + w;
    c.r = (r > 255) ? 255 : static_cast<uint8_t>(r);
    c.g = (g > 255) ? 255 : static_cast<uint8_t>(g);
    c.b = (b > 255) ? 255 : static_cast<uint8_t>(b);
}

} // namespace

bool BeatPulseStackEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseStackEffect::render(plugins::EffectContext& ctx) {
    // ---------------------------------------------------------------------
    // Beat source
    // ---------------------------------------------------------------------
    bool beatTick = false;
    float beatStrength = 1.0f;
    float silentScale = 1.0f;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
        beatStrength = clamp01(ctx.audio.beatStrength());
        silentScale = ctx.audio.controlBus.silentScale;
    } else {
        // Fallback metronome (matches the HTML mock: 128 BPM).
        const uint32_t nowMs = ctx.totalTimeMs;
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, m_fallbackBpm);
        if (m_lastBeatTimeMs == 0 || (nowMs - m_lastBeatTimeMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastBeatTimeMs = nowMs;
        }
        beatStrength = 1.0f;
        silentScale = 1.0f;
    }

    // On beat: the HTML sets beatIntensity = 1.0. We scale slightly by beat strength and
    // by the Intensity knob so weak/uncertain tracking doesn’t feel identical to confident locks.
    if (beatTick) {
        const float intensityNorm = static_cast<float>(ctx.intensity) / 255.0f;
        const float minLatch = 0.20f + 0.25f * intensityNorm;
        m_beatIntensity = fmaxf(m_beatIntensity, fmaxf(minLatch, 0.35f + 0.65f * beatStrength));
    }

    // ---------------------------------------------------------------------
    // Decay (dt-correct) to match: beatIntensity *= 0.94 per frame at ~60 FPS
    // ---------------------------------------------------------------------
    const float dt = ctx.getSafeDeltaSeconds();
    const float decay = powf(0.94f, dt * 60.0f);
    m_beatIntensity *= decay;
    if (m_beatIntensity < 0.0005f) {
        m_beatIntensity = 0.0f;
    }

    // ---------------------------------------------------------------------
    // Render (centre-origin, static palette gradient + white push)
    // ---------------------------------------------------------------------
    const float intensityNorm = static_cast<float>(ctx.intensity) / 255.0f;
    const float complexityNorm = static_cast<float>(ctx.complexity) / 255.0f;
    const float variationNorm = static_cast<float>(ctx.variation) / 255.0f;

    // Complexity tunes ring sharpness (higher = tighter ring), Variation trims starting radius.
    const float ringSharpness = 2.25f + 4.50f * complexityNorm;           // 2.25..6.75
    const float ringStartRadius = 0.40f + 0.35f * variationNorm;          // 0.40..0.75 (at beatIntensity=1)
    const float whiteScale = 0.16f + 0.30f * intensityNorm;               // 0.16..0.46
    const float boostScale = 0.30f + 0.45f * intensityNorm;               // 0.30..0.75
    const float baseBrightness = 0.40f + 0.22f * intensityNorm;           // 0.40..0.62

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Distance 0 at centre → ~1 at edges (centre is between the pair).
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // HTML pulse shape:
        // wavePos = beatIntensity * 1.2
        // waveHit = 1 - min(1, abs(dist - wavePos*0.5) * 3)
        // intensity = max(0, waveHit) * beatIntensity
        const float ringCentre = ringStartRadius * m_beatIntensity;
        const float waveHit = 1.0f - fminf(1.0f, fabsf(dist01 - ringCentre) * ringSharpness);
        float intensity = fmaxf(0.0f, waveHit) * m_beatIntensity;
        intensity *= silentScale;

        // Base gradient (static): palette indexed by distance.
        const uint8_t paletteIdx = static_cast<uint8_t>(clamp01(dist01) * 255.0f);

        // Brightness boost on beat (HTML: baseBrightness=0.5, + intensity*0.5)
        const float brightnessFactor = baseBrightness + intensity * boostScale;
        const uint8_t brightness8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * brightnessFactor);
        CRGB c = ctx.palette.getColor(paletteIdx, brightness8);

        // White push (HTML: whiteMix = intensity * 0.3)
        const float whiteMix = intensity * whiteScale;
        const uint8_t white8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * whiteMix);
        addWhiteSaturating(c, white8);

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseStackEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseStackEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Stack)",
        "UI preview parity: static palette gradient with beat-driven white push",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect
