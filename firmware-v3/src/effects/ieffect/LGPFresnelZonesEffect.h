// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPFresnelZonesEffect.h
 * @brief LGP Fresnel Zones - Fresnel lens zone plate pattern
 * 
 * Effect ID: 32
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFresnelZonesEffect : public plugins::IEffect {
public:
    LGPFresnelZonesEffect() = default;
    ~LGPFresnelZonesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
