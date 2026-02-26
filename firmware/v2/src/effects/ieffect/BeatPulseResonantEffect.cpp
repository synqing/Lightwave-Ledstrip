/**
 * @file BeatPulseResonantEffect.cpp
 * @brief Beat Pulse (Resonant) - Dual-ring anatomy: white attack snap over warm resonant body
 *
 * VISUAL IDENTITY:
 * You see the ANATOMY of the hit. Two distinct rings: thin bright ATTACK snap
 * (white flash) leads, wide warm BODY thud (saturated colour) follows.
 * Both contract inward from edge to centre.
 *
 * The attack ring is THIN, HARD-edged, nearly WHITE, with FAST travel and decay.
 * The body ring is WIDE, GAUSSIAN soft-edged, SATURATED palette colour, with
 * SLOWER travel and decay. Body colour travels WITH the ring (palette indexed
 * by body position, not LED position).
 *
 * Both rings are visible simultaneously via ADDITIVE blending.
 */

#include "BeatPulseResonantEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:BeatPulseResonantEffect
namespace {
constexpr float kBeatPulseResonantEffectSpeedScale = 1.0f;
constexpr float kBeatPulseResonantEffectOutputGain = 1.0f;
constexpr float kBeatPulseResonantEffectCentreBias = 1.0f;

float gBeatPulseResonantEffectSpeedScale = kBeatPulseResonantEffectSpeedScale;
float gBeatPulseResonantEffectOutputGain = kBeatPulseResonantEffectOutputGain;
float gBeatPulseResonantEffectCentreBias = kBeatPulseResonantEffectCentreBias;

const lightwaveos::plugins::EffectParameter kBeatPulseResonantEffectParameters[] = {
    {"beat_pulse_resonant_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kBeatPulseResonantEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"beat_pulse_resonant_effect_output_gain", "Output Gain", 0.25f, 2.0f, kBeatPulseResonantEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"beat_pulse_resonant_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kBeatPulseResonantEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:BeatPulseResonantEffect

namespace lightwaveos::effects::ieffect {

// ============================================================================
// Constants: Dual-ring timing and shape
// ============================================================================

// Attack ring: percussive snap (thin, hard, white, fast)
constexpr float ATTACK_TRAVEL_MS = 280.0f;   // Fast travel edge->centre
constexpr float ATTACK_DECAY_MS = 150.0f;    // Quick decay
constexpr float ATTACK_WIDTH = 0.06f;        // Thin ring
constexpr float ATTACK_SOFTNESS = 0.012f;    // Hard edge with minimal AA

// Body ring: resonant thud (wide, soft, saturated, slower)
constexpr float BODY_TRAVEL_MS = 480.0f;     // Slower travel
constexpr float BODY_DECAY_MS = 380.0f;      // Longer decay
constexpr float BODY_SIGMA = 0.14f;          // Gaussian sigma (wide, soft)

// Colour treatment
constexpr float ATTACK_WHITE = 0.85f;        // Attack is nearly white (desaturated)

// ============================================================================
// Effect Implementation
// ============================================================================
bool  BeatPulseResonantEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:BeatPulseResonantEffect
    gBeatPulseResonantEffectSpeedScale = kBeatPulseResonantEffectSpeedScale;
    gBeatPulseResonantEffectOutputGain = kBeatPulseResonantEffectOutputGain;
    gBeatPulseResonantEffectCentreBias = kBeatPulseResonantEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:BeatPulseResonantEffect

    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseResonantEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // RESONANT: See the ANATOMY of the hit.
    // Attack ring = white flash snap. Body ring = warm resonant thud.
    // Both contract inward. Both visible via additive blend.
    // =========================================================================

    // --- Beat source ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastBeatTimeMs);

    const uint32_t nowMs = ctx.rawTotalTimeMs;

    // --- Slam to 1.0 on beat ---
    if (beatTick) {
        m_beatIntensity = 1.0f;
        m_lastBeatTimeMs = nowMs;
    }

    // --- Age since last beat ---
    float ageMs = 999999.0f;
    if (m_lastBeatTimeMs != 0) {
        ageMs = static_cast<float>(nowMs - m_lastBeatTimeMs);
    }

    // --- Two ring positions (both contracting inward: edge -> centre) ---
    const float attackPos = 1.0f - clamp01(ageMs / ATTACK_TRAVEL_MS);
    const float bodyPos = 1.0f - clamp01(ageMs / BODY_TRAVEL_MS);

    // --- Separate envelopes ---
    const float attackEnv = expf(-ageMs / ATTACK_DECAY_MS) * m_beatIntensity;
    const float bodyEnv = expf(-ageMs / BODY_DECAY_MS) * m_beatIntensity;

    // --- Body colour: palette indexed by BODY RING POSITION (travels with thud) ---
    const uint8_t bodyPaletteIdx = floatToByte(bodyPos);

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Attack ring: HARD EDGE (thin, sharp snap)
        const float attackDiff = fabsf(dist01 - attackPos);
        const float attackHit = RingProfile::hardEdge(attackDiff, ATTACK_WIDTH, ATTACK_SOFTNESS) * attackEnv;

        // Body ring: GAUSSIAN (wide, soft, warm)
        const float bodyDiff = fabsf(dist01 - bodyPos);
        const float bodyHit = RingProfile::gaussian(bodyDiff, BODY_SIGMA) * bodyEnv;

        // Body colour: saturated palette colour at body ring position
        CRGB bodyColor = ctx.palette.getColor(bodyPaletteIdx, scaleBrightness(ctx.brightness, bodyHit));

        // Attack colour: nearly WHITE (desaturated flash)
        const uint8_t attackBrightness = scaleBrightness(ctx.brightness, attackHit * ATTACK_WHITE);
        CRGB attackColor = CRGB(attackBrightness, attackBrightness, attackBrightness);

        // ADDITIVE blend: attack over body (both visible simultaneously)
        CRGB c = ColourUtil::additive(bodyColor, attackColor);

        SET_CENTER_PAIR(ctx, dist, c);
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:BeatPulseResonantEffect
uint8_t BeatPulseResonantEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kBeatPulseResonantEffectParameters) / sizeof(kBeatPulseResonantEffectParameters[0]));
}

const plugins::EffectParameter* BeatPulseResonantEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kBeatPulseResonantEffectParameters[index];
}

bool BeatPulseResonantEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "beat_pulse_resonant_effect_speed_scale") == 0) {
        gBeatPulseResonantEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_resonant_effect_output_gain") == 0) {
        gBeatPulseResonantEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "beat_pulse_resonant_effect_centre_bias") == 0) {
        gBeatPulseResonantEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float BeatPulseResonantEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "beat_pulse_resonant_effect_speed_scale") == 0) return gBeatPulseResonantEffectSpeedScale;
    if (strcmp(name, "beat_pulse_resonant_effect_output_gain") == 0) return gBeatPulseResonantEffectOutputGain;
    if (strcmp(name, "beat_pulse_resonant_effect_centre_bias") == 0) return gBeatPulseResonantEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:BeatPulseResonantEffect

void BeatPulseResonantEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseResonantEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Resonant)",
        "Dual-ring anatomy: white attack snap over warm resonant body",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}


} // namespace lightwaveos::effects::ieffect
