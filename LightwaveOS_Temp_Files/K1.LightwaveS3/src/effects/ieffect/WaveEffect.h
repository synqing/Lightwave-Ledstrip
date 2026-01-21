/**
 * @file WaveEffect.h
 * @brief Wave - Audio-reactive sine wave propagation
 *
 * Effect ID: 7
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * Visual Foundation: Time-based wave propagation
 * Audio Enhancement: Audio modulates amplitude/brightness only
 *
 * Audio Integration:
 * - RMS → wave amplitude (louder = taller waves)
 * - Flux → brightness boost on transients
 * - Speed: TIME-BASED (prevents jitter from audio→speed coupling)
 *
 * Instance State:
 * - m_waveOffset: Wave phase accumulator
 * - m_fallbackPhase: For tempo fallback
 * - m_lastFlux, m_fluxBoost: Transient detection
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class WaveEffect : public plugins::IEffect {
public:
    WaveEffect();
    ~WaveEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Wave state
    uint32_t m_waveOffset;      // Wave phase accumulator

    // Audio fallback state
    float m_fallbackPhase;      // Free-running phase when tempo confidence low
    float m_lastFlux;           // Previous flux for transient detection
    float m_fluxBoost;          // Brightness boost from flux transients
    
    // Audio smoothing (AsymmetricFollower for mood-adjusted smoothing)
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    
    // Hop sequence tracking
    uint32_t m_lastHopSeq = 0;
    float m_targetRms = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

