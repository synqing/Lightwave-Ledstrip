/**
 * @file PerlinBreathingEffect.h
 * @brief Perlin Breathing - Organic noise field with beat-synchronized breathing
 *
 * Effect ID: (auto-assigned)
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | AUDIO_REACTIVE
 *
 * Mathematical Basis:
 * - Perlin noise: inoise16(x + dist * spatialScale, y + (noiseTime >> 4))
 * - beatsin16 oscillation: modulates spatial scale and brightness envelope
 * - Combination: Creates organic "breathing" Perlin field that expands/contracts
 *
 * Visual Foundation: Time-based Perlin noise with beatsin16 modulation
 * Audio Enhancement: Audio modulates breathing depth and color saturation
 *
 * Instance State:
 * - m_noiseTime: Perlin noise time accumulator
 * - m_noiseX, m_noiseY: Perlin noise field coordinates
 * - m_smoothRms, m_smoothBass: Smoothed audio parameters
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"
#include "../CoreEffects.h"
#include "../utils/FastLEDOptim.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

/**
 * @brief Perlin noise field with beatsin16 breathing modulation
 *
 * Combines two complementary techniques:
 * - Perlin noise for organic, natural-looking patterns
 * - beatsin16 for synchronized breathing rhythm
 *
 * The beatsin16 modulates:
 * - Spatial scale: Controls how "zoomed in" the noise appears (13 BPM)
 * - Brightness envelope: Overall breathing brightness (7 BPM slow breathe)
 *
 * CENTER ORIGIN compliant: All patterns radiate from LED 79/80.
 * Dual strip rendering with +24 hue offset for LGP interference.
 */
class PerlinBreathingEffect : public plugins::IEffect {
public:
    PerlinBreathingEffect();
    ~PerlinBreathingEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Perlin noise state
    uint32_t m_noiseTime;     // Time accumulator for noise animation
    uint16_t m_noiseX;        // Noise field X coordinate
    uint16_t m_noiseY;        // Noise field Y coordinate

    // Audio state tracking (hop_seq checking for fresh data)
    uint32_t m_lastHopSeq;

    // Audio smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_rmsFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_bassFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_beatFollower{0.0f, 0.05f, 0.30f};

    // Target values (updated only on new hops)
    float m_targetRms;
    float m_targetBass;
    float m_targetBeatStrength;

    // Smoothed audio parameters (from AsymmetricFollower)
    float m_smoothRms;
    float m_smoothBass;
    float m_smoothBeatStrength;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
