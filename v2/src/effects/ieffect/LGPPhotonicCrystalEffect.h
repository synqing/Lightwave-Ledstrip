/**
 * @file LGPPhotonicCrystalEffect.h
 * @brief LGP Photonic Crystal - Audio-reactive bandgap structure simulation
 *
 * Effect ID: 33
 * Family: ADVANCED_OPTICAL
 * Tags: CENTER_ORIGIN, AUDIO_REACTIVE
 *
 * v6: PURE DSP - No Saliency/Narrative/Style/Chord Detection
 * All "musical intelligence" APIs were outputting corrupted, unusable data.
 * This version uses ONLY direct DSP signals from Goertzel analyzer and K1.
 *
 * Direct DSP signals used:
 * - heavyBass() → Lattice breathing
 * - heavyMid() → Bandgap modulation
 * - heavyTreble() → Shimmer intensity
 * - flux() → Speed modulation + hue variation
 * - rms() → Brightness baseline
 * - isSnareHit()/isHihatHit() → Onset pulses + speed bursts
 * - beatPhase()/isOnBeat()/beatStrength() → Beat-locked brightness
 * - tempoConfidence() → Gating unreliable K1 beats
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPPhotonicCrystalEffect : public plugins::IEffect {
public:
    LGPPhotonicCrystalEffect();
    ~LGPPhotonicCrystalEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Phase accumulator (float for smooth motion)
    float m_phase;

    // v6: PURE DSP - New state variables
    float m_speedBurst = 1.0f;   // Hi-hat triggered speed burst (decays to 1.0)
    float m_beatPulse = 0.0f;    // K1 beat-locked brightness pulse

    // =========================================================================
    // TRUE EXPONENTIAL SMOOTHING (SmoothingEngine primitives)
    // Uses: alpha = 1.0f - expf(-dt / tau) - Frame-rate INDEPENDENT
    // =========================================================================

    // Structural parameters (slow, dreamier smoothing for visual stability)
    // riseTau=0.15s (fast attack), fallTau=0.40s (slow release)
    enhancement::AsymmetricFollower m_latticeFollower{8.0f, 0.15f, 0.40f};
    enhancement::AsymmetricFollower m_bandgapFollower{0.5f, 0.15f, 0.40f};

    // Brightness (faster response for beat reactivity)
    // riseTau=0.05s (very fast attack), fallTau=0.20s (medium release)
    enhancement::AsymmetricFollower m_brightnessFollower{0.8f, 0.05f, 0.20f};

    // Beat pulse uses exponential decay toward zero
    // lambda=12 means ~83ms to reach 63% of target (fast decay)
    enhancement::ExpDecay m_beatPulseDecay{0.0f, 12.0f};

    // Hue split (medium speed for smooth color transitions)
    enhancement::AsymmetricFollower m_hueSplitFollower{48.0f, 0.10f, 0.25f};

    // Phase speed smoothing (prevents lurching on narrative changes)
    // Uses spring physics for natural momentum
    enhancement::Spring m_phaseSpeedSpring;

    // v6: Treble energy smoothing for shimmer (was timbralSaliency - CORRUPTED)
    enhancement::AsymmetricFollower m_trebleFollower{0.0f, 0.08f, 0.15f};

    // Tempo breathing smoothing
    enhancement::AsymmetricFollower m_breathingFollower{0.0f, 0.12f, 0.06f};

    // =========================================================================
    // HISTORY BUFFERS for spike filtering (4-frame rolling average)
    // =========================================================================
    static constexpr uint8_t HISTORY_SIZE = 4;
    float m_latticeTargetHist[HISTORY_SIZE] = {0};
    float m_bandgapTargetHist[HISTORY_SIZE] = {0};
    float m_latticeTargetSum = 0.0f;
    float m_bandgapTargetSum = 0.0f;
    uint8_t m_histIdx = 0;

    // Hop sequence tracking for MOOD coefficient updates
    uint32_t m_lastHopSeq = 0;

    // =========================================================================
    // WAVE COLLISION PATTERN: Relative Energy Detection
    // Uses 4-hop rolling average to detect energy ABOVE baseline
    // =========================================================================
    static constexpr uint8_t ENERGY_HISTORY = 4;
    float m_energyHist[ENERGY_HISTORY] = {0};
    float m_energySum = 0.0f;
    uint8_t m_energyHistIdx = 0;

    // Raw energy values (updated per hop)
    float m_energyAvg = 0.0f;      // Rolling average of recent energy
    float m_energyDelta = 0.0f;    // Current energy ABOVE average (positive only)

    // Smoothed energy values (updated per frame, asymmetric rise/fall)
    float m_energyAvgSmooth = 0.0f;
    float m_energyDeltaSmooth = 0.0f;

    // =========================================================================
    // WAVE COLLISION PATTERN: Slew-Limited Speed
    // Prevents jitter from rapid audio changes or jog-dial adjustments
    // =========================================================================
    float m_speedScaleSmooth = 1.0f;  // Slew-limited speed multiplier

    // =========================================================================
    // ONSET DETECTION (percussion-triggered, spatial decay)
    // =========================================================================
    float m_onsetPulse = 0.0f;    // Onset pulse with exponential decay

};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
