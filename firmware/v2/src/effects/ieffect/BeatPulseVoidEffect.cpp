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
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

// =============================================================================
// Visual Identity Constants
// =============================================================================

constexpr float RING_WIDTH = 0.10f;        // Ring half-width in dist01 space
constexpr float RING_CENTRE_FACTOR = 0.6f; // Ring contracts from 0.6 -> 0
constexpr float DECAY_MS = 280.0f;         // Slightly faster decay for punch
constexpr float CORE_WHITE_FACTOR = 0.6f;  // Aggressive white in ring core

bool BeatPulseVoidEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseVoidEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // VOID DETONATION: Hard-edged ring in absolute darkness.
    // Strip is TRUE BLACK between beats. Ring has white-hot core,
    // saturated palette colour at edges. Colour travels with ring.
    // =========================================================================

    // --- Beat source ---
    bool beatTick = false;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
    } else {
        const uint32_t nowMs = ctx.totalTimeMs;
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, m_fallbackBpm);
        if (m_lastBeatTimeMs == 0 || (nowMs - m_lastBeatTimeMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastBeatTimeMs = nowMs;
        }
    }

    // --- Slam to 1.0 on beat ---
    if (beatTick) {
        m_beatIntensity = 1.0f;
    }

    // --- DT-correct exponential decay ---
    const float dt = ctx.getSafeDeltaSeconds();
    const float decayRate = 1000.0f / DECAY_MS;  // Decay constant
    m_beatIntensity *= expf(-decayRate * dt);
    if (m_beatIntensity < 0.001f) {
        m_beatIntensity = 0.0f;
    }

    // --- Ring position (contracting inward) ---
    const float ringCentre = RING_CENTRE_FACTOR * m_beatIntensity;

    // --- Palette index based on ring position (colour travels with ring) ---
    const uint8_t paletteIdx = floatToByte(ringCentre);

    // --- Render: detonation in the void ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Hard-edged ring profile
        const float diff = fabsf(dist01 - ringCentre);
        float hit = RingProfile::hardEdge(diff, RING_WIDTH, 0.012f);
        hit *= m_beatIntensity;

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

uint8_t BeatPulseVoidEffect::getParameterCount() const { return 0; }

const plugins::EffectParameter* BeatPulseVoidEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseVoidEffect::setParameter(const char* name, float value) {
    (void)name; (void)value;
    return false;
}

float BeatPulseVoidEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
