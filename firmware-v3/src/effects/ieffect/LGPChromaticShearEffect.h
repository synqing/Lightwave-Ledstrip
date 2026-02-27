// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPChromaticShearEffect.h
 * @brief LGP Chromatic Shear - Color-splitting shear effect
 * 
 * Effect ID: 30
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Instance State:
 * - m_phase: Shear phase accumulator
 * - m_paletteOffset: Palette rotation offset
 * - m_lastUpdateFrame: Frame marker for palette rotation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPChromaticShearEffect : public plugins::IEffect {
public:
    LGPChromaticShearEffect();
    ~LGPChromaticShearEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    uint16_t m_phase;
    uint8_t m_paletteOffset;
    uint32_t m_lastUpdateFrame;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
