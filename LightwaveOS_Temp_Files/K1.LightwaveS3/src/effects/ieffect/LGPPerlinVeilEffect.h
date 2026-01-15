/**
 * @file LGPPerlinVeilEffect.h
 * @brief LGP Perlin Veil - Slow drifting curtains/fog from centre
 * 
 * Effect ID: 77
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Visual Foundation: Time-based Perlin noise advection
 * Audio Enhancement: Audio modulates contrast and depth (NOT speed)
 * 
 * Audio-reactive:
 * - RMS → contrast modulation
 * - Bass → depth variation
 * - Advection: TIME-BASED (prevents jitter)
 * 
 * Instance State:
 * - m_noiseX, m_noiseY, m_noiseZ: Perlin noise field coordinates
 * - m_contrast: Audio-modulated contrast level
 * - m_depthVariation: Audio-modulated depth variation
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinVeilEffect : public plugins::IEffect {
public:
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
    
    // Audio-modulated state
    float m_contrast;      // Contrast level (0.0-1.0, modulated by RMS)
    float m_depthVariation; // Depth variation (modulated by bass)
    
    // Audio state tracking (hop_seq checking for fresh data)
    uint32_t m_lastHopSeq;
    
    // Audio smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_fluxFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_beatFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_bassFollower{0.0f, 0.05f, 0.30f};
    
    // Target values (updated only on new hops)
    float m_targetRms;
    float m_targetFlux;
    float m_targetBeatStrength;
    float m_targetBass;
    
    // Smoothed audio parameters (from AsymmetricFollower)
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

