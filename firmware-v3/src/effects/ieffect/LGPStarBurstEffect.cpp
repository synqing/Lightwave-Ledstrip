// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPStarBurstEffect.cpp
 * @brief LGP Star Burst effect - explosive radial lines from center
 *
 * REPAIRED: Simplified to match Wave Collision's proven pattern.
 * Previous version was over-engineered with 9+ audio inputs causing chaos.
 *
 * Pattern: CENTER_ORIGIN radial waves with snare-driven bursts
 *
 * Audio Integration (Wave Collision pattern):
 * - Heavy Bass → Speed modulation (slew-limited)
 * - Snare Hit → Burst flash (center-focused)
 * - Chroma → Color (dominant bin for hue offset)
 */

#include "LGPStarBurstEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPStarBurstEffect::LGPStarBurstEffect()
    : m_phase(0.0f)
{
}

bool LGPStarBurstEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_burst = 0.0f;
    m_lastHopSeq = 0;
    m_dominantBin = 0;
    m_dominantBinSmooth = 0.0f;

    // Initialize spring physics for natural speed momentum
    m_phaseSpeedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
    m_phaseSpeedSpring.reset(1.0f);        // Start at base speed
    
    // Initialize smoothing
    m_heavyBassSmooth = 0.0f;
    m_heavyBassSmoothInitialized = false;

    return true;
}

void LGPStarBurstEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Star-like patterns radiating from centre
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    const bool hasAudio = ctx.audio.available;

    // =========================================================================
    // Audio Analysis (per-hop, like Wave Collision)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // Simple chroma analysis for color (dominant bin only)
            float maxBinVal = 0.0f;
            for (uint8_t i = 0; i < 12; ++i) {
                float bin = ctx.audio.controlBus.chroma[i];
                if (bin > maxBinVal) {
                    maxBinVal = bin;
                    m_dominantBin = i;
                }
            }

            // Snare = burst (SIMPLE - like Wave Collision)
            if (ctx.audio.isSnareHit()) {
                m_burst = 1.0f;
            }
        }
    }
#endif

    // =========================================================================
    // Per-frame Updates (smooth animation)
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();
    if (dt > 0.1f) dt = 0.1f;  // Clamp for safety

    // Smooth dominant bin (for color stability)
    float alphaBin = dt / (0.25f + dt);
    m_dominantBinSmooth += (m_dominantBin - m_dominantBinSmooth) * alphaBin;
    if (m_dominantBinSmooth < 0.0f) m_dominantBinSmooth = 0.0f;
    if (m_dominantBinSmooth > 11.0f) m_dominantBinSmooth = 11.0f;

    // Use heavy_bands for speed (like Wave Collision) with EMA smoothing
    float heavyEnergy = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        float rawHeavyBass = ctx.audio.heavyBass();
        
        // EMA smoothing with frame-rate-independent alpha (tau = 50ms)
        const float tau = 0.05f;
        float alpha = 1.0f - expf(-dt / tau);
        
        // CRITICAL: Initialize to raw value on first frame (no ramp-from-zero)
        if (!m_heavyBassSmoothInitialized) {
            m_heavyBassSmooth = rawHeavyBass;
            m_heavyBassSmoothInitialized = true;
        } else {
            m_heavyBassSmooth += (rawHeavyBass - m_heavyBassSmooth) * alpha;
        }
        
        heavyEnergy = m_heavyBassSmooth;
    }
#endif

    // Spring physics for speed modulation (natural momentum, no jitter)
    float targetSpeed = 0.7f + 0.6f * heavyEnergy;
    float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, dt);
    if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;
    if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;  // Prevent stalling

    // Phase advancement (PROVEN PATTERN - 240.0f multiplier)
    m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at 100*2π

    // Simple burst decay (like Wave Collision)
    m_burst *= 0.88f;

    // =========================================================================
    // Rendering
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Anti-aliased burst core at true center (79.5) using SubpixelRenderer
    if (m_burst > 0.05f) {
        uint8_t baseHue = (uint8_t)(ctx.gHue + m_dominantBinSmooth * (255.0f / 12.0f));
        CRGB burstColor = ctx.palette.getColor(baseHue, 255);
        uint8_t burstBright = (uint8_t)(m_burst * 200.0f * intensityNorm);

        // Render bright core at fractional center (between LED 79 and 80)
        enhancement::SubpixelRenderer::renderPoint(
            ctx.leds, STRIP_LENGTH, 79.5f, burstColor, burstBright
        );

        // Also render on strip 2
        if (STRIP_LENGTH * 2 <= ctx.ledCount) {
            enhancement::SubpixelRenderer::renderPoint(
                ctx.leds + STRIP_LENGTH, STRIP_LENGTH, 79.5f,
                ctx.palette.getColor(baseHue + 90, 255), burstBright
            );
        }
    }

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float distFromCenter = (float)centerPairDistance((uint16_t)i);

        // FIXED frequency - no kick modulation (like Wave Collision)
        const float freqBase = 0.25f;
        float star = sinf(distFromCenter * freqBase - m_phase);

        // Center-focused burst flash (like Wave Collision's collision flash)
        float burstFlash = m_burst * expf(-distFromCenter * 0.12f);

        // Simple audio gain (like Wave Collision)
        float audioGain = 0.5f + 0.5f * heavyEnergy;
        float pattern = star * audioGain + burstFlash * 0.8f;

        // tanhf for uniform brightness (PROVEN PATTERN)
        pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;

        uint8_t brightness = (uint8_t)(pattern * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(distFromCenter * 2.0f + pattern * 50.0f);
        uint8_t baseHue = (uint8_t)(ctx.gHue + m_dominantBinSmooth * (255.0f / 12.0f));

        ctx.leds[i] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex), brightness);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ctx.palette.getColor((uint8_t)(baseHue + paletteIndex + 90), brightness);
        }
    }
}

void LGPStarBurstEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPStarBurstEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Star Burst",
        "Explosive radial lines",
        plugins::EffectCategory::GEOMETRIC,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
