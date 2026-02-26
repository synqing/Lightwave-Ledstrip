/**
 * @file LGPBassBreathEffect.h
 * @brief Organic breathing effect driven by bass energy
 *
 * Inhale: bass attack expands from center
 * Exhale: slow decay back to dim
 * Color shifts with mid/treble
 *
 * Effect ID: 71 (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | ORGANIC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBassBreathEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_BASS_BREATH;

    LGPBassBreathEffect() = default;
    ~LGPBassBreathEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    float m_breathLevel = 0.0f;  // Current "breath" intensity
    // Circular chroma EMA state (radians) — replaces linear hue anchor.
    float m_chromaAngle = 0.0f;
    uint32_t m_lastHopSeq = 0;
    float m_lastBass = 0.0f;
    float m_lastFastFlux = 0.0f;
    float m_fluxKick = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
