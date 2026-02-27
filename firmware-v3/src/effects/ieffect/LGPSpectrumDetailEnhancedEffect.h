// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LGPSpectrumDetailEnhancedEffect.h
 * @brief Enhanced 64-bin FFT spectrum visualization matching Sensory Bridge pattern
 *
 * Enhanced version of LGP Spectrum Detail implementing Sensory Bridge spectrogram smoothing:
 * - Hop-based target updates (only on new audio hops)
 * - 4-frame history buffer for spike filtering (BEFORE smoothing)
 * - Symmetric 0.75 linear interpolation (Sensory Bridge get_smooth_spectrogram pattern)
 * - Center-to-edges motion (reversed mapping: treble at center, bass at edges)
 * - High amplitude gain for full LED range utilization
 *
 * Uses the full 64-bin Goertzel spectrum (110-4186 Hz) for detailed frequency visualization.
 * Logarithmic mapping places low frequencies at center, high frequencies at edges.
 * Center-origin pattern: bass at center (LEDs 79/80), treble at edges.
 *
 * Effect ID: 94
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | SPECTRUM
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPSpectrumDetailEnhancedEffect : public plugins::IEffect {
public:
    LGPSpectrumDetailEnhancedEffect() = default;
    ~LGPSpectrumDetailEnhancedEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Sensory Bridge pattern: History buffer BEFORE smoothing (4-frame rolling average)
    static constexpr uint8_t HISTORY_SIZE = 4;
    float m_binHistory[HISTORY_SIZE][64] = {0.0f};
    uint8_t m_historyIdx = 0;
    
    // Sensory Bridge pattern: Simple symmetric smoothing (not AsymmetricFollower)
    float m_binSmoothing[64] = {0.0f};
    
    // Hop sequence tracking (update targets only on new hops)
    uint32_t m_lastHopSeq = 0;

    // =========================================================================
    // FRAME-RATE INDEPENDENT TIME CONSTANTS (in seconds)
    // Using tau-based exponential decay: alpha = 1 - exp(-dt/tau)
    // This ensures identical visual behavior at any frame rate (60, 120, 144 FPS)
    // =========================================================================
    static constexpr float FRAME_DT = 1.0f / 120.0f;     // 8.33ms at 120 FPS
    static constexpr float SMOOTHING_TAU = 0.050f;       // 50ms spectral smoothing
    static constexpr float ATTACK_TAU = 0.020f;          // 20ms momentum attack (fast)
    static constexpr float RELEASE_TAU = 0.300f;         // 300ms momentum release (slow)
    static constexpr float DECAY_TAU = 2.000f;           // 2s decay half-life for trails
    static constexpr float SHIMMER_SMOOTH_TAU = 0.100f;  // 100ms shimmer amplitude smoothing

    // Pre-computed alpha values for 120 FPS (computed once, used every frame)
    // alpha = 1 - exp(-dt/tau) for each time constant
    float m_smoothingAlpha = 0.0f;
    float m_attackAlpha = 0.0f;
    float m_releaseAlpha = 0.0f;
    float m_decayAlpha = 0.0f;
    float m_shimmerAlpha = 0.0f;

    // Smoothed shimmer amplitude per bin (prevents amplitude jumps)
    float m_shimmerAmp[64] = {0.0f};
    
    // Reverse trail radial buffer (like Ripple Enhanced) for backbeat-triggered trails
    CRGB m_radialTrail[HALF_LENGTH];
    
    // Beat tracking for reverse trail trigger
    uint8_t m_lastBeatInBar = 255;  // Track to detect beat changes
    float m_lastBarPhase = 0.0f;    // Track bar phase for fallback detection

    // Motion physics v2: Direct Energy Coupling (F1 Driver Response)
    // Position responds DIRECTLY to energy for instant attack, controlled release
    float m_binDistance[64];        // Current distance from center (dynamic)
    float m_binMomentum[64];        // Asymmetric follower: fast attack (0.8), slow release (0.03)

    // Strip 2 gap detection uses local arrays in render() - no persistent state needed
    // Gap detection is recomputed each frame based on current bin positions

    // Frequency-to-color mapping helper (uses palette system)
    CRGB frequencyToColor(uint8_t bin, const plugins::EffectContext& ctx) const;
    
    // Logarithmic mapping: map bin index to LED distance from center
    uint16_t binToLedDistance(uint8_t bin) const;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
