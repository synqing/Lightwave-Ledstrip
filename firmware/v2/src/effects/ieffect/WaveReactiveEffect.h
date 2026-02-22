/**
 * @file WaveReactiveEffect.h
 * @brief Wave Reactive - Energy-accumulating wave with smooth audio-driven motion
 *
 * Effect ID: TBD (new effect)
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * Pattern: REACTIVE (Sensory Bridge Kaleidoscope-style)
 * - Motion: Energy ACCUMULATION (pos += energy, not speed = metric)
 * - Audio: RMS → accumulated energy → wave speed boost
 * - Flux → brightness boost on transients
 *
 * Key insight: Audio-driven motion is VALID when using accumulation
 * rather than directly setting speed from noisy metrics.
 *
 * Instance State:
 * - m_waveOffset: Wave phase accumulator
 * - m_energyAccum: Audio energy accumulator (Kaleidoscope pattern)
 * - m_lastFlux, m_fluxBoost: Transient detection
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class WaveReactiveEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_WAVE_REACTIVE;

    WaveReactiveEffect();
    ~WaveReactiveEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Wave state
    uint32_t m_waveOffset;      // Wave phase accumulator

    // Reactive pattern state (Kaleidoscope-style)
    float m_energyAccum;        // Energy accumulator - smooths audio input

    // Transient state
    float m_lastFlux;           // Previous flux for transient detection
    float m_fluxBoost;          // Brightness boost from flux transients
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

