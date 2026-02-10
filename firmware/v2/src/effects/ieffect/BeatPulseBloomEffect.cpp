/**
 * @file BeatPulseBloomEffect.cpp
 * @brief Beat Pulse (Bloom) - subpixel advection transport + temporal feedback
 */

#include "BeatPulseBloomEffect.h"

#include "BeatPulseRenderUtils.h"
#include "BeatPulseTransportCore.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <Arduino.h>
#endif

#include <cmath>
#include <cstring>

namespace lightwaveos::effects::ieffect {

// Shared transport state for this effect instance.
// NOTE: ZoneComposer uses one shared effect instance, so TransportCore itself must be per-zone internally.
static BeatPulseTransportCore g_transport;

// Global debug flag - toggled by 'd' key in main.cpp
bool g_bloomDebugEnabled = false;

static uint32_t g_lastDebugMs = 0;
static constexpr uint32_t DEBUG_INTERVAL_MS = 500;  // Log every 500ms

BeatPulseBloomEffect::BeatPulseBloomEffect()
    : m_meta(
          "Beat Pulse (Bloom)",
          "Bloom-style transport: advected trails + centre injection (liquid motion)",
          plugins::EffectCategory::PARTY,
          1,
          "LightwaveOS") {}

bool BeatPulseBloomEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    for (int i = 0; i < 4; i++) {
        m_beatEnv[i] = 0.0f;
        m_lastBeatMs[i] = 0;
    }
    g_transport.resetAll();
    m_hasEverRendered = false;
    return true;
}

static inline float norm01_u8(uint8_t v) { return static_cast<float>(v) / 255.0f; }

/**
 * @brief Simple saturation control without HSV conversion.
 * sat=255 → unchanged; sat=0 → greyscale.
 */
static inline CRGB applySaturation(CRGB c, uint8_t sat) {
    if (sat >= 255) return c;
    const uint16_t grey = (static_cast<uint16_t>(c.r) + c.g + c.b) / 3;
    const uint16_t s = sat;
    const uint16_t inv = 255 - sat;

    c.r = static_cast<uint8_t>((grey * inv + static_cast<uint16_t>(c.r) * s) / 255);
    c.g = static_cast<uint8_t>((grey * inv + static_cast<uint16_t>(c.g) * s) / 255);
    c.b = static_cast<uint8_t>((grey * inv + static_cast<uint16_t>(c.b) * s) / 255);
    return c;
}

void BeatPulseBloomEffect::render(plugins::EffectContext& ctx) {
    // Lazy safety: if init() was never called (zone system), ensure sane defaults.
    if (!m_hasEverRendered) {
        for (int i = 0; i < 4; i++) {
            m_beatEnv[i] = 0.0f;
            m_lastBeatMs[i] = 0;
        }
        g_transport.resetAll();
        m_hasEverRendered = true;
    }

    // zoneId: 0xFF means global (non-zone mode) → treat as zone 0
    const uint8_t zoneId = (ctx.zoneId == 0xFF) ? 0 : (ctx.zoneId & 0x03);
    const float dt = ctx.getSafeDeltaSeconds();
    const uint32_t nowMs = ctx.totalTimeMs;

    g_transport.setNowMs(nowMs);

    // ---------------------------------------------------------------------
    // Beat source (audio preferred; fallback metronome if no audio)
    // ---------------------------------------------------------------------
    // NOTE: silentScale is NOT applied here - RendererActor handles it globally
    // to avoid double-gating which kills punch.
    bool beatTick = false;
    float beatStrength = 1.0f;

    if (ctx.audio.available) {
        beatTick = ctx.audio.isOnBeat();
        beatStrength = clamp01(ctx.audio.beatStrength());
    } else {
        // Fallback metronome: tie BPM to speed so it stays alive.
        const float speed01 = clamp01(ctx.speed / 100.0f);
        const float bpm = lerp(96.0f, 140.0f, speed01);
        const float beatIntervalMs = 60000.0f / fmaxf(30.0f, bpm);

        if (m_lastBeatMs[zoneId] == 0 || (nowMs - m_lastBeatMs[zoneId]) >= static_cast<uint32_t>(beatIntervalMs)) {
            beatTick = true;
            m_lastBeatMs[zoneId] = nowMs;
        }
    }

    // Beat envelope: slam to 1.0 on beat, dt-correct exponential decay.
    BeatPulseHTML::updateBeatIntensity(m_beatEnv[zoneId], beatTick, dt);

    // ---------------------------------------------------------------------
    // Transport tuning (Sensory Bridge Bloom parity)
    // ---------------------------------------------------------------------
    // Ported from Bloom's draw_sprite():
    //   position = 0.250 + 1.750 * MOOD  (MOOD drives transport speed)
    //   alpha = 0.99                      (persistence)
    //
    // MOOD affects how the light MOVES - low mood = slow/dreamy, high mood = fast/reactive.
    // Speed is just a multiplier keeping the effect lively without stalling.
    //
    const float mood01 = ctx.getMoodNormalized();

    // speed=0..100 -> multiplier 0.70..1.50 (keeps default lively, avoids extreme stalls)
    const float speed01 = clamp01(ctx.speed / 100.0f);
    const float speedMul = lerp(0.70f, 1.50f, speed01);

    // MOOD drives transport: 0.25 pixels/frame at mood=0, up to 2.0 at mood=1
    const float offsetPerFrame60 = (0.250f + 1.750f * mood01) * speedMul;

    // fadeAmount=0..255 -> persistence 0.995..0.90 per 60Hz frame
    // Low fade = long viscous trails, high fade = short snappy trails
    const float fade01 = norm01_u8(ctx.fadeAmount);
    const float persistencePerFrame60 = lerp(0.995f, 0.90f, fade01);

    // complexity=0..100 -> diffusion 0.0..1.0
    const float complexity01 = clamp01(ctx.complexity / 100.0f);
    const float diffusion01 = complexity01;

    // Radial length derived from centrePoint (79 -> 80 bins for a 160 strip).
    const uint16_t radialLen = static_cast<uint16_t>(ctx.centerPoint + 1);

    // Advect + decay (+ optional diffusion)
    g_transport.advectOutward(zoneId, radialLen, offsetPerFrame60, persistencePerFrame60, diffusion01, dt);

#ifndef NATIVE_BUILD
    // Debug output (toggle with 'd' key)
    if (g_bloomDebugEnabled) {
        // Rate-limited transport tuning output (any zone, but show which)
        if ((nowMs - g_lastDebugMs) >= DEBUG_INTERVAL_MS) {
            g_lastDebugMs = nowMs;
            Serial.printf("[BLOOM z%d] mood=%.2f spdMul=%.2f vel=%.2f | fade=%d persist=%.3f | cplx=%d diff=%.2f | dt=%.4f\n",
                zoneId, mood01, speedMul, offsetPerFrame60,
                ctx.fadeAmount, persistencePerFrame60,
                ctx.complexity, diffusion01,
                dt);
        }
        // Immediate beat event logging
        if (beatTick) {
            Serial.printf("[BLOOM z%d] >>> BEAT! strength=%.2f env=%.2f audio=%s\n",
                zoneId, beatStrength, m_beatEnv[zoneId],
                ctx.audio.available ? "yes" : "no(metro)");
        }
    }
#endif

    // ---------------------------------------------------------------------
    // Centre injection (audio → colour + energy)
    // ---------------------------------------------------------------------
    // Colour: palette with chord-root shift when confidence is meaningful.
    uint8_t paletteShift = 0;
    if (ctx.audio.available && ctx.audio.chordConfidence() > 0.20f) {
        // 12 notes → 0..252 shift (wrap naturally in uint8).
        paletteShift = static_cast<uint8_t>(ctx.audio.rootNote() * 21);
    }
    const uint8_t baseIdx = static_cast<uint8_t>(paletteShift + ctx.gHue);
    CRGB inject = ctx.palette.getColor(baseIdx);

    // Apply global saturation knob.
    inject = applySaturation(inject, ctx.saturation);

    // Energy: blend beat slam with continuous drive (rms+flux) so it stays alive between beats.
    float drive = 0.0f;
    if (ctx.audio.available) {
        // These are already normalised-ish in the control bus (0..1 in most cases).
        // NOTE: No silentScale here - renderer handles global silence gating.
        drive = clamp01(ctx.audio.rms() * 0.35f + ctx.audio.fastFlux() * 1.25f + ctx.audio.beatStrength() * 0.25f);
    }

    // intensity=0..100 -> injection gain 0.35..1.0 (keeps visible even at low intensity)
    const float intensity01 = clamp01(ctx.intensity / 100.0f);
    const float injGain = lerp(0.35f, 1.0f, intensity01);

    // Beat env dominates attack; drive fills gaps.
    const float injectAmount = clamp01((0.80f * m_beatEnv[zoneId] + 0.35f * drive) * injGain);

    // White push: subtle specular punch on beats (reduced from 0.10-0.35 to preserve colour)
    const float whitePush01 = clamp01(m_beatEnv[zoneId] * lerp(0.05f, 0.15f, intensity01));
    ColourUtil::addWhiteSaturating(inject, floatToByte(whitePush01));

    // variation=0..100 -> injection spread 0.05..0.85 (low variation = tight core)
    const float variation01 = clamp01(ctx.variation / 100.0f);
    const float spread01 = lerp(0.05f, 0.85f, variation01);

    g_transport.injectAtCentre(zoneId, radialLen, inject, injectAmount, spread01);

#ifndef NATIVE_BUILD
    // Rate-limited injection logging (only if we just logged transport)
    if (g_bloomDebugEnabled && (nowMs - g_lastDebugMs) < 50) {
        Serial.printf("[BLOOM z%d] inj: drive=%.2f amt=%.2f spread=%.2f | rgb=(%d,%d,%d) palIdx=%d\n",
            zoneId, drive, injectAmount, spread01,
            inject.r, inject.g, inject.b,
            baseIdx);
    }
#endif

    // ---------------------------------------------------------------------
    // Output mapping (centre-origin dual strip with palette enhancement)
    // ---------------------------------------------------------------------
    // NOTE: No silentScale here - renderer handles global silence gating.
    const float outGain = clamp01(static_cast<float>(ctx.brightness) / 255.0f);

    // Palette mix: variation=0..100 → 0.15..0.45 (subtle palette tinting as light travels)
    // This creates richer colour variation without losing the transport character.
    const float paletteMix = lerp(0.15f, 0.45f, variation01);

#ifndef NATIVE_BUILD
    // Rate-limited output logging (only if we just logged transport)
    if (g_bloomDebugEnabled && (nowMs - g_lastDebugMs) < 50) {
        Serial.printf("[BLOOM z%d] out: gain=%.2f palMix=%.2f var=%d | radLen=%d\n",
            zoneId, outGain, paletteMix, ctx.variation, radialLen);
    }
#endif

    g_transport.readoutToLedsWithPalette(zoneId, ctx, radialLen, outGain, baseIdx, paletteMix);
}

void BeatPulseBloomEffect::cleanup() {}

const plugins::EffectMetadata& BeatPulseBloomEffect::getMetadata() const { return m_meta; }

uint8_t BeatPulseBloomEffect::getParameterCount() const { return 0; }
const plugins::EffectParameter* BeatPulseBloomEffect::getParameter(uint8_t index) const { (void)index; return nullptr; }
bool BeatPulseBloomEffect::setParameter(const char* name, float value) { (void)name; (void)value; return false; }
float BeatPulseBloomEffect::getParameter(const char* name) const { (void)name; return 0.0f; }

} // namespace lightwaveos::effects::ieffect
