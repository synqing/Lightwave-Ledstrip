/**
 * @file BeatPulseShockwaveEffect.cpp
 * @brief Beat Pulse shockwave (outward/inward) with knob-driven shaping
 */

#include "BeatPulseShockwaveEffect.h"

#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect {

namespace {

static constexpr float kPi = 3.14159265358979323846f;

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline uint8_t clampU8FromFloat(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

static inline void addWhiteSaturating(CRGB& c, uint8_t w) {
    uint16_t r = static_cast<uint16_t>(c.r) + w;
    uint16_t g = static_cast<uint16_t>(c.g) + w;
    uint16_t b = static_cast<uint16_t>(c.b) + w;
    c.r = (r > 255) ? 255 : static_cast<uint8_t>(r);
    c.g = (g > 255) ? 255 : static_cast<uint8_t>(g);
    c.b = (b > 255) ? 255 : static_cast<uint8_t>(b);
}

static inline float ringHitTent(float diff, float width) {
    if (width <= 0.0001f) return 0.0f;
    float x = diff / width;
    if (x >= 1.0f) return 0.0f;
    return 1.0f - x;
}

static inline float ringHitSmooth(float diff, float width) {
    float t = ringHitTent(diff, width);
    // smoothstep(t): t^2 (3-2t)
    return t * t * (3.0f - 2.0f * t);
}

static inline float ringHitCosine(float diff, float width) {
    if (width <= 0.0001f) return 0.0f;
    float x = diff / width;
    if (x >= 1.0f) return 0.0f;
    return 0.5f + 0.5f * cosf(kPi * x);
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

BeatPulseShockwaveEffect::BeatPulseShockwaveEffect(bool inward)
    : m_inward(inward)
    , m_meta(
          inward ? "Beat Pulse (Shockwave In)" : "Beat Pulse (Shockwave)",
          inward ? "Edge→centre shockwave driven by beat tick/strength" : "Centre→edge shockwave driven by beat tick/strength",
          plugins::EffectCategory::PARTY,
          1,
          "LightwaveOS")
{
}

bool BeatPulseShockwaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_lastBeatTimeMs = 0;
    m_latchedBeatStrength = 0.0f;
    std::memset(m_trail, 0, sizeof(m_trail));
    return true;
}

void BeatPulseShockwaveEffect::render(plugins::EffectContext& ctx) {
    // ---------------------------------------------------------------------
    // Beat source (audio preferred, fallback metronome)
    // ---------------------------------------------------------------------
    bool beatTick = false;
    float beatStrength = 1.0f;
    float silentScale = 1.0f;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
        beatStrength = clamp01(ctx.audio.beatStrength());
        silentScale = ctx.audio.controlBus.silentScale;
    } else {
        // Fallback metronome: tie BPM to speed a little so it stays "alive".
        // speed=0..100 → 96..140 BPM.
        float speed01 = clamp01(ctx.speed / 100.0f);
        float bpm = lerp(96.0f, 140.0f, speed01);
        float beatIntervalMs = 60000.0f / fmaxf(30.0f, bpm);
        uint32_t nowMs = ctx.totalTimeMs;
        if (m_lastBeatTimeMs == 0 || (nowMs - m_lastBeatTimeMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
        }
        beatStrength = 1.0f;
        silentScale = 1.0f;
    }

    const uint32_t nowMs = ctx.totalTimeMs;
    if (beatTick) {
        m_lastBeatTimeMs = nowMs;
        // Latch strength on tick so envelope doesn’t depend on the backend’s internal decay.
        const float intensityNorm = static_cast<float>(ctx.intensity) / 255.0f;
        const float minLatch = 0.20f + 0.25f * intensityNorm;
        m_latchedBeatStrength = fmaxf(minLatch, beatStrength);
    }

    // ---------------------------------------------------------------------
    // Knob mapping
    // ---------------------------------------------------------------------
    const float dt = ctx.getSafeDeltaSeconds();
    const float speed01 = clamp01(ctx.speed / 100.0f);
    const float intensityNorm = static_cast<float>(ctx.intensity) / 255.0f;
    const float complexityNorm = static_cast<float>(ctx.complexity) / 255.0f;
    const float variationNorm = static_cast<float>(ctx.variation) / 255.0f;

    // Travel time: faster at higher speed (centre→edge or edge→centre).
    const float travelMs = lerp(900.0f, 220.0f, speed01);
    // Decay time: slightly longer at higher intensity/complexity so trails can breathe.
    const float decayMs = lerp(240.0f, 520.0f, 0.55f * intensityNorm + 0.45f * complexityNorm);
    // Ring width: thicker when low complexity, thinner when high complexity.
    const float baseWidth = lerp(0.22f, 0.06f, complexityNorm);
    const float width = baseWidth * lerp(1.20f, 0.85f, variationNorm);

    // Trails decay derived from fadeAmount (0 = no fade, 255 = instant fade).
    const float fade01 = static_cast<float>(ctx.fadeAmount) / 255.0f;
    const float baseTrailMul = clamp(1.0f - fade01, 0.0f, 1.0f);
    const float trailMul = powf(baseTrailMul, dt * 60.0f);

    // Echoes: add one faint echo ring when complexity is high.
    const bool enableEcho = (complexityNorm > 0.62f);
    const float echoSpacing = lerp(0.10f, 0.045f, variationNorm);  // in dist01 units
    const float echoGain = 0.35f + 0.15f * complexityNorm;

    // ---------------------------------------------------------------------
    // Ring position + envelope
    // ---------------------------------------------------------------------
    float ageMs = 999999.0f;
    if (m_lastBeatTimeMs != 0) {
        ageMs = static_cast<float>(nowMs - m_lastBeatTimeMs);
    }
    const float pos01raw = clamp01(ageMs / travelMs);
    const float pos01 = m_inward ? (1.0f - pos01raw) : pos01raw;

    // Exponential decay envelope (amplitude), scaled by intensity and silent gating.
    float env = 0.0f;
    if (ageMs < (travelMs + 2.0f * decayMs)) {
        env = expf(-ageMs / decayMs);
        env *= m_latchedBeatStrength;
        env *= (0.35f + 0.65f * intensityNorm);
        env *= silentScale;
        env = clamp01(env);
    }

    // ---------------------------------------------------------------------
    // Render: static palette gradient + pulse overlay + white push + trails
    // ---------------------------------------------------------------------
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);
        const float diff = fabsf(dist01 - pos01);

        float hit = 0.0f;
        // Variation selects profile family.
        if (ctx.variation < 85) {
            hit = ringHitTent(diff, width);
        } else if (ctx.variation < 170) {
            hit = ringHitSmooth(diff, width);
        } else {
            hit = ringHitCosine(diff, width);
        }

        float intensity = env * hit;

        if (enableEcho) {
            float echoPos = m_inward ? (pos01 + echoSpacing) : (pos01 - echoSpacing);
            echoPos = clamp01(echoPos);
            float echoDiff = fabsf(dist01 - echoPos);
            float echoHit = ringHitSmooth(echoDiff, width * 1.10f);
            intensity = fmaxf(intensity, env * echoHit * echoGain);
        }

        // Trails: keep the maximum intensity over recent frames with knob-driven decay.
        m_trail[dist] *= trailMul;
        if (intensity > m_trail[dist]) {
            m_trail[dist] = intensity;
        }
        const float effIntensity = m_trail[dist];

        // Base gradient by distance (matches preview concept: paletteColour(dist)).
        const uint8_t paletteIdx = clampU8FromFloat(dist01 * 255.0f);

        // Brightness: base + pulse boost (knob-driven).
        const float baseBrightness = lerp(0.30f, 0.62f, intensityNorm);
        const float boost = effIntensity * lerp(0.35f, 0.65f, intensityNorm);
        const float brightnessFactor = clamp01(baseBrightness + boost);

        const uint8_t bri8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * brightnessFactor);
        CRGB c = ctx.palette.getColor(paletteIdx, bri8);

        // White push: specular punch, scaled by intensity knob.
        const float whiteMix = effIntensity * lerp(0.12f, 0.38f, intensityNorm);
        const uint8_t w8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * whiteMix);
        addWhiteSaturating(c, w8);

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseShockwaveEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveEffect::getMetadata() const {
    return m_meta;
}

} // namespace lightwaveos::effects::ieffect

