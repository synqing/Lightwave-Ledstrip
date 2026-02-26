/**
 * @file LGPPerlinBackendEmotiscopeQuarterEffect.cpp
 * @brief Perlin Backend Test C: Emotiscope 2.0 seedable Perlin (quarter-res)
 */

#include "LGPPerlinBackendEmotiscopeQuarterEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinBackendEmotiscopeQuarterEffect
namespace {
constexpr float kLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinBackendEmotiscopeQuarterEffectOutputGain = 1.0f;
constexpr float kLGPPerlinBackendEmotiscopeQuarterEffectCentreBias = 1.0f;

float gLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale = kLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale;
float gLGPPerlinBackendEmotiscopeQuarterEffectOutputGain = kLGPPerlinBackendEmotiscopeQuarterEffectOutputGain;
float gLGPPerlinBackendEmotiscopeQuarterEffectCentreBias = kLGPPerlinBackendEmotiscopeQuarterEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinBackendEmotiscopeQuarterEffectParameters[] = {
    {"lgpperlin_backend_emotiscope_quarter_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_backend_emotiscope_quarter_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinBackendEmotiscopeQuarterEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_backend_emotiscope_quarter_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinBackendEmotiscopeQuarterEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinBackendEmotiscopeQuarterEffect

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Emotiscope 2.0 hash function (same as full-res)
unsigned int LGPPerlinBackendEmotiscopeQuarterEffect::hash(unsigned int x, unsigned int seed) {
    const unsigned int m = 0x5bd1e995U;
    unsigned int h = seed;
    unsigned int k = x;
    k *= m;
    k ^= k >> 24;
    k *= m;
    h *= m;
    h ^= k;
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

unsigned int LGPPerlinBackendEmotiscopeQuarterEffect::hashVec2(UVec2 x, unsigned int seed) {
    const unsigned int m = 0x5bd1e995U;
    unsigned int h = seed;
    unsigned int k;
    
    k = x.x;
    k *= m;
    k ^= k >> 24;
    k *= m;
    h *= m;
    h ^= k;
    
    k = x.y;
    k *= m;
    k ^= k >> 24;
    k *= m;
    h *= m;
    h ^= k;
    
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

Vec2 LGPPerlinBackendEmotiscopeQuarterEffect::gradientDirection(unsigned int hash) {
    switch (hash & 7) {
        case 0: return Vec2(1.0f, 1.0f);
        case 1: return Vec2(-1.0f, 1.0f);
        case 2: return Vec2(1.0f, -1.0f);
        case 3: return Vec2(-1.0f, -1.0f);
        case 4: return Vec2(1.0f, 0.0f);
        case 5: return Vec2(-1.0f, 0.0f);
        case 6: return Vec2(0.0f, 1.0f);
        case 7: return Vec2(0.0f, -1.0f);
    }
    return Vec2(0.0f, 0.0f);
}

float LGPPerlinBackendEmotiscopeQuarterEffect::interpolatePerlin(float v1, float v2, float v3, float v4, Vec2 t) {
    float mix1 = v1 + t.x * (v2 - v1);
    float mix2 = v3 + t.x * (v4 - v3);
    return mix1 + t.y * (mix2 - mix1);
}

Vec2 LGPPerlinBackendEmotiscopeQuarterEffect::fade(Vec2 t) {
    float tx = t.x * t.x * t.x * (t.x * (t.x * 6.0f - 15.0f) + 10.0f);
    float ty = t.y * t.y * t.y * (t.y * (t.y * 6.0f - 15.0f) + 10.0f);
    return Vec2(tx, ty);
}

float LGPPerlinBackendEmotiscopeQuarterEffect::dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

float LGPPerlinBackendEmotiscopeQuarterEffect::perlinNoise(Vec2 position, unsigned int seed) {
    Vec2 floorPos(floorf(position.x), floorf(position.y));
    Vec2 fractPos(position.x - floorPos.x, position.y - floorPos.y);
    UVec2 cell((unsigned int)floorPos.x, (unsigned int)floorPos.y);
    
    float v1 = dot(gradientDirection(hashVec2(cell, seed)), fractPos);
    float v2 = dot(gradientDirection(hashVec2(UVec2(cell.x + 1, cell.y), seed)), Vec2(fractPos.x - 1.0f, fractPos.y));
    float v3 = dot(gradientDirection(hashVec2(UVec2(cell.x, cell.y + 1), seed)), Vec2(fractPos.x, fractPos.y - 1.0f));
    float v4 = dot(gradientDirection(hashVec2(UVec2(cell.x + 1, cell.y + 1), seed)), Vec2(fractPos.x - 1.0f, fractPos.y - 1.0f));
    
    return interpolatePerlin(v1, v2, v3, v4, fade(fractPos));
}

float LGPPerlinBackendEmotiscopeQuarterEffect::perlinNoiseOctaves(Vec2 position, int frequency, int octaveCount, float persistence, float lacunarity, unsigned int seed) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float currentFreq = (float)frequency;
    unsigned int currentSeed = seed;
    
    for (int i = 0; i < octaveCount; i++) {
        currentSeed = hash(currentSeed, 0x0U);
        value += perlinNoise(Vec2(position.x * currentFreq, position.y * currentFreq), currentSeed) * amplitude;
        amplitude *= persistence;
        currentFreq *= lacunarity;
    }
    return value;
}

LGPPerlinBackendEmotiscopeQuarterEffect::LGPPerlinBackendEmotiscopeQuarterEffect()
    : m_noiseBuffer{}
    , m_seed(0)
    , m_positionX(0.0f)
    , m_positionY(0.0f)
    , m_lastUpdateMs(0)
{
    for (uint8_t i = 0; i < QUARTER_RES; i++) {
        m_noiseBuffer[i] = 0.0f;
    }
}

bool LGPPerlinBackendEmotiscopeQuarterEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinBackendEmotiscopeQuarterEffect
    gLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale = kLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale;
    gLGPPerlinBackendEmotiscopeQuarterEffectOutputGain = kLGPPerlinBackendEmotiscopeQuarterEffectOutputGain;
    gLGPPerlinBackendEmotiscopeQuarterEffectCentreBias = kLGPPerlinBackendEmotiscopeQuarterEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinBackendEmotiscopeQuarterEffect

    m_seed = (unsigned int)((uint32_t)random16() << 16 | random16());
    m_positionX = (float)(random16() % 1000);
    m_positionY = (float)(random16() % 1000);
    m_lastUpdateMs = ctx.totalTimeMs;
    
    // Initialize buffer
    for (uint8_t i = 0; i < QUARTER_RES; i++) {
        m_noiseBuffer[i] = 0.0f;
    }
    
    return true;
}

void LGPPerlinBackendEmotiscopeQuarterEffect::render(plugins::EffectContext& ctx) {
    // CENTRE ORIGIN - Emotiscope 2.0 Perlin quarter-res + interpolation test
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;
    
    // Advection (use ms-based delta like Emotiscope 2.0)
    // Clamp delta to prevent teleport jumps after stalls (max 50ms)
    float deltaMs = ctx.deltaTimeSeconds * 1000.0f;
    if (deltaMs > 50) deltaMs = 50;
    m_positionY += 0.001f * (float)deltaMs * (1.0f + speedNorm);
    
    // Update quarter-res buffer periodically (every 10ms like Emotiscope 2.0)
    bool shouldUpdate = (ctx.totalTimeMs - m_lastUpdateMs >= UPDATE_INTERVAL_MS);
    if (shouldUpdate) {
        m_lastUpdateMs = ctx.totalTimeMs;
        
        // Sample at quarter resolution (20 points across 80 distances)
        for (uint8_t i = 0; i < QUARTER_RES; i++) {
            // Map quarter-res index to distance (0..79)
            float dist = (float)i * (79.0f / (QUARTER_RES - 1));
            
            Vec2 pos(m_positionX + dist * SPATIAL_SCALE, m_positionY);
            float noiseValue = perlinNoiseOctaves(pos, (int)FREQUENCY, OCTAVE_COUNT, PERSISTENCE, LACUNARITY, m_seed);
            
            // Normalize to [0, 1]
            m_noiseBuffer[i] = (noiseValue + 1.0f) * 0.5f;
            m_noiseBuffer[i] = fmaxf(0.0f, fminf(1.0f, m_noiseBuffer[i]));
        }
    }
    
    // No fadeToBlackBy - we overwrite every LED each frame
    
    // Render: interpolate from quarter-res buffer
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        uint16_t dist = centerPairDistance(i);
        
        // Map distance (0..79) to quarter-res buffer index (0..19)
        float bufferIndex = (float)dist * ((QUARTER_RES - 1) / 79.0f);
        uint8_t idx0 = (uint8_t)bufferIndex;
        uint8_t idx1 = (idx0 < QUARTER_RES - 1) ? idx0 + 1 : idx0;
        float t = bufferIndex - (float)idx0;
        
        // Linear interpolation
        float noiseNorm = m_noiseBuffer[idx0] * (1.0f - t) + m_noiseBuffer[idx1] * t;
        noiseNorm = fmaxf(0.0f, fminf(1.0f, noiseNorm));
        
        // Same shaping as other tests
        noiseNorm = noiseNorm * noiseNorm;
        float brightnessNorm = 0.2f + noiseNorm * 0.8f;
        uint8_t brightness = (uint8_t)(brightnessNorm * 255.0f * intensityNorm);
        uint8_t paletteIndex = (uint8_t)(noiseNorm * 255.0f) + ctx.gHue;
        
        CRGB color = ctx.palette.getColor(paletteIndex, brightness);
        ctx.leds[i] = color;
        
        if (i + STRIP_LENGTH < ctx.ledCount) {
            uint8_t paletteIndex2 = (uint8_t)(paletteIndex + 32);
            CRGB color2 = ctx.palette.getColor(paletteIndex2, brightness);
            ctx.leds[i + STRIP_LENGTH] = color2;
        }
    }
}


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinBackendEmotiscopeQuarterEffect
uint8_t LGPPerlinBackendEmotiscopeQuarterEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinBackendEmotiscopeQuarterEffectParameters) / sizeof(kLGPPerlinBackendEmotiscopeQuarterEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinBackendEmotiscopeQuarterEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinBackendEmotiscopeQuarterEffectParameters[index];
}

bool LGPPerlinBackendEmotiscopeQuarterEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_speed_scale") == 0) {
        gLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_output_gain") == 0) {
        gLGPPerlinBackendEmotiscopeQuarterEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_centre_bias") == 0) {
        gLGPPerlinBackendEmotiscopeQuarterEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinBackendEmotiscopeQuarterEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_speed_scale") == 0) return gLGPPerlinBackendEmotiscopeQuarterEffectSpeedScale;
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_output_gain") == 0) return gLGPPerlinBackendEmotiscopeQuarterEffectOutputGain;
    if (strcmp(name, "lgpperlin_backend_emotiscope_quarter_effect_centre_bias") == 0) return gLGPPerlinBackendEmotiscopeQuarterEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinBackendEmotiscopeQuarterEffect

void LGPPerlinBackendEmotiscopeQuarterEffect::cleanup() {
    // No resources to free
}

const plugins::EffectMetadata& LGPPerlinBackendEmotiscopeQuarterEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Perlin Test: Emotiscope2 Quarter",
        "Emotiscope 2.0 Perlin quarter-res (TEST)",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

