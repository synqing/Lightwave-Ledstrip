// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPHolographicEffect.h
 * @brief LGP Holographic - Multi-layer interference with depth illusion
 * 
 * Effect ID: 14
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE | DEPTH
 * 
 * Instance State:
 * - m_phase1/m_phase2/m_phase3: Layer phases
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHolographicEffect : public plugins::IEffect {
public:
    LGPHolographicEffect();
    ~LGPHolographicEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_phase1;
    float m_phase2;
    float m_phase3;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
