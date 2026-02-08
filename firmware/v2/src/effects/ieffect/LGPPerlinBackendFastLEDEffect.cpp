/**
 * @file LGPPerlinBackendFastLEDEffect.cpp
 * @brief Perlin Backend Test A: FastLED inoise8 baseline
 */

#include "LGPPerlinBackendFastLEDEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

LGPPerlinBackendFastLEDEffect::LGPPerlinBackendFastLEDEffect()
    : m_seed(0)
    , m_noiseX(0)
    , m_noiseY(0)
    , m_noiseZ(0)
    , m_momentum(0.0f)
    , m_time(0)
{
}

bool LGPPerlinBackendFastLEDEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // Seed for "non-reproducible" feel (different each boot/init)
    m_seed = (uint32_t)random16() << 16 | random16();
    
    // Initialize noise coordinates with seed offset
    m_noiseX = (uint16_t)(m_seed & 0xFFFF);
    m_noiseY = (uint16_t)((m_seed >> 16) & 0xFFFF);
    m_noiseZ = (uint16_t)((m_seed * 0x9E3779B9) & 0xFFFF);
    m_momentum = 0.0f;
    m_time = 0;
    
    return true;
}

void LGPPerlinBackendFastLEDEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - FastLED inoise8 baseline test
    const bool hasAudio = ctx.audio.available;
    float dt = ctx.getSafeDeltaSeconds();
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    
    // =========================================================================
    // Audio-Driven Momentum (Emotiscope-style)
    // =========================================================================
    float push = 0.0f;
#if FEATURE_AUDIO_SYNC
    if (hasAudio) {
        float energy = ctx.audio.rms();
        push = energy * energy * energy * energy * speedNorm * 0.1f; // Heavy emphasis on loud
    }
#endif
    m_momentum *= powf(0.99f, dt * 60.0f); // Smooth decay (dt-corrected)
    if (push > m_momentum) {
        m_momentum = push; // Only boost, no jarring drops
    }
    
    // =========================================================================
    // Advection: slow drift through noise field (REVERSED for center→edges)
    // =========================================================================
    uint16_t baseStep = (uint16_t)(4 + (uint16_t)(speedNorm * 12.0f));
    uint16_t momentumStep = (uint16_t)(m_momentum * 600.0f); // Scale momentum to step size
    uint16_t tStep = baseStep + momentumStep;
    
    // Reverse: subtract instead of add (makes pattern flow center→edges)
    m_time = (uint16_t)(m_time - tStep);
    m_noiseX = (uint16_t)(m_noiseX - (uint16_t)(3 + (tStep >> 1)));
    m_noiseY = (uint16_t)(m_noiseY - (uint16_t)(2 + (tStep >> 2)));
    m_noiseZ = (uint16_t)(m_noiseZ - (uint16_t)(1 + (tStep >> 3)));
    
    // No fadeToBlackBy - we overwrite every LED each frame
    
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Centre-origin: distance from centre pair
        uint16_t dist = centerPairDistance(i);
        
        // Sample noise with SMALL multipliers (like working effects: i*5, not dist*28)
        // NO >>8 right-shifting (that collapses coordinate space!)
        uint16_t x = (uint16_t)(m_noiseX + dist * 5);
        uint16_t y = (uint16_t)(m_noiseY + m_time);
        uint16_t z = m_noiseZ;
        
        // Sample 3D noise
        uint8_t noise = inoise8(x, y, z);
        
        // Map to palette and brightness (same shaping for all three tests)
        uint8_t paletteIndex = noise + ctx.gHue;
        float noiseNorm = noise / 255.0f;
        noiseNorm = noiseNorm * noiseNorm; // Bias toward darker, stronger highlights
        float brightnessNorm = 0.2f + noiseNorm * 0.8f;
        uint8_t brightness = (uint8_t)(brightnessNorm * 255.0f * intensityNorm);
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            // Strip 2: phase offset for interference
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 32);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}

void LGPPerlinBackendFastLEDEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinBackendFastLEDEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Perlin Test: FastLED",
        "FastLED inoise8 baseline (TEST)",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

