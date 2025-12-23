/**
 * @file WaveEffect.h
 * @brief Wave - Simple sine wave propagation
 * 
 * Effect ID: 7
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 * 
 * Instance State:
 * - m_waveOffset: Wave phase accumulator
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
    // Instance state (was: static uint32_t waveOffset in effectWave)
    uint32_t m_waveOffset;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

