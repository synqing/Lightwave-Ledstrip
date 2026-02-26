/**
 * @file BeatPulseRippleEffect.cpp
 * @brief Beat Pulse (Ripple) - Cascading water ripples with interference patterns
 *
 * VISUAL IDENTITY: Stone dropped in water. Multiple rings propagate inward,
 * INTERFERE where they overlap, creating organic complexity. Rapid beats
 * produce cascading concentric ripples.
 *
 * Key features:
 * - Up to 3 simultaneous rings (ring buffer)
 * - ADDITIVE blending with soft accumulation - rings layer and interfere
 * - Each successive ring is dimmer (1.0, 0.55, 0.30)
 * - GLOW profile (core + halo) for water-like spread
 * - Colour is weighted average of ring positions (travels with ripples)
 * - White sparkle at interference peaks (where rings overlap)
 *
 * See BeatPulseRippleEffect.h for design rationale.
 */

#include "BeatPulseRippleEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseRippleEffect
namespace {
constexpr float kBeatPulseRippleEffectSpeedScale = 1.0f;
constexpr float kBeatPulseRippleEffectOutputGain = 1.0f;
constexpr float kBeatPulseRippleEffectCentreBias = 1.0f;

float gBeatPulseRippleEffectSpeedScale = kBeatPulseRippleEffectSpeedScale;
float gBeatPulseRippleEffectOutputGain = kBeatPulseRippleEffectOutputGain;
float gBeatPulseRippleEffectCentreBias = kBeatPulseRippleEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseRippleEffectParameters[] = {
    {"beat_pulse_ripple_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseRippleEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_ripple_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseRippleEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_ripple_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseRippleEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseRippleEffect

namespace lightwaveos::effects::ieffect {

// ============================================================================
// Constants
// ============================================================================

constexpr uint8_t MAX_RINGS = 3;
constexpr float TRAVEL_MS = 450.0f;           // Time for ring to travel edge to centre
constexpr float DECAY_MS = 380.0f;            // Exponential envelope decay
constexpr float CORE_WIDTH = 0.06f;           // Glow profile core width
constexpr float HALO_WIDTH = 0.08f;           // Glow profile halo width
constexpr float RING_GAINS[] = {1.0f, 0.55f, 0.30f};  // Successive dimming
constexpr float INTERFERENCE_THRESHOLD = 0.65f;
constexpr float BASE_BRIGHTNESS = 0.06f;      // Dim background

// ============================================================================
// Implementation
// ============================================================================
bool  BeatPulseRippleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseRippleEffect
    gBeatPulseRippleEffectSpeedScale = kBeatPulseRippleEffectSpeedScale;
    gBeatPulseRippleEffectOutputGain = kBeatPulseRippleEffectOutputGain;
    gBeatPulseRippleEffectCentreBias = kBeatPulseRippleEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseRippleEffect

    for (uint8_t i = 0; i < MAX_RINGS; ++i) {
        m_rings[i].birthMs = 0;
        m_rings[i].active = false;
    }
    m_nextSlot = 0;
    m_fallbackBpm = 128.0f;
    m_lastFallbackBeatMs = 0;
    return true;
}

void BeatPulseRippleEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // RIPPLE: Up to 3 rings alive, each contracting edge->centre.
    // Rings INTERFERE where they overlap creating organic complexity.
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastFallbackBeatMs);

    const uint32_t nowMs = ctx.rawTotalTimeMs;

    // --- Spawn new ring on beat ---
    if (beatTick) {
        m_rings[m_nextSlot].birthMs = nowMs;
        m_rings[m_nextSlot].active = true;
        m_nextSlot = (m_nextSlot + 1) % MAX_RINGS;
    }

    // --- Compute max life for ring expiry ---
    const float maxLifeMs = TRAVEL_MS + 2.5f * DECAY_MS;

    // --- Pre-compute ring states ---
    struct RingState {
        float pos;      // Position [0, 1] where 0 = centre, 1 = edge
        float env;      // Envelope (decay)
        float gain;     // Successive dimming gain
        bool alive;
    };
    RingState ringStates[MAX_RINGS];

    // Calculate age order to determine successive dimming
    // Younger rings get lower gain indices (dimmer)
    struct AgeSlot {
        uint8_t slot;
        float ageMs;
    };
    AgeSlot ageOrder[MAX_RINGS];
    uint8_t activeCount = 0;

    for (uint8_t r = 0; r < MAX_RINGS; ++r) {
        ringStates[r].alive = false;

        if (!m_rings[r].active) continue;

        const float ageMs = static_cast<float>(nowMs - m_rings[r].birthMs);
        if (ageMs > maxLifeMs) {
            m_rings[r].active = false;
            continue;
        }

        ringStates[r].alive = true;
        ringStates[r].pos = 1.0f - clamp01(ageMs / TRAVEL_MS);  // Contracting inward
        ringStates[r].env = clamp01(expf(-ageMs / DECAY_MS));

        // Track for age-based gain assignment
        ageOrder[activeCount].slot = r;
        ageOrder[activeCount].ageMs = ageMs;
        activeCount++;
    }

    // Sort by age (oldest first = gets highest gain)
    // Simple insertion sort for 3 elements
    for (uint8_t i = 1; i < activeCount; ++i) {
        AgeSlot key = ageOrder[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && ageOrder[j].ageMs < key.ageMs) {
            ageOrder[j + 1] = ageOrder[j];
            j--;
        }
        ageOrder[j + 1] = key;
    }

    // Assign gains: oldest (index 0) gets RING_GAINS[0] = 1.0, etc.
    for (uint8_t i = 0; i < activeCount; ++i) {
        uint8_t slot = ageOrder[i].slot;
        ringStates[slot].gain = RING_GAINS[i];
    }

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Accumulate intensity from all rings (additive blending)
        float accumulatedIntensity = 0.0f;
        float paletteWeightSum = 0.0f;
        float palettePositionSum = 0.0f;

        for (uint8_t r = 0; r < MAX_RINGS; ++r) {
            if (!ringStates[r].alive) continue;

            const float diff = fabsf(dist01 - ringStates[r].pos);

            // GLOW profile (core + soft halo) for water-like spread
            float hit = RingProfile::glow(diff, CORE_WIDTH, HALO_WIDTH);
            hit *= ringStates[r].env * ringStates[r].gain;

            // ADDITIVE accumulation
            accumulatedIntensity += hit;

            // Weighted palette contribution (colour travels with ripples)
            palettePositionSum += ringStates[r].pos * hit;
            paletteWeightSum += hit;
        }

        // Soft accumulation (handles multiple layers gracefully)
        const float intensity = BlendMode::softAccumulate(accumulatedIntensity, 1.8f);

        // Final brightness: dim base + intensity-driven boost
        const float brightnessFactor = clamp01(BASE_BRIGHTNESS + intensity * (1.0f - BASE_BRIGHTNESS));

        // Weighted average palette position (colour travels with ripples)
        const float palPos = (paletteWeightSum > 0.01f)
            ? palettePositionSum / paletteWeightSum
            : dist01;
        const uint8_t paletteIdx = floatToByte(palPos);

        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightnessFactor));

        // White sparkle at INTERFERENCE peaks (where rings overlap)
        if (intensity > INTERFERENCE_THRESHOLD) {
            const float sparkle = (intensity - INTERFERENCE_THRESHOLD) / (1.0f - INTERFERENCE_THRESHOLD);
            ColourUtil::addWhiteSaturating(c, floatToByte(sparkle * 0.35f));
        }

        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseRippleEffect
uint8_t BeatPulseRippleEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseRippleEffectParameters) / sizeof(kBeatPulseRippleEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseRippleEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseRippleEffectParameters[index];
}

bool BeatPulseRippleEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_ripple_effect_speed_scale") == 0) {
        gBeatPulseRippleEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_ripple_effect_output_gain") == 0) {
        gBeatPulseRippleEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_ripple_effect_centre_bias") == 0) {
        gBeatPulseRippleEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseRippleEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_ripple_effect_speed_scale") == 0) return gBeatPulseRippleEffectSpeedScale;
    if (strcmp(name, "beat_pulse_ripple_effect_output_gain") == 0) return gBeatPulseRippleEffectOutputGain;
    if (strcmp(name, "beat_pulse_ripple_effect_centre_bias") == 0) return gBeatPulseRippleEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseRippleEffect

void BeatPulseRippleEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseRippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Ripple)",
        "Cascading water ripples with interference patterns",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}


} // namespace lightwaveos::effects::ieffect
