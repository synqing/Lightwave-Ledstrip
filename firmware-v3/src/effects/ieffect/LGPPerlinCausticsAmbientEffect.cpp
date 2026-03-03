/**
 * @file LGPPerlinCausticsAmbientEffect.cpp
 * @brief LGP Perlin Caustics Ambient - Sparkling caustic lobes (time-driven)
 * 
 * Ambient Perlin caustics effect:
 * - Multiple octaves of noise create caustic-like patterns
 * - Time-driven slow parameter modulation
 */

#include "LGPPerlinCausticsAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinCausticsAmbientEffect::LGPPerlinCausticsAmbientEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_time(0)
{
}

bool LGPPerlinCausticsAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_time = 0;
    return true;
}

void LGPPerlinCausticsAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Caustic lobes radiating from centre (ambient)
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // =========================================================================
    // Trail persistence: fade previous frame (long ambient trails)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, 10);

    // =========================================================================
    // Time-driven Parameter Modulation
    // =========================================================================
    float angle = ctx.totalTimeMs * 0.001f;
    float sparkleDensity = 0.8f + 0.4f * sinf(angle * 0.2f); // 0.8-1.2
    float lobeScale = 0.7f + 0.3f * sinf(angle * 0.15f);     // 0.7-1.0
    uint8_t brightnessMod = (uint8_t)(204 + 51.0f * sinf(angle * 0.25f)); // ~0.8-1.0 in 0-255

    // =========================================================================
    // Noise Field Updates — frame-rate independent using dt
    // =========================================================================
    float dtScale = dt * 120.0f;
    m_noiseX += (uint16_t)((40 + speedNorm * 80.0f) * dtScale);
    m_noiseY += (uint16_t)((20 + speedNorm * 60.0f) * dtScale);
    m_noiseZ += (uint16_t)((10 + speedNorm * 30.0f) * dtScale);
    m_time += (uint16_t)((50 + speedNorm * 100.0f) * dtScale);

    // =========================================================================
    // Rendering (centre-origin pattern with caustic lobes)
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        float distNorm = dist / 79.0f;

        // Sample multiple octaves of noise
        uint16_t baseX = m_noiseX + (uint16_t)(dist * lobeScale * 8);
        uint16_t baseY = m_noiseY + m_time;
        uint8_t baseNoise = inoise8(baseX, baseY);

        uint16_t detailX = m_noiseX + (uint16_t)(dist * sparkleDensity * 16) + 10000;
        uint16_t detailY = m_noiseY + (m_time >> 1) + 5000;
        uint8_t detailNoise = inoise8(detailX, detailY);

        uint16_t depthX = m_noiseX + (uint16_t)(dist * 6) + 20000;
        uint16_t depthY = m_noiseZ + (m_time >> 2);
        uint8_t depthNoise = inoise8(depthX, depthY);

        // Combine noise octaves
        float baseNorm = baseNoise / 255.0f;
        float detailNorm = detailNoise / 255.0f;
        float depthNorm = depthNoise / 255.0f;

        float caustic = baseNorm * (0.5f + detailNorm * 0.5f) + depthNorm * 0.3f;
        caustic = fmaxf(0.0f, fminf(1.0f, caustic));
        caustic = caustic * caustic;

        float centreFalloff = 1.0f - distNorm * 0.3f;
        caustic *= centreFalloff;

        // Brightness via scale8 chain: caustic range -> brightnessMod -> ctx.brightness
        uint8_t paletteIndex = (uint8_t)(caustic * 255.0f);
        uint8_t rawBright = (uint8_t)(76 + caustic * 179.0f); // 0.3 + caustic * 0.7
        uint8_t brightness = scale8(scale8(rawBright, brightnessMod), ctx.brightness);

        CRGB color = ctx.palette.getColor(paletteIndex, brightness);

        // Blend into faded buffer (temporal smoothing — not hard overwrite)
        nblend(ctx.leds[i], color, 80);

        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 48);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            nblend(ctx.leds[i + STRIP_LENGTH], color2, 80);
        }
    }
}

void LGPPerlinCausticsAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinCausticsAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Caustics Ambient",
        "Sparkling caustic lobes, time-driven modulation",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

