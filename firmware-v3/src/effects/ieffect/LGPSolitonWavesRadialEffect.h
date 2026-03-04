/**
 * @file LGPSolitonWavesRadialEffect.h
 * @brief LGP Soliton Waves (Radial) - Radial soliton shells
 *
 * Radial variant of LGPSolitonWavesEffect.
 * Soliton shells expand/contract from center, bouncing at center and edge.
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS | TRAVELING
 *
 * Instance State:
 * - m_pos/m_vel/m_amp/m_hue: Soliton shell parameters (radial distance)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSolitonWavesRadialEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SOLITON_WAVES_RADIAL;

    LGPSolitonWavesRadialEffect();
    ~LGPSolitonWavesRadialEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_pos[4];     // radial distance from center
    float m_vel[4];     // radial velocity (positive = outward)
    uint8_t m_amp[4];
    uint8_t m_hue[4];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
