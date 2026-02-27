// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

/**
 * TrinityTestEffect - Diagnostic effect for validating PRISM Trinity data flow
 *
 * This effect visualizes each Trinity data stream with distinct visual feedback:
 *
 * LED Layout (CENTER ORIGIN, 160 LEDs per strip, 320 total):
 *
 *   Strip 1: [0]<--[79] | [80]-->[159]
 *   Strip 2: [160]<--[239] | [240]-->[319]
 *
 * Visualization Zones (from center outward):
 *   - BEAT PHASE RING: LEDs 0-19 from center - sweeping indicator (white)
 *   - ENERGY ZONE: LEDs 20-39 from center - RMS level (yellow/orange)
 *   - BASS ZONE: LEDs 40-54 from center - bass_weight (red)
 *   - TREBLE ZONE: LEDs 55-69 from center - brightness (blue/cyan)
 *   - OUTER RING: LEDs 70-79 from center - beat flash (purple)
 *
 * Indicators:
 *   - BEAT TICK: All LEDs flash white momentarily on isOnBeat()
 *   - NO DATA: Solid dim red when ctx.audio.available is false
 *   - STALENESS: Pulsing amber when Trinity proxy times out
 *
 * Use this effect to verify:
 *   1. trinity.beat messages are received (beat phase sweep + flash)
 *   2. trinity.macro messages are received (zone levels respond)
 *   3. Trinity sync is active (any animation vs solid red)
 */
class TrinityTestEffect : public plugins::IEffect {
public:
    TrinityTestEffect() = default;
    ~TrinityTestEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // State tracking
    float m_beatFlashDecay = 0.0f;      // Decays after beat tick
    float m_lastBeatPhase = 0.0f;       // For detecting phase wraps
    uint32_t m_lastDataTime = 0;        // Track staleness
    uint32_t m_frameCount = 0;          // For fallback animation

    // Smoothed values for stable visualization
    float m_smoothEnergy = 0.0f;
    float m_smoothBass = 0.0f;
    float m_smoothTreble = 0.0f;
    float m_smoothPerc = 0.0f;
    float m_smoothVocal = 0.0f;

    // Zone boundaries (distance from center, 0-79)
    static constexpr uint8_t ZONE_PHASE_END = 19;
    static constexpr uint8_t ZONE_ENERGY_END = 39;
    static constexpr uint8_t ZONE_BASS_END = 54;
    static constexpr uint8_t ZONE_TREBLE_END = 69;
    // LEDs 70-79 are the outer beat flash ring

    // Helper to set CENTER ORIGIN symmetric LEDs
    void setSymmetricLEDs(plugins::EffectContext& ctx, uint8_t distFromCenter, CRGB color);

    // Render "NO DATA" warning pattern
    void renderNoDataWarning(plugins::EffectContext& ctx);

    // Smooth a value with exponential decay
    float smooth(float current, float target, float dt, float speed = 8.0f);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
