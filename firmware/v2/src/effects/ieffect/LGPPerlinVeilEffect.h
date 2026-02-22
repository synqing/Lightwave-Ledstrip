/**
 * @file LGPPerlinVeilEffect.h
 * @brief LGP Perlin Veil - Slow drifting curtains/fog from centre
 * 
 * Effect ID: 77
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Audio-reactive: Audio increases advection speed and contrast
 * - Flux/beatStrength → advection momentum
 * - RMS → contrast modulation
 * - Bass → depth variation
 * 
 * Instance State:
 * - m_noiseX, m_noiseY, m_noiseZ: Perlin noise field coordinates
 * - m_momentum: Audio-driven advection momentum
 * - m_contrast: Audio-modulated contrast level
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinVeilEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_VEIL;

    LGPPerlinVeilEffect();
    ~LGPPerlinVeilEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Noise field coordinates (using uint16_t for wider range)
    uint16_t m_noiseX;
    uint16_t m_noiseY;
    uint16_t m_noiseZ;
    
    // Audio-driven state
    float m_momentum;      // Advection momentum (decays, boosted by audio)
    float m_contrast;      // Contrast level (0.0-1.0, modulated by RMS)
    float m_depthVariation; // Depth variation (modulated by bass)
    
    // Audio state tracking (hop_seq checking for fresh data)
    uint32_t m_lastHopSeq;
    
    // Target values (updated only on new hops)
    float m_targetRms;
    float m_targetFlux;
    float m_targetBeatStrength;
    float m_targetBass;
    
    // Smoothed audio parameters (eased toward targets every frame)
    float m_smoothRms;
    float m_smoothFlux;
    float m_smoothBeatStrength;
    float m_smoothBass;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

