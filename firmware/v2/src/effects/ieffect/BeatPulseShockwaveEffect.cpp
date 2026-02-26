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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseShockwaveEffect
namespace {
constexpr float kBeatPulseShockwaveEffectSpeedScale = 1.0f;
constexpr float kBeatPulseShockwaveEffectOutputGain = 1.0f;
constexpr float kBeatPulseShockwaveEffectCentreBias = 1.0f;

float gBeatPulseShockwaveEffectSpeedScale = kBeatPulseShockwaveEffectSpeedScale;
float gBeatPulseShockwaveEffectOutputGain = kBeatPulseShockwaveEffectOutputGain;
float gBeatPulseShockwaveEffectCentreBias = kBeatPulseShockwaveEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseShockwaveEffectParameters[] = {
    {"beat_pulse_shockwave_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseShockwaveEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_shockwave_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseShockwaveEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_shockwave_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseShockwaveEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseShockwaveEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseShockwaveEffect
    gBeatPulseShockwaveEffectSpeedScale = kBeatPulseShockwaveEffectSpeedScale;
    gBeatPulseShockwaveEffectOutputGain = kBeatPulseShockwaveEffectOutputGain;
    gBeatPulseShockwaveEffectCentreBias = kBeatPulseShockwaveEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseShockwaveEffect


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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseShockwaveEffect
uint8_t BeatPulseShockwaveEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseShockwaveEffectParameters) / sizeof(kBeatPulseShockwaveEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseShockwaveEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseShockwaveEffectParameters[index];
}

bool BeatPulseShockwaveEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_shockwave_effect_speed_scale") == 0) {
        gBeatPulseShockwaveEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_shockwave_effect_output_gain") == 0) {
        gBeatPulseShockwaveEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_shockwave_effect_centre_bias") == 0) {
        gBeatPulseShockwaveEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseShockwaveEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_shockwave_effect_speed_scale") == 0) return gBeatPulseShockwaveEffectSpeedScale;
    if (strcmp(name, "beat_pulse_shockwave_effect_output_gain") == 0) return gBeatPulseShockwaveEffectOutputGain;
    if (strcmp(name, "beat_pulse_shockwave_effect_centre_bias") == 0) return gBeatPulseShockwaveEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseShockwaveEffect

void BeatPulseShockwaveEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveEffect::getMetadata() const {
    return m_meta;
}


} // namespace lightwaveos::effects::ieffect
