// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPGravitationalLensingEffect.h
 * @brief LGP Gravitational Lensing - Light bending around mass
 * 
 * Effect ID: 41
 * Family: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_massPos/m_massVel: Mass positions and velocities
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGravitationalLensingEffect : public plugins::IEffect {
public:
    LGPGravitationalLensingEffect();
    ~LGPGravitationalLensingEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    float m_massPos[3];
    float m_massVel[3];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
