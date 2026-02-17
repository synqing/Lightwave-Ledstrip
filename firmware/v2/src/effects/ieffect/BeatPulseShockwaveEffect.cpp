/**
 * @file BeatPulseShockwaveEffect.cpp
 * @brief Beat Pulse (Shockwave) - HTML PARITY implementation
 *
 * VISUAL IDENTITY:
 * Single ring expanding OUTWARD from centre (or INWARD from edges) with
 * AMPLITUDE-DRIVEN motion. Same HTML core maths as Stack, different direction.
 *
 * HTML PARITY (LOCKED):
 * - beatIntensity slams to 1.0 on beat, decays *= 0.94^(dt*60)
 * - Ring position inverted for outward vs inward:
 *   - Outward: ringPos = ringCentre (starts at centre, moves toward edge)
 *   - Inward: ringPos = 1 - ringCentre (starts at edge, moves toward centre)
 * - Triangle profile: waveHit = 1 - min(1, abs(dist - ringPos) * 3)
 * - intensity = max(0, waveHit) * beatIntensity
 * - brightness = 0.5 + intensity * 0.5
 * - whiteMix = intensity * 0.3
 */

#include "BeatPulseShockwaveEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseCore.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

BeatPulseShockwaveEffect::BeatPulseShockwaveEffect(bool inward)
    : m_inward(inward)
    , m_meta(
          inward ? "Beat Pulse (Shockwave In)" : "Beat Pulse (Shockwave)",
          inward ? "HTML parity: amplitude-driven ring edge→centre" : "HTML parity: amplitude-driven ring centre→edge",
          plugins::EffectCategory::PARTY,
          1,
          "LightwaveOS")
{
}

bool BeatPulseShockwaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    BeatPulseCore::reset(m_state, 128.0f);
    return true;
}

void BeatPulseShockwaveEffect::render(plugins::EffectContext& ctx) {
    const BeatPulseCore::Params p{
        m_inward, // inward
        3.0f,     // profileSlope
        0.5f,     // brightnessBase
        0.5f,     // brightnessGain
        0.3f      // whiteGain
    };
    BeatPulseCore::renderSingleRing(ctx, m_state, p);
}

void BeatPulseShockwaveEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveEffect::getMetadata() const {
    return m_meta;
}

uint8_t BeatPulseShockwaveEffect::getParameterCount() const {
    return 0;
}

const plugins::EffectParameter* BeatPulseShockwaveEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseShockwaveEffect::setParameter(const char* name, float value) {
    (void)name;
    (void)value;
    return false;
}

float BeatPulseShockwaveEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
