// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPBeatPulseEffect.cpp
 * @brief Beat-synchronized radial pulse implementation
 */

#include "LGPBeatPulseEffect.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool LGPBeatPulseEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Primary kick/beat pulse
    m_pulsePosition = 0.0f;
    m_pulseIntensity = 0.0f;
    m_fallbackPhase = 0.0f;
    m_lastBeatPhase = 0.0f;

    // Snare detection and secondary pulse
    m_lastMidEnergy = 0.0f;
    m_snarePulsePos = 0.0f;
    m_snarePulseInt = 0.0f;

    // Hi-hat detection and shimmer
    m_lastTrebleEnergy = 0.0f;
    m_hihatShimmer = 0.0f;

    return true;
}

void LGPBeatPulseEffect::render(plugins::EffectContext& ctx) {
    float beatPhase;
    float bassEnergy;
    float midEnergy = 0.0f;
    float trebleEnergy = 0.0f;
    bool onBeat = false;
    bool snareHit = false;
    bool hihatHit = false;

    // Spike detection thresholds
    constexpr float SNARE_SPIKE_THRESH = 0.25f;   // Mid energy spike threshold
    constexpr float HIHAT_SPIKE_THRESH = 0.20f;   // Treble energy spike threshold

    if (ctx.audio.available) {
        beatPhase = ctx.audio.beatPhase();
        bassEnergy = ctx.audio.bass();
        midEnergy = ctx.audio.mid();
        trebleEnergy = ctx.audio.treble();
        onBeat = ctx.audio.isOnBeat();

        // Snare detection: spike in mid-frequency energy
        float midDelta = midEnergy - m_lastMidEnergy;
        if (midDelta > SNARE_SPIKE_THRESH && midEnergy > 0.4f) {
            snareHit = true;
        }

        // Hi-hat detection: spike in high-frequency energy
        float trebleDelta = trebleEnergy - m_lastTrebleEnergy;
        if (trebleDelta > HIHAT_SPIKE_THRESH && trebleEnergy > 0.3f) {
            hihatHit = true;
        }

        // Store for next frame
        m_lastMidEnergy = midEnergy;
        m_lastTrebleEnergy = trebleEnergy;
    } else {
        // Fallback: 120 BPM with simulated snare on off-beats
        m_fallbackPhase += ((ctx.deltaTimeSeconds * 1000.0f) / 500.0f);
        if (m_fallbackPhase >= 1.0f) m_fallbackPhase -= 1.0f;
        beatPhase = m_fallbackPhase;
        bassEnergy = 0.5f;

        // Detect beat crossing (kick)
        if (beatPhase < 0.05f && m_lastBeatPhase > 0.95f) {
            onBeat = true;
        }
        // Simulate snare on off-beat (around 0.5 phase)
        if (beatPhase > 0.48f && beatPhase < 0.52f && m_lastBeatPhase < 0.48f) {
            snareHit = true;
        }
        // Simulate hi-hat every eighth note
        float hihatPhase = fmodf(beatPhase * 4.0f, 1.0f);
        float lastHihatPhase = fmodf(m_lastBeatPhase * 4.0f, 1.0f);
        if (hihatPhase < 0.1f && lastHihatPhase > 0.9f) {
            hihatHit = true;
        }
    }
    m_lastBeatPhase = beatPhase;

    // === PRIMARY PULSE (Kick/Beat) ===
    if (onBeat) {
        m_pulsePosition = 0.0f;
        m_pulseIntensity = 0.3f + bassEnergy * 0.7f;
    }

    // Expand pulse outward (~400ms to reach edge)
    m_pulsePosition += (ctx.deltaTimeSeconds * 1000.0f) / 400.0f;
    if (m_pulsePosition > 1.0f) m_pulsePosition = 1.0f;

    // Decay intensity
    m_pulseIntensity *= 0.95f;
    if (m_pulseIntensity < 0.01f) m_pulseIntensity = 0.0f;

    // === SECONDARY PULSE (Snare) ===
    if (snareHit) {
        m_snarePulsePos = 0.0f;
        m_snarePulseInt = 0.6f + midEnergy * 0.4f;
    }

    // Snare pulse expands faster (~300ms)
    m_snarePulsePos += (ctx.deltaTimeSeconds * 1000.0f) / 300.0f;
    if (m_snarePulsePos > 1.0f) m_snarePulsePos = 1.0f;

    // Faster decay for snare
    m_snarePulseInt *= 0.92f;
    if (m_snarePulseInt < 0.01f) m_snarePulseInt = 0.0f;

    // === SHIMMER OVERLAY (Hi-hat) ===
    if (hihatHit) {
        m_hihatShimmer = 0.8f + trebleEnergy * 0.2f;
    }

    // Very fast decay for hi-hat sparkle (~150ms)
    m_hihatShimmer *= 0.88f;
    if (m_hihatShimmer < 0.01f) m_hihatShimmer = 0.0f;

    // Clear buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

    // === RENDER CENTER PAIR OUTWARD ===
    for (int dist = 0; dist < HALF_LENGTH; ++dist) {
        float normalizedDist = (float)dist / HALF_LENGTH;

        // --- Primary pulse ring (kick) ---
        float ringDist = fabsf(normalizedDist - m_pulsePosition);
        float ringWidth = 0.15f;
        float primaryBright = 0.0f;

        if (ringDist < ringWidth) {
            primaryBright = (1.0f - ringDist / ringWidth) * m_pulseIntensity;
        }

        // --- Secondary pulse ring (snare) - thinner, faster ---
        float snareRingDist = fabsf(normalizedDist - m_snarePulsePos);
        float snareRingWidth = 0.08f;  // Thinner than primary
        float snareBright = 0.0f;

        if (snareRingDist < snareRingWidth) {
            snareBright = (1.0f - snareRingDist / snareRingWidth) * m_snarePulseInt;
        }

        // --- Hi-hat shimmer overlay ---
        // Sparkle pattern: pseudo-random based on position and time
        float shimmerBright = 0.0f;
        if (m_hihatShimmer > 0.05f) {
            // Create sparkle pattern using position hash
            uint8_t sparkleHash = (uint8_t)((dist * 73 + (int)(beatPhase * 256)) & 0xFF);
            if ((sparkleHash & 0x0F) < 3) {  // ~20% of LEDs sparkle
                shimmerBright = m_hihatShimmer * ((sparkleHash >> 4) / 16.0f);
            }
        }

        // Background glow based on beat phase
        float bgBright = 0.08f + beatPhase * 0.12f;

        // --- Combine all layers ---
        // Primary pulse uses base hue
        uint8_t primaryHue = ctx.gHue + (uint8_t)(beatPhase * 64);

        // Snare uses complementary hue (+128 = opposite on color wheel)
        uint8_t snareHue = primaryHue + 128;

        // Hi-hat uses offset hue (+64)
        uint8_t hihatHue = primaryHue + 64;

        // Calculate combined color
        CRGB finalColor = CRGB::Black;

        // Layer 1: Background glow
        uint8_t bgBrightness = (uint8_t)(bgBright * ctx.brightness);
        finalColor = ctx.palette.getColor(primaryHue, bgBrightness);

        // Layer 2: Primary pulse (additive blend)
        if (primaryBright > 0.01f) {
            uint8_t pBright = (uint8_t)(primaryBright * ctx.brightness);
            CRGB primaryColor = ctx.palette.getColor(primaryHue, pBright);
            finalColor.r = (finalColor.r + primaryColor.r > 255) ? 255 : finalColor.r + primaryColor.r;
            finalColor.g = (finalColor.g + primaryColor.g > 255) ? 255 : finalColor.g + primaryColor.g;
            finalColor.b = (finalColor.b + primaryColor.b > 255) ? 255 : finalColor.b + primaryColor.b;
        }

        // Layer 3: Snare pulse (additive blend with complementary color)
        if (snareBright > 0.01f) {
            uint8_t sBright = (uint8_t)(snareBright * ctx.brightness * 0.7f);  // Slightly dimmer
            CRGB snareColor = ctx.palette.getColor(snareHue, sBright);
            finalColor.r = (finalColor.r + snareColor.r > 255) ? 255 : finalColor.r + snareColor.r;
            finalColor.g = (finalColor.g + snareColor.g > 255) ? 255 : finalColor.g + snareColor.g;
            finalColor.b = (finalColor.b + snareColor.b > 255) ? 255 : finalColor.b + snareColor.b;
        }

        // Layer 4: Hi-hat shimmer (additive sparkle)
        if (shimmerBright > 0.01f) {
            uint8_t hBright = (uint8_t)(shimmerBright * ctx.brightness * 0.5f);  // Subtle
            CRGB hihatColor = ctx.palette.getColor(hihatHue, hBright);
            finalColor.r = (finalColor.r + hihatColor.r > 255) ? 255 : finalColor.r + hihatColor.r;
            finalColor.g = (finalColor.g + hihatColor.g > 255) ? 255 : finalColor.g + hihatColor.g;
            finalColor.b = (finalColor.b + hihatColor.b > 255) ? 255 : finalColor.b + hihatColor.b;
        }

        SET_CENTER_PAIR(ctx, dist, finalColor);
    }
}

void LGPBeatPulseEffect::cleanup() {}

const plugins::EffectMetadata& LGPBeatPulseEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Beat Pulse",
        "Radial pulse with snare/hihat response",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
