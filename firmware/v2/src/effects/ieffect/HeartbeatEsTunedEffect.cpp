/**
 * @file HeartbeatEsTunedEffect.cpp
 * @brief Heartbeat (ES tuned) effect implementation
 */

#include "HeartbeatEsTunedEffect.h"
#include "ChromaUtils.h"
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

static inline const float* selectChroma12(const audio::ControlBusFrame& cb) {
    // Both backends now produce normalised chroma via Stage A/B pipeline.
    return cb.chroma;
}

static inline CRGB addSat(CRGB a, CRGB b) {
    a.r = qadd8(a.r, b.r);
    a.g = qadd8(a.g, b.g);
    a.b = qadd8(a.b, b.b);
    return a;
}

} // namespace

bool HeartbeatEsTunedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_lastHopSeq = 0;
    m_chromaAngle = 0.0f;

    m_lastBeatPhase = 0.0f;
    m_lastFastFlux = 0.0f;
    m_dubPending = false;

    m_lubRadius = 999.0f;
    m_dubRadius = 999.0f;
    m_lubIntensity = 0.0f;
    m_dubIntensity = 0.0f;

    m_lastBeatTimeMs = millis();
    m_beatState = 0;
    return true;
}

void HeartbeatEsTunedEffect::render(plugins::EffectContext& ctx) {
    // Trails: keep the original aesthetic, but all motion/trigger logic becomes audio-aware.
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    const float rawDt = ctx.getSafeRawDeltaSeconds();
    const float dt = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // ---------------------------------------------------------------------
    // Chroma anchor (non-rainbow): circular weighted mean, smoothed.
    // ---------------------------------------------------------------------
    uint8_t baseHue = 0;
    if (ctx.audio.available) {
        const float* chroma = selectChroma12(ctx.audio.controlBus);
        baseHue = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, rawDt, 0.25f);
    } else {
        // Drift very slowly back to 0 when audio is absent (keeps a stable default) - dt-corrected
        m_chromaAngle *= powf(0.995f, rawDt * 60.0f);
        baseHue = static_cast<uint8_t>(m_chromaAngle * (255.0f / 6.2831853f));
    }
    const uint8_t lubHue = baseHue;
    const uint8_t dubHue = (uint8_t)(baseHue + 36);

    // ---------------------------------------------------------------------
    // Trigger logic
    // ---------------------------------------------------------------------
    bool useAudioBeat = false;
    float beatStrength = 0.0f;
    float beatPhase = 0.0f;
    // silentScale handled globally by RendererActor

    if (ctx.audio.available) {
        beatStrength = ctx.audio.beatStrength();
        beatPhase = ctx.audio.beatPhase();

        // Require some confidence before we trust beat ticks for the heartbeat cadence.
        useAudioBeat = (ctx.audio.tempoConfidence() > 0.40f);

        if (useAudioBeat && ctx.audio.isOnBeat()) {
            // "Lub"
            m_lubRadius = 0.0f;
            m_lubIntensity = 0.30f + 0.70f * clamp01(beatStrength);
            m_dubPending = true;
        }

        // "Dub" timing: beat-phase offset OR flux spike (backend-agnostic transient proxy).
        constexpr float DUB_PHASE = 0.28f;
        bool dubTrigger = false;
        if (useAudioBeat && m_dubPending) {
            // Phase crossing detector (handles wrap cleanly because beatPhase resets).
            if (m_lastBeatPhase < DUB_PHASE && beatPhase >= DUB_PHASE) {
                dubTrigger = true;
            }

            float flux = ctx.audio.fastFlux();
            float fluxDelta = flux - m_lastFastFlux;
            m_lastFastFlux = flux;
            if (fluxDelta > 0.22f && flux > 0.25f && beatPhase > 0.10f && beatPhase < 0.65f) {
                dubTrigger = true;
            }

            if (dubTrigger) {
                m_dubRadius = 0.0f;
                float accent = clamp01(beatStrength * 0.75f + ctx.audio.fastFlux() * 0.35f);
                m_dubIntensity = 0.20f + 0.65f * accent;
                m_dubPending = false;
            }
        }

        m_lastBeatPhase = beatPhase;
    }

    // Fallback behaviour: original fixed lub-dub cadence (~75 BPM).
    if (!ctx.audio.available || !useAudioBeat) {
        uint32_t nowMs = millis();
        constexpr uint32_t BEAT2_DELAY_MS = 200;
        constexpr uint32_t CYCLE_TIME_MS = 800;
        uint32_t cyclePos = (nowMs - m_lastBeatTimeMs);

        if (cyclePos >= CYCLE_TIME_MS) {
            m_lastBeatTimeMs = nowMs;
            m_beatState = 1;
            m_lubRadius = 0.0f;
            m_lubIntensity = 0.55f;
        } else if (cyclePos >= BEAT2_DELAY_MS && m_beatState == 1) {
            m_beatState = 2;
            m_dubRadius = 0.0f;
            m_dubIntensity = 0.45f;
        }
    }

    // ---------------------------------------------------------------------
    // Integrate ring motion (dt-based)
    // ---------------------------------------------------------------------
    const float speedLedsPerSec = 220.0f * (0.35f + 1.0f * speedNorm);
    const float strengthSpeed = 0.80f + 0.50f * clamp01(beatStrength);
    const float adv = speedLedsPerSec * strengthSpeed * dt;

    if (m_lubIntensity > 0.001f && m_lubRadius < (float)HALF_LENGTH + 10.0f) {
        m_lubRadius += adv;
        m_lubIntensity *= expf(-rawDt / 0.28f);
    } else {
        m_lubIntensity = 0.0f;
    }

    if (m_dubIntensity > 0.001f && m_dubRadius < (float)HALF_LENGTH + 10.0f) {
        m_dubRadius += adv * 1.10f;
        m_dubIntensity *= expf(-rawDt / 0.22f);
    } else {
        m_dubIntensity = 0.0f;
    }

    // ---------------------------------------------------------------------
    // Render rings (centre-origin, mirrored)
    // ---------------------------------------------------------------------
    constexpr float LUB_WIDTH = 8.0f;
    constexpr float DUB_WIDTH = 6.0f;

    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        CRGB out = CRGB::Black;

        if (m_lubIntensity > 0.001f) {
            float delta = fabsf((float)dist - m_lubRadius);
            if (delta < LUB_WIDTH) {
                float t = 1.0f - (delta / LUB_WIDTH);
                float fade = 1.0f - (m_lubRadius / (float)HALF_LENGTH) * 0.75f;
                float amp = clamp01(t * m_lubIntensity * fade);
                uint8_t b = (uint8_t)(amp * 255.0f);
                b = scale8(b, ctx.brightness);
                uint8_t hue = (uint8_t)(lubHue + (uint8_t)(dist * 2));
                out = addSat(out, ctx.palette.getColor(hue, b));
            }
        }

        if (m_dubIntensity > 0.001f) {
            float delta = fabsf((float)dist - m_dubRadius);
            if (delta < DUB_WIDTH) {
                float t = 1.0f - (delta / DUB_WIDTH);
                float fade = 1.0f - (m_dubRadius / (float)HALF_LENGTH) * 0.78f;
                float amp = clamp01(t * m_dubIntensity * fade);
                uint8_t b = (uint8_t)(amp * 255.0f);
                b = scale8(b, ctx.brightness);
                uint8_t hue = (uint8_t)(dubHue + (uint8_t)(dist * 2));
                out = addSat(out, ctx.palette.getColor(hue, b));
            }
        }

        if (out.r || out.g || out.b) {
            SET_CENTER_PAIR(ctx, dist, out);
        }
    }

    // Subtle centre fill on strong beats (adds “cardiac core” presence).
    if (ctx.audio.available && useAudioBeat) {
        float core = clamp01(beatStrength * 0.35f + ctx.audio.fastRms() * 0.20f);
        if (core > 0.02f) {
            uint8_t b = (uint8_t)(core * 255.0f);
            b = scale8(b, ctx.brightness);
            CRGB coreCol = ctx.palette.getColor((uint8_t)(baseHue + 8), b);
            ctx.leds[ctx.centerPoint - 1] = addSat(ctx.leds[ctx.centerPoint - 1], coreCol);
            ctx.leds[ctx.centerPoint] = addSat(ctx.leds[ctx.centerPoint], coreCol);
        }
    }
}

void HeartbeatEsTunedEffect::cleanup() {}

const plugins::EffectMetadata& HeartbeatEsTunedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Heartbeat (ES tuned)",
        "Beat-locked lub-dub pulses with chroma colour (ES backend tuned)",
        plugins::EffectCategory::AMBIENT,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace lightwaveos::effects::ieffect

