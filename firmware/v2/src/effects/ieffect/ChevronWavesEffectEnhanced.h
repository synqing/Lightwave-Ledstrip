/**
 * @file ChevronWavesEnhancedEffect.h
 * @brief LGP Chevron Waves Enhanced - Enhanced version with heavy_chroma, 64-bin sub-bass, snare sharpness boost
 *
 * Effect ID: 90
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 *
 * Enhancements over ChevronWavesEffect (ID 22):
 * - Uses heavy_chroma consistently for color
 * - 64-bin sub-bass (bins 0-5) for bass-driven speed modulation
 * - Adds isSnareHit() for sharpness boost on chevron edges
 * - Uses beatPhase() for wave synchronization
 * - Improved Spring smoothing parameters for speed
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ChevronWavesEnhancedEffect : public plugins::IEffect {
public:
    ChevronWavesEnhancedEffect();
    ~ChevronWavesEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state
    float m_chevronPos;
    uint32_t m_lastHopSeq = 0;

    // Chroma history for energy baseline
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;

    // Raw energy values (updated per hop)
    float m_energyAvg = 0.0f;
    float m_energyDelta = 0.0f;
    uint8_t m_dominantBin = 0;
    float m_dominantBinSmooth = 0.0f;

    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Enhancement utilities (Spring + AsymmetricFollower)
    enhancement::Spring m_phaseSpeedSpring;                              // Natural momentum for speed
    enhancement::AsymmetricFollower m_energyAvgFollower{0.0f, 0.20f, 0.50f};   // 200ms rise, 500ms fall
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.25f, 0.40f}; // 250ms rise, 400ms fall
    
    // Enhanced: 64-bin sub-bass tracking
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    float m_subBassEnergy = 0.0f;
    float m_targetSubBass = 0.0f;
    
    // Enhanced: Snare sharpness boost
    float m_snareSharpness = 0.0f;
    
    // Tempo lock hysteresis (Schmitt trigger: 0.6 lock / 0.4 unlock)
    bool m_tempoLocked = false;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
