/**
 * @file BPMEffect.h
 * @brief BPM - Syncs to detected tempo with musical color and energy response
 *
 * Effect ID: 6
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * Audio Integration (Sensory Bridge pattern):
 * - Chromagram → Color: Musical pitch content drives visual color
 * - RMS Energy → Brightness: Overall audio energy modulates brightness
 * - Beat Phase → Motion: Smooth phase-locked pulsing (no jitter)
 * - BPM-Adaptive Decay: Beat pulse decay matches musical tempo
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BPMEffect : public plugins::IEffect {
public:
    BPMEffect();
    ~BPMEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    float m_beatPulse;  // Decaying pulse on beat detection
    bool m_tempoLocked = false;  // Tempo lock state with hysteresis
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
