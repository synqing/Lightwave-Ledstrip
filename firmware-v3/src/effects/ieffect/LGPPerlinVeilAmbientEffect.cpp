/**
 * @file LGPPerlinVeilAmbientEffect.cpp
 * @brief LGP Perlin Veil Ambient - Slow drifting curtains/fog from centre (time-driven)
 * 
 * Ambient Perlin noise field effect:
 * - Two independent noise fields: hue index and luminance mask
 * - Centre-origin sampling: distance from centre pair (79/80)
 * - Time-driven slow drift and breathing contrast
 */

#include "LGPPerlinVeilAmbientEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinVeilAmbientEffect::LGPPerlinVeilAmbientEffect()
    : m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_time(0)
{
}

bool LGPPerlinVeilAmbientEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_time = 0;
    return true;
}

void LGPPerlinVeilAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Perlin noise veils drifting from centre (ambient)
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;

    // =========================================================================
    // Trail persistence: fade previous frame (long ambient trails)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, 10);

    // =========================================================================
    // Time-driven State Updates (no audio)
    // =========================================================================
    // Slow sine-based drift
    float angle = ctx.totalTimeMs * 0.001f;
    float sine = sinf(angle * 0.1f);

    // Breathing contrast modulation
    float contrast = 0.4f + 0.2f * sinf(angle * 0.3f); // Slow breathing

    // =========================================================================
    // Noise Field Advection — frame-rate independent using dt
    // =========================================================================
    // Scale noise step by dt (targeting ~120 FPS base rate)
    float dtScale = dt * 120.0f;
    m_noiseX += (uint16_t)((40 + speedNorm * 80.0f + sine * 20.0f) * dtScale);
    m_noiseY += (uint16_t)((20 + speedNorm * 60.0f) * dtScale);
    m_noiseZ += (uint16_t)((10 + speedNorm * 30.0f) * dtScale);

    m_time += (uint16_t)((40 + speedNorm * 80.0f) * dtScale);

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair (0 at centre, 79 at edges)
        uint16_t dist = centerPairDistance(i);

        // Sample noise fields at centre-origin coordinates
        uint16_t noiseXCoord = m_noiseX + (dist * 4);
        uint16_t noiseYCoord = m_noiseY + m_time;

        // Sample two independent noise fields
        uint8_t hueNoise = inoise8(noiseXCoord, noiseYCoord);
        uint8_t lumNoise = inoise8(noiseXCoord + 10000, noiseYCoord + 5000);

        // Map hue noise to palette index
        uint8_t paletteIndex = hueNoise;

        // Map luminance noise with contrast modulation
        float lumNorm = (lumNoise / 255.0f);
        // Apply contrast
        if (lumNorm < 0.5f) {
            lumNorm = lumNorm * (1.0f - contrast * 0.5f);
        } else {
            lumNorm = 0.5f + (lumNorm - 0.5f) * (1.0f + contrast * 0.5f);
        }
        lumNorm = fmaxf(0.0f, fminf(1.0f, lumNorm));

        // Square for bias toward darker
        lumNorm = lumNorm * lumNorm;

        // Scale to brightness range using scale8
        uint8_t rawBright = (uint8_t)(25 + lumNorm * 230.0f);
        uint8_t brightness = scale8(rawBright, ctx.brightness);

        // Get colour from palette
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);

        // Blend into faded buffer (temporal smoothing — not hard overwrite)
        nblend(ctx.leds[i], color, 80);

        // Apply to strip 2 (mirrored)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 32);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            nblend(ctx.leds[i + STRIP_LENGTH], color2, 80);
        }
    }
}

void LGPPerlinVeilAmbientEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinVeilAmbientEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Perlin Veil Ambient",
        "Slow drifting noise curtains from centre, time-driven",
        plugins::EffectCategory::AMBIENT,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

