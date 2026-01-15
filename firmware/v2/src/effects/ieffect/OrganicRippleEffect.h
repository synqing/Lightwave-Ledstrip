/**
 * @file OrganicRippleEffect.h
 * @brief Organic Ripple - Audio-reactive ripples with Perlin noise modulation
 *
 * Effect ID: TBD (assigned during registration)
 * Family: WATER
 * Tags: CENTER_ORIGIN | TRAVELING | ORGANIC
 *
 * Mathematical Basis:
 * - Radial Expansion: r(t) = r(t-1) + v * dt (kinematics)
 * - Perlin noise: Modulates velocity and brightness per ripple
 * - Combination: Ripples expand at organic, varying speeds with noise-based texturing
 *
 * Instance State:
 * - m_ripples[8]: Array of active ripple states with Perlin seeds
 * - m_noiseTime: Global noise time coordinate
 * - m_spawnCooldown: Prevents overlapping spawns
 * - Audio smoothing followers for reactive spawning
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class OrganicRippleEffect : public plugins::IEffect {
public:
    OrganicRippleEffect();
    ~OrganicRippleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Ripple state with Perlin seed
    struct Ripple {
        float radius;           ///< Current expansion radius (0-80)
        float baseVelocity;     ///< Base expansion speed (LEDs/sec)
        float brightness;       ///< Current brightness [0-1]
        float decay;            ///< Brightness decay rate
        uint8_t hue;            ///< Palette index offset
        uint16_t noiseOffset;   ///< Unique noise seed per ripple
        bool active;            ///< Is this ripple currently rendering?
    };

    static constexpr uint8_t MAX_RIPPLES = 8;
    Ripple m_ripples[MAX_RIPPLES];

    // Perlin noise state
    uint32_t m_noiseTime = 0;

    // Spawn control
    uint8_t m_spawnCooldown = 0;
    uint32_t m_lastHopSeq = 0;

    // Radial history buffer (centre-out rendering)
    CRGB m_radial[HALF_LENGTH];

    // Audio smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_bassFollower{0.0f, 0.03f, 0.25f};
    enhancement::AsymmetricFollower m_fluxFollower{0.0f, 0.02f, 0.20f};

    // Audio targets (updated on hop)
    float m_targetBass = 0.0f;
    float m_targetFlux = 0.0f;
    float m_smoothBass = 0.0f;
    float m_smoothFlux = 0.0f;

    // Helper methods
    void spawnRipple(plugins::EffectContext& ctx, float intensity);
    void renderRipple(plugins::EffectContext& ctx, const Ripple& ripple);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
