/**
 * @file StyleAdaptiveEffect.h
 * @brief Effect that adapts behavior based on detected music style
 *
 * Responds differently to EDM (rhythmic), jazz (harmonic), ambient (texture),
 * orchestral (dynamic), and vocal pop (melodic) music styles.
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

class StyleAdaptiveEffect : public plugins::IEffect {
public:
    StyleAdaptiveEffect() = default;
    ~StyleAdaptiveEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Visual Foundation: Radial wave pattern phase (time-based)
    float m_phase = 0.0f;              // Main phase for radial waves
    
    // Audio Enhancement: Style-specific modulation
    float m_rhythmicPulse = 0.0f;      // Fast pulses for EDM/hip-hop
    float m_harmonicDrift = 0.0f;      // Slow color drift for jazz/classical
    float m_melodicShimmer = 0.0f;     // Treble shimmer for vocal pop
    float m_textureFlow = 0.0f;        // Ambient texture for ambient/drone
    float m_dynamicBreath = 0.0f;      // RMS breathing for orchestral
    
    // Style transition smoothing
    uint8_t m_currentStyle = 0;       // Current style enum value
    float m_styleConfidence = 0.0f;    // Style detection confidence
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

