// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file JuggleEffect.h
 * @brief Juggle - Multiple colored balls juggling
 * 
 * Effect ID: 5
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class JuggleEffect : public plugins::IEffect {
public:
    JuggleEffect();
    ~JuggleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // No instance state needed - uses beatsin16 which is deterministic
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

