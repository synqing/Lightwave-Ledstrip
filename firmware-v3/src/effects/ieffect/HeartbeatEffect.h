// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file HeartbeatEffect.h
 * @brief Heartbeat - Rhythmic cardiac pulsing
 * 
 * Effect ID: 9
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN
 * 
 * Instance State:
 * - m_lastBeatTime: Last beat timestamp
 * - m_beatState: Current beat state (0=waiting, 1=first beat, 2=second beat)
 * - m_pulseRadius: Current pulse expansion radius
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class HeartbeatEffect : public plugins::IEffect {
public:
    HeartbeatEffect();
    ~HeartbeatEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Instance state (was: static uint32_t lastBeatTime, static uint8_t beatState, static float pulseRadius)
    uint32_t m_lastBeatTime;
    uint8_t m_beatState;  // 0=waiting, 1=first beat, 2=second beat
    float m_pulseRadius;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos

