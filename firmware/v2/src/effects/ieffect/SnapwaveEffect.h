/**
 * @file SnapwaveEffect.h
 * @brief Snapwave - Time-based oscillating visualization with chromagram-driven color
 *
 * Effect ID: TBD (will be assigned in CoreEffects.cpp)
 * Family: PARTY
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | TRAVELING
 *
 * Ported from original v1 implementation. Creates a time-based oscillating
 * visualization that combines:
 * - Time-domain sine oscillations with chromagram note contributions
 * - Hyperbolic tangent normalization for "snappy" motion
 * - Audio-driven amplitude modulation
 * - Dynamic trail system with energy-based persistence
 * - Scrolling waveform display
 *
 * Instance State:
 * - m_waveformPeakScaledLast: Smoothed waveform peak (0.0-1.0)
 * - m_chromaSmoothed[12]: Smoothed chromagram for color calculation
 * - m_lastColor: Last computed color from chromagram
 * - m_scrollBuffer[160]: Scrolling history buffer (per strip)
 * - m_lastHopSeq: Track audio hop sequence for updates
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../CoreEffects.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#include <cstring>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SnapwaveEffect : public plugins::IEffect {
public:
    SnapwaveEffect() = default;
    ~SnapwaveEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Waveform history buffer (4 frames x 128 samples) - matches Sensory Bridge
    static constexpr uint8_t WAVEFORM_HISTORY_SIZE = 4;
    static constexpr uint8_t WAVEFORM_SIZE = 128;  // CONTROLBUS_WAVEFORM_N
    int16_t m_waveformHistory[WAVEFORM_HISTORY_SIZE][WAVEFORM_SIZE] = {{0}};
    uint8_t m_historyIndex = 0;
    
    // Peak follower (asymmetric: fast attack, slow release)
    enhancement::AsymmetricFollower m_peakFollower{0.0f, 0.05f, 0.30f};
    
    // Position smoothing (asymmetric follower for smooth motion)
    enhancement::AsymmetricFollower m_positionFollower{0.0f, 0.05f, 0.30f};
    
    // Chromagram smoothing state (for color calculation)
    float m_chromaSmoothed[12] = {0.0f};
    
    // Last computed color from chromagram
    CRGB m_lastColor = CRGB::Black;
    
    // Scrolling history buffer (per strip, 160 LEDs)
    CRGB m_scrollBuffer[STRIP_LENGTH];
    
    // Track audio hop sequence for updates
    uint32_t m_lastHopSeq = 0;
    
    // Sum color smoothing state (RGB) - per-component smoothing (0.05/0.95)
    float m_sumColorLast[3] = {0.0f, 0.0f, 0.0f};
    
    // Per-sample waveform smoothing state (for full waveform rendering)
    float m_waveformLast[WAVEFORM_SIZE] = {0.0f};
    
    // Waveform peak smoothing (0.05/0.95 ratio)
    float m_waveformPeakScaledLast = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

