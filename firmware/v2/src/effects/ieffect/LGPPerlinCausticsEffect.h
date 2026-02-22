/**
 * @file LGPPerlinCausticsEffect.h
 * @brief LGP Perlin Caustics - Sparkling caustic lobes
 * 
 * Effect ID: 79
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN | SPECTRAL
 * 
 * Audio-reactive: Hi-hat/treble increases sparkle density; bass increases lobe scale
 * - Treble/hi-hat → sparkle density
 * - Bass → lobe scale/size
 * - Mid → brightness modulation
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

class LGPPerlinCausticsEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_CAUSTICS;

    LGPPerlinCausticsEffect();
    ~LGPPerlinCausticsEffect() override = default;

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
    
    // Audio state tracking (hop_seq checking for fresh data)
    uint32_t m_lastHopSeq;
    
    // Target values (updated only on new hops)
    float m_targetTreble;
    float m_targetBass;
    float m_targetMid;
    float m_targetHihat;
    
    // Smoothed audio parameters (eased toward targets every frame)
    float m_smoothTreble;
    float m_smoothBass;
    float m_smoothMid;
    float m_smoothHihat;
    
    // Time accumulator
    uint16_t m_time;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

