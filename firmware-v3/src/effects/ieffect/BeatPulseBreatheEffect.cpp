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

bool BeatPulseBreatheEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
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

uint8_t BeatPulseBreatheEffect::getParameterCount() const { return 0; }

const plugins::EffectParameter* BeatPulseBreatheEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseBreatheEffect::setParameter(const char* name, float value) {
    (void)name; (void)value;
    return false;
}

float BeatPulseBreatheEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
