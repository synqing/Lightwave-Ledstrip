// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPPerlinShocklinesAmbientEffect.cpp
 * @brief LGP Perlin Shocklines Ambient - Time-driven travelling ridges
 * 
 * Ambient Perlin effect:
 * - Base noise field provides organic texture
 * - Periodic shockwave injection at centre (time-based)
 * - Shockwaves propagate outward and dissolve
 */

#include "LGPPerlinShocklinesAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinShocklinesAmbientEffect::LGPPerlinShocklinesAmbientEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_time(0)
    , m_lastShockTime(0)
{
    memset(m_shockBuffer, 0, sizeof(m_shockBuffer));
}

bool LGPPerlinShocklinesAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_time = 0;
    m_lastShockTime = 0;
    memset(m_shockBuffer, 0, sizeof(m_shockBuffer));
    return true;
}

void LGPPerlinShocklinesAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Shockwaves propagate outward from centre (ambient)
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Periodic Shockwave Injection (time-based, no audio)
    // =========================================================================
    // Inject shockwave every 2-4 seconds (speed-dependent)
    uint32_t shockInterval = 2000 + (uint32_t)((1.0f - speedNorm) * 2000); // 2-4 seconds
    if (ctx.totalTimeMs - m_lastShockTime > shockInterval) {
        m_lastShockTime = ctx.totalTimeMs;
        // Inject energy at centre pair
        float shockEnergy = 0.5f + random8(50) / 255.0f; // 0.5-1.0
        m_shockBuffer[CENTER_LEFT] += shockEnergy;
        m_shockBuffer[CENTER_RIGHT] += shockEnergy;
    }

    // =========================================================================
    // Shockwave Propagation (centre-origin)
    // =========================================================================
    float propagationSpeed = speedNorm * 0.5f;
    float decayRate = 0.92f + speedNorm * 0.06f;
    
    // Propagate from centre outward (left side)
    for (int16_t i = CENTER_LEFT - 1; i >= 0; i--) {
        if (i + 1 < STRIP_LENGTH) {
            float energy = m_shockBuffer[i + 1] * propagationSpeed;
            m_shockBuffer[i] += energy;
        }
    }
    
    // Propagate from centre outward (right side)
    for (uint16_t i = CENTER_RIGHT + 1; i < STRIP_LENGTH; i++) {
        if (i > 0) {
            float energy = m_shockBuffer[i - 1] * propagationSpeed;
            m_shockBuffer[i] += energy;
        }
    }
    
    // Decay all shock energy
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        m_shockBuffer[i] *= decayRate;
        if (m_shockBuffer[i] < 0.01f) {
            m_shockBuffer[i] = 0.0f;
        }
    }

    // =========================================================================
    // Noise Field Updates
    // =========================================================================
    m_noiseX += (uint16_t)(speedNorm * 3.0f);
    m_noiseY += (uint16_t)(speedNorm * 1.5f);
    m_time += (uint16_t)(speedNorm * 2.0f);

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    float sharpness = 0.5f; // Fixed sharpness for ambient

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        
        // Sample base noise field
        uint16_t noiseXCoord = m_noiseX + (dist * 4);
        uint16_t noiseYCoord = m_noiseY + m_time;
        uint8_t baseNoise = inoise8(noiseXCoord >> 8, noiseYCoord >> 8);
        
        // Get shockwave energy
        float shockEnergy = m_shockBuffer[i];
        
        // Combine noise with shockwave
        float noiseNorm = baseNoise / 255.0f;
        float shockNorm = shockEnergy;
        
        float combined = noiseNorm + shockNorm * sharpness * 2.0f;
        combined = fmaxf(0.0f, fminf(1.0f, combined));
        
        // Map to palette and brightness
        uint8_t paletteIndex = (uint8_t)(combined * 255.0f);
        uint8_t brightness = (uint8_t)((0.2f + combined * 0.8f) * 255.0f * intensityNorm);
        
        if (shockEnergy > 0.1f) {
            brightness = qadd8(brightness, (uint8_t)(shockEnergy * 100.0f));
        }
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 64);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinShocklinesAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinShocklinesAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Shocklines Ambient",
        "Time-driven travelling ridges propagating from centre",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

