/**
 * @file LGPFresnelCausticSweepEffect.h
 * @brief LGP Fresnel Caustic Focus Sweep -- scanning lens caustic with Fresnel sidelobes
 *
 * Effect ID: 0x1B04 (EID_LGP_FRESNEL_CAUSTIC_SWEEP)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | OPTICS | AUDIO_REACTIVE
 *
 * Physics: A moving caustic/focus point that behaves like a scanning lens inside
 * acrylic.  The focus position sweeps sinusoidally in distance-from-centre space
 * (0..80).  At the focus there is a narrow Gaussian-like core (~3 LEDs wide),
 * and beyond that an oscillatory Fresnel ring structure whose phase drifts slowly
 * so the sidelobes "breathe".  A centre-weighted envelope keeps the brightest
 * action near the middle of the strip.
 *
 * State budget: ~28 bytes (no PSRAM, no heap allocation in render).
 *
 * Audio: circularChromaHueSmoothed for hue.  RMS modulates focus sweep speed.
 * Beat triggers a specular flash at the current focus position.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFresnelCausticSweepEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_FRESNEL_CAUSTIC_SWEEP;

    LGPFresnelCausticSweepEffect();
    ~LGPFresnelCausticSweepEffect() override = default;

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
    // Focus position sweep phase (radians, wraps at 2*PI) -- ~4 bytes
    float m_focusPhase;

    // Sidelobe ring breathing phase (radians, slower) -- ~4 bytes
    float m_ringPhase;

    // Circular chroma angle (persisted across frames for EMA smoothing) -- ~4 bytes
    float m_chromaAngle;

    // Beat-triggered specular flash intensity (0..1, decays per frame) -- ~4 bytes
    float m_beatFlash;

    // Fallback phase accumulator (no-audio mode) -- ~4 bytes
    float m_fallbackPhase;

#if FEATURE_AUDIO_SYNC
    // Hop sequence tracking -- ~4 bytes
    uint32_t m_lastHopSeq;

    // Smoothed chroma bins (12 floats = 48 bytes, each <64B individually) -- ~48 bytes
    float m_chromaSmoothed[12];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
