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
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinVeilAmbientEffect
namespace {
constexpr float kLGPPerlinVeilAmbientEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinVeilAmbientEffectOutputGain = 1.0f;
constexpr float kLGPPerlinVeilAmbientEffectCentreBias = 1.0f;

float gLGPPerlinVeilAmbientEffectSpeedScale = kLGPPerlinVeilAmbientEffectSpeedScale;
float gLGPPerlinVeilAmbientEffectOutputGain = kLGPPerlinVeilAmbientEffectOutputGain;
float gLGPPerlinVeilAmbientEffectCentreBias = kLGPPerlinVeilAmbientEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinVeilAmbientEffectParameters[] = {
    {"lgpperlin_veil_ambient_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinVeilAmbientEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_veil_ambient_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinVeilAmbientEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_veil_ambient_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinVeilAmbientEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinVeilAmbientEffect

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
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinVeilAmbientEffect
    gLGPPerlinVeilAmbientEffectSpeedScale = kLGPPerlinVeilAmbientEffectSpeedScale;
    gLGPPerlinVeilAmbientEffectOutputGain = kLGPPerlinVeilAmbientEffectOutputGain;
    gLGPPerlinVeilAmbientEffectCentreBias = kLGPPerlinVeilAmbientEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinVeilAmbientEffect

    m_noiseX = random16();
    m_noiseY = random16();
    m_noiseZ = random16();
    m_time = 0;
    return true;
}

void LGPPerlinVeilAmbientEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Perlin noise veils drifting from centre (ambient)
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    // =========================================================================
    // Time-driven State Updates (no audio)
    // =========================================================================
    // Slow sine-based drift
    float angle = ctx.totalTimeMs * 0.001f;
    float sine = sinf(angle * 0.1f);
    
    // Breathing contrast modulation
    float contrast = 0.4f + 0.2f * sinf(angle * 0.3f); // Slow breathing

    // =========================================================================
    // Noise Field Advection (centre-origin sampling)
    // =========================================================================
    // Update noise coordinates (slow drift)
    m_noiseX += (uint16_t)((0.01f * sine) * 255.0f);
    m_noiseY += (uint16_t)(0.0001f * 255.0f);
    m_noiseZ += (uint16_t)(0.0005f * 255.0f);
    
    m_time += (uint16_t)(speedNorm * 2.0f);

    // =========================================================================
    // Rendering (centre-origin pattern)
    // =========================================================================
    fadeToBlackBy(ctx.leds, ctx.ledCount, ctx.fadeAmount);

    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Calculate distance from centre pair (0 at centre, 79 at edges)
        uint16_t dist = centerPairDistance(i);
        
        // Sample noise fields at centre-origin coordinates
        uint16_t noiseXCoord = m_noiseX + (dist * 4);
        uint16_t noiseYCoord = m_noiseY + m_time;
        uint16_t noiseZCoord = m_noiseZ + (dist * 2);
        
        // Sample two independent noise fields
        uint8_t hueNoise = inoise8(noiseXCoord >> 8, noiseYCoord >> 8);
        uint8_t lumNoise = inoise8((noiseXCoord + 10000) >> 8, (noiseYCoord + 5000) >> 8);
        
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
        
        // Scale to brightness range
        float brightnessNorm = 0.1f + lumNorm * 0.9f;
        uint8_t brightness = (uint8_t)(brightnessNorm * 255.0f * intensityNorm);
        
        // Get colour from palette
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        
        // Apply to strip 1
        ctx.leds[i] = color;
        
        // Apply to strip 2 (mirrored)
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 32);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinVeilAmbientEffect
uint8_t LGPPerlinVeilAmbientEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinVeilAmbientEffectParameters) / sizeof(kLGPPerlinVeilAmbientEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinVeilAmbientEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinVeilAmbientEffectParameters[index];
}

bool LGPPerlinVeilAmbientEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_veil_ambient_effect_speed_scale") == 0) {
        gLGPPerlinVeilAmbientEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_veil_ambient_effect_output_gain") == 0) {
        gLGPPerlinVeilAmbientEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_veil_ambient_effect_centre_bias") == 0) {
        gLGPPerlinVeilAmbientEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinVeilAmbientEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_veil_ambient_effect_speed_scale") == 0) return gLGPPerlinVeilAmbientEffectSpeedScale;
    if (strcmp(name, "lgpperlin_veil_ambient_effect_output_gain") == 0) return gLGPPerlinVeilAmbientEffectOutputGain;
    if (strcmp(name, "lgpperlin_veil_ambient_effect_centre_bias") == 0) return gLGPPerlinVeilAmbientEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinVeilAmbientEffect

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

