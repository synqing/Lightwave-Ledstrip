// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file SnapwaveLinearEffect.h
 * @brief Scrolling waveform with time-based oscillation
 *
 * ORIGINAL SNAPWAVE ALGORITHM - Restored from SensoryBridge light_mode_snapwave()
 *
 * Visual behavior:
 * 1. Dot bounces based on time-based oscillation + chromagram
 * 2. HISTORY BUFFER tracks previous dot positions
 * 3. Trail renders at previous positions with fading brightness
 * 4. Mirrored for CENTER ORIGIN compliance
 *
 * The characteristic "snap" comes from tanh() normalization.
 *
 * Effect ID: 98
 * Family: PARTY (audio-reactive)
 *
 * @author LightwaveOS Team
 * @version 4.1.0 - Added explicit history buffer for trails
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

class SnapwaveLinearEffect : public plugins::IEffect {
public:
    SnapwaveLinearEffect() = default;
    ~SnapwaveLinearEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // ============================================================================
    // Algorithm Constants
    // ============================================================================

    // Peak smoothing: 2% new, 98% old (very aggressive)
    static constexpr float PEAK_ATTACK = 0.02f;
    static constexpr float PEAK_DECAY = 0.98f;

    // History buffer for trails
    static constexpr uint16_t HISTORY_SIZE = 40;  // 40 frames of history = ~333ms at 120fps

    // Trail fade: each history step gets dimmer
    // 0.85^40 ≈ 0.001, so oldest entries are nearly black
    static constexpr float TRAIL_FADE_FACTOR = 0.85f;

    // Oscillation parameters
    static constexpr float BASE_FREQUENCY = 0.001f;     // Base frequency for sin()
    static constexpr float PHASE_SPREAD = 0.5f;         // Phase offset per chromagram note
    static constexpr float TANH_SCALE = 2.0f;           // tanh() input multiplier
    static constexpr float NOTE_THRESHOLD = 0.1f;       // Min chromagram value to contribute
    static constexpr float AMPLITUDE_MIX = 0.7f;        // Oscillation × peak mix factor
    static constexpr float ENERGY_GATE_THRESHOLD = 0.05f; // RMS below this = silence (no movement)

    // Color thresholds
    static constexpr float COLOR_THRESHOLD = 0.05f;     // Min brightness for color contribution

    // ============================================================================
    // State Variables
    // ============================================================================

    float m_peakSmoothed = 0.0f;

    // HISTORY BUFFER - stores previous dot distances from center
    // distance 0 = center, distance 79 = edge
    uint8_t m_distanceHistory[HISTORY_SIZE];
    CRGB m_colorHistory[HISTORY_SIZE];
    uint8_t m_historyIndex = 0;

    // ============================================================================
    // Helper Methods
    // ============================================================================

    float computeOscillation(const plugins::EffectContext& ctx);
    CRGB computeChromaColor(const plugins::EffectContext& ctx);
    void pushHistory(uint8_t distance, CRGB color);
    void renderHistoryToLeds(plugins::EffectContext& ctx);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
