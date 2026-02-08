/**
 * @file BeatPulseSpectralEffect.cpp
 * @brief Beat Pulse (Spectral) - Frequency decomposition: bass glow, mid crack, treble shimmer
 *
 * Visual identity: You SEE the drum kit spatially decomposed.
 *   - BASS: Wide warm GLOW filling outer region (thud)
 *   - MID: Sharp RING at middle position with position modulation (crack)
 *   - TREBLE: SPARKLE/shimmer near centre, noise-modulated (sizzle)
 *
 * Each band has DISTINCT visual character and uses different palette regions.
 * Additive combination shows all three bands simultaneously.
 *
 * See BeatPulseSpectralEffect.h for class definition.
 */

#include "BeatPulseSpectralEffect.h"
#include "../CoreEffects.h"
#include "BeatPulseRenderUtils.h"

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

// ============================================================================
// Band smoothing (different attack/release per band)
// ============================================================================
constexpr float BASS_SMOOTH = 0.85f;    // Slower (weighty)
constexpr float MID_SMOOTH = 0.88f;
constexpr float TREBLE_SMOOTH = 0.92f;  // Faster (snappy)

// ============================================================================
// Spatial regions
// ============================================================================
constexpr float BASS_START = 0.55f;     // Outer 45% of strip
constexpr float MID_POS = 0.40f;        // Middle ring centre
constexpr float MID_WIDTH = 0.07f;      // Sharp ring
constexpr float TREBLE_END = 0.18f;     // Inner 18% of strip

// ============================================================================
// Palette regions (warm/neutral/cool)
// ============================================================================
constexpr uint8_t BASS_PALETTE = 40;    // Warm
constexpr uint8_t MID_PALETTE = 128;    // Neutral
constexpr uint8_t TREBLE_PALETTE = 200; // Cool

// ============================================================================
// Ring edge sharpness
// ============================================================================
constexpr float MID_EDGE_SOFTNESS = 0.015f;

} // namespace

bool BeatPulseSpectralEffect::init(plugins::EffectContext& ctx) {
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

void BeatPulseSpectralEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SPECTRAL: Frequency decomposition with distinct visual character per band.
    // Bass = outer GLOW (warm), Mid = SHARP ring (neutral), Treble = SPARKLE (cool).
    // Additive combination - you see all three bands simultaneously.
    // =========================================================================

    const float dt = ctx.getSafeDeltaSeconds();

    // --- Read frequency bands ---
    float rawBass = 0.0f;
    float rawMid = 0.0f;
    float rawTreble = 0.0f;
    bool beatTick = false;

    if (ctx.audio.available) {
        rawBass = clamp01(ctx.audio.bass());
        rawMid = clamp01(ctx.audio.mid());
        rawTreble = clamp01(ctx.audio.treble());
        beatTick = ctx.audio.isOnBeat();
    } else {
        // Fallback: simulate gentle pulsing
        const uint32_t nowMs = ctx.totalTimeMs;
        m_fallbackPhase += dt * 2.0f;
        if (m_fallbackPhase > 6.28318f) m_fallbackPhase -= 6.28318f;
        rawBass = 0.4f + 0.3f * sinf(m_fallbackPhase);
        rawMid = 0.3f + 0.2f * sinf(m_fallbackPhase * 1.5f);
        rawTreble = 0.2f + 0.15f * sinf(m_fallbackPhase * 2.5f);

        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, m_fallbackBpm);
        if (m_lastFallbackBeatMs == 0 || (nowMs - m_lastFallbackBeatMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastFallbackBeatMs = nowMs;
        }
    }

    // --- Smooth each band (dt-correct exponential smoothing) ---
    // Different attack/release rates per band
    const float bassSmooth = 1.0f - powf(BASS_SMOOTH, dt * 60.0f);   // slower (bass is weighty)
    const float midSmooth = 1.0f - powf(MID_SMOOTH, dt * 60.0f);
    const float trebleSmooth = 1.0f - powf(TREBLE_SMOOTH, dt * 60.0f); // faster (treble is snappy)

    m_smoothBass += (rawBass - m_smoothBass) * bassSmooth;
    m_smoothMid += (rawMid - m_smoothMid) * midSmooth;
    m_smoothTreble += (rawTreble - m_smoothTreble) * trebleSmooth;

    // --- Beat boost (brief global pump, unused in additive version but kept for continuity) ---
    if (beatTick) {
        m_beatBoost = 0.3f;
    }
    m_beatBoost *= powf(0.90f, dt * 60.0f);
    if (m_beatBoost < 0.001f) m_beatBoost = 0.0f;

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // === BASS: Wide warm GLOW in outer region ===
        float bassHit = 0.0f;
        if (dist01 > BASS_START) {
            // Fills region, intensity fades toward edge
            float zonePos = (dist01 - BASS_START) / (1.0f - BASS_START);
            bassHit = (1.0f - zonePos * 0.3f) * m_smoothBass;  // Slight edge fade
        }

        // === MID: Sharp ring with position modulation ===
        float midHit = 0.0f;
        {
            // Ring position shifts slightly with intensity (crack moves)
            const float modPos = MID_POS + m_smoothMid * 0.12f;
            const float diff = fabsf(dist01 - modPos);
            midHit = RingProfile::hardEdge(diff, MID_WIDTH, MID_EDGE_SOFTNESS) * m_smoothMid;
        }

        // === TREBLE: Sparkle shimmer at centre ===
        float trebleHit = 0.0f;
        if (dist01 < TREBLE_END) {
            float zonePos = dist01 / TREBLE_END;
            // High-frequency noise modulation (sparkle)
            float noise = 0.5f + 0.5f * sinf(static_cast<float>(dist) * 23.7f + static_cast<float>(ctx.totalTimeMs) * 0.035f);
            noise *= 0.5f + 0.5f * sinf(static_cast<float>(dist) * 11.3f - static_cast<float>(ctx.totalTimeMs) * 0.021f);
            trebleHit = (1.0f - zonePos) * m_smoothTreble * noise;
        }

        // === Colour per band (different palette regions) ===
        const uint8_t bassBright = scaleBrightness(ctx.brightness, bassHit * 0.9f);
        CRGB bassColor = ctx.palette.getColor(BASS_PALETTE + floatToByte(bassHit * 0.15f), bassBright);

        const uint8_t midBright = scaleBrightness(ctx.brightness, midHit);
        CRGB midColor = ctx.palette.getColor(MID_PALETTE, midBright);

        const uint8_t trebleBright = scaleBrightness(ctx.brightness, trebleHit * 1.1f);
        CRGB trebleColor = ctx.palette.getColor(TREBLE_PALETTE + floatToByte(trebleHit * 0.2f), trebleBright);

        // ADDITIVE combination
        CRGB c = ColourUtil::additive(ColourUtil::additive(bassColor, midColor), trebleColor);

        // White sparkle on TREBLE only
        if (trebleHit > 0.25f) {
            ColourUtil::addWhiteSaturating(c, floatToByte((trebleHit - 0.25f) * 0.5f));
        }

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseSpectralEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseSpectralEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Spectral)",
        "Frequency decomposition: bass glow, mid crack, treble shimmer",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

uint8_t BeatPulseSpectralEffect::getParameterCount() const { return 0; }

const plugins::EffectParameter* BeatPulseSpectralEffect::getParameter(uint8_t index) const {
    (void)index;
    return nullptr;
}

bool BeatPulseSpectralEffect::setParameter(const char* name, float value) {
    (void)name; (void)value;
    return false;
}

float BeatPulseSpectralEffect::getParameter(const char* name) const {
    (void)name;
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
