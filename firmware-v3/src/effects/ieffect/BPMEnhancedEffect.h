/**
 * @file BPMEnhancedEffect.h
 * @brief BPM Enhanced - Enhanced version with 64-bin spectrum, heavy_chroma, beatPhase sync, snare triggers
 *
 * Effect ID: 88
 * Family: GEOMETRIC
 * Tags: CENTER_ORIGIN | TRAVELING | AUDIO_REACTIVE
 *
 * Enhancements over BPMEffect (ID 6):
 * - Uses heavy_chroma for color (not raw chroma)
 * - 64-bin sub-bass detection (bins 0-5) for kick-triggered ring intensity
 * - Uses beatPhase() for better beat synchronization
 * - Adds isSnareHit() trigger for additional ring spawns
 * - Improved Spring smoothing for speed modulation
 * - Better fallback: Slow time-based animation when audio unavailable
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../effects/enhancement/SmoothingEngine.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class BPMEnhancedEffect : public plugins::IEffect {
public:
    BPMEnhancedEffect();
    ~BPMEnhancedEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Wave phase accumulator
    float m_phase = 0.0f;

    // Spring for smooth speed modulation
    enhancement::Spring m_speedSpring;
    
    // Audio smoothing (AsymmetricFollower for natural attack/release)
    enhancement::AsymmetricFollower m_heavyEnergyFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_beatStrengthFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_tempoConfFollower{0.0f, 0.05f, 0.30f};
    enhancement::AsymmetricFollower m_subBassFollower{0.0f, 0.05f, 0.30f};
    
    // Chromagram smoothing for color
    enhancement::AsymmetricFollower m_chromaFollowers[12];
    float m_chromaSmoothed[12] = {0.0f};
    float m_chromaTargets[12] = {0.0f};
    
    // Hop sequence tracking
    uint32_t m_lastHopSeq = 0;
    float m_targetHeavyEnergy = 0.0f;
    float m_targetBeatStrength = 0.0f;
    float m_targetTempoConf = 0.0f;
    float m_targetSubBass = 0.0f;

    // Ring buffer for beat-triggered expanding rings
    static constexpr int MAX_RINGS = 4;
    float m_ringRadius[MAX_RINGS] = {0};
    float m_ringIntensity[MAX_RINGS] = {0};
    uint8_t m_nextRing = 0;

    // Tempo lock hysteresis
    bool m_tempoLocked = false;
    
    // Fallback animation (when audio unavailable)
    float m_fallbackPhase = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
