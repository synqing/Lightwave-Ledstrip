/**
 * @file LGPPerlinInterferenceWeaveAmbientEffect.cpp
 * @brief LGP Perlin Interference Weave Ambient - Dual-strip moiré (time-driven)
 * 
 * Ambient Perlin interference effect:
 * - Two strips sample same noise field with phase offset
 * - Phase offset creates moiré interference pattern
 * - Time-driven slow phase modulation
 */

#include "LGPPerlinInterferenceWeaveAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinInterferenceWeaveAmbientEffect::LGPPerlinInterferenceWeaveAmbientEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_phaseOffset(0.0f)
    , m_time(0)
{
}

bool LGPPerlinInterferenceWeaveAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_phaseOffset = 32.0f;
    m_time = 0;
    return true;
}

void LGPPerlinInterferenceWeaveAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Dual-strip interference weave (ambient)
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Phase Offset Update (time-modulated)
    // =========================================================================
    float angle = ctx.totalTimeMs * 0.001f;
    float basePhaseOffset = 32.0f;
    float phaseMod = 16.0f * sinf(angle * 0.2f); // Slow modulation
    float targetPhaseOffset = basePhaseOffset + phaseMod;
    
    float alpha = 1.0f - expf(-dt / 0.2f);  // True exponential, tau=200ms
    m_phaseOffset += (targetPhaseOffset - m_phaseOffset) * alpha;

    // =========================================================================
    // Noise Field Updates
    // =========================================================================
    m_noiseX += (uint16_t)(speedNorm * 2.0f);
    m_noiseY += (uint16_t)(speedNorm * 1.0f);
    m_time += (uint16_t)(speedNorm * 3.0f);

    // =========================================================================
    // Rendering (centre-origin pattern with dual-strip interference)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    float weaveIntensity = 0.6f + 0.2f * sinf(angle * 0.3f); // 0.6-0.8

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        
        // Sample noise field for strip 1
        uint16_t noiseX1 = m_noiseX + (dist * 4);
        uint16_t noiseY1 = m_noiseY + m_time;
        uint8_t noise1 = inoise8(noiseX1 >> 8, noiseY1 >> 8);
        
        // Sample noise field for strip 2 (phase offset)
        uint16_t noiseX2 = m_noiseX + (dist * 4) + (uint16_t)m_phaseOffset;
        uint16_t noiseY2 = m_noiseY + m_time + (uint16_t)(m_phaseOffset * 0.5f);
        uint8_t noise2 = inoise8(noiseX2 >> 8, noiseY2 >> 8);
        
        float norm1 = noise1 / 255.0f;
        float norm2 = noise2 / 255.0f;
        
        // Interference calculation
        float interference = fabsf(norm1 - norm2);
        interference = interference * interference;
        
        float combined1 = norm1 * (1.0f - weaveIntensity * 0.3f) + interference * weaveIntensity;
        float combined2 = norm2 * (1.0f - weaveIntensity * 0.3f) + interference * weaveIntensity;
        
        combined1 = fmaxf(0.0f, fminf(1.0f, combined1));
        combined2 = fmaxf(0.0f, fminf(1.0f, combined2));
        
        uint8_t paletteIndex1 = (uint8_t)(combined1 * 255.0f);
        uint8_t paletteIndex2 = (uint8_t)(combined2 * 255.0f) + 32; // Fixed offset
        
        uint8_t brightness1 = (uint8_t)((0.2f + combined1 * 0.8f) * 255.0f * intensityNorm);
        uint8_t brightness2 = (uint8_t)((0.2f + combined2 * 0.8f) * 255.0f * intensityNorm);
        
        CRGB color1 = ctx.palette.getColor(paletteIndex1, brightness1);
        CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness2);
        
        ctx.leds[i] = color1;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinInterferenceWeaveAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinInterferenceWeaveAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Interference Weave Ambient",
        "Dual-strip moiré interference, time-driven phase",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

