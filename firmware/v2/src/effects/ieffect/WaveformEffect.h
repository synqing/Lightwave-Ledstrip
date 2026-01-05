/**
 * @file WaveformEffect.h
 * @brief Waveform - Direct waveform visualization matching Sensory Bridge 3.1.0
 *
 * Effect ID: TBD (will be assigned in CoreEffects.cpp)
 * Family: AUDIO
 * Tags: AUDIO_SYNC | LINEAR
 *
 * 1:1 visual parity with Sensory Bridge 3.1.0 light_mode_waveform().
 * Renders full waveform pattern with:
 * - 4-frame waveform history averaging
 * - MOOD-based smoothing
 * - Chromagram-driven color (with chromagram_max_val normalization)
 * - Linear LED mapping (NO centre-origin)
 * - Direct waveform rendering (no scrolling)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cstring>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class WaveformEffect : public plugins::IEffect {
public:
    WaveformEffect() = default;
    ~WaveformEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Waveform history (4 frames x 128 samples)
    static constexpr uint8_t WAVEFORM_HISTORY_SIZE = 4;
    static constexpr uint8_t WAVEFORM_SIZE = 128;  // CONTROLBUS_WAVEFORM_N
    int16_t m_waveformHistory[WAVEFORM_HISTORY_SIZE][WAVEFORM_SIZE] = {{0}};
    uint8_t m_historyIndex = 0;
    
    // Per-sample smoothing state
    float m_waveformLast[WAVEFORM_SIZE] = {0.0f};
    
    // Peak tracking
    float m_waveformPeakScaled = 0.0f;
    float m_waveformPeakScaledLast = 0.0f;
    
    // Audio smoothing (AsymmetricFollower for mood-adjusted smoothing)
    enhancement::AsymmetricFollower m_peakFollower{0.0f, 0.05f, 0.30f};
    
    // Hop sequence tracking
    uint32_t m_lastHopSeq = 0;
    float m_targetPeak = 0.0f;
    
    // Color smoothing
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

