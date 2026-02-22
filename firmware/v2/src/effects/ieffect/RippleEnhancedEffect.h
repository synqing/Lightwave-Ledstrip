/**
 * @file RippleEnhancedEffect.h
 * @brief Ripple Enhanced - improved thresholds, snare triggers, treble shimmer
 *
 * Effect ID: 97
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | TRAVELING
 *
 * Enhancements over RippleEffect (ID 8):
 * - Improved 64-bin kick threshold (0.4f instead of 0.5f)
 * - Enhanced treble shimmer threshold (0.08f instead of 0.1f)
 * - Guaranteed snare hit ripple spawn
 * - Beat/downbeat edge spawn (latched, not hop-gated)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"
#include <FastLED.h>

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../config/effect_ids.h"
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class RippleEnhancedEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_RIPPLE_ENHANCED;

    RippleEnhancedEffect();
    ~RippleEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
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

    // Beat tracking (edge-latched, not hop-gated)
    bool m_lastBeatState = false;
    bool m_lastDownbeatState = false;

#ifndef NATIVE_BUILD
    struct RippleEnhancedPsram {
        CRGB radial[HALF_LENGTH];
        CRGB radialAux[HALF_LENGTH];
        enhancement::AsymmetricFollower chromaFollowers[12];
        float chromaSmoothed[12];
        float chromaTargets[12];
    };
    RippleEnhancedPsram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif

    // Circular chroma EMA state (radians)
    float m_chromaAngle = 0.0f;

    enhancement::AsymmetricFollower m_kickFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_trebleFollower{0.0f, 0.05f, 0.30f};

    // 64-bin spectrum tracking for enhanced audio response
    float m_kickPulse = 0.0f;
    float m_trebleShimmer = 0.0f;
    float m_targetKick = 0.0f;
    float m_targetTreble = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
