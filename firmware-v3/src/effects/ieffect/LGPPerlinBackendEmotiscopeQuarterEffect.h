/**
 * @file LGPPerlinBackendEmotiscopeQuarterEffect.h
 * @brief Perlin Backend Test C: Emotiscope 2.0 seedable Perlin (quarter-res + interpolation)
 * 
 * Effect ID: 87 (TEST)
 * Family: EXPERIMENTAL
 * Tags: CENTER_ORIGIN, TEST
 * 
 * Test effect for comparing noise backends.
 * Uses Emotiscope 2.0's seedable Perlin noise.
 * Quarter resolution: samples 20 points, updates periodically, interpolates for LEDs.
 * Lower CPU cost than full-res variant.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "PerlinNoiseTypes.h"
#include <FastLED.h>
#include <cmath>
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPerlinBackendEmotiscopeQuarterEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER;

    LGPPerlinBackendEmotiscopeQuarterEffect();
    ~LGPPerlinBackendEmotiscopeQuarterEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Emotiscope 2.0 Perlin functions (same as full-res)
    static unsigned int hash(unsigned int x, unsigned int seed);
    static unsigned int hashVec2(UVec2 x, unsigned int seed);
    static Vec2 gradientDirection(unsigned int hash);
    static float interpolatePerlin(float v1, float v2, float v3, float v4, Vec2 t);
    static Vec2 fade(Vec2 t);
    static float dot(Vec2 a, Vec2 b);
    static float perlinNoise(Vec2 position, unsigned int seed);
    static float perlinNoiseOctaves(Vec2 position, int frequency, int octaveCount, float persistence, float lacunarity, unsigned int seed);
    
    // Quarter-resolution buffer (80 distances -> 20 samples)
    static constexpr uint8_t QUARTER_RES = 20;
    float m_noiseBuffer[QUARTER_RES];
    
    // Seed for "non-reproducible" feel
    unsigned int m_seed;
    
    // Advection position
    float m_positionX;
    float m_positionY;
    
    // Update timing (refresh buffer every 10ms like Emotiscope 2.0)
    uint32_t m_lastUpdateMs;
    static constexpr uint32_t UPDATE_INTERVAL_MS = 10;
    
    // Perlin parameters
    static constexpr float FREQUENCY = 2.0f;
    static constexpr float PERSISTENCE = 0.5f;
    static constexpr float LACUNARITY = 2.0f;
    static constexpr int OCTAVE_COUNT = 2;
    
    // Spatial scale
    static constexpr float SPATIAL_SCALE = 0.025f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

