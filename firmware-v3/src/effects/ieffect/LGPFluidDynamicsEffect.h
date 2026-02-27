// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPFluidDynamicsEffect.h
 * @brief LGP Fluid Dynamics - Fluid flow simulation
 * 
 * Effect ID: 39
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_velocity/m_pressure: Flow fields
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFluidDynamicsEffect : public plugins::IEffect {
public:
    LGPFluidDynamicsEffect();
    ~LGPFluidDynamicsEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    float m_velocity[STRIP_LENGTH];
    float m_pressure[STRIP_LENGTH];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
