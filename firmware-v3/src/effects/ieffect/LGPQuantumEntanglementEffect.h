// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPQuantumEntanglementEffect.h
 * @brief LGP Quantum Entanglement - Correlated state collapse
 * 
 * Effect ID: 62
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_collapseRadius/m_holdTime/m_measurementTimer/m_quantumPhase
 * - m_collapsing/m_collapsed/m_collapsedHue
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuantumEntanglementEffect : public plugins::IEffect {
public:
    LGPQuantumEntanglementEffect();
    ~LGPQuantumEntanglementEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_collapseRadius;
    bool m_collapsing;
    bool m_collapsed;
    float m_holdTime;
    uint8_t m_collapsedHue;
    float m_quantumPhase;
    float m_measurementTimer;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
