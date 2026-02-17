/**
 * @file BeatPulseStackEffect.cpp
 * @brief Beat Pulse (Stack) - HTML PARITY implementation
 *
 * VISUAL IDENTITY:
 * Single ring contracting inward (edge to centre) with AMPLITUDE-DRIVEN motion.
 * Clean. Definitive. The kick drum. HTML parity locked.
 *
 * HTML PARITY (LOCKED):
 * - beatIntensity slams to 1.0 on beat, decays *= 0.94^(dt*60)
 * - ringCentre = beatIntensity * 0.6 (amplitude-driven, not time-driven)
 * - Triangle profile: waveHit = 1 - min(1, abs(dist - ringCentre) * 3)
 * - intensity = max(0, waveHit) * beatIntensity
 * - brightness = 0.5 + intensity * 0.5
 * - whiteMix = intensity * 0.3
 *
 * Effect ID: 110
 */

#include "BeatPulseStackEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseCore.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

bool BeatPulseStackEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    BeatPulseCore::reset(m_state, 128.0f);
    return true;
}

void BeatPulseStackEffect::render(plugins::EffectContext& ctx) {
    const BeatPulseCore::Params p{
        false, // inward
        3.0f,  // profileSlope
        0.5f,  // brightnessBase
        0.5f,  // brightnessGain
        0.3f   // whiteGain
    };
    BeatPulseCore::renderSingleRing(ctx, m_state, p);
}

void BeatPulseStackEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseStackEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Stack)",
        "HTML parity: amplitude-driven ring contracting to centre",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

uint8_t BeatPulseStackEffect::getParameterCount() const {
    return 0;
}

const plugins::EffectParameter* BeatPulseStackEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseStackEffect::setParameter(const char* name, float value) {
    (void)name;
    (void)value;
    return false;
}

float BeatPulseStackEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
