/**
 * @file AudioReactivePolicy.h
 * @brief Shared timing and beat-gate policy for audio-reactive effects.
 *
 * This header centralises the "audio uses raw time" contract so effect
 * implementations stay consistent and SPEED does not distort DSP-coupled maths.
 */

#pragma once

#include "../../plugins/api/EffectContext.h"
#include "BeatPulseRenderUtils.h"

namespace lightwaveos::effects::ieffect::AudioReactivePolicy {

enum class TriggerMode : uint8_t {
    Tempo = 0,
    Transient,
    Kick,
    Snare,
    Hihat,
    HybridTempoTransient,
    HybridTempoKick,
};

struct TriggerDecision {
    bool fired = false;
    float strength01 = 0.0f;
    float level01 = 0.0f;
    bool reliable = false;
};

/**
 * @brief Delta seconds for audio-coupled maths (unscaled by SPEED).
 */
static inline float signalDt(const plugins::EffectContext& ctx) {
    return ctx.getSafeRawDeltaSeconds();
}

/**
 * @brief Delta seconds for visual-only motion (SPEED-scaled).
 */
static inline float visualDt(const plugins::EffectContext& ctx) {
    return ctx.getSafeDeltaSeconds();
}

/**
 * @brief Resolve an effect trigger source to a single semantic decision.
 */
static inline TriggerDecision audioTrigger(
    const plugins::EffectContext& ctx,
    TriggerMode mode,
    float fallbackBpm,
    uint32_t& lastBeatMs
) {
    auto clamp01 = [](float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    };
    auto metronomeFallback = [&]() {
        const uint32_t nowMs = ctx.rawTotalTimeMs;
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, fallbackBpm);
        bool fired = false;
        if (lastBeatMs == 0 || (nowMs - lastBeatMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            lastBeatMs = nowMs;
            fired = true;
        }
        return TriggerDecision{fired, fired ? 1.0f : 0.0f, fired ? 1.0f : 0.0f, false};
    };

    const bool beatReliable = ctx.audio.available && (ctx.audio.tempoConfidence() >= BeatPulseTiming::kTempoConfMin);
    const TriggerDecision beat{
        ctx.audio.isOnBeat(),
        clamp01(ctx.audio.beatStrength()),
        clamp01(ctx.audio.beatStrength()),
        beatReliable,
    };
    const TriggerDecision transient{
        ctx.audio.hasOnsetEvent(),
        clamp01(ctx.audio.onsetEvent()),
        clamp01(ctx.audio.onsetEnv()),
        ctx.audio.available && !ctx.audio.trinityActive,
    };
    const TriggerDecision kick{
        ctx.audio.isKickHit(),
        clamp01(ctx.audio.kickFlux()),
        clamp01(ctx.audio.kickLevel()),
        ctx.audio.available && !ctx.audio.trinityActive,
    };
    const TriggerDecision snare{
        ctx.audio.isSnareHit(),
        clamp01(fmaxf(ctx.audio.snareFlux(), ctx.audio.snare())),
        clamp01(ctx.audio.snare()),
        ctx.audio.available && !ctx.audio.trinityActive,
    };
    const TriggerDecision hihat{
        ctx.audio.isHihatHit(),
        clamp01(fmaxf(ctx.audio.hihatFlux(), ctx.audio.hihat())),
        clamp01(ctx.audio.hihat()),
        ctx.audio.available && !ctx.audio.trinityActive,
    };

    switch (mode) {
        case TriggerMode::Tempo:
            return beat.reliable ? beat : metronomeFallback();
        case TriggerMode::Transient:
            return transient;
        case TriggerMode::Kick:
            return kick;
        case TriggerMode::Snare:
            return snare;
        case TriggerMode::Hihat:
            return hihat;
        case TriggerMode::HybridTempoTransient:
            if (beat.reliable) return beat;
            if (transient.reliable) return transient;
            return metronomeFallback();
        case TriggerMode::HybridTempoKick:
            if (beat.reliable) return beat;
            if (kick.reliable) return kick;
            return metronomeFallback();
        default:
            return metronomeFallback();
    }
}

/**
 * @brief Unified beat gate: confidence-gated audio beat with raw-time fallback.
 */
static inline bool audioBeatTick(
    const plugins::EffectContext& ctx,
    float fallbackBpm,
    uint32_t& lastBeatMs
) {
    return audioTrigger(ctx, TriggerMode::Tempo, fallbackBpm, lastBeatMs).fired;
}

} // namespace lightwaveos::effects::ieffect::AudioReactivePolicy
