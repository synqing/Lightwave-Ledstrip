/**
 * @file BeatPulseShockwaveCascadeEffect.cpp
 * @brief Beat Pulse (Shockwave Cascade) - Outward expansion with echo rings
 *
 * See BeatPulseShockwaveCascadeEffect.h for design rationale.
 */

#include "BeatPulseShockwaveCascadeEffect.h"

#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos::effects::ieffect {

namespace {

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
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

} // namespace

bool BeatPulseShockwaveCascadeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    return true;
}

void BeatPulseShockwaveCascadeEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // SHOCKWAVE CASCADE: Primary ring expands outward, 2 echo rings trail behind.
    // Like a stone dropped in water — concentric pressure waves.
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
        }
    }

    const uint32_t nowMs = ctx.totalTimeMs;

    // --- Slam to 1.0 on beat ---
    if (beatTick) {
        m_beatIntensity = 1.0f;
        m_lastBeatTimeMs = nowMs;
    }

    // --- Fixed constants ---
    constexpr float TRAVEL_MS = 400.0f;
    constexpr float DECAY_MS = 320.0f;
    constexpr float RING_WIDTH = 0.10f;

    // Echo offsets (behind primary in dist01 space)
    constexpr float ECHO1_OFFSET = 0.12f;
    constexpr float ECHO1_GAIN = 0.45f;
    constexpr float ECHO2_OFFSET = 0.24f;
    constexpr float ECHO2_GAIN = 0.25f;

    constexpr float BASE_BRIGHTNESS = 0.05f;
    constexpr float BRIGHTNESS_BOOST = 0.95f;
    constexpr float WHITE_MIX_FACTOR = 0.35f;

    // --- Age and position ---
    float ageMs = 999999.0f;
    if (m_lastBeatTimeMs != 0) {
        ageMs = static_cast<float>(nowMs - m_lastBeatTimeMs);
    }
    const float maxLifeMs = TRAVEL_MS + 2.0f * DECAY_MS;

    // Exponential envelope
    float env = 0.0f;
    if (ageMs < maxLifeMs) {
        env = expf(-ageMs / DECAY_MS) * m_beatIntensity;
        env = clamp01(env);
    }

    // Primary ring position: expanding outward (centre→edge)
    const float primaryPos = clamp01(ageMs / TRAVEL_MS);

    // --- Render ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // Primary ring: tent profile
        float primaryHit = 0.0f;
        {
            const float diff = fabsf(dist01 - primaryPos);
            if (diff < RING_WIDTH) {
                primaryHit = 1.0f - diff / RING_WIDTH;
            }
        }

        // Echo 1: trailing behind primary
        float echo1Hit = 0.0f;
        {
            const float echo1Pos = fmaxf(0.0f, primaryPos - ECHO1_OFFSET);
            const float diff = fabsf(dist01 - echo1Pos);
            if (diff < RING_WIDTH) {
                echo1Hit = (1.0f - diff / RING_WIDTH) * ECHO1_GAIN;
            }
        }

        // Echo 2: trailing further behind
        float echo2Hit = 0.0f;
        {
            const float echo2Pos = fmaxf(0.0f, primaryPos - ECHO2_OFFSET);
            const float diff = fabsf(dist01 - echo2Pos);
            if (diff < RING_WIDTH) {
                echo2Hit = (1.0f - diff / RING_WIDTH) * ECHO2_GAIN;
            }
        }

        // Take maximum (they layer, not add)
        const float intensity = fmaxf(primaryHit, fmaxf(echo1Hit, echo2Hit)) * env;

        // Palette indexed by distance
        const uint8_t paletteIdx = clampU8FromFloat(dist01 * 255.0f);

        // Brightness
        const float brightnessFactor = clamp01(BASE_BRIGHTNESS + intensity * BRIGHTNESS_BOOST);
        const uint8_t brightness8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * brightnessFactor);
        CRGB c = ctx.palette.getColor(paletteIdx, brightness8);

        // White push only on primary ring (echoes are purely coloured)
        const float primaryIntensity = primaryHit * env;
        const float whiteMix = primaryIntensity * WHITE_MIX_FACTOR;
        const uint8_t white8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * whiteMix);
        addWhiteSaturating(c, white8);

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseShockwaveCascadeEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseShockwaveCascadeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Shockwave Cascade)",
        "Outward pressure wave with trailing echo rings",
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
