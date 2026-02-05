/**
 * @file HeartbeatEsTunedEffect.h
 * @brief Heartbeat (ES tuned) - tempo/flux driven cardiac pulse from centre
 *
 * Effect ID: 107
 * Family: FLUID_PLASMA
 * Tags: CENTER_ORIGIN | AUDIO_SYNC
 *
 * Design goals:
 * - Lock "lub" to beat tick when tempo confidence is high
 * - Trigger "dub" on beat-phase offset and/or flux spike (backend-agnostic onset proxy)
 * - Use chroma-anchored palette hue (non-rainbow; no time-based hue cycling)
 * - Fall back gracefully to the original fixed lub-dub timing when audio is unavailable
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class HeartbeatEsTunedEffect final : public plugins::IEffect {
public:
    HeartbeatEsTunedEffect() = default;
    ~HeartbeatEsTunedEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Chroma anchor (stable across hops)
    uint32_t m_lastHopSeq = 0;
    uint8_t m_dominantChromaBin = 0;
    float m_dominantChromaBinSmooth = 0.0f;

    // Beat + onset state
    float m_lastBeatPhase = 0.0f;
    float m_lastFastFlux = 0.0f;
    bool m_dubPending = false;

    // Ring state (radius in "distance from centre" LEDs, 0..HALF_LENGTH)
    float m_lubRadius = 999.0f;
    float m_dubRadius = 999.0f;
    float m_lubIntensity = 0.0f;
    float m_dubIntensity = 0.0f;

    // Fallback timing (original heartbeat lub-dub)
    uint32_t m_lastBeatTimeMs = 0;
    uint8_t m_beatState = 0;
};

} // namespace lightwaveos::effects::ieffect

