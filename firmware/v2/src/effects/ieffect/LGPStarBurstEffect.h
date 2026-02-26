/**
 * @file LGPStarBurstEffect.h
 * @brief LGP Star Burst - Explosive radial lines from center
 *
 * Effect ID: 24
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN
 *
 * REPAIRED: Simplified to match Wave Collision's proven pattern.
 * Audio: snare→burst, heavyBass→speed, chroma→color
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "ChromaUtils.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPStarBurstEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_STAR_BURST;

    LGPStarBurstEffect();
    ~LGPStarBurstEffect() override = default;

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
    // Core state (simplified - matches Wave Collision pattern)
    float m_phase;              // Phase accumulator
    float m_burst = 0.0f;       // Snare-driven burst intensity
    uint32_t m_lastHopSeq = 0;  // Audio hop sequence tracking

    // Circular chroma hue state (replaces argmax + linear EMA)
    float m_chromaAngle = 0.0f;      // Persistent angle for circular EMA (radians)

    // Speed state (Spring physics for natural momentum)
    enhancement::Spring m_phaseSpeedSpring;  // Critically damped - no overshoot
    
    // EMA smoothing for heavyBass (prevents pops)
    float m_heavyBassSmooth = 0.0f;
    bool m_heavyBassSmoothInitialized = false;

    // Tunables
    float m_phaseRate = 240.0f;
    float m_freqBase = 0.12f;
    float m_burstDecay = 0.88f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
