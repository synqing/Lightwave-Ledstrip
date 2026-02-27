/**
 * @file LGPHolographicEsTunedEffect.h
 * @brief LGP Holographic (ES tuned) - multi-layer interference, musically driven
 *
 * Effect ID: 108
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE | DEPTH | AUDIO_SYNC
 *
 * Reactivity design:
 * - Band energy controls layer gains (wide→tight layers)
 * - Beat phase drives phase ratios (1×, 2×, 4×) when tempo confidence is high
 * - Flux spikes add “refraction” shimmer accents (backend-agnostic onset proxy)
 * - Chroma anchors colour (non-rainbow: no time-based hue cycling)
 * - Downbeats briefly “focus” the hologram (phase alignment + contrast)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos::effects::ieffect {

class LGPHolographicEsTunedEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HOLOGRAPHIC_ES_TUNED;
    LGPHolographicEsTunedEffect() = default;
    ~LGPHolographicEsTunedEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Phase state (dt-based; always advances so it still looks alive without beat lock)
    float m_phase1 = 0.0f;
    float m_phase2 = 0.0f;
    float m_phase3 = 0.0f;

    // Chroma anchor (circular EMA state, radians)
    uint32_t m_lastHopSeq = 0;
    float m_chromaAngle = 0.0f;

    // Flux/refraction accent
    float m_lastFastFlux = 0.0f;
    float m_refraction = 0.0f;

    // Downbeat focus accent
    float m_focus = 0.0f;

    // Beat phase tracking
    float m_lastBeatPhase = 0.0f;
};

} // namespace lightwaveos::effects::ieffect

