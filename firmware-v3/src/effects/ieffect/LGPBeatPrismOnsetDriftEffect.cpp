/**
 * @file LGPBeatPrismOnsetDriftEffect.cpp
 * @brief Outward Drift variant of LGP Beat Prism Onset.
 *
 * VARIANT 1 — Outward Drift (0x1E01)
 *
 * WHAT CHANGED (background motion only):
 *   - Spring breathing (expanding/contracting phase) replaced with
 *     continuous one-way outward drift from centre.
 *   - New state: m_bgDriftPhase wraps [0,1) at a rate modulated by prism.
 *   - Background field samples at (d - m_bgDriftPhase) so structure
 *     appears to flow outward perpetually.
 *   - Spoke density fixed at ~8.5 (no pumping from prism).
 *   - Facets and refraction also use the drift-offset sampling position.
 *
 * WHAT IS PRESERVED (identical to base):
 *   - Kick trigger → travelling front pulse
 *   - Snare burst → refraction accent
 *   - Hihat shimmer → high-frequency overlay
 *   - Chord/hue logic (selectMusicalHue, dominantNoteFromChroma, smoothHue)
 *   - Palette logic and colour indexing
 *   - Brightness gating (audioPresence, audioConfidence)
 *   - Centre-origin topology (setCentrePairMono)
 *   - Mirrored addressing layout
 */

#include "LGPBeatPrismOnsetDriftEffect.h"
#include "ChromaUtils.h"
#include "AudioReactivePolicy.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstdint>

namespace lightwaveos {
namespace effects {
namespace ieffect {

namespace {

static constexpr float EX_PI  = 3.14159265358979323846f;
static constexpr float EX_TAU = 6.28318530717958647692f;
static constexpr uint16_t STRIP_LENGTH = 160;
static constexpr uint16_t HALF_LENGTH  = 80;
static constexpr uint16_t CENTER_LEFT  = 79;
static constexpr uint16_t CENTER_RIGHT = 80;

static constexpr uint8_t NOTE_HUES[12] = {
    0, 12, 24, 40, 56, 74, 92, 112, 134, 154, 178, 202
};

static inline float clamp01f(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float smoothTo(float current, float target, float dt, float tauS) {
    const float alpha = 1.0f - expf(-dt / fmaxf(tauS, 0.001f));
    return current + (target - current) * alpha;
}

static inline float decay(float value, float dt, float tauS) {
    return value * expf(-dt / fmaxf(tauS, 0.001f));
}

static inline uint8_t toBrightness(float intensity, float master) {
    return static_cast<uint8_t>(255.0f * clamp01f(intensity) * clamp01f(master));
}

static inline void setCentrePairMono(
    plugins::EffectContext& ctx,
    uint16_t dist,
    const CRGB& color
) {
    const uint16_t left1  = CENTER_LEFT - dist;
    const uint16_t right1 = CENTER_RIGHT + dist;
    const uint16_t left2  = STRIP_LENGTH + CENTER_LEFT - dist;
    const uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;
    if (left1  < ctx.ledCount) ctx.leds[left1]  = color;
    if (right1 < ctx.ledCount) ctx.leds[right1] = color;
    if (left2  < ctx.ledCount) ctx.leds[left2]  = color;
    if (right2 < ctx.ledCount) ctx.leds[right2] = color;
}

static inline uint8_t dominantNoteFromChroma(const plugins::EffectContext& ctx) {
    if (!ctx.audio.available) return 0;
    const float* scores = ctx.audio.chroma();
    float cx = 0.0f, sy = 0.0f;
    for (uint8_t i = 0; i < 12; ++i) {
        cx += scores[i] * effects::chroma::kCos[i];
        sy += scores[i] * effects::chroma::kSin[i];
    }
    float angle = atan2f(sy, cx);
    if (angle < 0.0f) angle += EX_TAU;
    return static_cast<uint8_t>(roundf(angle * (12.0f / EX_TAU))) % 12;
}

static inline uint8_t selectMusicalHue(const plugins::EffectContext& ctx, bool& chordGateOpen) {
    if (!ctx.audio.available) return 24;
    const float conf = ctx.audio.chordConfidence();
    if (conf >= 0.40f) chordGateOpen = true;
    else if (conf <= 0.25f) chordGateOpen = false;
    const uint8_t note = chordGateOpen
        ? static_cast<uint8_t>(ctx.audio.rootNote() % 12)
        : dominantNoteFromChroma(ctx);
    return NOTE_HUES[note];
}

static inline float smoothHue(float current, float target, float dt, float tauS) {
    float diff = target - current;
    if (diff > 128.0f) diff -= 256.0f;
    if (diff < -128.0f) diff += 256.0f;
    const float alpha = 1.0f - expf(-dt / fmaxf(tauS, 0.001f));
    float result = current + diff * alpha;
    if (result < 0.0f) result += 256.0f;
    if (result >= 256.0f) result -= 256.0f;
    return result;
}

static inline float trackAudioPresence(float current, bool audioAvailable, float dt) {
    const float tau = audioAvailable ? 0.06f : 0.32f;
    return smoothTo(current, audioAvailable ? 1.0f : 0.0f, dt, tau);
}

static inline float fallbackSine(uint32_t rawMs, float rate, float phaseOffset = 0.0f) {
    return 0.5f + 0.5f * sinf(static_cast<float>(rawMs) * rate + phaseOffset);
}

} // namespace

// ---------------------------------------------------------------------------

bool LGPBeatPrismOnsetDriftEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_prism = 0.0f;
    m_kickPulse = 0.0f;
    m_snareBurst = 0.0f;
    m_hihatShimmer = 0.0f;
    m_hue = 24.0f;
    m_audioPresence = 0.0f;
    m_chordGateOpen = false;
    m_bgDriftPhase = 0.0f;
    return true;
}

void LGPBeatPrismOnsetDriftEffect::render(plugins::EffectContext& ctx) {
    const float dtSignal = AudioReactivePolicy::signalDt(ctx);
    const float dtVisual = AudioReactivePolicy::visualDt(ctx);
    m_audioPresence = trackAudioPresence(m_audioPresence, ctx.audio.available, dtSignal);
    if (m_audioPresence <= 0.001f) {
        fadeToBlackBy(ctx.leds, ctx.ledCount, 30);
        return;
    }
    const float confidence = ctx.audio.controlBus.audioConfidence;
    const float master = (ctx.brightness / 255.0f) * m_audioPresence * confidence;

    // --- Onset-driven pulse channels (PRESERVED) ---
    const auto& cb = ctx.audio.controlBus;

    if (cb.kickTrigger) {
        m_kickPulse = 1.0f;
    }
    m_kickPulse = decay(m_kickPulse, dtSignal, 0.24f);

    if (cb.snareTrigger) {
        m_snareBurst = 1.0f;
    }
    m_snareBurst = decay(m_snareBurst, dtSignal, 0.15f);

    if (cb.hihatTrigger) {
        m_hihatShimmer = 1.0f;
    }
    m_hihatShimmer = decay(m_hihatShimmer, dtSignal, 0.08f);

    // Prism intensity (PRESERVED)
    const auto& bands = ctx.audio.bands();
    const float treble = ctx.audio.available
        ? (bands[5] + bands[6] + bands[7]) * (1.0f / 3.0f)
        : 0.0f;
    const float prismTarget = ctx.audio.available
        ? clamp01f(0.28f * treble + 0.32f * m_snareBurst + 0.10f * m_hihatShimmer)
        : fallbackSine(ctx.rawTotalTimeMs, 0.0011f, 1.0f);
    m_prism = smoothTo(m_prism, prismTarget, dtSignal, 0.12f);

    // --- VARIANT: Outward drift phase (replaces spring breathing) ---
    // Continuous one-way drift. Prism affects speed subtly, NOT density.
    m_bgDriftPhase += (0.10f + 0.18f * m_prism) * dtVisual;
    if (m_bgDriftPhase >= 1.0f) m_bgDriftPhase -= 1.0f;

    // Phase still advances for shimmer timing (reduced coupling)
    m_phase += 0.90f * (0.55f + 0.75f * m_prism) * dtVisual;
    m_phase = fmodf(m_phase, EX_TAU);

    // Beat-front position from kick pulse (PRESERVED)
    const float frontPos = clamp01f(1.0f - m_kickPulse);

    // Hue from chord detection (PRESERVED)
    const float hueTarget = static_cast<float>(static_cast<uint8_t>(selectMusicalHue(ctx, m_chordGateOpen) + 8));
    m_hue = smoothHue(m_hue, hueTarget, dtSignal, 0.45f);
    const uint8_t baseHue = static_cast<uint8_t>(m_hue);

    // --- Render ---
    fadeToBlackBy(ctx.leds, ctx.ledCount, 30);

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float d = static_cast<float>(dist) / static_cast<float>(HALF_LENGTH);

        // --- VARIANT: Sample at drift-offset position ---
        const float sampleD = d - m_bgDriftPhase;

        // Spoke field: stable density ~8.5 (no prism pumping)
        const float spokes = fabsf(sinf((sampleD * 8.5f) * EX_PI));
        // Facets and refract also use sampleD
        const float facets = 0.5f + 0.5f * cosf((sampleD * 3.5f + m_phase * 0.35f) * EX_TAU);
        const float refract = 0.5f + 0.5f * sinf(sampleD * (2.2f + 3.2f * m_prism) * EX_TAU - m_phase * 1.10f);

        // Kick front (PRESERVED)
        const float front = expf(-fabsf(d - frontPos) * (7.5f + 4.5f * m_prism)) * m_kickPulse;

        // Hihat shimmer (PRESERVED)
        const float shimmer = m_hihatShimmer > 0.01f
            ? (0.5f + 0.5f * sinf(d * 34.0f + m_phase * 3.6f)) * m_hihatShimmer * 0.18f
            : 0.0f;

        const float intensity = clamp01f(
            (0.20f + 0.80f * spokes) * (0.25f + 0.75f * facets) *
            (0.20f + 0.80f * refract) + front * 1.10f + shimmer
        );

        // Colour indexing (PRESERVED)
        const uint8_t br = toBrightness(intensity, master);
        const uint8_t idxA = static_cast<uint8_t>(
            baseHue + static_cast<uint8_t>(spokes * 26.0f) +
            static_cast<uint8_t>(d * 28.0f) +
            static_cast<uint8_t>(m_snareBurst * 10.0f)
        );
        setCentrePairMono(ctx, dist, ctx.palette.getColor(idxA, br));
    }
}

void LGPBeatPrismOnsetDriftEffect::cleanup() {}

const plugins::EffectMetadata& LGPBeatPrismOnsetDriftEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Beat Prism Onset Drift",
        "Outward drift — centre emits energy outward, no contraction",
        plugins::EffectCategory::PARTY,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
