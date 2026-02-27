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
 * @brief Unified beat gate: confidence-gated audio beat with raw-time fallback.
 */
static inline bool audioBeatTick(
    const plugins::EffectContext& ctx,
    float fallbackBpm,
    uint32_t& lastBeatMs
) {
    return BeatPulseTiming::computeBeatTick(ctx, fallbackBpm, lastBeatMs);
}

} // namespace lightwaveos::effects::ieffect::AudioReactivePolicy
