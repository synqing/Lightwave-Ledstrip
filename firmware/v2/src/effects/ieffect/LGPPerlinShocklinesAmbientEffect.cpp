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
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinShocklinesAmbientEffect::LGPPerlinShocklinesAmbientEffect()
    : m_ps(nullptr)
    , m_noiseX(0)
    , m_noiseY(0)
    , m_time(0)
    , m_lastShockTime(0)
{
}

bool LGPPerlinShocklinesAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_time = 0;
    m_lastShockTime = 0;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<ShocklinesPsram*>(
            heap_caps_malloc(sizeof(ShocklinesPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(ShocklinesPsram));
#endif
    return true;
}

void LGPPerlinShocklinesAmbientEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
    // CENTRE ORIGIN - Shockwaves propagate outward from centre (ambient)
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Periodic Shockwave Injection (time-based, no audio)
    // =========================================================================
    uint32_t shockInterval = 2000 + (uint32_t)((1.0f - speedNorm) * 2000);
    if (ctx.totalTimeMs - m_lastShockTime > shockInterval) {
        m_lastShockTime = ctx.totalTimeMs;
        float shockEnergy = 0.5f + random8(50) / 255.0f;
        m_ps->shockBuffer[CENTER_LEFT] += shockEnergy;
        m_ps->shockBuffer[CENTER_RIGHT] += shockEnergy;
    }

    // =========================================================================
    // Shockwave Propagation (centre-origin)
    // =========================================================================
    float propagationSpeed = speedNorm * 0.5f;
    float decayRate = 0.92f + speedNorm * 0.06f;

    for (int16_t i = CENTER_LEFT - 1; i >= 0; i--) {
        if (i + 1 < STRIP_LENGTH) {
            float energy = m_ps->shockBuffer[i + 1] * propagationSpeed;
            m_ps->shockBuffer[i] += energy;
        }
    }

    for (uint16_t i = CENTER_RIGHT + 1; i < STRIP_LENGTH; i++) {
        if (i > 0) {
            float energy = m_ps->shockBuffer[i - 1] * propagationSpeed;
            m_ps->shockBuffer[i] += energy;
        }
    }

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        m_ps->shockBuffer[i] *= decayRate;
        if (m_ps->shockBuffer[i] < 0.01f) {
            m_ps->shockBuffer[i] = 0.0f;
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
        float shockEnergy = m_ps->shockBuffer[i];
        
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
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
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

