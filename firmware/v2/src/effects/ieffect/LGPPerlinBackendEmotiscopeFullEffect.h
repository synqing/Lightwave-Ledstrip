/**
 * @file LGPPerlinBackendEmotiscopeFullEffect.h
 * @brief Perlin Backend Test B: Emotiscope 2.0 seedable Perlin (full-res per-frame)
 * 
 * Effect ID: 86 (TEST)
 * Family: EXPERIMENTAL
 * Tags: CENTER_ORIGIN, TEST
 * 
 * Test effect for comparing noise backends.
 * Uses Emotiscope 2.0's seedable Perlin noise (hash-based gradients, octaves).
 * Full resolution: samples every LED per frame.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "PerlinNoiseTypes.h"
#include <FastLED.h>
#include <cmath>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinBackendEmotiscopeFullEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL;

    LGPPerlinBackendEmotiscopeFullEffect();
    ~LGPPerlinBackendEmotiscopeFullEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;


    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;
private:
    // Emotiscope 2.0 Perlin functions
    static unsigned int hash(unsigned int x, unsigned int seed);
    static unsigned int hashVec2(UVec2 x, unsigned int seed);
    static Vec2 gradientDirection(unsigned int hash);
    static float interpolatePerlin(float v1, float v2, float v3, float v4, Vec2 t);
    static Vec2 fade(Vec2 t);
    static float dot(Vec2 a, Vec2 b);
    static float perlinNoise(Vec2 position, unsigned int seed);
    static float perlinNoiseOctaves(Vec2 position, int frequency, int octaveCount, float persistence, float lacunarity, unsigned int seed);
    
    // Seed for "non-reproducible" feel
    unsigned int m_seed;
    
    // Advection position (drifts through noise field)
    float m_positionX;
    float m_positionY;
    
    // Audio-driven momentum (Emotiscope-style)
    float m_momentum;

#ifndef NATIVE_BUILD
    struct PerlinBackendPsram {
        float noiseArray[80];
    };
    PerlinBackendPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // Update timing (like Emotiscope 2.0's 10ms interval)
    uint32_t m_lastUpdateMs;
    static constexpr uint32_t UPDATE_INTERVAL_MS = 10;
    
    // Pre-compute function (matches Emotiscope 2.0's generate_perlin_noise)
    void generateNoiseArray();
    
    // Normalize array: convert [-1,1] to [0,1] (like Emotiscope 2.0)
    void normalizeNoiseArray();
    
    // Perlin parameters (matching Emotiscope 2.0 defaults)
    static constexpr float FREQUENCY = 2.0f;
    static constexpr float PERSISTENCE = 0.5f;
    static constexpr float LACUNARITY = 2.0f;
    static constexpr int OCTAVE_COUNT = 2;
    
    // Spatial scale (matches Emotiscope 2.0's num_leds_float_lookup mapping)
    static constexpr float SPATIAL_SCALE = 0.025f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

