/**
 * @file BeatPulseVoidEffect.cpp
 * @brief Beat Pulse (Void) - Hard detonation in absolute darkness
 *
 * Visual identity: Maximum contrast effect. TRUE BLACK between beats with
 * a crisp, hard-edged ring that has a white-hot core and saturated palette
 * colour at the edges. The colour travels WITH the ring (palette indexed
 * by ring position, not LED position).
 *
 * This is the most dramatic effect in the Beat Pulse family.
 */

#include "BeatPulseVoidEffect.h"
#include "../CoreEffects.h"
#include "AudioReactivePolicy.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseVoidEffect
namespace {
constexpr float kBeatPulseVoidEffectSpeedScale = 1.0f;
constexpr float kBeatPulseVoidEffectOutputGain = 1.0f;
constexpr float kBeatPulseVoidEffectCentreBias = 1.0f;

float gBeatPulseVoidEffectSpeedScale = kBeatPulseVoidEffectSpeedScale;
float gBeatPulseVoidEffectOutputGain = kBeatPulseVoidEffectOutputGain;
float gBeatPulseVoidEffectCentreBias = kBeatPulseVoidEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseVoidEffectParameters[] = {
    {"beat_pulse_void_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseVoidEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_void_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseVoidEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_void_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseVoidEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseVoidEffect

namespace lightwaveos::effects::ieffect {

// =============================================================================
// Visual Identity Constants
// =============================================================================

constexpr float RING_WIDTH = 0.10f;        // Ring half-width in dist01 space
constexpr float RING_CENTRE_FACTOR = 0.6f; // Ring contracts from 0.6 -> 0
constexpr float DECAY_MS = 280.0f;         // Slightly faster decay for punch
constexpr float CORE_WHITE_FACTOR = 0.6f;  // Aggressive white in ring core
bool  BeatPulseVoidEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseVoidEffect
    gBeatPulseVoidEffectSpeedScale = kBeatPulseVoidEffectSpeedScale;
    gBeatPulseVoidEffectOutputGain = kBeatPulseVoidEffectOutputGain;
    gBeatPulseVoidEffectCentreBias = kBeatPulseVoidEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseVoidEffect

    BeatPulseCore::reset(m_state, 128.0f);
    return true;
}

void BeatPulseVoidEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // VOID DETONATION: Hard-edged ring in absolute darkness.
    // Strip is TRUE BLACK between beats. Ring has white-hot core,
    // saturated palette colour at edges. Colour travels with ring.
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = AudioReactivePolicy::audioBeatTick(ctx, m_state.fallbackBpm, m_state.lastBeatMs);

    // --- Slam to 1.0 on beat ---
    if (beatTick) {
        m_state.beatIntensity = 1.0f;
    }

    // --- DT-correct exponential decay ---
    const float dt = ctx.getSafeRawDeltaSeconds();
    const float decayRate = 1000.0f / DECAY_MS;  // Decay constant
    m_state.beatIntensity *= expf(-decayRate * dt);
    if (m_state.beatIntensity < 0.001f) {
        m_state.beatIntensity = 0.0f;
    }

    // --- Ring position (contracting inward) ---
    const float ringCentre = RING_CENTRE_FACTOR * m_state.beatIntensity;

    // --- Palette index based on ring position (colour travels with ring) ---
    const uint8_t paletteIdx = floatToByte(ringCentre);

    // --- Render: detonation in the void ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Hard-edged ring profile
        const float diff = fabsf(dist01 - ringCentre);
        float hit = RingProfile::hardEdge(diff, RING_WIDTH, 0.012f);
        hit *= m_state.beatIntensity;

        // TRUE BLACK outside ring - maximum contrast
        if (hit < 0.01f) {
            SET_CENTER_PAIR(ctx, dist, CRGB(0, 0, 0));
            continue;
        }

        // Get colour from palette (indexed by ring position, not LED position)
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, hit));

        // White-hot CORE (centre of ring is desaturated towards white)
        const float ringLocalPos = 1.0f - diff / RING_WIDTH;  // 1.0 at ring centre, 0.0 at edge
        if (ringLocalPos > 0.5f) {
            const float coreWhite = (ringLocalPos - 0.5f) * 2.0f * CORE_WHITE_FACTOR * hit;
            ColourUtil::addWhiteSaturating(c, floatToByte(coreWhite * static_cast<float>(ctx.brightness) / 255.0f));
        }

        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseVoidEffect
uint8_t BeatPulseVoidEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseVoidEffectParameters) / sizeof(kBeatPulseVoidEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseVoidEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseVoidEffectParameters[index];
}

bool BeatPulseVoidEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_void_effect_speed_scale") == 0) {
        gBeatPulseVoidEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_void_effect_output_gain") == 0) {
        gBeatPulseVoidEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_void_effect_centre_bias") == 0) {
        gBeatPulseVoidEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseVoidEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_void_effect_speed_scale") == 0) return gBeatPulseVoidEffectSpeedScale;
    if (strcmp(name, "beat_pulse_void_effect_output_gain") == 0) return gBeatPulseVoidEffectOutputGain;
    if (strcmp(name, "beat_pulse_void_effect_centre_bias") == 0) return gBeatPulseVoidEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseVoidEffect

void BeatPulseVoidEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseVoidEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Void)",
        "Hard detonation in absolute darkness: white-hot core, crisp edges, true black background",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}


} // namespace lightwaveos::effects::ieffect
