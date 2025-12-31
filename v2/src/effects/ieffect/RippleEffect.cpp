/**
 * @file RippleEffect.cpp
 * @brief Ripple effect implementation
 */

#include "RippleEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

RippleEffect::RippleEffect()
{
    // Initialize all ripples as inactive
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].speed = 0;
        m_ripples[i].hue = 0;
        m_ripples[i].intensity = 255;
    }
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
}

bool RippleEffect::init(plugins::EffectContext& ctx) {
    // Reset all ripples
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].intensity = 255;
    }
    m_lastHopSeq = 0;
    m_spawnCooldown = 0;
    m_lastChromaEnergy = 0.0f;
    m_chromaEnergySum = 0.0f;
    m_chromaHistIdx = 0;
    for (uint8_t i = 0; i < CHROMA_HISTORY; ++i) {
        m_chromaEnergyHist[i] = 0.0f;
    }
    memset(m_radial, 0, sizeof(m_radial));
    memset(m_radialAux, 0, sizeof(m_radialAux));
    m_kickPulse = 0.0f;
    m_trebleShimmer = 0.0f;
    return true;
}

void RippleEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN RIPPLE - Ripples expand from center
    //
    // STATEFUL EFFECT: This effect maintains ripple state (position, speed, hue) across frames
    // in the m_ripples[] array. Ripples spawn at center and expand outward. Identified
    // in PatternRegistry::isStatefulEffect().

    // INCREASED DECAY: Faster fade (20→45) prevents white accumulation from
    // overlapping ripples with WHITE_HEAVY palettes. ~2.25x faster decay.
    fadeToBlackBy(m_radial, HALF_LENGTH, 45);

    const bool hasAudio = ctx.audio.available;
    bool newHop = false;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // =====================================================================
            // 64-bin Sub-Bass Kick Detection (bins 0-5 = 110-155 Hz)
            // Deep kick drums that the 12-bin chromagram misses entirely.
            // Gives powerful punch on bass drops - instant attack, fast decay.
            // =====================================================================
            float kickSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                kickSum += ctx.audio.bin(i);
            }
            float kickAvg = kickSum / 6.0f;
            if (kickAvg > m_kickPulse) {
                m_kickPulse = kickAvg;  // Instant attack
            } else {
                m_kickPulse *= 0.80f;   // ~80ms decay at 60fps for punchy response
            }

            // =====================================================================
            // 64-bin Treble Shimmer (bins 48-63 = 1.3-4.2 kHz)
            // Hi-hat and cymbal energy for wavefront sparkle enhancement.
            // =====================================================================
            float trebleSum = 0.0f;
            for (uint8_t i = 48; i < 64; ++i) {
                trebleSum += ctx.audio.bin(i);
            }
            m_trebleShimmer = trebleSum / 16.0f;
        }
    }
#endif

    uint8_t spawnChance = 0;
    float energyNorm = 0.0f;
    float energyDelta = 0.0f;
    uint8_t dominantBin = 0;
    float energyAvg = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio && newHop) {
        const float led_share = 255.0f / 12.0f;
        float chromaEnergy = 0.0f;
        float maxBinVal = 0.0f;
        for (uint8_t i = 0; i < 12; ++i) {
            float bin = ctx.audio.controlBus.chroma[i];
            float bright = bin;
            bright = bright * bright;
            bright *= 1.5f;
            if (bright > 1.0f) bright = 1.0f;
            if (bright > maxBinVal) {
                maxBinVal = bright;
                dominantBin = i;
            }
            chromaEnergy += bright * led_share;
        }
        energyNorm = chromaEnergy / 255.0f;
        if (energyNorm < 0.0f) energyNorm = 0.0f;
        if (energyNorm > 1.0f) energyNorm = 1.0f;

        m_chromaEnergySum -= m_chromaEnergyHist[m_chromaHistIdx];
        m_chromaEnergyHist[m_chromaHistIdx] = energyNorm;
        m_chromaEnergySum += energyNorm;
        m_chromaHistIdx = (m_chromaHistIdx + 1) % CHROMA_HISTORY;
        energyAvg = m_chromaEnergySum / CHROMA_HISTORY;

        energyDelta = energyNorm - energyAvg;
        if (energyDelta < 0.0f) energyDelta = 0.0f;
        m_lastChromaEnergy = energyNorm;

        float chanceF = energyDelta * 510.0f + energyAvg * 80.0f;
        if (chanceF > 255.0f) chanceF = 255.0f;
        spawnChance = (uint8_t)chanceF;
    } else
#endif
    if (!hasAudio) {
        spawnChance = 0;  // No ripples without audio - was 10, causing animation without audio
    }

    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

    // Spawn new ripples at centre (audio-reactive when available)
    if (hasAudio && !newHop) {
        spawnChance = 0;
    }

    // Calculate chord-based hue shift (palette INDEX offset, not HSV rotation)
    int8_t chordHueShift = 0;
#if FEATURE_AUDIO_SYNC
    if (hasAudio && ctx.audio.chordConfidence() > 0.3f) {
        if (ctx.audio.isMajor()) {
            chordHueShift = 25;   // Warm shift for major chords
        } else if (ctx.audio.isMinor()) {
            chordHueShift = -25;  // Cool shift for minor chords
        }
    }
#endif

    // =========================================================================
    // UNIFIED SPEED SCALING - all ripple types use ctx.speed
    // Range: 1.0 (slow) to 3.0 (fast) based on speed slider
    // =========================================================================
    const float speedScale = 1.0f + 2.0f * (ctx.speed / 50.0f);

    // =========================================================================
    // 64-bin KICK-TRIGGERED RIPPLE (sub-bass bins 0-5)
    // Most powerful ripple spawn - deep bass drops that chromagram misses.
    // Bypasses normal spawn chance for immediate punchy response.
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio && m_kickPulse > 0.5f && m_spawnCooldown == 0) {
        for (uint8_t r = 0; r < MAX_RIPPLES; ++r) {
            if (!m_ripples[r].active) {
                m_ripples[r].radius = 0.0f;      // Start at center
                m_ripples[r].intensity = 255;    // Max intensity for bass punch
                m_ripples[r].speed = speedScale; // Uses unified speed from slider
                // Bass-driven warm hue (palette index based on kick intensity)
                m_ripples[r].hue = ctx.gHue + (uint8_t)(m_kickPulse * 30.0f) + chordHueShift;
                m_ripples[r].active = true;
                m_spawnCooldown = 2;             // Prevent overlapping bass ripples
                break;
            }
        }
    }
#endif

    // Force ripple spawn on snare hit (percussion trigger)
#if FEATURE_AUDIO_SYNC
    if (hasAudio && ctx.audio.isSnareHit()) {
        for (uint8_t r = 0; r < MAX_RIPPLES; ++r) {
            if (!m_ripples[r].active) {
                m_ripples[r].radius = 0.0f;      // Reset at center
                m_ripples[r].intensity = 255;    // Max intensity for snare burst
                m_ripples[r].speed = speedScale; // Uses unified speed from slider
                // Use chord root note for hue (0-11 mapped to palette index range)
                m_ripples[r].hue = (uint8_t)((ctx.audio.rootNote() * 21) + chordHueShift);
                m_ripples[r].active = true;
                m_spawnCooldown = 1;             // Brief cooldown
                break;
            }
        }
    }
#endif

    if (spawnChance > 0 && m_spawnCooldown == 0 && random8() < spawnChance) {
        for (int i = 0; i < MAX_RIPPLES; i++) {
            if (!m_ripples[i].active) {
                // Random variation around speedScale (0.5 to 1.5x speedScale)
                float speedVariation = 0.5f + (random8() / 255.0f);
                uint8_t intensity = 180;
                if (hasAudio) {
                    float energy = energyAvg;
                    // Energy boost adds 0 to 0.5x variation
                    speedVariation += (energy * 0.3f + energyDelta * 0.2f);
                    float intensityF = 100.0f + energy * 155.0f + energyDelta * 100.0f;
                    if (intensityF > 255.0f) intensityF = 255.0f;
                    intensity = (uint8_t)intensityF;
                }

                m_ripples[i].radius = 0;
                m_ripples[i].speed = speedScale * speedVariation;
                if (m_ripples[i].speed > speedScale * 1.5f) m_ripples[i].speed = speedScale * 1.5f;
                if (hasAudio) {
                    // Apply chord warmth shift to chroma-based hue
                    float hueBase = (dominantBin * (255.0f / 12.0f)) + ctx.gHue;
                    m_ripples[i].hue = (uint8_t)(hueBase + chordHueShift);
                } else {
                    m_ripples[i].hue = random8();
                }
                m_ripples[i].intensity = intensity;
                m_ripples[i].active = true;
                m_spawnCooldown = hasAudio ? 1 : 4;
                break;
            }
        }
    }

    // Update and render ripples
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) continue;

        m_ripples[r].radius += m_ripples[r].speed * (ctx.speed / 10.0f);

        if (m_ripples[r].radius > HALF_LENGTH) {
            m_ripples[r].active = false;
            continue;
        }

        // Draw ripple into radial history buffer
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float wavePos = (float)dist - m_ripples[r].radius;
            float waveAbs = fabsf(wavePos);
            if (waveAbs < 3.0f) {
                uint8_t brightness = 255 - (uint8_t)(waveAbs * 85.0f);
                uint8_t edgeFade = (uint8_t)((HALF_LENGTH - m_ripples[r].radius) * 255.0f / HALF_LENGTH);
                brightness = scale8(brightness, edgeFade);
                brightness = scale8(brightness, m_ripples[r].intensity);

                // =========================================================
                // 64-bin TREBLE SHIMMER on wavefront (bins 48-63)
                // Hi-hat/cymbal energy adds sparkle to the leading edge.
                // Stronger at wavefront (waveAbs~0), fades toward trailing edge.
                // =========================================================
                if (m_trebleShimmer > 0.1f) {
                    float shimmerFade = 1.0f - (waveAbs / 3.0f);  // 1.0 at front, 0.0 at back
                    uint8_t shimmerBoost = (uint8_t)(m_trebleShimmer * shimmerFade * 60.0f);
                    brightness = qadd8(brightness, shimmerBoost);
                }

                CRGB color = ctx.palette.getColor(
                    m_ripples[r].hue + (uint8_t)dist,
                    brightness);
                m_radial[dist].r = qadd8(m_radial[dist].r, color.r);
                m_radial[dist].g = qadd8(m_radial[dist].g, color.g);
                m_radial[dist].b = qadd8(m_radial[dist].b, color.b);
            }
        }
    }

    memcpy(m_radialAux, m_radial, sizeof(m_radial));

    // Render radial history buffer to LEDs (centre-origin)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_radialAux[dist]);
    }
}

void RippleEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& RippleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple",
        "Audio-reactive ripples expanding from centre",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
