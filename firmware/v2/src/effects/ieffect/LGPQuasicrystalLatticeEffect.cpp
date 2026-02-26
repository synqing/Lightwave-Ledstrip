/**
 * @file LGPQuasicrystalLatticeEffect.cpp
 * @brief LGP Quasicrystal Lattice - Penrose-like incommensurate interference lattice
 *
 * Five spatial sine components at Fibonacci-ratio frequencies (13, 21, 34, 55, 89)
 * summed and passed through nonlinear threshold extraction to produce crisp
 * quasi-periodic lattice nodes radiating from centre.
 *
 * Audio: circularChromaHueSmoothed for hue, RMS modulates time phase speed,
 * beat briefly brightens lattice nodes.
 */

#include "LGPQuasicrystalLatticeEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPQuasicrystalLatticeEffect::LGPQuasicrystalLatticeEffect()
    : m_timeA(0.0f)
    , m_timeB(0.0f)
    , m_chromaAngle(0.0f)
    , m_rmsSmooth(0.0f)
    , m_beatFlash(0.0f)
{
}

bool LGPQuasicrystalLatticeEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_timeA = 0.0f;
    m_timeB = 0.0f;
    m_chromaAngle = 0.0f;
    m_rmsSmooth = 0.0f;
    m_beatFlash = 0.0f;
    return true;
}

void LGPQuasicrystalLatticeEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // TIMING
    // =========================================================================
    const float rawDt = ctx.getSafeRawDeltaSeconds();
    const float dt    = ctx.getSafeDeltaSeconds();
    const float speedNorm = ctx.speed / 50.0f;

    // Base time advance rates (slow drift for quasi-static lattice)
    float timeRateA = 18.0f * speedNorm;
    float timeRateB = 12.0f * speedNorm;

    // Default audio-derived values
    uint8_t chromaHueOffset = 0;

    // =========================================================================
    // AUDIO REACTIVITY
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // --- Chroma hue via circular weighted mean + circular EMA ---
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            ctx.audio.controlBus.heavy_chroma, m_chromaAngle, rawDt, 0.22f);

        // --- RMS modulates time-phase speed (smooth follower) ---
        const float rmsTarget = ctx.audio.rms();
        const float rmsAlpha = 1.0f - expf(-4.0f * rawDt);
        m_rmsSmooth += (rmsTarget - m_rmsSmooth) * rmsAlpha;
        // Boost time rate by up to 60% at full RMS
        timeRateA *= (1.0f + 0.6f * m_rmsSmooth);
        timeRateB *= (1.0f + 0.6f * m_rmsSmooth);

        // --- Beat flash: brief brightness boost on beat ---
        if (ctx.audio.isOnBeat()) {
            const float strength = ctx.audio.beatStrength();
            if (strength > m_beatFlash) {
                m_beatFlash = fminf(strength, 1.0f);
            }
        }
    }
#endif

    // Decay beat flash (dt-corrected: ~0.88 per frame at 60fps)
    m_beatFlash = effects::chroma::dtDecay(m_beatFlash, 0.88f, rawDt);

    // =========================================================================
    // PHASE ACCUMULATION
    // =========================================================================
    m_timeA += timeRateA * dt;
    m_timeB += timeRateB * dt;

    // Wrap to prevent float precision loss after long runtime
    // Using 1024.0 (power of 2) for clean wrap boundary
    if (m_timeA > 1024.0f) m_timeA -= 1024.0f;
    if (m_timeB > 1024.0f) m_timeB -= 1024.0f;

    // Integer time phases for sin8 (0-255 domain)
    const uint8_t tA  = (uint8_t)(int)(m_timeA) & 0xFF;
    const uint8_t tB  = (uint8_t)(int)(m_timeB) & 0xFF;
    const uint8_t tA2 = (uint8_t)(int)(m_timeA * 0.5f) & 0xFF;
    const uint8_t tB2 = (uint8_t)(int)(m_timeB * 0.5f) & 0xFF;
    const uint8_t tA4 = (uint8_t)(int)(m_timeA * 0.25f) & 0xFF;

    // Maximum distance from centre (for envelope normalisation)
    const uint16_t maxD = HALF_LENGTH;  // 80

    // =========================================================================
    // FADE FOR TRAILS
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // =========================================================================
    // RENDER LOOP (per strip-local LED, mirrored to strip 2)
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        const uint16_t d = centerPairDistance(i);

        // Cast to uint8_t for sin8 -- wraps naturally in 8-bit domain
        const uint8_t d8 = (uint8_t)(d & 0xFF);

        // =================================================================
        // 5 INCOMMENSURATE SPATIAL COMPONENTS (Fibonacci multipliers)
        // Each uses a different time phase offset for independent drift
        // =================================================================
        const uint8_t a = sin8((uint8_t)(d8 * 13 + tA));
        const uint8_t b = sin8((uint8_t)(d8 * 21 + tB));
        const uint8_t c = sin8((uint8_t)(d8 * 34 + tA2));
        const uint8_t e = sin8((uint8_t)(d8 * 55 + tB2));
        const uint8_t f = sin8((uint8_t)(d8 * 89 + tA4 + 40));

        // Average of 5 components (0..255)
        const uint16_t sum = (uint16_t)a + (uint16_t)b + (uint16_t)c
                           + (uint16_t)e + (uint16_t)f;
        const uint8_t avg = (uint8_t)(sum / 5);

        // =================================================================
        // NONLINEAR LATTICE EXTRACTION
        // Subtract threshold, square for sharpening, boost contrast
        // This turns a smooth wave sum into crisp node/antinode structure
        // =================================================================
        uint8_t hi = qsub8(avg, 150);          // Threshold: only peaks survive
        hi = scale8(hi, hi);                     // Square for sharpening
        hi = qadd8(hi, hi >> 1);                 // +50% contrast boost

        // =================================================================
        // CENTRE ENVELOPE (brighter at centre, darker at edges)
        // =================================================================
        const uint8_t env = (uint8_t)(255 - (d * 255 / maxD));
        const uint8_t v = scale8(hi, qadd8(70, env >> 1));

        // Apply master brightness
        const uint8_t brightness = scale8(v, ctx.brightness);

        // Beat flash: additive boost to lattice nodes
        const uint8_t beatBoost = (uint8_t)(m_beatFlash * 40.0f);
        const uint8_t finalBri = qadd8(brightness, scale8(beatBoost, hi));

        // =================================================================
        // COLOUR: Palette + chroma offset, secondary sheen layer
        // =================================================================
        const uint8_t baseHue = ctx.gHue + chromaHueOffset + (uint8_t)(d >> 2);
        const CRGB primary = ctx.palette.getColor(baseHue, finalBri);

        // Secondary "sheen" layer: hue+30, lower intensity for depth
        const uint8_t sheenBri = scale8(finalBri, 80);
        const CRGB sheen = ctx.palette.getColor((uint8_t)(baseHue + 30), sheenBri);

        // Blend primary + sheen
        CRGB pixel = primary;
        pixel.r = qadd8(pixel.r, sheen.r);
        pixel.g = qadd8(pixel.g, sheen.g);
        pixel.b = qadd8(pixel.b, sheen.b);

        // --- Strip 1 ---
        ctx.leds[i] = pixel;

        // --- Strip 2: different lattice orientation (+128 phase) and hue (+25) ---
        const uint16_t j = i + STRIP_LENGTH;
        if (j < ctx.ledCount) {
            const uint8_t a2 = sin8((uint8_t)(d8 * 13 + tA + 128));
            const uint8_t b2 = sin8((uint8_t)(d8 * 21 + tB + 128));
            const uint8_t c2 = sin8((uint8_t)(d8 * 34 + tA2 + 128));
            const uint8_t e2 = sin8((uint8_t)(d8 * 55 + tB2 + 128));
            const uint8_t f2 = sin8((uint8_t)(d8 * 89 + tA4 + 40 + 128));

            const uint16_t sum2 = (uint16_t)a2 + (uint16_t)b2 + (uint16_t)c2
                                + (uint16_t)e2 + (uint16_t)f2;
            const uint8_t avg2 = (uint8_t)(sum2 / 5);

            uint8_t hi2 = qsub8(avg2, 150);
            hi2 = scale8(hi2, hi2);
            hi2 = qadd8(hi2, hi2 >> 1);

            const uint8_t v2 = scale8(hi2, qadd8(70, env >> 1));
            const uint8_t bri2 = scale8(v2, ctx.brightness);
            const uint8_t finalBri2 = qadd8(bri2, scale8(beatBoost, hi2));

            const uint8_t hue2 = ctx.gHue + chromaHueOffset + 25 + (uint8_t)(d >> 2);
            const CRGB primary2 = ctx.palette.getColor(hue2, finalBri2);
            const uint8_t sheenBri2 = scale8(finalBri2, 80);
            const CRGB sheen2 = ctx.palette.getColor((uint8_t)(hue2 + 30), sheenBri2);

            CRGB pixel2 = primary2;
            pixel2.r = qadd8(pixel2.r, sheen2.r);
            pixel2.g = qadd8(pixel2.g, sheen2.g);
            pixel2.b = qadd8(pixel2.b, sheen2.b);

            ctx.leds[j] = pixel2;
        }
    }
}

void LGPQuasicrystalLatticeEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPQuasicrystalLatticeEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Quasicrystal Lattice",
        "Penrose-like incommensurate interference lattice with 5 Fibonacci spatial modes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
