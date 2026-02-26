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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinCausticsAmbientEffect
namespace {
constexpr float kLGPPerlinCausticsAmbientEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinCausticsAmbientEffectOutputGain = 1.0f;
constexpr float kLGPPerlinCausticsAmbientEffectCentreBias = 1.0f;

float gLGPPerlinCausticsAmbientEffectSpeedScale = kLGPPerlinCausticsAmbientEffectSpeedScale;
float gLGPPerlinCausticsAmbientEffectOutputGain = kLGPPerlinCausticsAmbientEffectOutputGain;
float gLGPPerlinCausticsAmbientEffectCentreBias = kLGPPerlinCausticsAmbientEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinCausticsAmbientEffectParameters[] = {
    {"lgpperlin_caustics_ambient_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinCausticsAmbientEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_caustics_ambient_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinCausticsAmbientEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_caustics_ambient_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinCausticsAmbientEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinCausticsAmbientEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinCausticsAmbientEffect
    gLGPPerlinCausticsAmbientEffectSpeedScale = kLGPPerlinCausticsAmbientEffectSpeedScale;
    gLGPPerlinCausticsAmbientEffectOutputGain = kLGPPerlinCausticsAmbientEffectOutputGain;
    gLGPPerlinCausticsAmbientEffectCentreBias = kLGPPerlinCausticsAmbientEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinCausticsAmbientEffect

    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_time = 0;
    return true;
}

void LGPPerlinCausticsAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Caustic lobes radiating from centre (ambient)
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Time-driven Parameter Modulation
    // =========================================================================
    float angle = ctx.totalTimeMs * 0.001f;
    float sparkleDensity = 0.8f + 0.4f * sinf(angle * 0.2f); // 0.8-1.2
    float lobeScale = 0.7f + 0.3f * sinf(angle * 0.15f);     // 0.7-1.0
    float brightnessMod = 0.8f + 0.2f * sinf(angle * 0.25f); // 0.8-1.0

    // =========================================================================
    // Noise Field Updates
    // =========================================================================
    m_noiseX += (uint16_t)(speedNorm * 2.0f);
    m_noiseY += (uint16_t)(speedNorm * 1.0f);
    m_noiseZ += (uint16_t)(speedNorm * 0.5f);
    m_time += (uint16_t)(speedNorm * 3.0f);

    // =========================================================================
    // Rendering (centre-origin pattern with caustic lobes)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        float distNorm = dist / 79.0f;
        
        // Sample multiple octaves of noise
        uint16_t baseX = m_noiseX + (uint16_t)(dist * lobeScale * 8);
        uint16_t baseY = m_noiseY + m_time;
        uint8_t baseNoise = inoise8(baseX >> 8, baseY >> 8);
        
        uint16_t detailX = m_noiseX + (uint16_t)(dist * sparkleDensity * 16) + 10000;
        uint16_t detailY = m_noiseY + (m_time >> 1) + 5000;
        uint8_t detailNoise = inoise8(detailX >> 8, detailY >> 8);
        
        uint16_t depthX = m_noiseX + (uint16_t)(dist * 6) + 20000;
        uint16_t depthY = m_noiseZ + (m_time >> 2);
        uint8_t depthNoise = inoise8(depthX >> 8, depthY >> 8);
        
        // Combine noise octaves
        float baseNorm = baseNoise / 255.0f;
        float detailNorm = detailNoise / 255.0f;
        float depthNorm = depthNoise / 255.0f;
        
        float caustic = baseNorm * (0.5f + detailNorm * 0.5f) + depthNorm * 0.3f;
        caustic = fmaxf(0.0f, fminf(1.0f, caustic));
        caustic = caustic * caustic;
        
        float centreFalloff = 1.0f - distNorm * 0.3f;
        caustic *= centreFalloff;
        
        uint8_t paletteIndex = (uint8_t)(caustic * 255.0f);
        uint8_t brightness = (uint8_t)((0.3f + caustic * 0.7f) * brightnessMod * 255.0f * intensityNorm);
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 48);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinCausticsAmbientEffect
uint8_t LGPPerlinCausticsAmbientEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinCausticsAmbientEffectParameters) / sizeof(kLGPPerlinCausticsAmbientEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinCausticsAmbientEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinCausticsAmbientEffectParameters[index];
}

bool LGPPerlinCausticsAmbientEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_speed_scale") == 0) {
        gLGPPerlinCausticsAmbientEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_output_gain") == 0) {
        gLGPPerlinCausticsAmbientEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_centre_bias") == 0) {
        gLGPPerlinCausticsAmbientEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinCausticsAmbientEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_speed_scale") == 0) return gLGPPerlinCausticsAmbientEffectSpeedScale;
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_output_gain") == 0) return gLGPPerlinCausticsAmbientEffectOutputGain;
    if (strcmp(name, "lgpperlin_caustics_ambient_effect_centre_bias") == 0) return gLGPPerlinCausticsAmbientEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinCausticsAmbientEffect

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

