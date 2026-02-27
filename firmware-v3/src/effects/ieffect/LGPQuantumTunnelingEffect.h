/**
 * @file LGPQuantumTunnelingEffect.h
 * @brief LGP Quantum Tunneling - Particles passing through barriers
 * 
 * Effect ID: 40
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_particlePos/m_particleEnergy/m_particleActive: Particle state
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuantumTunnelingEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_QUANTUM_TUNNELING;

    LGPQuantumTunnelingEffect();
    ~LGPQuantumTunnelingEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    uint8_t m_particlePos[10];
    uint8_t m_particleEnergy[10];
    bool m_particleActive[10];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
