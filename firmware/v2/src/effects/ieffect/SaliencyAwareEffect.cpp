/**
 * @file SaliencyAwareEffect.cpp
 * @brief Saliency-aware effect implementation
 *
 * Adapts visual behavior based on dominant saliency type from Musical Intelligence System.
 */

#include "SaliencyAwareEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace effects {
namespace ieffect {

bool SaliencyAwareEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;
    m_rhythmicPulse = 0.0f;
    m_timbralTexture = 0.0f;
    m_dynamicEnergy = 0.0f;
    m_modeTransition = 0.0f;
    m_prevHarmonic = 0.0f;
    m_prevRhythmic = 0.0f;
    m_prevTimbral = 0.0f;
    m_prevDynamic = 0.0f;
    return true;
}

void SaliencyAwareEffect::render(plugins::EffectContext& ctx) {
    // CENTER ORIGIN - Multi-layer interference pattern
    // Visual Foundation: Interference moiré patterns (like LGPInterferenceScanner)
    // Audio Enhancement: Saliency metrics modulate brightness/color and switch modes
    
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

#if !FEATURE_AUDIO_SYNC
    // Fallback: simple interference pattern
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    m_phase1 += speedNorm * 0.02f * dt;
    m_phase2 += speedNorm * 0.03f * dt;
    m_phase3 += speedNorm * 0.05f * dt;
    
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        float wave1 = sinf(distNorm * 0.16f * 2.0f * 3.14159f + m_phase1);
        float wave2 = sinf(distNorm * 0.28f * 2.0f * 3.14159f + m_phase2);
        float wave3 = sinf(distNorm * 0.12f * 2.0f * 3.14159f + m_phase3);
        float interference = (wave1 + wave2 * 0.6f + wave3 * 0.4f) / 2.0f;
        uint8_t bright = (uint8_t)(128 + 127 * interference);
        CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distNorm * 50.0f), bright);
        SET_CENTER_PAIR(ctx, dist, color);
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable
        float speedNorm = ctx.speed / 50.0f;
        float dt = ctx.getSafeDeltaSeconds();
        m_phase1 += speedNorm * 0.02f * dt;
        m_phase2 += speedNorm * 0.03f * dt;
        m_phase3 += speedNorm * 0.05f * dt;
        
        for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
            float distNorm = (float)dist / (float)HALF_LENGTH;
            float wave1 = sinf(distNorm * 0.16f * 2.0f * 3.14159f + m_phase1);
            float wave2 = sinf(distNorm * 0.28f * 2.0f * 3.14159f + m_phase2);
            float wave3 = sinf(distNorm * 0.12f * 2.0f * 3.14159f + m_phase3);
            float interference = (wave1 + wave2 * 0.6f + wave3 * 0.4f) / 2.0f;
            uint8_t bright = (uint8_t)(128 + 127 * interference);
            CRGB color = ctx.palette.getColor(ctx.gHue + (uint8_t)(distNorm * 50.0f), bright);
            SET_CENTER_PAIR(ctx, dist, color);
        }
        return;
    }

    // =========================================================================
    // Visual Foundation: TIME-BASED phase (prevents jitter)
    // =========================================================================
    float speedNorm = ctx.speed / 50.0f;
    float dt = ctx.getSafeDeltaSeconds();
    
    // Different speed rates per mode (all time-based)
    float speed1 = speedNorm * 0.02f;  // Slow for harmonic
    float speed2 = speedNorm * 0.03f;  // Medium
    float speed3 = speedNorm * 0.05f;  // Fast for rhythmic
    
    m_phase1 += speed1 * dt;  // Time-based only
    m_phase2 += speed2 * dt;
    m_phase3 += speed3 * dt;
    
    if (m_phase1 > 2.0f * 3.14159f) m_phase1 -= 2.0f * 3.14159f;
    if (m_phase2 > 2.0f * 3.14159f) m_phase2 -= 2.0f * 3.14159f;
    if (m_phase3 > 2.0f * 3.14159f) m_phase3 -= 2.0f * 3.14159f;

    // =========================================================================
    // Audio Enhancement: Saliency metrics
    // =========================================================================
    float harmonicSal = ctx.audio.harmonicSaliency();
    float rhythmicSal = ctx.audio.rhythmicSaliency();
    float timbralSal = ctx.audio.timbralSaliency();
    float dynamicSal = ctx.audio.dynamicSaliency();
    float overallSal = ctx.audio.overallSaliency();
    
    // Determine dominant saliency type
    float targetMode = 0.0f;
    if (ctx.audio.isHarmonicDominant()) {
        targetMode = 0.0f;  // Harmonic mode
    } else if (ctx.audio.isRhythmicDominant()) {
        targetMode = 1.0f;  // Rhythmic mode
    } else if (ctx.audio.isTimbralDominant()) {
        targetMode = 2.0f;  // Timbral mode
    } else if (ctx.audio.isDynamicDominant()) {
        targetMode = 3.0f;  // Dynamic mode
    }
    
    // Smooth mode transitions
    float modeAlpha = 0.1f;
    m_modeTransition += (targetMode - m_modeTransition) * modeAlpha;
    
    // Rhythmic: Pulse on beat with saliency intensity
    if (ctx.audio.isOnBeat()) {
        m_rhythmicPulse = 1.0f;
    } else {
        m_rhythmicPulse *= (1.0f - dt * 5.0f);  // ~200ms decay
    }
    m_rhythmicPulse = fmaxf(m_rhythmicPulse, rhythmicSal * 0.5f);
    
    // Timbral: Texture intensity based on spectral changes
    float timbralChange = fabsf(timbralSal - m_prevTimbral);
    m_timbralTexture += (timbralChange - m_timbralTexture) * 0.3f;
    
    // Dynamic: Energy level from RMS with saliency scaling
    float rms = ctx.audio.rms();
    m_dynamicEnergy += (rms * (1.0f + dynamicSal) - m_dynamicEnergy) * 0.2f;
    
    // Store previous values
    m_prevHarmonic = harmonicSal;
    m_prevRhythmic = rhythmicSal;
    m_prevTimbral = timbralSal;
    m_prevDynamic = dynamicSal;
    
    // =========================================================================
    // Render: Multi-layer interference pattern with saliency modulation
    // =========================================================================
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        float distFromCenter = (float)dist;
        
        // Visual Foundation: Multi-layer interference pattern
        // Different spatial frequencies create moiré beating patterns
        float freq1 = 0.16f;  // Low frequency (harmonic)
        float freq2 = 0.28f;  // Medium frequency (rhythmic)
        float freq3 = 0.12f;  // Very low frequency (timbral)
        
        // Adjust frequencies based on mode
        if (m_modeTransition >= 1.0f && m_modeTransition < 2.0f) {
            // Rhythmic mode: faster frequencies
            freq1 *= 1.5f;
            freq2 *= 1.3f;
        } else if (m_modeTransition >= 2.0f && m_modeTransition < 3.0f) {
            // Timbral mode: higher frequencies for texture
            freq1 *= 2.0f;
            freq2 *= 1.8f;
            freq3 *= 1.5f;
        }
        
        // Standing wave equations: sin(k*dist - phase) produces OUTWARD motion
        float wave1 = sinf(distFromCenter * freq1 - m_phase1);
        float wave2 = sinf(distFromCenter * freq2 - m_phase2);
        float wave3 = sinf(distFromCenter * freq3 - m_phase3);
        
        // Combine waves with weights (creates interference moiré)
        float interference = wave1 + wave2 * 0.6f + wave3 * 0.4f;
        interference /= 2.0f;  // Normalize
        
        // Audio Enhancement: Modulate brightness based on saliency
        float audioGain = 0.4f;
        
        // Harmonic: Slow color drift, chord-responsive
        if (m_modeTransition < 0.5f) {
            float harmonicWeight = 1.0f - m_modeTransition * 2.0f;
            audioGain += harmonicSal * 0.4f * harmonicWeight;
        }
        
        // Rhythmic: Beat-synced pulse
        if (m_modeTransition >= 0.5f && m_modeTransition < 1.5f) {
            float rhythmicWeight = 1.0f - fabsf(m_modeTransition - 1.0f) * 2.0f;
            float pulseBoost = m_rhythmicPulse * (1.0f - distNorm * 0.7f);
            audioGain += pulseBoost * 0.5f * rhythmicWeight;
        }
        
        // Timbral: High-frequency shimmer
        if (m_modeTransition >= 1.5f && m_modeTransition < 2.5f) {
            float timbralWeight = 1.0f - fabsf(m_modeTransition - 2.0f) * 2.0f;
            audioGain += m_timbralTexture * 0.4f * timbralWeight;
        }
        
        // Dynamic: Energy-based brightness
        if (m_modeTransition >= 2.5f) {
            float dynamicWeight = 1.0f - (m_modeTransition - 3.0f) * 2.0f;
            audioGain += m_dynamicEnergy * 0.5f * dynamicWeight;
        }
        
        // Apply audio gain to interference pattern
        float brightness = interference * audioGain;
        
        // Scale by overall saliency
        brightness *= (0.5f + overallSal * 0.5f);
        
        // Normalize brightness
        brightness = tanhf(brightness * 2.0f) * 0.5f + 0.5f;
        brightness = fminf(1.0f, brightness);
        
        // Color: Hue shifts based on mode and saliency
        uint8_t hue = ctx.gHue;
        if (ctx.audio.isHarmonicDominant() && ctx.audio.hasChord()) {
            // Harmonic mode: use chord root note
            hue += ctx.audio.rootNote() * 21;  // Chromatic circle
        } else {
            // Other modes: hue shifts with distance
            hue += (uint8_t)(distNorm * 50.0f);
        }
        
        // Get color from palette
        uint8_t bright = (uint8_t)(brightness * ctx.brightness);
        CRGB color = ctx.palette.getColor(hue, bright);
        
        // Set center pair
        SET_CENTER_PAIR(ctx, dist, color);
    }
    
    // Add overall saliency indicator at center
    if (overallSal > 0.5f) {
        uint8_t saliencyBoost = (uint8_t)(overallSal * 60.0f);
        for (uint16_t dist = 0; dist < 3; ++dist) {
            float fade = 1.0f - ((float)dist / 3.0f);
            uint8_t fadedBoost = (uint8_t)(saliencyBoost * fade);
            
            uint16_t leftIdx = ctx.centerPoint - 1 - dist;
            uint16_t rightIdx = ctx.centerPoint + dist;
            
            if (leftIdx < ctx.ledCount) {
                ctx.leds[leftIdx].r = qadd8(ctx.leds[leftIdx].r, fadedBoost);
                ctx.leds[leftIdx].g = qadd8(ctx.leds[leftIdx].g, fadedBoost);
                ctx.leds[leftIdx].b = qadd8(ctx.leds[leftIdx].b, fadedBoost);
            }
            if (rightIdx < ctx.ledCount) {
                ctx.leds[rightIdx].r = qadd8(ctx.leds[rightIdx].r, fadedBoost);
                ctx.leds[rightIdx].g = qadd8(ctx.leds[rightIdx].g, fadedBoost);
                ctx.leds[rightIdx].b = qadd8(ctx.leds[rightIdx].b, fadedBoost);
            }
        }
    }
#endif  // FEATURE_AUDIO_SYNC
}

void SaliencyAwareEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& SaliencyAwareEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Saliency Aware",
        "Adapts visual behavior based on musical saliency (harmonic, rhythmic, timbral, dynamic)",
        plugins::EffectCategory::PARTY,
        1,
        "LightwaveOS"
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

