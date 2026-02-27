// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ChevronWavesEffect.h
 * @brief LGP Chevron Waves - V-shaped wave propagation from center
 * 
 * Effect ID: 18
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_chevronPos: Phase accumulator for wave motion
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class ChevronWavesEffect : public plugins::IEffect {
public:
    ChevronWavesEffect();
    ~ChevronWavesEffect() override = default;

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

    // Enhancement utilities (Spring + AsymmetricFollower)
    enhancement::Spring m_phaseSpeedSpring;                              // Natural momentum for speed
    enhancement::AsymmetricFollower m_energyAvgFollower{0.0f, 0.20f, 0.50f};   // 200ms rise, 500ms fall
    enhancement::AsymmetricFollower m_energyDeltaFollower{0.0f, 0.25f, 0.40f}; // 250ms rise, 400ms fall

    // Constants
    static constexpr uint8_t FADE_AMOUNT = 40;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
