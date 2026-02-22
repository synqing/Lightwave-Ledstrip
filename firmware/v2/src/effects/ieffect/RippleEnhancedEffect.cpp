/**
 * @file RippleEnhancedEffect.cpp
 * @brief Ripple Enhanced effect - Enhanced version with improved thresholds and snare triggers
 */

#include "RippleEnhancedEffect.h"
#include "ChromaUtils.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

RippleEnhancedEffect::RippleEnhancedEffect()
    : m_ps(nullptr)
{
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0;
        m_ripples[i].speed = 0;
        m_ripples[i].hue = 0;
        m_ripples[i].intensity = 255;
    }
}

bool RippleEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
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
    m_lastBeatState = false;
    m_lastDownbeatState = false;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<RippleEnhancedPsram*>(
            heap_caps_malloc(sizeof(RippleEnhancedPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(RippleEnhancedPsram));
    for (uint8_t i = 0; i < 12; i++) {
        m_ps->chromaFollowers[i].reset(0.0f);
    }
#endif
    m_chromaAngle = 0.0f;
    m_kickFollower.reset(0.0f);
    m_trebleFollower.reset(0.0f);
    m_kickPulse = 0.0f;
    m_trebleShimmer = 0.0f;
    m_targetKick = 0.0f;
    m_targetTreble = 0.0f;
    return true;
}

void RippleEnhancedEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    fadeToBlackBy(m_ps->radial, HALF_LENGTH, ctx.fadeAmount);

    const bool hasAudio = ctx.audio.available;
    bool newHop = false;
    bool beatOnset = false;
    bool downbeatOnset = false;
    float rawDt = ctx.getSafeRawDeltaSeconds();
    float dt = ctx.getSafeDeltaSeconds();
    float moodNorm = ctx.getMoodNormalized();

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // Beat/downbeat edge detection (not hop-gated)
        const bool currentBeat = ctx.audio.isOnBeat();
        const bool currentDownbeat = ctx.audio.isOnDownbeat();
        beatOnset = currentBeat && !m_lastBeatState;
        downbeatOnset = currentDownbeat && !m_lastDownbeatState;
        m_lastBeatState = currentBeat;
        m_lastDownbeatState = currentDownbeat;

        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;

            // =====================================================================
            // 64-bin Sub-Bass Kick Detection (bins 0-5 = 110-155 Hz)
            // Deep kick drums that the 12-bin chromagram misses entirely.
            // =====================================================================
            float kickSum = 0.0f;
            for (uint8_t i = 0; i < 6; ++i) {
                kickSum += ctx.audio.bin(i);
            }
            m_targetKick = kickSum / 6.0f;

            // =====================================================================
            // 64-bin Treble Shimmer (bins 48-63 = 1.3-4.2 kHz)
            // Hi-hat and cymbal energy for wavefront sparkle enhancement.
            // =====================================================================
            float trebleSum = 0.0f;
            for (uint8_t i = 48; i < 64; ++i) {
                trebleSum += ctx.audio.bin(i);
            }
            m_targetTreble = trebleSum / 16.0f;

            // Update chromagram targets
            for (uint8_t i = 0; i < 12; i++) {
                m_ps->chromaTargets[i] = ctx.audio.controlBus.heavy_chroma[i];
            }
        }

        // Smooth toward targets every frame with MOOD-adjusted smoothing
        m_kickPulse = m_kickFollower.updateWithMood(m_targetKick, rawDt, moodNorm);
        m_trebleShimmer = m_trebleFollower.updateWithMood(m_targetTreble, rawDt, moodNorm);

        // Smooth chromagram with AsymmetricFollower
        for (uint8_t i = 0; i < 12; i++) {
            m_ps->chromaSmoothed[i] = m_ps->chromaFollowers[i].updateWithMood(
                m_ps->chromaTargets[i], rawDt, moodNorm);
        }
    } else {
        m_lastBeatState = false;
        m_lastDownbeatState = false;
    }
#endif

    uint8_t spawnChance = 0;
    float energyNorm = 0.0f;
    float energyDelta = 0.0f;
    float energyAvg = 0.0f;

    // Circular chroma hue (prevents argmax discontinuities and wrapping artefacts).
    uint8_t chromaHueOffset = 0;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        const float* chroma = ctx.audio.controlBus.chroma;
        chromaHueOffset = effects::chroma::circularChromaHueSmoothed(
            chroma, m_chromaAngle, rawDt, 0.20f);
    }
#endif

#if FEATURE_AUDIO_SYNC
    if (hasAudio && newHop) {
        const float led_share = 255.0f / 12.0f;
        float chromaEnergy = 0.0f;
        for (uint8_t i = 0; i < 12; ++i) {
            float bin = m_ps->chromaSmoothed[i];  // Use smoothed chromagram
            // FIX: Use sqrt scaling instead of squaring to preserve low-level signals
            float bright = sqrtf(bin) * 1.5f;
            if (bright > 1.0f) bright = 1.0f;
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
        // FIX: Allow small negative delta to still contribute (halve threshold)
        if (energyDelta < -0.1f) energyDelta = 0.0f;
        else if (energyDelta < 0.0f) energyDelta = 0.0f;  // Clamp to zero but don't skip
        m_lastChromaEnergy = energyNorm;

        // FIX: Lower spawn threshold multipliers for more activity
        float chanceF = energyDelta * 400.0f + energyAvg * 120.0f;
        if (chanceF > 255.0f) chanceF = 255.0f;
        spawnChance = (uint8_t)chanceF;
    } else
#endif
    if (!hasAudio) {
        spawnChance = 0;  // No ripples without audio - audio is mandatory for this product
    }

    if (m_spawnCooldown > 0) {
        m_spawnCooldown--;
    }

    // Spawn new ripples at centre (audio-reactive when available)
    if (hasAudio && !newHop) {
        spawnChance = 0;
    }

    // Enhanced: No chord reactivity (removed - not proven feature)

    // =========================================================================
    // UNIFIED SPEED SCALING - all ripple types use ctx.speed
    // Range: 1.0 (slow) to 3.0 (fast) based on speed slider
    // =========================================================================
    const float speedScale = 1.0f + 2.0f * (ctx.speed / 50.0f);

    // =========================================================================
    // BEAT/DOWNBEAT EDGE RIPPLE (latched, not hop-gated)
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio && (beatOnset || downbeatOnset)) {
        for (uint8_t r = 0; r < MAX_RIPPLES; ++r) {
            if (!m_ripples[r].active) {
                m_ripples[r].radius = 0.0f;
                m_ripples[r].intensity = downbeatOnset ? 255 : 210;
                m_ripples[r].speed = speedScale * (downbeatOnset ? 1.2f : 1.0f);
                float harmonicShift = ctx.audio.harmonicSaliency() * 40.0f;
                m_ripples[r].hue = ctx.gHue + (uint8_t)harmonicShift;
                m_ripples[r].active = true;
                m_spawnCooldown = 1;
                break;
            }
        }
    }
#endif

    // =========================================================================
    // 64-bin KICK-TRIGGERED RIPPLE (sub-bass bins 0-5)
    // Most powerful ripple spawn - deep bass drops that chromagram misses.
    // Bypasses normal spawn chance for immediate punchy response.
    // =========================================================================
#if FEATURE_AUDIO_SYNC
    if (hasAudio && m_kickPulse > 0.4f && m_spawnCooldown == 0) {  // Enhanced: Lower threshold (0.4f instead of 0.5f)
        for (uint8_t r = 0; r < MAX_RIPPLES; ++r) {
            if (!m_ripples[r].active) {
                m_ripples[r].radius = 0.0f;      // Start at center
                m_ripples[r].intensity = 255;    // Max intensity for bass punch
                m_ripples[r].speed = speedScale; // Uses unified speed from slider
                // Bass-driven warm hue (palette index based on kick intensity)
                m_ripples[r].hue = ctx.gHue + (uint8_t)(m_kickPulse * 30.0f);
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
                // Enhanced: Use circular chroma hue (no chord reactivity)
                m_ripples[r].hue = ctx.gHue + chromaHueOffset;
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
                    // Enhanced: Use circular chroma hue (no chord reactivity)
                    float hueBase = (float)chromaHueOffset + ctx.gHue;
                    m_ripples[i].hue = (uint8_t)hueBase;
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
                // Hi-hat/cymbal energy adds sparkle; capped to avoid brightness stacking wash (colour fix).
                // =========================================================
                if (m_trebleShimmer > 0.08f) {
                    float shimmerFade = 1.0f - (waveAbs / 3.0f);  // 1.0 at front, 0.0 at back
                    uint8_t shimmerBoost = (uint8_t)(m_trebleShimmer * shimmerFade * 30.0f);  // was 60; halved to preserve palette
                    brightness = qadd8(brightness, shimmerBoost);
                }

                CRGB color = ctx.palette.getColor(
                    m_ripples[r].hue + (uint8_t)dist,
                    brightness);
                // Keep colours within palette: select brightest contributor.
                if (brightness > m_ps->radial[dist].getAverageLight()) {
                    m_ps->radial[dist] = color;
                }
            }
        }
    }

    memcpy(m_ps->radialAux, m_ps->radial, sizeof(m_ps->radial));

    // Render radial history buffer to LEDs (centre-origin)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        SET_CENTER_PAIR(ctx, dist, m_ps->radialAux[dist]);
    }
}

void RippleEnhancedEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& RippleEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple Enhanced",
        "Enhanced: Improved 64-bin thresholds, snare triggers, treble shimmer",
        plugins::EffectCategory::WATER,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
