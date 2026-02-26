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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseStackEffect
namespace {
constexpr float kBeatPulseStackEffectSpeedScale = 1.0f;
constexpr float kBeatPulseStackEffectOutputGain = 1.0f;
constexpr float kBeatPulseStackEffectCentreBias = 1.0f;

float gBeatPulseStackEffectSpeedScale = kBeatPulseStackEffectSpeedScale;
float gBeatPulseStackEffectOutputGain = kBeatPulseStackEffectOutputGain;
float gBeatPulseStackEffectCentreBias = kBeatPulseStackEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseStackEffectParameters[] = {
    {"beat_pulse_stack_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseStackEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_stack_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseStackEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_stack_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseStackEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseStackEffect

namespace lightwaveos::effects::ieffect {
bool  BeatPulseStackEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseStackEffect
    gBeatPulseStackEffectSpeedScale = kBeatPulseStackEffectSpeedScale;
    gBeatPulseStackEffectOutputGain = kBeatPulseStackEffectOutputGain;
    gBeatPulseStackEffectCentreBias = kBeatPulseStackEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseStackEffect

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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseStackEffect
uint8_t BeatPulseStackEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseStackEffectParameters) / sizeof(kBeatPulseStackEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseStackEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseStackEffectParameters[index];
}

bool BeatPulseStackEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_stack_effect_speed_scale") == 0) {
        gBeatPulseStackEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_stack_effect_output_gain") == 0) {
        gBeatPulseStackEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_stack_effect_centre_bias") == 0) {
        gBeatPulseStackEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseStackEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_stack_effect_speed_scale") == 0) return gBeatPulseStackEffectSpeedScale;
    if (strcmp(name, "beat_pulse_stack_effect_output_gain") == 0) return gBeatPulseStackEffectOutputGain;
    if (strcmp(name, "beat_pulse_stack_effect_centre_bias") == 0) return gBeatPulseStackEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseStackEffect

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


} // namespace lightwaveos::effects::ieffect
