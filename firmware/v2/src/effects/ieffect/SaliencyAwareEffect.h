/**
 * @file SaliencyAwareEffect.h
 * @brief Effect that responds to Musical Intelligence System saliency metrics
 *
 * Demonstrates the full MIS feature set by adapting visual behavior based on
 * dominant saliency type (harmonic, rhythmic, timbral, dynamic).
 *
 * Effect ID: TBD (will be assigned in CoreEffects.cpp)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SaliencyAwareEffect : public plugins::IEffect {
public:
    SaliencyAwareEffect() = default;
    ~SaliencyAwareEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Visual Foundation: Multi-layer interference pattern phases
    float m_phase1 = 0.0f;              // Layer 1 phase (time-based)
    float m_phase2 = 0.0f;              // Layer 2 phase (time-based)
    float m_phase3 = 0.0f;              // Layer 3 phase (time-based)
    
    // Audio Enhancement: Saliency-driven modulation
    float m_rhythmicPulse = 0.0f;      // Pulse intensity for rhythmic saliency
    float m_timbralTexture = 0.0f;     // Texture intensity for timbral saliency
    float m_dynamicEnergy = 0.0f;      // Energy level for dynamic saliency
    
    // Smoothing for mode transitions
    float m_modeTransition = 0.0f;     // 0.0 = harmonic, 1.0 = rhythmic, etc.
    
    // Previous saliency values for change detection
    float m_prevHarmonic = 0.0f;
    float m_prevRhythmic = 0.0f;
    float m_prevTimbral = 0.0f;
    float m_prevDynamic = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

