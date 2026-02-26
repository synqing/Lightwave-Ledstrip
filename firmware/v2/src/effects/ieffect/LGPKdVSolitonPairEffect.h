/**
 * @file LGPKdVSolitonPairEffect.h
 * @brief LGP KdV Soliton Pair -- two sech^2 solitons pass through each other unchanged
 *
 * Effect ID: 0x1B01 (EID_LGP_KDV_SOLITON_PAIR)
 * Family: SHOWPIECE_PACK3
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | PHYSICS | TRAVELING
 *
 * Physics: Korteweg--de Vries soliton pair.  Two solitons with different
 * amplitudes travel in distance-from-centre space.  The taller one moves
 * faster (KdV velocity ~ amplitude), they collide near the centre, overlap
 * additively with a bright "spark" at the collision region, then re-emerge
 * unchanged -- the signature "impossible collision" of KdV dynamics.
 *
 * State: ~44 bytes total, NO PSRAM required, no heap allocation in render().
 *
 * Audio: circularChromaHueSmoothed for hue, RMS modulates amplitude +/-15%,
 * beat triggers a brief amplitude pulse on soliton 1.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPKdVSolitonPairEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_KDV_SOLITON_PAIR;

    LGPKdVSolitonPairEffect();
    ~LGPKdVSolitonPairEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // Soliton positions in distance-from-centre space (0 = centre, 80 = edge)
    float m_soliton1Pos;
    float m_soliton2Pos;

    // Accumulated time for the timed-sequence loop
    float m_time;

    // Current stage within the 12-second loop
    uint8_t m_stage;
    float m_stageTime;

    // Circular chroma angle (persisted across frames for EMA smoothing)
    float m_chromaAngle;

    // Beat-triggered amplitude pulse (decays per frame)
    float m_beatPulse;

    // Fallback phase accumulator (no-audio mode)
    float m_fallbackPhase;

    // Hop sequence tracking (audio path)
    uint32_t m_lastHopSeq;

    // Chromagram smoothing state (audio path)
    float m_chromaSmoothed[12];

    float m_a1 = 1.00f;
    float m_a2 = 0.55f;
    float m_width1 = 0.18f;
    float m_width2 = 0.23f;
    float m_sparkGain = 6.0f;
    float m_stage0Dur = 6.0f;
    float m_stage1Dur = 3.0f;
    float m_stage2Dur = 3.0f;
    float m_baseVel1 = 14.0f;
    float m_baseVel2 = 8.0f;
    uint8_t m_sparkHueShift = 20;
    uint8_t m_strip2HueShift = 30;
    uint8_t m_strip2Bright = 217;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
