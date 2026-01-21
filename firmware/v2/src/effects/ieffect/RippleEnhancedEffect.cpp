/**
 * @file RippleEnhancedEffect.cpp
 * @brief Beat-synchronized ripple effect with musical intelligence
 */

#include "RippleEnhancedEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace effects {
namespace ieffect {

RippleEnhancedEffect::RippleEnhancedEffect() {
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
        m_ripples[i].radius = 0.0f;
        m_ripples[i].speed = 0.0f;
        m_ripples[i].hue = 0;
        m_ripples[i].intensity = 255;
        m_ripples[i].isDownbeat = false;
    }
    memset(m_radial, 0, sizeof(m_radial));
}

bool RippleEnhancedEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    for (uint8_t i = 0; i < MAX_RIPPLES; i++) {
        m_ripples[i].active = false;
    }
    m_lastBeatState = false;
    m_lastDownbeatState = false;
    m_beatPhaseAccum = 0.0f;
    m_lastHopSeq = 0;
    m_styleSpeedMult = 1.0f;
    m_styleIntensityMult = 1.0f;
    memset(m_radial, 0, sizeof(m_radial));
    return true;
}

void RippleEnhancedEffect::render(plugins::EffectContext& ctx) {
    // =========================================================================
    // CENTER ORIGIN RIPPLE ENHANCED
    // Beat-synchronized ripples expanding from center with musical intelligence
    // =========================================================================

    // Fade previous frame
    fadeToBlackBy(m_radial, HALF_LENGTH, 40);

    const bool hasAudio = ctx.audio.available;

#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        // =================================================================
        // STYLE-ADAPTIVE PARAMETERS
        // Adjust ripple behavior based on detected music style
        // =================================================================
        if (ctx.audio.isRhythmicMusic()) {
            // EDM/Hip-hop: Fast, punchy ripples
            m_styleSpeedMult = 1.5f;
            m_styleIntensityMult = 1.2f;
        } else if (ctx.audio.isHarmonicMusic()) {
            // Jazz/Classical: Moderate speed, focus on color
            m_styleSpeedMult = 1.0f;
            m_styleIntensityMult = 1.0f;
        } else if (ctx.audio.isTextureMusic()) {
            // Ambient/Drone: Slow, gentle ripples
            m_styleSpeedMult = 0.6f;
            m_styleIntensityMult = 0.8f;
        } else if (ctx.audio.isMelodicMusic()) {
            // Vocal pop: Medium speed, melodic color shifts
            m_styleSpeedMult = 1.1f;
            m_styleIntensityMult = 1.0f;
        } else {
            // Default/Unknown
            m_styleSpeedMult = 1.0f;
            m_styleIntensityMult = 1.0f;
        }

        // =================================================================
        // BEAT-SYNC RIPPLE SPAWNING
        // Spawn ripples ON the beat for tight sync
        // Gate to audio hop rate (not every render frame)
        // =================================================================
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            bool currentBeat = ctx.audio.isOnBeat();
            bool currentDownbeat = ctx.audio.isOnDownbeat();

            // Edge detection: spawn only on beat onset (not while held)
            bool beatOnset = currentBeat && !m_lastBeatState;
            bool downbeatOnset = currentDownbeat && !m_lastDownbeatState;
            m_lastBeatState = currentBeat;
            m_lastDownbeatState = currentDownbeat;

            if (beatOnset || downbeatOnset) {
            // Find inactive ripple slot
            for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
                if (!m_ripples[r].active) {
                    m_ripples[r].radius = 0.0f;
                    m_ripples[r].active = true;
                    m_ripples[r].isDownbeat = downbeatOnset;

                    // Base speed from user slider, modified by style
                    float baseSpeed = 1.0f + 2.0f * (ctx.speed / 50.0f);
                    m_ripples[r].speed = baseSpeed * m_styleSpeedMult;

                    // Intensity: downbeats are stronger
                    float intensityBase = 200.0f;
                    if (downbeatOnset) {
                        intensityBase = 255.0f;
                        m_ripples[r].speed *= 1.2f;  // Downbeat ripples move faster
                    }
                    intensityBase *= m_styleIntensityMult;
                    if (intensityBase > 255.0f) intensityBase = 255.0f;
                    m_ripples[r].intensity = (uint8_t)intensityBase;

                    // Hue: base from gHue, shift by harmonic saliency
                    float harmonicShift = ctx.audio.harmonicSaliency() * 40.0f;
                    m_ripples[r].hue = ctx.gHue + (uint8_t)harmonicShift;

                    // Add chord-based warmth
                    if (ctx.audio.chordConfidence() > 0.3f) {
                        if (ctx.audio.isMajor()) {
                            m_ripples[r].hue += 20;  // Warm shift
                        } else if (ctx.audio.isMinor()) {
                            m_ripples[r].hue -= 20;  // Cool shift
                        }
                    }

                    break;  // Only spawn one ripple per beat
                }
            }
            }
        }

        // =================================================================
        // BEAT PHASE MODULATION
        // Smooth ripple expansion synced to beat phase
        // =================================================================
        float beatPhase = ctx.audio.beatPhase();
        m_beatPhaseAccum = beatPhase;
    }
#endif

    // =========================================================================
    // UPDATE AND RENDER RIPPLES
    // =========================================================================
    float dt = ctx.getSafeDeltaSeconds();

    for (uint8_t r = 0; r < MAX_RIPPLES; r++) {
        if (!m_ripples[r].active) continue;

        // Expand ripple
        m_ripples[r].radius += m_ripples[r].speed * dt * 60.0f;  // 60 = base FPS normalization

        // Deactivate when past edge
        if (m_ripples[r].radius > HALF_LENGTH + 3.0f) {
            m_ripples[r].active = false;
            continue;
        }

        // Draw ripple wavefront
        for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
            float wavePos = (float)dist - m_ripples[r].radius;
            float waveAbs = fabsf(wavePos);

            // Wavefront width: 3 LEDs for normal, 5 for downbeat
            float width = m_ripples[r].isDownbeat ? 5.0f : 3.0f;

            if (waveAbs < width) {
                // Brightness falls off from wavefront center
                float falloff = 1.0f - (waveAbs / width);
                uint8_t brightness = (uint8_t)(falloff * 255.0f);

                // Edge fade: ripples dim as they approach edge
                float edgeFade = 1.0f - (m_ripples[r].radius / (float)HALF_LENGTH);
                if (edgeFade < 0.0f) edgeFade = 0.0f;
                brightness = scale8(brightness, (uint8_t)(edgeFade * 255.0f));

                // Apply ripple intensity
                brightness = scale8(brightness, m_ripples[r].intensity);

                // Get color from palette
                uint8_t paletteIdx = m_ripples[r].hue + (uint8_t)(dist * 0.5f);
                CRGB color = ctx.palette.getColor(paletteIdx, brightness);

                // Additive blend to radial buffer
                m_radial[dist].r = qadd8(m_radial[dist].r, color.r);
                m_radial[dist].g = qadd8(m_radial[dist].g, color.g);
                m_radial[dist].b = qadd8(m_radial[dist].b, color.b);
            }
        }
    }

    // =========================================================================
    // OUTPUT TO LED STRIP (CENTER ORIGIN)
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        SET_CENTER_PAIR(ctx, dist, m_radial[dist]);
    }
}

void RippleEnhancedEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& RippleEnhancedEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Ripple Enhanced",
        "Beat-synchronized ripples with musical intelligence",
        plugins::EffectCategory::WATER,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
