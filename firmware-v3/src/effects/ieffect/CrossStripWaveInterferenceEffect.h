/**
 * @file CrossStripWaveInterferenceEffect.h
 * @brief F4 Cross-Strip Wave Interference.
 *
 * Phase 3 Move 3.2 of the Synergy-Topology programme. Direct dual-strip
 * effect using the Reflective Twin opt-in contract. Default phase offset is
 * 3pi/4 because Captain's K1v2 visual preflight on 2026-05-05 found that
 * offset most visibly separates the upper tooth edge from the lower trough.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class CrossStripWaveInterferenceEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_CROSS_STRIP_WAVE_INTERFERENCE;

    CrossStripWaveInterferenceEffect() = default;
    ~CrossStripWaveInterferenceEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override {}
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_phaseOffsetCycles = 0.375f;  // 3pi/4
    float m_wavePhase = 0.0f;
    float m_speedHz = 0.12f;
    float m_contrast = 0.88f;
    float m_wavelengthLeds = 12.0f;
};

}  // namespace ieffect
}  // namespace effects
}  // namespace lightwaveos
