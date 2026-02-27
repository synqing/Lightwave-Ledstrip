// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPPerlinShocklinesEffect.h
 * @brief LGP Perlin Shocklines - Beat/flux injects sharp travelling ridges
 * 
 * Effect ID: 78
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Audio-reactive: Beat/flux injects sharp travelling ridges that propagate outward
 * - Beat/flux → inject shockwave at centre
 * - Shockwaves propagate outward and dissolve back into noise
 * - Treble → sharpness of ridges
 * 
 * Instance State:
 * - m_noiseX, m_noiseY: Base noise field coordinates
 * - m_waveFront: Current wavefront distance (0..79)
 * - m_waveEnergy: Current wave energy (0..1)
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinShocklinesEffect : public plugins::IEffect {
public:
    LGPPerlinShocklinesEffect();
    ~LGPPerlinShocklinesEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;

    // Noise field coordinates
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    
    // Centre-origin travelling ridge state (no buffer, stable)
    uint8_t m_waveFront;
    float m_waveEnergy;

    // Time accumulator
    uint16_t m_time;
    
    // Audio-driven momentum (Emotiscope-style)
    float m_momentum;
    
    // Audio state tracking
    uint32_t m_lastHopSeq;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

