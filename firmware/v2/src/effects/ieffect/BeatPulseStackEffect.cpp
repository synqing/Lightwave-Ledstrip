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
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

bool BeatPulseStackEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseStackEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // HTML PARITY: Amplitude-driven ring position, locked constants.
    // =========================================================================

    // --- Beat source ---
    bool beatTick = false;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
    } else {
        // Fallback metronome (128 BPM)
        const uint32_t nowMs = ctx.totalTimeMs;
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, m_fallbackBpm);
        if (m_lastBeatTimeMs == 0 ||
            (nowMs - m_lastBeatTimeMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastBeatTimeMs = nowMs;
        }
    }

    // --- Update beatIntensity using HTML parity maths ---
    const float dt = ctx.getSafeDeltaSeconds();
    BeatPulseHTML::updateBeatIntensity(m_beatIntensity, beatTick, dt);

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Distance 0 at centre, ~1 at edges
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // HTML parity: intensity at this distance
        const float intensity = BeatPulseHTML::intensityAtDist(dist01, m_beatIntensity);

        // HTML parity: brightness = 0.5 + intensity * 0.5
        const float brightFactor = BeatPulseHTML::brightnessFactor(intensity);

        // Palette colour by distance
        const uint8_t paletteIdx = floatToByte(dist01);
        CRGB c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, brightFactor));

        // HTML parity: white mix = intensity * 0.3
        const float whiteMixVal = BeatPulseHTML::whiteMix(intensity);
        ColourUtil::addWhiteSaturating(c, floatToByte(whiteMixVal));

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

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

uint8_t BeatPulseStackEffect::getParameterCount() const {
    return 0;
}

const plugins::EffectParameter* BeatPulseStackEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseStackEffect::setParameter(const char* name, float value) {
    (void)name;
    (void)value;
    return false;
}

float BeatPulseStackEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
