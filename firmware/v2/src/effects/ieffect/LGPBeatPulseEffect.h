/**
 * @file LGPBeatPulseEffect.h
 * @brief Beat-synchronized radial pulse from CENTER ORIGIN
 *
 * Pulse expands outward on each beat, intensity modulated by bass.
 * Secondary pulse on snare hits, shimmer overlay on hi-hat.
 * Falls back to 120 BPM when audio unavailable.
 *
 * Effect ID: 69 (audio demo)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | BEAT | SNARE | HIHAT
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPBeatPulseEffect : public plugins::IEffect {
public:
    LGPBeatPulseEffect() = default;
    ~LGPBeatPulseEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Primary kick/beat pulse
    float m_pulsePosition = 0.0f;   // Current pulse distance from center (0-1)
    float m_pulseIntensity = 0.0f;  // Decaying pulse brightness
    float m_fallbackPhase = 0.0f;   // For non-audio fallback
    float m_lastBeatPhase = 0.0f;   // For detecting beat crossings

    // Snare detection and secondary pulse
    float m_lastMidEnergy = 0.0f;   // Previous frame mid energy for spike detection
    float m_snarePulsePos = 0.0f;   // Secondary pulse position (0-1)
    float m_snarePulseInt = 0.0f;   // Secondary pulse intensity

    // Hi-hat detection and shimmer overlay
    float m_lastTrebleEnergy = 0.0f; // Previous frame treble energy
    float m_hihatShimmer = 0.0f;     // Hi-hat shimmer intensity (decays fast)

    // ES/LWLS-agnostic onset proxy (flux) and chroma hue anchoring
    float m_lastFastFlux = 0.0f;
    uint32_t m_lastHopSeq = 0;
    uint8_t m_dominantChromaBin = 0;
    float m_dominantChromaBinSmooth = 0.0f;

    // Bands-dead guard: when bands stay near zero for N frames, don't trust band-based snare/hihat
    uint16_t m_bandsLowFrames = 0;
    static constexpr float BANDS_LOW_THRESHOLD = 0.02f;
    static constexpr uint16_t BANDS_LOW_FRAMES_MAX = 60;  // ~0.5 s at 120 FPS

    // Smoothed context so audio mapping doesn't cause flashing or cut-off display
    float m_smoothBrightness = 0.8f;   // 0-1, from ctx.brightness/255
    float m_smoothStrength = 0.3f;     // beatStrength
    float m_smoothBeatPhase = 0.0f;     // beatPhase for stable background
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
