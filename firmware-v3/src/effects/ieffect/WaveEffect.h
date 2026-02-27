// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WaveEffect.h
 * @brief Wave - Audio-reactive sine wave propagation
 *
 * Effect ID: 7
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * Audio Integration:
 * - RMS → wave amplitude (louder = taller waves)
 * - Beat phase → wave speed (synced to tempo when confident)
 * - Flux → brightness boost on transients
 * - Graceful fallback when beat tracking unreliable
 *
 * Instance State:
 * - m_waveOffset: Wave phase accumulator
 * - m_fallbackPhase: For tempo fallback
 * - m_lastFlux, m_fluxBoost: Transient detection
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
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
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

