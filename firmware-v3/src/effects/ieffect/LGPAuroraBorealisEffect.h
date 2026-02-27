// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPAuroraBorealisEffect.h
 * @brief LGP Aurora Borealis - Shimmering curtain lights
 * 
 * Effect ID: 34
 * Family: ORGANIC
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_time: Time accumulator
 * - m_curtainPhase: Per-curtain phase offsets
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAuroraBorealisEffect : public plugins::IEffect {
public:
    LGPAuroraBorealisEffect();
    ~LGPAuroraBorealisEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_time;
    uint8_t m_curtainPhase[5];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
