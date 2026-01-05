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
    // Clear output buffer
    memset(ctx.leds, 0, ctx.ledCount * sizeof(CRGB));

#if !FEATURE_AUDIO_SYNC
    // Fallback: simple color animation
    for (uint16_t i = 0; i < ctx.ledCount; ++i) {
        float phase = (float)i / (float)ctx.ledCount + (float)ctx.totalTimeMs * 0.001f;
        uint8_t bright = (uint8_t)(128 + 127 * sinf(phase * 2.0f * 3.14159f));
        ctx.leds[i] = ctx.palette.getColor(ctx.gHue + i, bright);
    }
    return;
#else
    if (!ctx.audio.available) {
        // Fallback when audio unavailable
        for (uint16_t i = 0; i < ctx.ledCount; ++i) {
            float phase = (float)i / (float)ctx.ledCount + (float)ctx.totalTimeMs * 0.001f;
            uint8_t bright = (uint8_t)(128 + 127 * sinf(phase * 2.0f * 3.14159f));
            ctx.leds[i] = ctx.palette.getColor(ctx.gHue + i, bright);
        }
        return;
    }

    // Get saliency metrics
    float harmonicSal = ctx.audio.harmonicSaliency();
    float rhythmicSal = ctx.audio.rhythmicSaliency();
    float timbralSal = ctx.audio.timbralSaliency();
    float dynamicSal = ctx.audio.dynamicSaliency();
    float overallSal = ctx.audio.overallSaliency();
    
    // Determine dominant saliency type
    float maxSal = fmaxf(fmaxf(harmonicSal, rhythmicSal), fmaxf(timbralSal, dynamicSal));
    
    // Update mode transition based on dominant type
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
    
    // Update state based on saliency type
    float dt = ctx.getSafeDeltaSeconds();
    
    // Harmonic: Slow color drift with chord changes
    if (harmonicSal > 0.3f) {
        m_harmonicPhase += dt * (0.5f + harmonicSal * 1.5f);
        if (m_harmonicPhase > 1.0f) m_harmonicPhase -= 1.0f;
    }
    
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
    
    // Render based on dominant mode (blended)
    for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
        float distNorm = (float)dist / (float)HALF_LENGTH;
        
        // Harmonic component: slow color drift
        float harmonicHue = ctx.gHue + (uint8_t)(m_harmonicPhase * 255.0f);
        float harmonicBright = 0.5f + 0.5f * sinf(m_harmonicPhase * 2.0f * 3.14159f + distNorm * 2.0f);
        float harmonicWeight = (m_modeTransition < 0.5f) ? (1.0f - m_modeTransition * 2.0f) : 0.0f;
        
        // Rhythmic component: beat-synced pulse
        float rhythmicBright = m_rhythmicPulse * (1.0f - distNorm * 0.7f);
        float rhythmicWeight = (m_modeTransition >= 0.5f && m_modeTransition < 1.5f) ? 
                               (1.0f - fabsf(m_modeTransition - 1.0f) * 2.0f) : 0.0f;
        
        // Timbral component: texture pattern
        float timbralHue = ctx.gHue + (uint8_t)(distNorm * 255.0f);
        float timbralBright = 0.3f + m_timbralTexture * 0.7f;
        float timbralWeight = (m_modeTransition >= 1.5f && m_modeTransition < 2.5f) ?
                              (1.0f - fabsf(m_modeTransition - 2.0f) * 2.0f) : 0.0f;
        
        // Dynamic component: energy-based brightness
        float dynamicBright = m_dynamicEnergy * (1.0f - distNorm * 0.5f);
        float dynamicWeight = (m_modeTransition >= 2.5f) ?
                              (1.0f - (m_modeTransition - 3.0f) * 2.0f) : 0.0f;
        
        // Blend components
        float finalBright = (harmonicBright * harmonicWeight +
                            rhythmicBright * rhythmicWeight +
                            timbralBright * timbralWeight +
                            dynamicBright * dynamicWeight);
        
        // Normalize if weights don't sum to 1.0
        float totalWeight = harmonicWeight + rhythmicWeight + timbralWeight + dynamicWeight;
        if (totalWeight > 0.01f) {
            finalBright /= totalWeight;
        } else {
            finalBright = 0.3f;  // Default when no mode is dominant
        }
        
        // Scale by overall saliency
        finalBright *= (0.5f + overallSal * 0.5f);
        finalBright = fminf(1.0f, finalBright);
        
        // Calculate hue (blend harmonic and timbral)
        float hueBlend = (harmonicWeight + timbralWeight) / fmaxf(totalWeight, 0.01f);
        uint8_t finalHue = (uint8_t)(harmonicHue * hueBlend + timbralHue * (1.0f - hueBlend));
        
        // Get color from palette
        uint8_t bright = (uint8_t)(finalBright * ctx.brightness);
        CRGB color = ctx.palette.getColor(finalHue, bright);
        
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

