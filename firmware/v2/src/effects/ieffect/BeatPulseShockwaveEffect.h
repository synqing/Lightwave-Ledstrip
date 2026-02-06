/**
 * @file BeatPulseShockwaveEffect.h
 * @brief Beat Pulse (Shockwave) - travelling centre-origin pulse (outward or inward)
 *
 * This effect implements the "canonical" Beat Pulse shockwave described in the
 * LED preview notes: ring position is driven by time-since-beat, while amplitude
 * is driven by beat strength and an exponential decay envelope.
 *
 * Knob mapping (meaningful, stable, and portable):
 * - `speed`      : travel time (centre→edge or edge→centre)
 * - `intensity`  : pulse amplitude + white push amount
 * - `complexity` : ring width + optional echoes
 * - `variation`  : ring profile (tent vs smoothstep vs cosine) + echo spacing trim
 * - `fadeAmount` : trails persistence (higher = shorter trails)
 *
 * Effect IDs:
 * - 111: Beat Pulse (Shockwave) [outward]
 * - 112: Beat Pulse (Shockwave In) [inward]
 */

#pragma once

#include "../CoreEffects.h"
#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

namespace lightwaveos::effects::ieffect {

class BeatPulseShockwaveEffect final : public plugins::IEffect {
public:
    explicit BeatPulseShockwaveEffect(bool inward);
    ~BeatPulseShockwaveEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    bool m_inward = false;
    plugins::EffectMetadata m_meta;

    uint32_t m_lastBeatTimeMs = 0;
    float m_latchedBeatStrength = 0.0f;

    // Trails are stored in centre-distance space (HALF_LENGTH entries).
    float m_trail[HALF_LENGTH] = {0.0f};
};

} // namespace lightwaveos::effects::ieffect
