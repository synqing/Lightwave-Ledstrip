// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FireEffect.h
 * @brief Fire - Realistic fire simulation radiating from center
 * 
 * Effect ID: 0
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_fireHeat[STRIP_LENGTH]: Heat array for fire simulation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class FireEffect : public plugins::IEffect {
public:
    FireEffect();
    ~FireEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static byte fireHeat[STRIP_LENGTH])
    static constexpr uint16_t STRIP_LENGTH = 160;  // From CoreEffects.h
    byte m_fireHeat[STRIP_LENGTH];
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

