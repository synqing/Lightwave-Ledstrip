/**
 * @file BeatPulseBreatheEffect.cpp
 * @brief Beat Pulse (Breathe) - Organic whole-strip breathing pulse
 *
 * The simplest, most primal beat-reactive effect - like a heartbeat or
 * subwoofer cone. NO ring. The entire strip pulses with strong centre-weighting.
 * Warm, organic, living.
 *
 * See BeatPulseBreatheEffect.h for design rationale.
 */

#include "BeatPulseBreatheEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseBreatheEffect
namespace {
constexpr float kBeatPulseBreatheEffectSpeedScale = 1.0f;
constexpr float kBeatPulseBreatheEffectOutputGain = 1.0f;
constexpr float kBeatPulseBreatheEffectCentreBias = 1.0f;

float gBeatPulseBreatheEffectSpeedScale = kBeatPulseBreatheEffectSpeedScale;
float gBeatPulseBreatheEffectOutputGain = kBeatPulseBreatheEffectOutputGain;
float gBeatPulseBreatheEffectCentreBias = kBeatPulseBreatheEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseBreatheEffectParameters[] = {
    {"beat_pulse_breathe_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseBreatheEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_breathe_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseBreatheEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_breathe_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseBreatheEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseBreatheEffect

namespace lightwaveos::effects::ieffect {

// ============================================================================
// Constants - Organic Breathing Feel
// ============================================================================

constexpr float DECAY_FACTOR = 0.88f;         // Slower than 0.94 (organic exhale)
constexpr float ATTACK_SMOOTHING = 0.25f;     // Soft attack (inhale)
constexpr float CENTRE_FALLOFF = 0.6f;        // Edge is 40% of centre brightness
constexpr float BASE_BRIGHTNESS = 0.20f;      // Higher resting state (warm glow)
constexpr float WARMTH_SHIFT = 0.25f;         // Palette shift toward warm on beat

// ============================================================================
// Lifecycle
// ============================================================================
bool  BeatPulseBreatheEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseBreatheEffect
    gBeatPulseBreatheEffectSpeedScale = kBeatPulseBreatheEffectSpeedScale;
    gBeatPulseBreatheEffectOutputGain = kBeatPulseBreatheEffectOutputGain;
    gBeatPulseBreatheEffectCentreBias = kBeatPulseBreatheEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseBreatheEffect

    m_beatIntensity = 0.0f;
    m_targetIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseBreatheEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // BREATHE: The simplest, most primal - like a heartbeat or subwoofer cone.
    // NO ring. The entire strip pulses with strong centre-weighting.
    // Organic, warm, living.
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastBeatTimeMs);

    // --- Delta time for frame-rate independent motion ---
    const float dt = ctx.getSafeRawDeltaSeconds();

    // --- Soft attack (the "inhale") - dt-corrected ---
    if (beatTick) {
        m_targetIntensity = 1.0f;
    }
    const float attackSmooth = 1.0f - powf(1.0f - ATTACK_SMOOTHING, dt * 60.0f);
    m_beatIntensity += (m_targetIntensity - m_beatIntensity) * attackSmooth;

    // --- Slower decay (the "exhale") ---
    const float decay = powf(DECAY_FACTOR, dt * 60.0f);
    m_beatIntensity *= decay;
    m_targetIntensity *= decay;

    // Clamp to zero when below threshold
    if (m_beatIntensity < 0.001f) {
        m_beatIntensity = 0.0f;
    }
    if (m_targetIntensity < 0.001f) {
        m_targetIntensity = 0.0f;
    }

    // --- Render: whole-strip breathing with strong centre weighting ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Strong centre weighting: centre = 1.0, edge = 0.4
        const float centreWeight = 1.0f - dist01 * CENTRE_FALLOFF;
        const float localIntensity = m_beatIntensity * centreWeight;

        // Colour: warm on hit, cool at rest
        // Shift palette index based on intensity (lower index = warmer colours typically)
        const float warmth = localIntensity * WARMTH_SHIFT;
        const uint8_t paletteIdx = floatToByte(dist01 * 0.8f + (1.0f - warmth) * 0.2f);

        // Higher resting brightness (warm ambient glow)
        const float brightnessFactor = BASE_BRIGHTNESS + localIntensity * (1.0f - BASE_BRIGHTNESS);
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightnessFactor));

        // Very subtle white only at centre on strong hits
        if (dist01 < 0.12f && localIntensity > 0.7f) {
            const uint8_t white = floatToByte((localIntensity - 0.7f) * 3.3f * 0.12f);
            ColourUtil::addWhiteSaturating(c, white);
        }

        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseBreatheEffect
uint8_t BeatPulseBreatheEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseBreatheEffectParameters) / sizeof(kBeatPulseBreatheEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseBreatheEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseBreatheEffectParameters[index];
}

bool BeatPulseBreatheEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_breathe_effect_speed_scale") == 0) {
        gBeatPulseBreatheEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_breathe_effect_output_gain") == 0) {
        gBeatPulseBreatheEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_breathe_effect_centre_bias") == 0) {
        gBeatPulseBreatheEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseBreatheEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_breathe_effect_speed_scale") == 0) return gBeatPulseBreatheEffectSpeedScale;
    if (strcmp(name, "beat_pulse_breathe_effect_output_gain") == 0) return gBeatPulseBreatheEffectOutputGain;
    if (strcmp(name, "beat_pulse_breathe_effect_centre_bias") == 0) return gBeatPulseBreatheEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseBreatheEffect

void BeatPulseBreatheEffect::cleanup() {}

// ============================================================================
// Metadata
// ============================================================================

const plugins::EffectMetadata& BeatPulseBreatheEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Breathe)",
        "Organic whole-strip breathing pulse with warm centre-weighted glow",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

// ============================================================================
// Parameters (none for this effect)
// ============================================================================


} // namespace lightwaveos::effects::ieffect
