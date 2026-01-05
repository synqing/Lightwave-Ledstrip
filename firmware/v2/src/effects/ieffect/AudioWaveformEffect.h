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
    static constexpr float SWEET_SPOT_MIN_LEVEL = 750.0f;
    static constexpr float PEAK_FOLLOW_ATTACK = 0.12f;   // Reduced from 0.25 for LGP smoothing
    static constexpr float PEAK_FOLLOW_RELEASE = 0.005f;
    static constexpr float PEAK_SCALE_ATTACK = 0.15f;    // Reduced from 0.25
    
    // Waveform history ring buffer (4 frames)
    int16_t m_waveformHistory[WAVEFORM_HISTORY_SIZE][WAVEFORM_SIZE];
    uint8_t m_historyIndex = 0;
    
    // Follower smoothing state
    float m_waveformLast[WAVEFORM_SIZE] = {0.0f};
    
    // Peak scaling state
    float m_maxWaveformValFollower = SWEET_SPOT_MIN_LEVEL;
    float m_waveformPeakScaled = 0.0f;
    float m_waveformPeakScaledLast = 0.0f;
    
    // Sum colour state (RGB smoothing)
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};
    
    // Track hop sequence for updates
    uint32_t m_lastHopSeq = 0;
    
    // Beat-sync mode state
    bool m_beatSyncMode = false;
    float m_beatSyncPhase = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
