// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file WaveAmbientEffect.h
 * @brief Wave Ambient - Time-driven sine wave with audio amplitude modulation
 *
 * Effect ID: 7
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_BRIGHTNESS
 *
 * Pattern: AMBIENT (Sensory Bridge Bloom-style)
 * - Motion: TIME-BASED (user speed parameter only)
 * - Audio: RMS → amplitude, Flux → brightness boost
 * - NO audio→speed coupling (prevents jitter)
 *
 * Instance State:
 * - m_waveOffset: Wave phase accumulator
 * - m_lastFlux, m_fluxBoost: Transient detection
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class WaveAmbientEffect : public plugins::IEffect {
public:
    WaveAmbientEffect();
    ~WaveAmbientEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Wave state
    uint32_t m_waveOffset;      // Wave phase accumulator

    // Audio state (brightness only, NOT speed)
    float m_lastFlux;           // Previous flux for transient detection
    float m_fluxBoost;          // Brightness boost from flux transients
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

