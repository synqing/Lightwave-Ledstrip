/**
 * @file BeatPulseCore.h
 * @brief Canonical beat-envelope + ring renderer for Beat Pulse variants.
 *
 * This is the shared baseline for Beat Pulse family effects:
 * - Beat tick policy: confidence-gated audio beat + raw-time fallback metronome.
 * - Envelope: HTML-parity beatIntensity behaviour.
 * - Ring profile: deterministic triangle profile (slope configurable).
 *
 * Keep this header-only and allocation-free for render-path safety.
 */

#pragma once

#include <cstdint>
#include <cmath>

#include "AudioReactivePolicy.h"
#include "BeatPulseRenderUtils.h"

namespace lightwaveos::effects::ieffect::BeatPulseCore {

struct State {
    float beatIntensity = 0.0f;
    uint32_t lastBeatMs = 0;
    float fallbackBpm = 128.0f;
};

struct Params {
    bool inward = false;
    float profileSlope = 3.0f;
    float brightnessBase = 0.5f;
    float brightnessGain = 0.5f;
    float whiteGain = 0.3f;
};

static inline void reset(State& s, float fallbackBpm = 128.0f) {
    s.beatIntensity = 0.0f;
    s.lastBeatMs = 0;
    s.fallbackBpm = fallbackBpm;
}

static inline bool stepEnvelope(const plugins::EffectContext& ctx, State& s) {
    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, s.fallbackBpm, s.lastBeatMs);
    const float dt = AudioReactivePolicy::signalDt(ctx);
    BeatPulseHTML::updateBeatIntensity(s.beatIntensity, beatTick, dt);
    return beatTick;
}

static inline float ringPosition01(const State& s, bool inward) {
    const float centre = BeatPulseHTML::ringCentre01(s.beatIntensity);
    return inward ? (1.0f - centre) : centre;
}

static inline float intensityAt(float dist01, float ringPos01, const State& s, float slope) {
    const float diff = fabsf(dist01 - ringPos01);
    const float waveHit = 1.0f - fminf(1.0f, diff * slope);
    return fmaxf(0.0f, waveHit) * s.beatIntensity;
}

static inline void renderSingleRing(plugins::EffectContext& ctx, State& s, const Params& p) {
    stepEnvelope(ctx, s);
    const float ringPos = ringPosition01(s, p.inward);
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);
        const float intensity = intensityAt(dist01, ringPos, s, p.profileSlope);
        const float brightFactor = clamp01(p.brightnessBase + intensity * p.brightnessGain);
        const uint8_t paletteIdx = floatToByte(dist01);
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightFactor));
        ColourUtil::addWhiteSaturating(c, floatToByte(clamp01(intensity * p.whiteGain)));
        SET_CENTER_PAIR(ctx, dist, c);
    }
}

} // namespace lightwaveos::effects::ieffect::BeatPulseCore

