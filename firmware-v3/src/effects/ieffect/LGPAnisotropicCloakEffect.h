// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPAnisotropicCloakEffect.h
 * @brief LGP Anisotropic Cloak - Direction-dependent visibility
 * 
 * Effect ID: 48
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_pos/m_vel: Cloak position and velocity
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAnisotropicCloakEffect : public plugins::IEffect {
public:
    LGPAnisotropicCloakEffect();
    ~LGPAnisotropicCloakEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    float m_pos;
    float m_vel;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
