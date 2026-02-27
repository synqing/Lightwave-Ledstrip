/**
 * @file LGPBassBreathEffect.h
 * @brief Organic breathing effect driven by bass energy
 *
 * Inhale: bass attack expands from center
 * Exhale: slow decay back to dim
 * Color shifts with mid/treble
 *
 * Effect ID: 71 (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | ORGANIC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBassBreathEffect : public plugins::IEffect {
public:
    LGPBassBreathEffect() = default;
    ~LGPBassBreathEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_breathLevel = 0.0f;  // Current "breath" intensity
    float m_hueShift = 0.0f;     // Accumulated hue from treble
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
