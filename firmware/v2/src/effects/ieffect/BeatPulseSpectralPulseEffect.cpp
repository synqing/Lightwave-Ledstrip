/**
 * @file BeatPulseSpectralPulseEffect.cpp
 * @brief Beat Pulse (Spectral Pulse) - Three-zone frequency pulse
 *
 * Visual identity: Three FIXED ZONES pulsing with frequency bands.
 * Simpler than Spectral - no sparkle, no ring movement.
 * Clean spatial separation. Readable at high BPM.
 *
 * Zone layout (hard divisions, NO crossfade):
 *   - Inner (0.00 - 0.28): Treble - high-frequency subtle flicker (4-5Hz)
 *   - Middle (0.28 - 0.62): Mid - punchy with white on strong hits
 *   - Outer (0.62 - 1.00): Bass - warm saturated, NO white (stays colourful)
 *
 * See BeatPulseSpectralPulseEffect.h for design rationale.
 */

#include "BeatPulseSpectralPulseEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

// ==================== Zone Boundaries (Hard Divisions) ====================
constexpr float TREBLE_END = 0.28f;     // 0.00 - 0.28
constexpr float MID_END = 0.62f;        // 0.28 - 0.62
// Bass: 0.62 - 1.00

// ==================== Band Smoothing ====================
constexpr float BASS_SMOOTH = 0.85f;
constexpr float MID_SMOOTH = 0.88f;
constexpr float TREBLE_SMOOTH = 0.92f;

// ==================== Treble Flicker ====================
// Creates shimmer at ~4Hz (0.028 radians per ms at 120fps)
constexpr float FLICKER_SPEED = 0.028f;

} // namespace

bool BeatPulseSpectralPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_smoothBass = 0.0f;
    m_smoothMid = 0.0f;
    m_smoothTreble = 0.0f;
    m_beatBoost = 0.0f;
    m_fallbackBpm = 128.0f;
    m_lastFallbackBeatMs = 0;
    m_fallbackPhase = 0.0f;
    return true;
}

void BeatPulseSpectralPulseEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // THREE-ZONE SPECTRAL PULSE
    // Inner = treble (flicker), Middle = mid (white punch), Outer = bass (saturated)
    // Hard zone boundaries - no crossfade, clean visual separation
    // =========================================================================

    const float dt = ctx.getSafeDeltaSeconds();

    // --- Read frequency bands ---
    float rawBass = 0.0f;
    float rawMid = 0.0f;
    float rawTreble = 0.0f;

    if (ctx.audio.available) {
        rawBass = clamp01(ctx.audio.bass());
        rawMid = clamp01(ctx.audio.mid());
        rawTreble = clamp01(ctx.audio.treble());
    } else {
        // Fallback: gentle simulated spectrum
        m_fallbackPhase += dt * 2.0f;
        if (m_fallbackPhase > 6.28318f) m_fallbackPhase -= 6.28318f;
        rawBass = 0.4f + 0.3f * sinf(m_fallbackPhase);
        rawMid = 0.3f + 0.2f * sinf(m_fallbackPhase * 1.5f);
        rawTreble = 0.2f + 0.15f * sinf(m_fallbackPhase * 2.5f);
    }

    // --- Smooth bands (dt-correct exponential) ---
    const float bassSmooth = 1.0f - powf(BASS_SMOOTH, dt * 60.0f);
    const float midSmooth = 1.0f - powf(MID_SMOOTH, dt * 60.0f);
    const float trebleSmooth = 1.0f - powf(TREBLE_SMOOTH, dt * 60.0f);

    m_smoothBass += (rawBass - m_smoothBass) * bassSmooth;
    m_smoothMid += (rawMid - m_smoothMid) * midSmooth;
    m_smoothTreble += (rawTreble - m_smoothTreble) * trebleSmooth;

    // --- Render: Three hard-divided zones ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        CRGB c;

        if (dist01 < TREBLE_END) {
            // === TREBLE ZONE: Cool, high-frequency flicker ===
            const float zonePos = dist01 / TREBLE_END;
            float intensity = m_smoothTreble;

            // Subtle flicker (4Hz modulation)
            intensity *= 0.75f + 0.25f * sinf(static_cast<float>(ctx.totalTimeMs) * FLICKER_SPEED + static_cast<float>(dist) * 0.4f);

            // Cool palette region (high indices)
            const uint8_t paletteIdx = 190 + floatToByte(zonePos * 0.25f);
            const float bright = 0.08f + intensity * 0.92f;
            c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, bright));

        } else if (dist01 < MID_END) {
            // === MID ZONE: Neutral, punchy with white ===
            const float zonePos = (dist01 - TREBLE_END) / (MID_END - TREBLE_END);
            const float intensity = m_smoothMid;

            // Neutral palette region
            const uint8_t paletteIdx = 100 + floatToByte(zonePos * 0.2f);
            const float bright = 0.06f + intensity * 0.94f;
            c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, bright));

            // WHITE PUNCH on mid hits (this is the "crack")
            if (intensity > 0.55f) {
                const float whitePunch = (intensity - 0.55f) * 2.2f * 0.35f;
                ColourUtil::addWhiteSaturating(c, floatToByte(whitePunch));
            }

        } else {
            // === BASS ZONE: Warm saturated, NO white ===
            const float zonePos = (dist01 - MID_END) / (1.0f - MID_END);
            const float intensity = m_smoothBass;

            // Warm palette region (low indices, inverted so outer = deepest)
            const uint8_t paletteIdx = 50 - floatToByte(zonePos * 0.2f);
            const float bright = 0.05f + intensity * 0.95f;
            c = ctx.palette.getColor(paletteIdx, scaleBrightness(ctx.brightness, bright));

            // NO white in bass - stays warm and saturated
        }

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseSpectralPulseEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseSpectralPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Spectral Pulse)",
        "Three-zone frequency pulse: flickering treble, punchy mid, warm bass",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

uint8_t BeatPulseSpectralPulseEffect::getParameterCount() const { return 0; }

const plugins::EffectParameter* BeatPulseSpectralPulseEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseSpectralPulseEffect::setParameter(const char* name, float value) {
    (void)name; (void)value;
    return false;
}

float BeatPulseSpectralPulseEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
