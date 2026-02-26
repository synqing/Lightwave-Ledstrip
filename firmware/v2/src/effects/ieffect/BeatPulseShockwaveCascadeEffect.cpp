/**
 * @file BeatPulseShockwaveCascadeEffect.cpp
 * @brief Beat Pulse (Shockwave Cascade) - Triple pressure wave with HTML shading
 *
 * Visual identity: Primary shockwave + 2 trailing echo fronts expanding outward.
 * Like multiple pressure waves from a single detonation.
 *
 * Key characteristics:
 *  - 3 rings total: primary + 2 echoes, all expanding OUTWARD
 *  - Echoes spawn with time delay (staggered birth)
 *  - Echoes are progressively THINNER and DIMMER
 *  - Colour by distance through palette
 *  - Brightness spine: 0.05 + intensity * 0.95
 *  - White mix: primary only (0.35)
 *
 * Effect ID: 116
 */

#include "BeatPulseShockwaveCascadeEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseShockwaveCascadeEffect
namespace {
constexpr float kBeatPulseShockwaveCascadeEffectSpeedScale = 1.0f;
constexpr float kBeatPulseShockwaveCascadeEffectOutputGain = 1.0f;
constexpr float kBeatPulseShockwaveCascadeEffectCentreBias = 1.0f;

float gBeatPulseShockwaveCascadeEffectSpeedScale = kBeatPulseShockwaveCascadeEffectSpeedScale;
float gBeatPulseShockwaveCascadeEffectOutputGain = kBeatPulseShockwaveCascadeEffectOutputGain;
float gBeatPulseShockwaveCascadeEffectCentreBias = kBeatPulseShockwaveCascadeEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseShockwaveCascadeEffectParameters[] = {
    {"beat_pulse_shockwave_cascade_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseShockwaveCascadeEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_shockwave_cascade_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseShockwaveCascadeEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_shockwave_cascade_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseShockwaveCascadeEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseShockwaveCascadeEffect

namespace lightwaveos::effects::ieffect {
bool  BeatPulseShockwaveCascadeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseShockwaveCascadeEffect
    gBeatPulseShockwaveCascadeEffectSpeedScale = kBeatPulseShockwaveCascadeEffectSpeedScale;
    gBeatPulseShockwaveCascadeEffectOutputGain = kBeatPulseShockwaveCascadeEffectOutputGain;
    gBeatPulseShockwaveCascadeEffectCentreBias = kBeatPulseShockwaveCascadeEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseShockwaveCascadeEffect

    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseShockwaveCascadeEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SHOCKWAVE CASCADE: Primary ring + 2 trailing echoes expanding outward.
    // Uses HTML parity shading spine.
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastBeatTimeMs);

    const uint32_t nowMs = ctx.rawTotalTimeMs;
    if (beatTick) {
        m_beatIntensity = 1.0f;
        m_lastBeatTimeMs = nowMs;
    }

    // --- Time-driven ring positions ---
    constexpr float TRAVEL_MS = 400.0f;
    constexpr float DECAY_MS = 320.0f;
    constexpr float RING_WIDTH = 0.10f;
    constexpr float ECHO1_OFFSET = 0.12f;
    constexpr float ECHO2_OFFSET = 0.24f;

    float ageMs = 999999.0f;
    if (m_lastBeatTimeMs != 0) {
        ageMs = static_cast<float>(nowMs - m_lastBeatTimeMs);
    }

    const float envelope = expf(-ageMs / DECAY_MS);
    const float primaryPos = clamp01(ageMs / TRAVEL_MS);  // centre -> edge
    const float echo1Pos = clamp01(primaryPos - ECHO1_OFFSET);
    const float echo2Pos = clamp01(primaryPos - ECHO2_OFFSET);

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        const float diffPrimary = fabsf(dist01 - primaryPos);
        const float primaryHit = RingProfile::tent(diffPrimary, RING_WIDTH);

        const float diffEcho1 = fabsf(dist01 - echo1Pos);
        const float echo1Hit = RingProfile::tent(diffEcho1, RING_WIDTH) * 0.45f;

        const float diffEcho2 = fabsf(dist01 - echo2Pos);
        const float echo2Hit = RingProfile::tent(diffEcho2, RING_WIDTH) * 0.25f;

        const float maxHit = fmaxf(primaryHit, fmaxf(echo1Hit, echo2Hit));
        const float intensity = clamp01(maxHit * envelope);

        // Brightness: 0.05 + intensity * 0.95
        const float brightFactor = clamp01(0.05f + intensity * 0.95f);

        // Palette colour by distance
        const uint8_t paletteIdx = floatToByte(dist01);
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightFactor));

        // White mix: primary hit only
        const float whiteMixVal = clamp01(primaryHit * envelope) * 0.35f;
        ColourUtil::addWhiteSaturating(c, floatToByte(whiteMixVal));

        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseShockwaveCascadeEffect
uint8_t BeatPulseShockwaveCascadeEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseShockwaveCascadeEffectParameters) / sizeof(kBeatPulseShockwaveCascadeEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseShockwaveCascadeEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseShockwaveCascadeEffectParameters[index];
}

bool BeatPulseShockwaveCascadeEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_speed_scale") == 0) {
        gBeatPulseShockwaveCascadeEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_output_gain") == 0) {
        gBeatPulseShockwaveCascadeEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_centre_bias") == 0) {
        gBeatPulseShockwaveCascadeEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseShockwaveCascadeEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_speed_scale") == 0) return gBeatPulseShockwaveCascadeEffectSpeedScale;
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_output_gain") == 0) return gBeatPulseShockwaveCascadeEffectOutputGain;
    if (strcmp(name, "beat_pulse_shockwave_cascade_effect_centre_bias") == 0) return gBeatPulseShockwaveCascadeEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseShockwaveCascadeEffect

void BeatPulseShockwaveCascadeEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveCascadeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Shockwave Cascade)",
        "HTML parity: triple wave cascade centre→edge",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}


} // namespace lightwaveos::effects::ieffect
