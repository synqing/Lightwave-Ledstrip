/**
 * @file LGPFresnelCausticReactiveEffect.h
 * @brief LGP Fresnel Caustic Reactive -- "Chromatic Resonance" multi-harmonic oscillator bank
 *
 * Extension of 0x1B04 (Fresnel Caustic Sweep). Same Fresnel ring geometry and
 * dual-strip parallax, but focus position is the WEIGHTED SUM of multiple
 * sine oscillators at incommensurate frequencies, whose amplitudes are
 * modulated by audio energy bands.
 *
 * Motion model (superposition of sines):
 *   focus(t) = SUM_k [ A_k(t) * sin(omega_k * t + phi_k) ]
 *
 * Each oscillator runs at a fixed frequency. Audio modulates the amplitude
 * envelope A_k(t) of each oscillator:
 *   - Bass energy weights slower oscillators (organic sway)
 *   - Treble/spectral flux weights faster oscillators (shimmer)
 *   - Beat detection triggers momentary amplitude bursts on fast oscillators
 *
 * Key insight: the sum of sines is ALWAYS smooth, so focus motion never
 * jitters regardless of audio noise. The incommensurate frequencies produce
 * complex, non-repeating Lissajous-like motion that stays musically coupled.
 *
 * Additional modulation:
 *   - Overall amplitude: audio energy scales total sweep range (loud = wide)
 *   - Ring phase speed: couples to audio energy (loud = faster ring breathing)
 *   - Fade: couples to energy squared (loud = crisp, quiet = lingering)
 *   - Colour: chroma-driven with spatial gradient
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPFresnelCausticReactiveEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_FRESNEL_CAUSTIC_REACTIVE;

    LGPFresnelCausticReactiveEffect();
    ~LGPFresnelCausticReactiveEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // Number of harmonic oscillators in the bank
    static constexpr uint8_t NUM_OSC = 4;

    // Oscillator bank: phases accumulate continuously, amplitudes are modulated
    float m_oscPhase[NUM_OSC];        // Phase of each oscillator (radians)
    float m_oscAmplitude[NUM_OSC];    // Smoothed amplitude envelope for each

    // Audio energy followers
    float m_energySmooth;     // Slow EMA of overall energy
    float m_energyPeak;       // Fast peak follower (captures transients)
    float m_bassSmooth;       // Smoothed bass energy
    float m_trebleSmooth;     // Smoothed treble/high energy

    // Beat burst amplitude (decaying pulse on fast oscillators)
    float m_beatBurst;

    // Ring sidelobe breathing phase
    float m_ringPhase;

    // Chroma-driven colour
    float m_chromaAngle;

    // Beat flash (brightness burst at focus)
    float m_beatFlash;

    // Fallback phase for no-audio mode
    float m_fallbackPhase;

#if FEATURE_AUDIO_SYNC
    uint32_t m_lastHopSeq;
    float m_chromaSmoothed[12];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
