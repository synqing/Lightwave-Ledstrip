/**
 * @file RippleEnhancedEffect.h
 * @brief Ripple Enhanced - Enhanced version with improved 64-bin thresholds, snare triggers, treble shimmer
 *
 * Effect ID: 95
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 *
 * Enhancements over RippleEffect (ID 8):
 * - Improved 64-bin kick threshold (0.4f instead of 0.5f)
 * - Enhanced treble shimmer threshold (0.08f instead of 0.1f)
 * - Guaranteed snare hit ripple spawn
 * - Removed chord reactivity (uses heavy_chroma only)
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

class RippleEnhancedEffect : public plugins::IEffect {
public:
    RippleEnhancedEffect();
    ~RippleEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static struct ripples[5])
    struct Ripple {
        float radius;
        float speed;
        uint8_t hue;
        uint8_t intensity;
        bool active;
    };
    static constexpr uint8_t MAX_RIPPLES = 5;
    Ripple m_ripples[MAX_RIPPLES];
    uint32_t m_lastHopSeq = 0;
    uint8_t m_spawnCooldown = 0;
    float m_lastChromaEnergy = 0.0f;
    static constexpr uint8_t CHROMA_HISTORY = 4;
    float m_chromaEnergyHist[CHROMA_HISTORY] = {0.0f};
    float m_chromaEnergySum = 0.0f;
    uint8_t m_chromaHistIdx = 0;

    // Radial LED history buffer (centre-out)
    CRGB m_radial[HALF_LENGTH];
    CRGB m_radialAux[HALF_LENGTH];

    // Audio smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    enhancement::AsymmetricFollower m_kickFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_trebleFollower{0.0f, 0.05f, 0.30f};
    
    // Chromagram smoothing state
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // 64-bin spectrum tracking for enhanced audio response
    float m_kickPulse = 0.0f;       ///< Sub-bass energy (bins 0-5) for kick-triggered ripples
    float m_trebleShimmer = 0.0f;   ///< Treble energy (bins 48-63) for wavefront sparkle
    float m_targetKick = 0.0f;
    float m_targetTreble = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
