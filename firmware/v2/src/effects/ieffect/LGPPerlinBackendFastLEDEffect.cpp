/**
 * @file LGPPerlinBackendFastLEDEffect.cpp
 * @brief Perlin Backend Test A: FastLED inoise8 baseline
 */

#include "LGPPerlinBackendFastLEDEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinBackendFastLEDEffect
namespace {
constexpr float kLGPPerlinBackendFastLEDEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinBackendFastLEDEffectOutputGain = 1.0f;
constexpr float kLGPPerlinBackendFastLEDEffectCentreBias = 1.0f;

float gLGPPerlinBackendFastLEDEffectSpeedScale = kLGPPerlinBackendFastLEDEffectSpeedScale;
float gLGPPerlinBackendFastLEDEffectOutputGain = kLGPPerlinBackendFastLEDEffectOutputGain;
float gLGPPerlinBackendFastLEDEffectCentreBias = kLGPPerlinBackendFastLEDEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinBackendFastLEDEffectParameters[] = {
    {"lgpperlin_backend_fast_ledeffect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinBackendFastLEDEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_backend_fast_ledeffect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinBackendFastLEDEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_backend_fast_ledeffect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinBackendFastLEDEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinBackendFastLEDEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinBackendFastLEDEffect
    gLGPPerlinBackendFastLEDEffectSpeedScale = kLGPPerlinBackendFastLEDEffectSpeedScale;
    gLGPPerlinBackendFastLEDEffectOutputGain = kLGPPerlinBackendFastLEDEffectOutputGain;
    gLGPPerlinBackendFastLEDEffectCentreBias = kLGPPerlinBackendFastLEDEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinBackendFastLEDEffect

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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinBackendFastLEDEffect
uint8_t LGPPerlinBackendFastLEDEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinBackendFastLEDEffectParameters) / sizeof(kLGPPerlinBackendFastLEDEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinBackendFastLEDEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinBackendFastLEDEffectParameters[index];
}

bool LGPPerlinBackendFastLEDEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_speed_scale") == 0) {
        gLGPPerlinBackendFastLEDEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_output_gain") == 0) {
        gLGPPerlinBackendFastLEDEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_centre_bias") == 0) {
        gLGPPerlinBackendFastLEDEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinBackendFastLEDEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_speed_scale") == 0) return gLGPPerlinBackendFastLEDEffectSpeedScale;
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_output_gain") == 0) return gLGPPerlinBackendFastLEDEffectOutputGain;
    if (strcmp(name, "lgpperlin_backend_fast_ledeffect_centre_bias") == 0) return gLGPPerlinBackendFastLEDEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinBackendFastLEDEffect

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

