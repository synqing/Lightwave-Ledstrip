/**
 * @file BreathingRippleEffect.h
 * @brief Breathing Ripple - Oscillating radius ripples that expand AND contract
 *
 * Effect ID: TBD (assigned during registration)
 * Family: WATER
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE | TRAVELING
 *
 * MATHEMATICAL BASIS:
 * Unlike standard ripples that only expand, these ripples "breathe":
 * - baseRadius: Slowly drifts outward over time (radial expansion)
 * - oscillation: beatsin16 modulates radius around baseRadius (breathing)
 * - Result: Ripples grow/shrink organically while propagating outward
 *
 * VISUAL EFFECT:
 * Concentric rings that pulse inward/outward while slowly migrating to edges.
 * Creates an organic, living feel - like bioluminescent waves or heartbeats.
 *
 * AUDIO REACTIVITY:
 * - Beat triggers spawn new ripples at center
 * - Harmonic saliency modulates breathing amplitude
 * - Rhythmic saliency adjusts breathing BPM
 *
 * Instance State:
 * - m_ripples[6]: Array of breathing ripple states
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

class BreathingRippleEffect : public plugins::IEffect {
public:
    BreathingRippleEffect();
    ~BreathingRippleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t MAX_RIPPLES = 6;

    /**
     * @brief Individual breathing ripple state
     *
     * Key innovation: radius = baseRadius + beatsin16(breathBpm, -amplitude, +amplitude)
     * This creates oscillating wavefronts that grow/shrink while drifting outward.
     */
    struct BreathingRipple {
        float baseRadius;       ///< Center of oscillation (slowly expands outward)
        uint8_t breathBpm;      ///< Individual breathing rate (15-35 BPM typical)
        uint8_t amplitude;      ///< Oscillation amplitude in LEDs (8-16 typical)
        uint16_t phaseOffset;   ///< Phase offset for visual variety
        float brightness;       ///< Current brightness (0.0-1.0, decays over time)
        float decay;            ///< Decay rate (higher = faster fade)
        uint8_t hue;            ///< Palette index for color
        bool active;            ///< Is this ripple currently rendering?
    };

    BreathingRipple m_ripples[MAX_RIPPLES];

    // Spawn control
    uint8_t m_spawnCooldown;            ///< Frames until next spawn allowed
    uint32_t m_lastSpawnTime;           ///< Timestamp of last spawn (for rate limiting)

    // Radial rendering buffer (CENTER ORIGIN pattern)
    CRGB m_radial[HALF_LENGTH];         ///< Radial history buffer (dist 0 = center)

    // Audio smoothing (for musical intelligence)
    enhancement::AsymmetricFollower m_harmonicFollower{0.0f, 0.08f, 0.25f};
    enhancement::AsymmetricFollower m_rhythmicFollower{0.0f, 0.10f, 0.20f};
    float m_harmonicSmoothed;
    float m_rhythmicSmoothed;

    // Hop sequence tracking
    uint32_t m_lastHopSeq;

    // Helper methods
    void spawnRipple(plugins::EffectContext& ctx);
    void updateRipples(plugins::EffectContext& ctx, float dt);
    void renderRipples(plugins::EffectContext& ctx);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
