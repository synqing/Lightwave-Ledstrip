// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPRadialRippleEffect.h
 * @brief LGP Radial Ripple - Complex radial wave interference
 * 
 * Effect ID: 27
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_time: Phase accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRadialRippleEffect : public plugins::IEffect {
public:
    LGPRadialRippleEffect();
    ~LGPRadialRippleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
