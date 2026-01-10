/**
 * @file StyleAdaptiveEffect.cpp
 * @brief Style-adaptive effect implementation
 *
 * Adapts visual behavior based on detected music style classification.
 */

#include "StyleAdaptiveEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include "../../audio/contracts/StyleDetector.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool StyleAdaptiveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_rhythmicPulse = 0.0f;
    m_harmonicDrift = 0.0f;
    m_melodicShimmer = 0.0f;
    m_textureFlow = 0.0f;
    m_dynamicBreath = 0.0f;
    m_currentStyle = 0;
    m_styleConfidence = 0.0f;
    m_rhythmicPulseFollower.reset(0.0f);
    m_dynamicBreathFollower.reset(0.0f);
    m_rmsFollower.reset(0.0f);
    m_lastHopSeq = 0;
    m_targetRms = 0.0f;
    return true;
}

void StyleAdaptiveEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Radial wave pattern (like LGPStarBurst)
    // Visual Foundation: Radial waves from center
    // Audio Enhancement: Style detection switches behaviors and modulates brightness/color
    
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

#if !FEATURE_AUDIO_SYNC
    // Fallback: simple radial wave pattern
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    m_phase += speedNorm * 240.0f * dt;  // Time-based only
    
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distFromCenter = (float)centerPairDistance((uint16_t)dist);
        const float freqBase = 0.25f;
        float star = sinf(distFromCenter * freqBase - m_phase);
        uint8_t bright = (uint8_t)(128 + 127 * star);
        CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distFromCenter * 2.0f), bright);
        SET_CENTER_PAIR(ctx, dist, color);
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable
        float speedNorm = ctx.speed / 50.0f;
        float dt = ctx.getSafeDeltaSeconds();
        m_phase += speedNorm * 240.0f * dt;
        
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float distFromCenter = (float)centerPairDistance((uint16_t)dist);
            const float freqBase = 0.25f;
            float star = sinf(distFromCenter * freqBase - m_phase);
            uint8_t bright = (uint8_t)(128 + 127 * star);
            CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distFromCenter * 2.0f), bright);
            SET_CENTER_PAIR(ctx, dist, color);
        }
        return;
    }

    // =========================================================================
    // Visual Foundation: TIME-BASED phase (prevents jitter)
    // Different speed rates per style (all time-based)
    // =========================================================================
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    
    // Get music style
    audio::MusicStyle style = ctx.audio.musicStyle();
    float confidence = ctx.audio.styleConfidence();
    
    // Update style state
    m_currentStyle = (uint8_t)style;
    m_styleConfidence = confidence;
    
    // Style-dependent speed rates (all time-based)
    float speedMultiplier = 1.0f;
    switch (style) {
        case audio::MusicStyle::RHYTHMIC_DRIVEN:
            speedMultiplier = 1.5f;  // Fast for EDM/hip-hop
            break;
        case audio::MusicStyle::HARMONIC_DRIVEN:
            speedMultiplier = 0.5f;  // Slow for jazz/classical
            break;
        case audio::MusicStyle::MELODIC_DRIVEN:
            speedMultiplier = 1.0f;  // Medium for vocal pop
            break;
        case audio::MusicStyle::TEXTURE_DRIVEN:
            speedMultiplier = 0.3f;  // Very slow for ambient
            break;
        case audio::MusicStyle::DYNAMIC_DRIVEN:
            speedMultiplier = 0.8f;  // Medium-slow for orchestral
            break;
        default:
            speedMultiplier = 1.0f;  // Default
            break;
    }
    
    // Update phase (time-based only)
    m_phase += speedNorm * 240.0f * speedMultiplier * dt;
    if (m_phase > 628.3f) m_phase -= 628.3f;  // Wrap at 100*2π

    // =========================================================================
    // Audio Enhancement: Style-specific modulation
    // =========================================================================
    // Get mood-normalized value (used throughout for mood-adjusted smoothing)
    float moodNorm = ctx.getMoodNormalized();
    
    // Hop-based RMS updates
    bool newHop = false;
#if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            m_targetRms = ctx.audio.rms();
        }
        
        // Smooth RMS every frame with mood-adjusted smoothing
        m_rmsFollower.updateWithMood(m_targetRms, dt, moodNorm);
    }
#endif
    
    switch (style) {
        case audio::MusicStyle::RHYTHMIC_DRIVEN: {
            // Fast pulses, bass-heavy
            float targetPulse = 0.0f;
            if (ctx.audio.isOnBeat()) {
                targetPulse = 1.0f;
            }
            // Boost with bass energy
            float bass = ctx.audio.bass();
            targetPulse = fmaxf(targetPulse, bass * 0.7f);
            
            // Smooth with mood-adjusted smoothing (moodNorm declared above)
            m_rhythmicPulse = m_rhythmicPulseFollower.updateWithMood(targetPulse, dt, moodNorm);
            break;
        }
        
        case audio::MusicStyle::HARMONIC_DRIVEN: {
            // Slow color drift, chord-responsive
            float chordChange = ctx.audio.chordConfidence() > 0.4f ? 1.0f : 0.0f;
            m_harmonicDrift += dt * (0.2f + chordChange * 0.5f);
            if (m_harmonicDrift > 1.0f) m_harmonicDrift -= 1.0f;
            break;
        }
        
        case audio::MusicStyle::MELODIC_DRIVEN: {
            // Treble shimmer for vocals
            float treble = ctx.audio.treble();
            m_melodicShimmer += dt * (1.0f + treble * 2.0f);
            if (m_melodicShimmer > 1.0f) m_melodicShimmer -= 1.0f;
            break;
        }
        
        case audio::MusicStyle::TEXTURE_DRIVEN: {
            // Ambient texture flow
            float flux = ctx.audio.flux();
            m_textureFlow += dt * (0.3f + flux * 0.7f);
            if (m_textureFlow > 1.0f) m_textureFlow -= 1.0f;
            break;
        }
        
        case audio::MusicStyle::DYNAMIC_DRIVEN: {
            // RMS breathing for orchestral (use smoothed RMS)
            float smoothedRms = m_rmsFollower.getCurrent();
            m_dynamicBreath = m_dynamicBreathFollower.updateWithMood(smoothedRms, dt, moodNorm);
            break;
        }
        
        default: {
            // UNKNOWN: blend all modes (use smoothed RMS)
            float smoothedRms = m_rmsFollower.getCurrent();
            m_rhythmicPulse = m_rhythmicPulseFollower.updateWithMood(smoothedRms * 0.5f, dt, moodNorm);
            m_harmonicDrift += dt * 0.3f;
            if (m_harmonicDrift > 1.0f) m_harmonicDrift -= 1.0f;
            m_melodicShimmer += dt * 0.5f;
            if (m_melodicShimmer > 1.0f) m_melodicShimmer -= 1.0f;
            m_textureFlow += dt * 0.4f;
            if (m_textureFlow > 1.0f) m_textureFlow -= 1.0f;
            m_dynamicBreath = m_dynamicBreathFollower.updateWithMood(smoothedRms, dt, moodNorm);
            break;
        }
    }
    
    // =========================================================================
    // Render: Radial wave pattern with style-dependent modulation
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distFromCenter = (float)centerPairDistance((uint16_t)dist);
        float distNorm = (float)dist / (float)HALF_LENGTH;
        
        // Visual Foundation: Radial wave pattern (like LGPStarBurst)
        const float freqBase = 0.25f;
        float star = sinf(distFromCenter * freqBase - m_phase);
        
        // Audio Enhancement: Style-dependent brightness modulation
        float audioGain = 0.5f;
        uint8_t hue = ctx.gHue;
        
        switch (style) {
            case audio::MusicStyle::RHYTHMIC_DRIVEN: {
                // Fast pulses, bass at center
                float pulse = m_rhythmicPulse * (1.0f - distNorm * 0.6f);
                audioGain = 0.4f + pulse * 0.6f;
                hue += (uint8_t)(distNorm * 30.0f);
                break;
            }
            
            case audio::MusicStyle::HARMONIC_DRIVEN: {
                // Slow color drift with chord influence
                float chordHue = ctx.audio.hasChord() ? 
                    (ctx.audio.rootNote() * 21) : 0;
                hue = ctx.gHue + (uint8_t)(m_harmonicDrift * 255.0f) + chordHue;
                float smoothedRms = m_rmsFollower.getCurrent();
                audioGain = 0.5f + smoothedRms * 0.5f;
                break;
            }
            
            case audio::MusicStyle::MELODIC_DRIVEN: {
                // Treble shimmer
                float treble = ctx.audio.treble();
                audioGain = 0.4f + treble * 0.6f;
                hue += (uint8_t)(distNorm * 60.0f + m_melodicShimmer * 50.0f);
                break;
            }
            
            case audio::MusicStyle::TEXTURE_DRIVEN: {
                // Ambient texture
                float flux = ctx.audio.flux();
                audioGain = 0.3f + flux * 0.4f;
                hue += (uint8_t)(m_textureFlow * 100.0f);
                break;
            }
            
            case audio::MusicStyle::DYNAMIC_DRIVEN: {
                // RMS breathing
                float breath = m_dynamicBreath * (1.0f - distNorm * 0.5f);
                audioGain = 0.3f + breath * 0.7f;
                hue += (uint8_t)(distNorm * 40.0f);
                break;
            }
            
            default: {
                // UNKNOWN: blend all modes
                float smoothedRms = m_rmsFollower.getCurrent();
                audioGain = 0.4f + smoothedRms * 0.4f;
                hue += (uint8_t)(distNorm * 50.0f);
                break;
            }
        }
        
        // Apply audio gain to radial wave pattern
        float brightness = star * audioGain;

        // Scale by style confidence with minimum floor for visibility
        // When style classification fails (low confidence), ensure fallback brightness of 0.4
        float confidenceScale = fmaxf(0.4f, 0.5f + confidence * 0.5f);
        brightness *= confidenceScale;

        // Normalize brightness
        brightness = tanhf(brightness * 2.0f) * 0.5f + 0.5f;
        brightness = fminf(1.0f, brightness);
        
        // Get color from palette
        uint8_t bright = (uint8_t)(brightness * ctx.brightness);
        CRGB color = ctx.palette.getColor(hue, bright);
        
        // Set center pair
        SET_CENTER_PAIR(ctx, dist, color);
    }
    
    // Style indicator at center (subtle)
    if (confidence > 0.5f) {
        uint8_t styleBoost = (uint8_t)(confidence * 30.0f);
        uint16_t leftIdx = ctx.centerPoint - 1;
        uint16_t rightIdx = ctx.centerPoint;
        
        if (leftIdx < ctx.ledCount) {
            ctx.leds[leftIdx].r = qadd8(ctx.leds[leftIdx].r, styleBoost);
            ctx.leds[leftIdx].g = qadd8(ctx.leds[leftIdx].g, styleBoost);
            ctx.leds[leftIdx].b = qadd8(ctx.leds[leftIdx].b, styleBoost);
        }
        if (rightIdx < ctx.ledCount) {
            ctx.leds[rightIdx].r = qadd8(ctx.leds[rightIdx].r, styleBoost);
            ctx.leds[rightIdx].g = qadd8(ctx.leds[rightIdx].g, styleBoost);
            ctx.leds[rightIdx].b = qadd8(ctx.leds[rightIdx].b, styleBoost);
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

void StyleAdaptiveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& StyleAdaptiveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Style Adaptive",
        "Adapts visual behavior based on detected music style (EDM, jazz, ambient, orchestral, pop)",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

