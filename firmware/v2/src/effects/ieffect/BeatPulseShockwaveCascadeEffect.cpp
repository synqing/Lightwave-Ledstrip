/**
 * @file BeatPulseShockwaveCascadeEffect.cpp
 * @brief Beat Pulse (Shockwave Cascade) - Triple pressure wave with HTML shading
 *
 * Visual identity: Primary shockwave + 2 trailing echo fronts expanding outward.
 * Like multiple pressure waves from a single detonation. Uses HTML PARITY shading
 * spine (brightness = 0.5 + intensity * 0.5, whiteMix = intensity * 0.3).
 *
 * Key characteristics:
 *  - 3 rings total: primary + 2 echoes, all expanding OUTWARD
 *  - Echoes spawn with time delay (staggered birth)
 *  - Echoes are progressively THINNER and DIMMER
 *  - Colour by distance through palette
 *  - HTML PARITY shading for all rings
 *
 * Effect ID: 116
 */

#include "BeatPulseShockwaveCascadeEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

bool BeatPulseShockwaveCascadeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
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

    // --- Update beatIntensity using HTML parity maths ---
    const float dt = ctx.getSafeDeltaSeconds();
    BeatPulseHTML::updateBeatIntensity(m_beatIntensity, beatTick, dt);

    // --- Ring configuration ---
    // Primary ring travels centre→edge; echoes trail behind with offsets
    const float htmlCentre = BeatPulseHTML::ringCentre01(m_beatIntensity);
    const float primaryPos = 1.0f - htmlCentre;  // Outward: 0→1 as beatIntensity decays

    // Echo positions trail behind the primary
    constexpr float ECHO1_OFFSET = 0.12f;
    constexpr float ECHO2_OFFSET = 0.24f;
    const float echo1Pos = clamp01(primaryPos - ECHO1_OFFSET);
    const float echo2Pos = clamp01(primaryPos - ECHO2_OFFSET);

    // Ring gains (primary = full, echoes progressively dimmer)
    constexpr float RING_GAINS[] = {1.0f, 0.45f, 0.22f};
    const float ringPositions[] = {primaryPos, echo1Pos, echo2Pos};

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        float maxIntensity = 0.0f;
        float primaryHit = 0.0f;

        // Process all 3 rings: primary + 2 echoes
        for (uint8_t e = 0; e < 3; ++e) {
            // Only show echo if primary has advanced far enough
            if (e > 0) {
                const float minAdvance = (e == 1) ? ECHO1_OFFSET : ECHO2_OFFSET;
                if (primaryPos < minAdvance * 1.5f) continue;
            }

            // HTML parity triangle profile (slope = 3)
            const float diff = fabsf(dist01 - ringPositions[e]);
            const float waveHit = 1.0f - fminf(1.0f, diff * 3.0f);
            const float hit = fmaxf(0.0f, waveHit) * RING_GAINS[e] * m_beatIntensity;

            if (hit > maxIntensity) {
                maxIntensity = hit;
            }

            if (e == 0) primaryHit = hit;
        }

        // HTML parity: brightness = 0.5 + intensity * 0.5
        const float brightFactor = BeatPulseHTML::brightnessFactor(maxIntensity);

        // Palette colour by distance
        const uint8_t paletteIdx = floatToByte(dist01);
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightFactor));

        // HTML parity: white mix = intensity * 0.3 (primary only)
        const float whiteMixVal = BeatPulseHTML::whiteMix(primaryHit);
        ColourUtil::addWhiteSaturating(c, floatToByte(whiteMixVal));

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

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

uint8_t BeatPulseShockwaveCascadeEffect::getParameterCount() const { return 0; }

const plugins::EffectParameter* BeatPulseShockwaveCascadeEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseShockwaveCascadeEffect::setParameter(const char* name, float value) {
    (void)name; (void)value;
    return false;
}

float BeatPulseShockwaveCascadeEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
