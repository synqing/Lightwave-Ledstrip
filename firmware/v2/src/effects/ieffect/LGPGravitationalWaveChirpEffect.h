/**
 * @file LGPGravitationalWaveChirpEffect.h
 * @brief LGP Gravitational Wave Chirp - Inspiral merger signal
 * 
 * Effect ID: 61
 * Family: NOVEL_PHYSICS
 * Tags: CENTER_ORIGIN | PHYSICS
 * 
 * Instance State:
 * - m_inspiralProgress/m_ringdownPhase/m_mergeFlash/m_phase1/m_phase2
 * - m_merging/m_ringdown
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPGravitationalWaveChirpEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_GRAVITATIONAL_WAVE_CHIRP;

    LGPGravitationalWaveChirpEffect();
    ~LGPGravitationalWaveChirpEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    float m_inspiralProgress;
    float m_ringdownPhase;
    bool m_merging;
    bool m_ringdown;
    float m_mergeFlash;
    float m_phase1;
    float m_phase2;
    float m_chirpBase;
    float m_chirpScale;
    float m_mergeDecay;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
