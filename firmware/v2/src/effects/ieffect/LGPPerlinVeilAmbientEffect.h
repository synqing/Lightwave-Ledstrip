/**
 * @file LGPPerlinVeilAmbientEffect.h
 * @brief LGP Perlin Veil Ambient - Slow drifting curtains/fog from centre (time-driven)
 * 
 * Effect ID: 81
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Ambient: Time-driven only, no audio coupling
 * - Slow sine-based drift
 * - Breathing contrast modulation
 * 
 * Instance State:
 * - m_noiseX, m_noiseY, m_noiseZ: Perlin noise field coordinates
 * - m_time: Time accumulator
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinVeilAmbientEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_VEIL_AMBIENT;

    LGPPerlinVeilAmbientEffect();
    ~LGPPerlinVeilAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Noise field coordinates
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    uint16_t m_noiseZ;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

