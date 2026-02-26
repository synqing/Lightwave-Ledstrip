/**
 * @file LGPPerlinBackendEmotiscopeFullEffect.cpp
 * @brief Perlin Backend Test B: Emotiscope 2.0 seedable Perlin (full-res)
 */

#include "LGPPerlinBackendEmotiscopeFullEffect.h"
#include "../CoreEffects.h"
#include "../../config/features.h"
#include <FastLED.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>


// AUTO_TUNABLES_BULK_BEGIN:LGPPerlinBackendEmotiscopeFullEffect
namespace {
constexpr float kLGPPerlinBackendEmotiscopeFullEffectSpeedScale = 1.0f;
constexpr float kLGPPerlinBackendEmotiscopeFullEffectOutputGain = 1.0f;
constexpr float kLGPPerlinBackendEmotiscopeFullEffectCentreBias = 1.0f;

float gLGPPerlinBackendEmotiscopeFullEffectSpeedScale = kLGPPerlinBackendEmotiscopeFullEffectSpeedScale;
float gLGPPerlinBackendEmotiscopeFullEffectOutputGain = kLGPPerlinBackendEmotiscopeFullEffectOutputGain;
float gLGPPerlinBackendEmotiscopeFullEffectCentreBias = kLGPPerlinBackendEmotiscopeFullEffectCentreBias;

const lightwaveos::plugins::EffectParameter kLGPPerlinBackendEmotiscopeFullEffectParameters[] = {
    {"lgpperlin_backend_emotiscope_full_effect_speed_scale", "Speed Scale", 0.25f, 2.0f, kLGPPerlinBackendEmotiscopeFullEffectSpeedScale, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "timing", "x", false},
    {"lgpperlin_backend_emotiscope_full_effect_output_gain", "Output Gain", 0.25f, 2.0f, kLGPPerlinBackendEmotiscopeFullEffectOutputGain, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "blend", "x", false},
    {"lgpperlin_backend_emotiscope_full_effect_centre_bias", "Centre Bias", 0.50f, 1.50f, kLGPPerlinBackendEmotiscopeFullEffectCentreBias, lightwaveos::plugins::EffectParameterType::FLOAT, 0.05f, "wave", "x", false},
};
} // namespace
// AUTO_TUNABLES_BULK_END:LGPPerlinBackendEmotiscopeFullEffect
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

// Emotiscope 2.0 hash function
unsigned int LGPPerlinBackendEmotiscopeFullEffect::hash(unsigned int x, unsigned int seed) {
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

unsigned int LGPPerlinBackendEmotiscopeFullEffect::hashVec2(UVec2 x, unsigned int seed) {
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

Vec2 LGPPerlinBackendEmotiscopeFullEffect::gradientDirection(unsigned int hash) {
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

float LGPPerlinBackendEmotiscopeFullEffect::interpolatePerlin(float v1, float v2, float v3, float v4, Vec2 t) {
    float mix1 = v1 + t.x * (v2 - v1);
    float mix2 = v3 + t.x * (v4 - v3);
    return mix1 + t.y * (mix2 - mix1);
}

Vec2 LGPPerlinBackendEmotiscopeFullEffect::fade(Vec2 t) {
    float tx = t.x * t.x * t.x * (t.x * (t.x * 6.0f - 15.0f) + 10.0f);
    float ty = t.y * t.y * t.y * (t.y * (t.y * 6.0f - 15.0f) + 10.0f);
    return Vec2(tx, ty);
}

float LGPPerlinBackendEmotiscopeFullEffect::dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

float LGPPerlinBackendEmotiscopeFullEffect::perlinNoise(Vec2 position, unsigned int seed) {
    Vec2 floorPos(floorf(position.x), floorf(position.y));
    Vec2 fractPos(position.x - floorPos.x, position.y - floorPos.y);
    UVec2 cell((unsigned int)floorPos.x, (unsigned int)floorPos.y);
    
    float v1 = dot(gradientDirection(hashVec2(cell, seed)), fractPos);
    float v2 = dot(gradientDirection(hashVec2(UVec2(cell.x + 1, cell.y), seed)), Vec2(fractPos.x - 1.0f, fractPos.y));
    float v3 = dot(gradientDirection(hashVec2(UVec2(cell.x, cell.y + 1), seed)), Vec2(fractPos.x, fractPos.y - 1.0f));
    float v4 = dot(gradientDirection(hashVec2(UVec2(cell.x + 1, cell.y + 1), seed)), Vec2(fractPos.x - 1.0f, fractPos.y - 1.0f));
    
    return interpolatePerlin(v1, v2, v3, v4, fade(fractPos));
}

float LGPPerlinBackendEmotiscopeFullEffect::perlinNoiseOctaves(Vec2 position, int frequency, int octaveCount, float persistence, float lacunarity, unsigned int seed) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float currentFreq = (float)frequency;
    unsigned int currentSeed = seed;
    
    for (int i = 0; i < octaveCount; i++) {
        currentSeed = hash(currentSeed, 0x0U); // New seed per octave
        value += perlinNoise(Vec2(position.x * currentFreq, position.y * currentFreq), currentSeed) * amplitude;
        amplitude *= persistence;
        currentFreq *= lacunarity;
    }
    return value;
}

LGPPerlinBackendEmotiscopeFullEffect::LGPPerlinBackendEmotiscopeFullEffect()
    : m_seed(0)
    , m_positionX(0.0f)
    , m_ps(nullptr)
    , m_positionY(0.0f)
    , m_momentum(0.0f)
    , m_lastUpdateMs(0)
{
}

bool LGPPerlinBackendEmotiscopeFullEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    // AUTO_TUNABLES_BULK_RESET_BEGIN:LGPPerlinBackendEmotiscopeFullEffect
    gLGPPerlinBackendEmotiscopeFullEffectSpeedScale = kLGPPerlinBackendEmotiscopeFullEffectSpeedScale;
    gLGPPerlinBackendEmotiscopeFullEffectOutputGain = kLGPPerlinBackendEmotiscopeFullEffectOutputGain;
    gLGPPerlinBackendEmotiscopeFullEffectCentreBias = kLGPPerlinBackendEmotiscopeFullEffectCentreBias;
    // AUTO_TUNABLES_BULK_RESET_END:LGPPerlinBackendEmotiscopeFullEffect

    m_seed = (unsigned int)((uint32_t)random16() << 16 | random16());
    m_positionX = (float)(random16() % 1000);
    m_positionY = (float)(random16() % 1000);
    m_momentum = 0.0f;
    m_lastUpdateMs = ctx.totalTimeMs;
#ifndef NATIVE_BUILD
    if (!m_ps) {
        m_ps = static_cast<PerlinBackendPsram*>(
            heap_caps_malloc(sizeof(PerlinBackendPsram), MALLOC_CAP_SPIRAM));
        if (!m_ps) return false;
    }
    memset(m_ps, 0, sizeof(PerlinBackendPsram));
#endif
    generateNoiseArray();
    normalizeNoiseArray();
    return true;
}

void LGPPerlinBackendEmotiscopeFullEffect::generateNoiseArray() {
    if (!m_ps) return;
    for (int i = 0; i < 80; i++) {
        Vec2 pos(m_positionX + (float)i * SPATIAL_SCALE, m_positionY);
        float noiseValue = perlinNoiseOctaves(pos, (int)FREQUENCY, OCTAVE_COUNT, PERSISTENCE, LACUNARITY, m_seed);
        m_ps->noiseArray[i] = noiseValue;
    }
}

void LGPPerlinBackendEmotiscopeFullEffect::normalizeNoiseArray() {
    if (!m_ps) return;
    for (int i = 0; i < 80; i++) {
        m_ps->noiseArray[i] = (m_ps->noiseArray[i] + 1.0f) * 0.5f;
        m_ps->noiseArray[i] = fmaxf(0.0f, fminf(1.0f, m_ps->noiseArray[i]));
    }
}

void LGPPerlinBackendEmotiscopeFullEffect::render(plugins::EffectContext& ctx) {
    if (!m_ps) return;
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
    // Advection (REVERSED for center→edges flow, use ms-based delta like Emotiscope 2.0)
    // =========================================================================
    // Clamp delta to prevent teleport jumps after stalls (max 50ms)
    float deltaMs = ctx.deltaTimeSeconds * 1000.0f;
    if (deltaMs > 50) deltaMs = 50;
    // Reverse: subtract instead of add (makes pattern flow center→edges)
    m_positionY -= 0.001f * (float)deltaMs * (1.0f + speedNorm + m_momentum);
    
    // Update pre-computed array periodically (every 10ms like Emotiscope 2.0)
    // Use ctx.totalTimeMs for consistent timing (no millis() mixing)
    if (ctx.totalTimeMs - m_lastUpdateMs >= UPDATE_INTERVAL_MS) {
        generateNoiseArray();
        normalizeNoiseArray();
        m_lastUpdateMs = ctx.totalTimeMs;
    }
    
    // No fadeToBlackBy - we overwrite every LED each frame
    
    // Render from pre-computed array (array lookups, not recomputation)
    for (uint16_t i = 0; i < STRIP_LENGTH; i++) {
        // Centre-origin: distance from centre pair
        uint16_t dist = centerPairDistance(i);
        
        // Bounds check (dist should be 0-79)
        if (dist >= 80) dist = 79;
        
        // Array lookup (not recomputation!)
        float noiseNorm = m_ps->noiseArray[dist];
        
        // Same shaping as FastLED test (for fair comparison)
        noiseNorm = noiseNorm * noiseNorm; // Bias toward darker, stronger highlights
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


// AUTO_TUNABLES_BULK_METHODS_BEGIN:LGPPerlinBackendEmotiscopeFullEffect
uint8_t LGPPerlinBackendEmotiscopeFullEffect::getParameterCount() const {
    return static_cast<uint8_t>(sizeof(kLGPPerlinBackendEmotiscopeFullEffectParameters) / sizeof(kLGPPerlinBackendEmotiscopeFullEffectParameters[0]));
}

const plugins::EffectParameter* LGPPerlinBackendEmotiscopeFullEffect::getParameter(uint8_t index) const {
    if (index >= getParameterCount()) return nullptr;
    return &kLGPPerlinBackendEmotiscopeFullEffectParameters[index];
}

bool LGPPerlinBackendEmotiscopeFullEffect::setParameter(const char* name, float value) {
    if (!name) return false;
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_speed_scale") == 0) {
        gLGPPerlinBackendEmotiscopeFullEffectSpeedScale = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_output_gain") == 0) {
        gLGPPerlinBackendEmotiscopeFullEffectOutputGain = constrain(value, 0.25f, 2.0f);
        return true;
    }
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_centre_bias") == 0) {
        gLGPPerlinBackendEmotiscopeFullEffectCentreBias = constrain(value, 0.50f, 1.50f);
        return true;
    }
    return false;
}

float LGPPerlinBackendEmotiscopeFullEffect::getParameter(const char* name) const {
    if (!name) return 0.0f;
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_speed_scale") == 0) return gLGPPerlinBackendEmotiscopeFullEffectSpeedScale;
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_output_gain") == 0) return gLGPPerlinBackendEmotiscopeFullEffectOutputGain;
    if (strcmp(name, "lgpperlin_backend_emotiscope_full_effect_centre_bias") == 0) return gLGPPerlinBackendEmotiscopeFullEffectCentreBias;
    return 0.0f;
}
// AUTO_TUNABLES_BULK_METHODS_END:LGPPerlinBackendEmotiscopeFullEffect

void LGPPerlinBackendEmotiscopeFullEffect::cleanup() {
#ifndef NATIVE_BUILD
    if (m_ps) {
        heap_caps_free(m_ps);
        m_ps = nullptr;
    }
#endif
}

const plugins::EffectMetadata& LGPPerlinBackendEmotiscopeFullEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "Perlin Test: Emotiscope2 Full",
        "Emotiscope 2.0 Perlin full-res (TEST)",
        plugins::EffectCategory::UNCATEGORIZED,
        1
    };
    return meta;
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

