/**
 * @file BeatPulseStackEffect.cpp
 * @brief Beat Pulse (Stack) - UI preview parity pulse (static palette gradient + white push)
 */

#include "BeatPulseStackEffect.h"

#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <cstring>

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

static inline float clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float trailsMulFromFadeAmount(uint8_t fadeAmount, float dtSeconds) {
    // Match FastLED fadeToBlackBy() semantics: scale channels by (255-fade)/256 each frame.
    // We do a dt-correct version for 120 FPS render cadence.
    const float perFrame = (255.0f - static_cast<float>(fadeAmount)) / 255.0f;
    const float clamped = clamp(perFrame, 0.0f, 1.0f);
    return powf(clamped, dtSeconds * 120.0f);
}

} // namespace

bool BeatPulseStackEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_beatIntensity = 0.0f;
    m_lastBeatTimeMs = 0;
    m_fallbackBpm = 128.0f;
    m_stackGlow = 0.75f;
    std::memset(m_trail, 0, sizeof(m_trail));
    return true;
}

void BeatPulseStackEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // HTML PARITY: This effect matches docs/ui-mockups/components/v2/led-preview-stack.html
    // DO NOT add trails, knob remapping, or creative modifications here.
    // For variants, create separate effects (BeatPulseTrailsEffect, etc).
    // =========================================================================

    // ---------------------------------------------------------------------
    // Beat source
    // ---------------------------------------------------------------------
    bool beatTick = false;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
    } else {
        // Fallback metronome (matches the HTML mock: 128 BPM).
        const uint32_t nowMs = ctx.totalTimeMs;
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, m_fallbackBpm);
        if (m_lastBeatTimeMs == 0 || (nowMs - m_lastBeatTimeMs) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastBeatTimeMs = nowMs;
        }
    }

    // ---------------------------------------------------------------------
    // HTML PARITY: On beat, SLAM to 1.0. No scaling by beatStrength.
    // beatStrength can be used for subtle scaler later, but never cap the hit.
    // ---------------------------------------------------------------------
    if (beatTick) {
        m_beatIntensity = 1.0f;
    }

    // ---------------------------------------------------------------------
    // Decay: dt-correct form of beatIntensity *= 0.94 @ 60fps
    // ---------------------------------------------------------------------
    const float dt = ctx.getSafeDeltaSeconds();
    const float decay = powf(0.94f, dt * 60.0f);
    m_beatIntensity *= decay;
    if (m_beatIntensity < 0.001f) {
        m_beatIntensity = 0.0f;
    }

    // ---------------------------------------------------------------------
    // Render (centre-origin, static palette gradient + white push)
    // HTML PARITY: Fixed constants, no knob remapping of core shape.
    // ---------------------------------------------------------------------
    // HTML constants:
    //   ringCentre = 0.6 * beatIntensity
    //   sharpness = 3.0
    //   baseBrightness = 0.5
    //   brightnessBoost = intensity * 0.5
    //   whiteMix = intensity * 0.3
    constexpr float RING_CENTRE_FACTOR = 0.6f;
    constexpr float RING_SHARPNESS = 3.0f;
    constexpr float BASE_BRIGHTNESS = 0.5f;
    constexpr float BRIGHTNESS_BOOST = 0.5f;
    constexpr float WHITE_MIX_FACTOR = 0.3f;

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        // Distance 0 at centre → ~1 at edges (centre is between the pair).
        const float dist01 = (static_cast<float>(dist) + 0.5f) / static_cast<float>(HALF_LENGTH);

        // HTML pulse shape:
        //   ringCentre = 0.6 * beatIntensity
        //   waveHit = 1 - min(1, abs(dist01 - ringCentre) * 3)
        //   intensity = max(0, waveHit) * beatIntensity
        const float ringCentre = RING_CENTRE_FACTOR * m_beatIntensity;
        const float waveHit = 1.0f - fminf(1.0f, fabsf(dist01 - ringCentre) * RING_SHARPNESS);
        const float intensity = fmaxf(0.0f, waveHit) * m_beatIntensity;

        // Base gradient (static): palette indexed by distance.
        const uint8_t paletteIdx = static_cast<uint8_t>(clamp01(dist01) * 255.0f);

        // HTML: brightness = 0.5 + intensity * 0.5
        const float brightnessFactor = clamp01(BASE_BRIGHTNESS + intensity * BRIGHTNESS_BOOST);
        const uint8_t brightness8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * brightnessFactor);
        CRGB c = ctx.palette.getColor(paletteIdx, brightness8);

        // HTML: whiteMix = intensity * 0.3
        const float whiteMix = intensity * WHITE_MIX_FACTOR;
        const uint8_t white8 = clampU8FromFloat(static_cast<float>(ctx.brightness) * whiteMix);
        addWhiteSaturating(c, white8);

        SET_CENTER_PAIR(ctx, dist, c);
    }
}

void BeatPulseStackEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseStackEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse (Stack)",
        "UI preview parity: static palette gradient with beat-driven white push",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

uint8_t BeatPulseStackEffect::getParameterCount() const {
    return 1;
}

const plugins::EffectParameter* BeatPulseStackEffect::getParameter(uint8_t index) const {
    static plugins::EffectParameter params[] = {
        plugins::EffectParameter("stackGlow", "Stack Glow", 0.0f, 1.0f, 0.75f),
    };
    if (index >= (sizeof(params) / sizeof(params[0]))) {
        return nullptr;
    }
    return &params[index];
}

bool BeatPulseStackEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "stackGlow") == 0) {
        m_stackGlow = clamp01(value);
        return true;
    }
    return false;
}

float BeatPulseStackEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "stackGlow") == 0) {
        return m_stackGlow;
    }
    return 0.0f;
}

} // namespace lightwaveos::effects::ieffect
