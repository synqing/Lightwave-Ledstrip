/**
 * @file EsWaveformRefEffect.h
 * @brief ES v1.1 "Waveform" reference show (time-domain strip).
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsWaveformRefEffect : public plugins::IEffect {
public:
    EsWaveformRefEffect() = default;
    ~EsWaveformRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint16_t SAMPLE_RATE_HZ = 12800;
    static constexpr float CUTOFF_HZ = 2110.0f;  // ES: 110 + 2000*(1.0 - bass); bass disabled in ref.
    static constexpr uint8_t FILTER_PASSES = 3;

    float m_alpha = 0.0f;
    float m_samples[128] = {0.0f};
};

} // namespace lightwaveos::effects::ieffect::esv11_reference

