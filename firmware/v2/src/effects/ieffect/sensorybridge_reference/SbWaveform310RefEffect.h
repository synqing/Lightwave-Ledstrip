/**
 * @file SbWaveform310RefEffect.h
 * @brief Sensory Bridge 3.1.0 reference: waveform light show mode (centre-origin adaptation)
 *
 * This is a port of `light_mode_waveform()` from SensoryBridge-3.1.0.
 * It uses:
 * - 4-frame waveform history averaging
 * - Mood-dependent per-sample smoothing
 * - Note-chromagram → colour summation (chromatic mode) or single hue (non-chromatic)
 * - Peak follower scaling (`waveform_peak_scaled_last * 4.0`)
 *
 * Effect ID: 109
 */

#pragma once

#include "../../CoreEffects.h"
#include "../../../plugins/api/IEffect.h"
#include "../../../plugins/api/EffectContext.h"
#include "../../CoreEffects.h"

namespace lightwaveos::effects::ieffect::sensorybridge_reference {

class SbWaveform310RefEffect final : public plugins::IEffect {
public:
    SbWaveform310RefEffect() = default;
    ~SbWaveform310RefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t WAVEFORM_POINTS = lightwaveos::audio::CONTROLBUS_WAVEFORM_N;
    static constexpr uint8_t HISTORY_FRAMES = 4;

    uint32_t m_lastHopSeq = 0;
    uint8_t m_historyIndex = 0;
    bool m_historyPrimed = false;

    int16_t m_waveformHistory[HISTORY_FRAMES][WAVEFORM_POINTS] = {{0}};
    float m_waveformLast[lightwaveos::effects::HALF_LENGTH] = {0};

    float m_waveformPeakScaledLast = 0.0f;
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};
};

} // namespace lightwaveos::effects::ieffect::sensorybridge_reference
