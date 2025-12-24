/**
 * @file AudioWaveformEffect.h
 * @brief Sensory Bridge-style waveform visualization
 *
 * Uses waveform history, follower smoothing, and chromagram-driven colour.
 * Centre-origin mirrored mapping.
 *
 * Effect ID: 72
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | WAVEFORM
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

class AudioWaveformEffect : public plugins::IEffect {
public:
    AudioWaveformEffect() = default;
    ~AudioWaveformEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t WAVEFORM_HISTORY_SIZE = 4;
    static constexpr uint8_t WAVEFORM_SIZE = 128;  // CONTROLBUS_WAVEFORM_N
    
    // Waveform history ring buffer (4 frames)
    int16_t m_waveformHistory[WAVEFORM_HISTORY_SIZE][WAVEFORM_SIZE];
    uint8_t m_historyIndex = 0;
    
    // Follower smoothing state
    float m_waveformLast[WAVEFORM_SIZE] = {0.0f};
    
    // Peak scaling state
    float m_waveformPeakScaledLast = 0.0f;
    
    // Sum colour state (RGB smoothing)
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};
    
    // Track hop sequence for updates
    uint32_t m_lastHopSeq = 0;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

