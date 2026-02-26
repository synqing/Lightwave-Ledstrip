/**
 * @file LGPSolitonWavesEffect.h
 * @brief LGP Soliton Waves - Self-reinforcing wave packets
 * 
 * Effect ID: 43
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS | TRAVELING
 * 
 * Instance State:
 * - m_pos/m_vel/m_amp/m_hue: Soliton parameters
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSolitonWavesEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SOLITON_WAVES;

    LGPSolitonWavesEffect();
    ~LGPSolitonWavesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_pos[4];
    float m_vel[4];
    uint8_t m_amp[4];
    uint8_t m_hue[4];
    int m_solitonCount;
    float m_damping;
    float m_velocityScale;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
