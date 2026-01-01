/**
 * @file LGPPerlinInterferenceWeaveEffect.cpp
 * @brief LGP Perlin Interference Weave - Dual-strip phase-offset noise creates moiré
 * 
 * Audio-reactive Perlin interference effect:
 * - Two strips sample same noise field with phase offset
 * - Phase offset creates moiré interference pattern on LGP
 * - Beat modulates phase separation
 * - Chroma provides colour offset between strips
 */

#include "LGPPerlinInterferenceWeaveEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinInterferenceWeaveEffect::LGPPerlinInterferenceWeaveEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_phaseOffset(0.0f)
    , m_time(0)
    , m_lastHopSeq(0)
    , m_dominantChromaBin(0)
{
}

bool LGPPerlinInterferenceWeaveEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_phaseOffset = 0.0f;
    m_time = 0;
    m_lastHopSeq = 0;
    m_dominantChromaBin = 0;
    return true;
}

void LGPPerlinInterferenceWeaveEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Dual-strip interference weave
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Audio Analysis
    // =========================================================================
    float beatMod = 0.0f;
    float fluxNorm = 0.0f;
    
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
        if (newHop) {
            m_lastHopSeq = ctx.audio.controlBus.hop_seq;
            
            // Chroma analysis for colour offset
            float maxBinVal = 0.0f;
            for (uint8_t i = 0; i < 12; ++i) {
                float bin = ctx.audio.controlBus.chroma[i];
                if (bin > maxBinVal) {
                    maxBinVal = bin;
                    m_dominantChromaBin = i;
                }
            }
        }
        
        // Beat → phase separation modulation
        beatMod = ctx.audio.beatStrength();
        
        // Flux → weave intensity
        fluxNorm = ctx.audio.flux();
    }
#endif

    // =========================================================================
    // Phase Offset Update (audio-modulated)
    // =========================================================================
    // Base phase offset (slow drift)
    float basePhaseOffset = 32.0f; // Base offset in noise space
    
    // Beat modulates phase separation (creates dynamic moiré)
    float beatPhaseMod = beatMod * 16.0f; // 0-16 additional offset
    float targetPhaseOffset = basePhaseOffset + beatPhaseMod;
    
    // Smooth phase offset changes
    float alpha = dt / (0.15f + dt); // ~150ms smoothing
    m_phaseOffset += (targetPhaseOffset - m_phaseOffset) * alpha;

    // =========================================================================
    // Noise Field Updates (increased advection for visible motion)
    // =========================================================================
    m_noiseX += (uint16_t)(6 + speedNorm * 12.0f);
    m_noiseY += (uint16_t)(4 + speedNorm * 8.0f);
    m_time += (uint16_t)(8 + speedNorm * 16.0f);

    // =========================================================================
    // Rendering (centre-origin pattern with dual-strip interference)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    // Weave intensity (flux-modulated)
    float weaveIntensity = 0.5f + fluxNorm * 0.5f; // 0.5-1.0

    // Chroma-based colour offset
    uint8_t chromaOffset = (uint8_t)(m_dominantChromaBin * (255 / 12));

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair
        uint16_t dist = centerPairDistance(i);
        
        // Sample noise field for strip 1 (base phase)
        // Removed >>8 collapse - use proper coordinate scaling like working effects
        uint16_t noiseX1 = m_noiseX + (dist * 8);
        uint16_t noiseY1 = m_noiseY + m_time;
        uint8_t noise1 = inoise8(noiseX1, noiseY1);
        
        // Sample noise field for strip 2 (phase offset for interference)
        uint16_t noiseX2 = m_noiseX + (dist * 8) + (uint16_t)m_phaseOffset;
        uint16_t noiseY2 = m_noiseY + m_time + (uint16_t)(m_phaseOffset * 0.5f);
        uint8_t noise2 = inoise8(noiseX2, noiseY2);
        
        // Convert to normalized values
        float norm1 = noise1 / 255.0f;
        float norm2 = noise2 / 255.0f;
        
        // Interference calculation: difference creates moiré pattern
        float interference = fabsf(norm1 - norm2);
        interference = interference * interference; // Square for emphasis
        
        // Combine base noise with interference
        float combined1 = norm1 * (1.0f - weaveIntensity * 0.3f) + interference * weaveIntensity;
        float combined2 = norm2 * (1.0f - weaveIntensity * 0.3f) + interference * weaveIntensity;
        
        combined1 = fmaxf(0.0f, fminf(1.0f, combined1));
        combined2 = fmaxf(0.0f, fminf(1.0f, combined2));
        
        // Map to palette and brightness
        uint8_t paletteIndex1 = (uint8_t)(combined1 * 255.0f) + ctx.gHue;
        uint8_t paletteIndex2 = (uint8_t)(combined2 * 255.0f) + chromaOffset + ctx.gHue;
        
        uint8_t brightness1 = (uint8_t)((0.2f + combined1 * 0.8f) * 255.0f * intensityNorm);
        uint8_t brightness2 = (uint8_t)((0.2f + combined2 * 0.8f) * 255.0f * intensityNorm);
        
        CRGB color1 = ctx.palette.getColor(paletteIndex1, brightness1);
        CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness2);
        
        // Apply to strip 1
        ctx.leds[i] = color1;
        
        // Apply to strip 2 (with phase offset for interference)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinInterferenceWeaveEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinInterferenceWeaveEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Interference Weave",
        "Dual-strip moiré interference, beat→phase, chroma→colour",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

