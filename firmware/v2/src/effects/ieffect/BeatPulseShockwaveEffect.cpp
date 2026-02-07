/**
 * @file BeatPulseShockwaveEffect.cpp
 * @brief Beat Pulse (Shockwave) - HTML PARITY implementation
 *
 * VISUAL IDENTITY:
 * Single ring expanding OUTWARD from centre (or INWARD from edges) with
 * AMPLITUDE-DRIVEN motion. Same HTML core maths as Stack, different direction.
 *
 * HTML PARITY (LOCKED):
 * - beatIntensity slams to 1.0 on beat, decays *= 0.94^(dt*60)
 * - Ring position inverted for outward vs inward:
 *   - Outward: ringPos = 1 - ringCentre (starts at edge, moves toward centre)
 *   - Inward: ringPos = ringCentre (starts at centre, moves toward edge)
 * - Triangle profile: waveHit = 1 - min(1, abs(dist - ringPos) * 3)
 * - intensity = max(0, waveHit) * beatIntensity
 * - brightness = 0.5 + intensity * 0.5
 * - whiteMix = intensity * 0.3
 */

#include "BeatPulseShockwaveEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

BeatPulseShockwaveEffect::BeatPulseShockwaveEffect(bool inward)
    : m_inward(inward)
    , m_meta(
          inward ? "Beat Pulse (Shockwave In)" : "Beat Pulse (Shockwave)",
          inward ? "HTML parity: amplitude-driven ring edge→centre" : "HTML parity: amplitude-driven ring centre→edge",
          plugins::EffectCategory::PARTY,
          1,
          "LightwaveOS")
{
}

bool BeatPulseShockwaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseShockwaveEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // HTML PARITY: Amplitude-driven ring position, locked constants.
    // Direction: outward (centre→edge) or inward (edge→centre).
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

    // --- Ring position ---
    // HTML ringCentre is 0..0.6 as beatIntensity decays 1.0..0.0
    // For OUTWARD: ring starts at centre (0) and expands to edge (0.6)
    //              So we use ringCentre directly
    // For INWARD:  ring starts at edge (~0.6) and contracts to centre (0)
    //              So we invert: 1.0 - ringCentre → 0.4..1.0
    const float htmlCentre = BeatPulseHTML::ringCentre01(m_beatIntensity);
    const float ringPos = m_inward ? htmlCentre : (1.0f - htmlCentre);

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Distance 0 at centre, ~1 at edges
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // HTML parity triangle profile (slope = 3)
        const float diff = fabsf(dist01 - ringPos);
        const float waveHit = 1.0f - fminf(1.0f, diff * 3.0f);
        const float hit = fmaxf(0.0f, waveHit);
        const float intensity = hit * m_beatIntensity;

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

void BeatPulseShockwaveEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveEffect::getMetadata() const {
    return m_meta;
}

uint8_t BeatPulseShockwaveEffect::getParameterCount() const {
    return 0;
}

const plugins::EffectParameter* BeatPulseShockwaveEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseShockwaveEffect::setParameter(const char* name, float value) {
    (void)name;
    (void)value;
    return false;
}

float BeatPulseShockwaveEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
