// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPWaveCollisionEnhancedEffect.h
 * @brief LGP Wave Collision Enhanced - Enhanced version with 64-bin sub-bass, enhanced snare/hi-hat triggers
 *
 * Effect ID: 96
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 *
 * Enhancements over LGPWaveCollisionEffect (ID 17):
 * - 64-bin sub-bass (bins 0-5) for bass-driven wave intensity
 * - Enhanced isSnareHit() collision boost intensity
 * - Improved isHihatHit() speed burst
 * - Better use of beatPhase() for wave packet timing
 * - Uses heavy_chroma consistently for color
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPWaveCollisionEnhancedEffect : public plugins::IEffect {
public:
    LGPWaveCollisionEnhancedEffect() = default;
    ~LGPWaveCollisionEnhancedEffect() override = default;

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
    uint8_t m_dominantBin = 0;
    float m_dominantBinSmooth = 0.0f;
    float m_collisionBoost = 0.0f;

    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Enhancement utilities (Spring + AsymmetricFollower)
    enhancement::Spring m_speedSpring;                                        // Natural momentum for speed
    enhancement::AsymmetricFollower m_energyAvgFollower{0.0f, 0.20f, 0.50f};  // 200ms rise, 500ms fall
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.25f, 0.40f}; // 250ms rise, 400ms fall

    // Percussion-driven animation
    float m_speedTarget = 1.0f;       // Hi-hat speed boost target (decays to 1.0)
    
    // Enhanced: 64-bin sub-bass tracking
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    float m_subBassEnergy = 0.0f;
    float m_targetSubBass = 0.0f;

    // Validation instrumentation
    float m_prevPhaseDelta = 0.0f;    // Previous frame phase delta for reversal detection
    
    // Tempo lock hysteresis (Schmitt trigger: 0.6 lock / 0.4 unlock)
    bool m_tempoLocked = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
