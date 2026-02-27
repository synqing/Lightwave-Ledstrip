// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPInterferenceScannerEnhancedEffect.h
 * @brief LGP Interference Scanner Enhanced - Enhanced version with optimized 64-bin usage, enhanced snare boost
 *
 * Effect ID: 91
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | TRAVELING
 *
 * Enhancements over LGPInterferenceScannerEffect (ID 16):
 * - Optimized 64-bin spectrum thresholds
 * - Uses heavy_chroma consistently for color
 * - Enhanced isSnareHit() for snare boost
 * - Improved Spring smoothing for speed
 * - Better use of beatPhase() for pattern synchronization
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPInterferenceScannerEnhancedEffect : public plugins::IEffect {
public:
    LGPInterferenceScannerEnhancedEffect();
    ~LGPInterferenceScannerEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_scanPhase;
    uint32_t m_lastHopSeq = 0;
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;
    float m_energyAvg = 0.0f;
    float m_energyDelta = 0.0f;
    uint8_t m_dominantBin = 0;
    float m_dominantBinSmooth = 0.0f;

    // Chromagram smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Enhancement utilities (Spring + AsymmetricFollower)
    enhancement::Spring m_speedSpring;                                        // Natural momentum for speed
    enhancement::AsymmetricFollower m_energyAvgFollower{0.0f, 0.20f, 0.50f};  // 200ms rise, 500ms fall
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.25f, 0.40f}; // 250ms rise, 400ms fall
    enhancement::AsymmetricFollower m_bassFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_trebleFollower{0.0f, 0.05f, 0.30f};
    
    // Hop sequence tracking
    float m_targetBass = 0.0f;
    float m_targetTreble = 0.0f;

    // Validation instrumentation
    float m_prevPhaseDelta = 0.0f;    // Previous frame phase delta for reversal detection

    // 64-bin spectrum tracking for enhanced audio response
    float m_bassWavelength = 0.0f;    ///< Sub-bass energy (bins 0-5) modulates pattern width
    float m_trebleOverlay = 0.0f;     ///< Treble energy (bins 48-63) adds sparkle overlay
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
