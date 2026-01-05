/**
 * @file SpectrumAnalyzerEffect.h
 * @brief Classic frequency spectrum analyzer using 64-bin Goertzel output
 *
 * Maps the full 64-bin Goertzel spectrum (A2-C8, 110-4186 Hz) to LED positions
 * with center-origin layout. Bass frequencies at center, treble at edges.
 *
 * Effect ID: TBD (will be assigned in CoreEffects.cpp)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SpectrumAnalyzerEffect : public plugins::IEffect {
public:
    SpectrumAnalyzerEffect() = default;
    ~SpectrumAnalyzerEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Smoothing buffers for each bin (64 bins)
    float m_binSmoothing[64] = {0};
    
    // Beat-sync mode state
    bool m_beatSyncMode = false;
    float m_beatSyncPhase = 0.0f;
    
    // Peak hold for visual feedback
    float m_peakHold[64] = {0};
    uint32_t m_peakHoldTime[64] = {0};
    static constexpr uint32_t PEAK_HOLD_DURATION_MS = 200;  // 200ms peak hold
    
    // Phase accumulator for time-based motion
    float m_phase = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

