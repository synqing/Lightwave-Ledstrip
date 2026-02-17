/**
 * @file BeatPulseSpectralPulseEffect.cpp
 * @brief Beat Pulse (Spectral Pulse) - Three-zone frequency pulse
 *
 * Visual identity: Three FIXED ZONES pulsing with frequency bands.
 * Simpler than Spectral - no sparkle, no ring movement.
 * Clean spatial separation. Readable at high BPM.
 *
 * Zone layout (soft crossfades, width 0.08):
 *   - Inner (0.00 - 0.33): Treble - high-frequency subtle flicker
 *   - Middle (0.33 - 0.66): Mid - neutral pulse
 *   - Outer (0.66 - 1.00): Bass - warm saturated (stays colourful)
 *
 * See BeatPulseSpectralPulseEffect.h for design rationale.
 */

#include "BeatPulseSpectralPulseEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

// ==================== Zone Boundaries (Soft Crossfades) ====================
constexpr float TREBLE_END = 0.33f;     // 0.00 - 0.33
constexpr float MID_END = 0.66f;        // 0.33 - 0.66
// Bass: 0.66 - 1.00
constexpr float CROSSFADE_WIDTH = 0.08f;

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
    // Soft crossfades preserve clean zones without hard seams.
    // =========================================================================

    const float dt = ctx.getSafeRawDeltaSeconds();

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

    // --- Beat boost (brief global pump) ---
    const bool beatTick = BeatPulseTiming::computeBeatTick(ctx, m_fallbackBpm, m_lastFallbackBeatMs);
    if (beatTick) {
        m_beatBoost = 0.25f;
    }
    m_beatBoost *= powf(0.90f, dt * 60.0f);
    if (m_beatBoost < 0.001f) m_beatBoost = 0.0f;

    // --- Render: Three soft-crossfaded zones ---
    auto zoneWeight = [](float x, float start, float end, float fade) -> float {
        if (x < start - fade || x > end + fade) return 0.0f;
        if (x < start) return clamp01((x - (start - fade)) / fade);
        if (x > end) return clamp01(((end + fade) - x) / fade);
        return 1.0f;
    };

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Zone weights (soft crossfade)
        const float wTreble = zoneWeight(dist01, 0.0f, TREBLE_END, CROSSFADE_WIDTH);
        const float wMid = zoneWeight(dist01, TREBLE_END, MID_END, CROSSFADE_WIDTH);
        const float wBass = zoneWeight(dist01, MID_END, 1.0f, CROSSFADE_WIDTH);

        // Intensity from weighted bands + beat boost
        float intensity = wTreble * m_smoothTreble + wMid * m_smoothMid + wBass * m_smoothBass;
        intensity = clamp01(intensity + m_beatBoost);

        const float brightnessFactor = clamp01(0.10f + intensity * 0.90f);

        // === TREBLE ZONE: Cool, high-frequency flicker ===
        float trebleIntensity = m_smoothTreble;
        trebleIntensity *= 0.75f + 0.25f * sinf(static_cast<float>(ctx.rawTotalTimeMs) * FLICKER_SPEED + static_cast<float>(dist) * 0.4f);
        const float treblePos = clamp01(dist01 / TREBLE_END);
        const uint8_t trebleIdx = 190 + floatToByte(treblePos * 0.25f);
        const float trebleMod = clamp01(0.7f + 0.3f * trebleIntensity);
        CRGB trebleColor = ctx.palette.getColor(
            trebleIdx,
            scaleBrightness(ctx.brightness, clamp01(brightnessFactor * trebleMod))
        );

        // === MID ZONE: Neutral ===
        const float midPos = clamp01((dist01 - TREBLE_END) / (MID_END - TREBLE_END));
        const uint8_t midIdx = 100 + floatToByte(midPos * 0.2f);
        CRGB midColor = ctx.palette.getColor(midIdx, scaleBrightness(ctx.brightness, brightnessFactor));

        // === BASS ZONE: Warm saturated ===
        const float bassPos = clamp01((dist01 - MID_END) / (1.0f - MID_END));
        const uint8_t bassIdx = 50 - floatToByte(bassPos * 0.2f);
        CRGB bassColor = ctx.palette.getColor(bassIdx, scaleBrightness(ctx.brightness, brightnessFactor));

        // Blend colours by zone weights
        const float wSum = wTreble + wMid + wBass;
        if (wSum <= 0.0001f) {
            SET_CENTER_PAIR(ctx, dist, CRGB(0, 0, 0));
            continue;
        }
        const float inv = 1.0f / wSum;
        uint8_t r = static_cast<uint8_t>((trebleColor.r * wTreble + midColor.r * wMid + bassColor.r * wBass) * inv);
        uint8_t g = static_cast<uint8_t>((trebleColor.g * wTreble + midColor.g * wMid + bassColor.g * wBass) * inv);
        uint8_t b = static_cast<uint8_t>((trebleColor.b * wTreble + midColor.b * wMid + bassColor.b * wBass) * inv);
        CRGB c(r, g, b);

        // White mix per spec
        ColourUtil::addWhiteSaturating(c, floatToByte(intensity * 0.20f));

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
