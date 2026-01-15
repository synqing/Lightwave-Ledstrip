/**
 * @file SpectrumCenterEffect.h
 * @brief CENTER ORIGIN spectrum analyzer using all 64 FFT bins
 *
 * Full 64-bin spectrum visualization with:
 * - Bass (low frequencies) at center (LEDs 79/80), treble at edges
 * - Sensory Bridge asymmetric smoothing (50ms attack, 300ms release)
 * - Peak hold indicators with decay
 * - Beat pulse overlay at center
 * - Dual strip rendering with +90 hue offset for Strip 2
 *
 * Effect ID: TBD (assigned in effect registry)
 * Family: AUDIO_REACTIVE
 * Tags: CENTER_ORIGIN | AUDIO_SYNC | SPECTRUM | 64BIN
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../enhancement/SmoothingEngine.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

class SpectrumCenterEffect : public plugins::IEffect {
public:
    SpectrumCenterEffect() = default;
    ~SpectrumCenterEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // =========================================================================
    // Constants
    // =========================================================================
    static constexpr uint8_t NUM_BINS = 64;
    static constexpr float RISE_TAU = 0.05f;   // 50ms attack (Sensory Bridge spec)
    static constexpr float FALL_TAU = 0.30f;   // 300ms release (Sensory Bridge spec)

    // Peak hold parameters
    static constexpr float PEAK_HOLD_TIME = 0.15f;   // Hold peak for 150ms
    static constexpr float PEAK_DECAY_RATE = 3.0f;   // Decay rate after hold expires

    // Beat pulse parameters
    static constexpr float BEAT_PULSE_ATTACK = 0.02f;   // 20ms rise
    static constexpr float BEAT_PULSE_DECAY = 0.25f;    // 250ms fall
    static constexpr uint16_t BEAT_PULSE_RADIUS = 15;   // LEDs affected by beat pulse

    // =========================================================================
    // Smoothing State
    // =========================================================================

    // Per-bin asymmetric followers for natural attack/release
    enhancement::AsymmetricFollower m_binFollowers[NUM_BINS];
    float m_smoothedBins[NUM_BINS] = {0};
    float m_targetBins[NUM_BINS] = {0};

    // Peak hold state per LED position (80 positions = center to edge)
    float m_peakValues[80] = {0};       // Peak brightness values
    float m_peakHoldTimers[80] = {0};   // Time remaining in hold phase

    // Beat pulse state
    float m_beatPulseIntensity = 0.0f;  // Current beat pulse brightness (0-1)
    float m_lastBeatPhase = 0.0f;       // Previous frame's beat phase for edge detection

    // Hop sequence tracking for efficient updates
    uint32_t m_lastHopSeq = 0;

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Map LED distance from center to FFT bin index
     * @param dist Distance from center (0 = center, 79 = edge)
     * @return Bin index (0-63)
     *
     * Uses perceptual (quasi-logarithmic) mapping:
     * - Distance 0-10: bins 0-7 (sub-bass, kick fundamentals)
     * - Distance 11-25: bins 8-23 (bass, low-mids)
     * - Distance 26-50: bins 24-47 (mids, presence)
     * - Distance 51-79: bins 48-63 (treble, air)
     */
    uint8_t distanceToBin(uint16_t dist) const;

    /**
     * @brief Get smoothed bin value with adjacent bin averaging
     * @param binIndex Primary bin index (0-63)
     * @return Averaged value from binIndex-1, binIndex, binIndex+1
     *
     * Averaging reduces single-bin noise for smoother display.
     */
    float getAveragedBinValue(uint8_t binIndex) const;

    /**
     * @brief Map bin index to palette hue
     * @param binIndex Bin index (0-63)
     * @param baseHue Base hue from ctx.gHue
     * @return Hue value (0-255)
     *
     * Low frequencies = warm colors (near baseHue)
     * High frequencies = cool colors (baseHue + spread)
     */
    uint8_t binToHue(uint8_t binIndex, uint8_t baseHue) const;

    /**
     * @brief Update peak hold state for a position
     * @param posIndex Position index (0-79)
     * @param newValue Current bin value
     * @param dt Delta time in seconds
     */
    void updatePeakHold(uint8_t posIndex, float newValue, float dt);

    /**
     * @brief Update beat pulse state
     * @param ctx Effect context with audio data
     * @param dt Delta time in seconds
     */
    void updateBeatPulse(const plugins::EffectContext& ctx, float dt);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
