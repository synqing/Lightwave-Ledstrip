/**
 * @file LGPWaveCollisionEffect.h
 * @brief LGP Wave Collision - Colliding wave fronts from center
 * 
 * Effect ID: 17
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
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

class LGPWaveCollisionEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_WAVE_COLLISION;

    LGPWaveCollisionEffect() = default;
    ~LGPWaveCollisionEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // CRITICAL FIX: Single phase accumulator for traveling waves (not radii for standing waves)
    float m_phase = 0.0f;
    uint32_t m_lastHopSeq = 0;
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;
    float m_energyAvg = 0.0f;
    float m_energyDelta = 0.0f;
    float m_chromaAngle = 0.0f;       // Circular EMA angle for chroma hue (radians)
    float m_collisionBoost = 0.0f;

    // Enhancement utilities (Spring + AsymmetricFollower)
    enhancement::Spring m_speedSpring;                                        // Natural momentum for speed
    enhancement::AsymmetricFollower m_energyAvgFollower{0.0f, 0.20f, 0.50f};  // 200ms rise, 500ms fall
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.25f, 0.40f}; // 250ms rise, 400ms fall

    // Percussion-driven animation
    float m_speedTarget = 1.0f;       // Hi-hat speed boost target (decays to 1.0)

    // Validation instrumentation
    float m_prevPhaseDelta = 0.0f;    // Previous frame phase delta for reversal detection
    
    // EMA smoothing for energyDelta (prevents pops)
    float m_energyDeltaEMASmooth = 0.0f;
    bool m_energyDeltaEMAInitialized = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
