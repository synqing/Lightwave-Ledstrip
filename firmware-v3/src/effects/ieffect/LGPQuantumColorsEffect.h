// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPQuantumColorsEffect.h
 * @brief LGP Quantum Colors - Quantized energy levels
 * 
 * Effect ID: 53
 * Family: COLOR_MIXING
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_waveFunction: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPQuantumColorsEffect : public plugins::IEffect {
public:
    LGPQuantumColorsEffect();
    ~LGPQuantumColorsEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_waveFunction;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
