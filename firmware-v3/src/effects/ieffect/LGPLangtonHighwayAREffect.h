/**
 * @file LGPLangtonHighwayAREffect.h
 * @brief Langton Highway (5-Layer AR) — REWRITTEN
 *
 * Effect ID: 0x1C0E (EID_LGP_LANGTON_HIGHWAY_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 *
 * Langton's ant on 64x64 grid, projected to 1D via drifting diagonal slice.
 * PSRAM-allocated grid.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPLangtonHighwayAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_LANGTON_HIGHWAY_AR;

    LGPLangtonHighwayAREffect();
    ~LGPLangtonHighwayAREffect() override;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    // Langton's ant state
    static constexpr uint8_t W = 64;
    static constexpr uint8_t H = 64;
    uint8_t* m_grid = nullptr; // PSRAM allocation (W*H bytes)

    int8_t m_antX = 32;
    int8_t m_antY = 32;
    int8_t m_antDir = 0; // 0=N, 1=E, 2=S, 3=W

    float m_antStepAccum = 0.0f;
    float m_sliceOffset = 0.0f;

    // Single-stage smoothed audio
    float m_bass       = 0.0f;
    float m_treble     = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_trebleMax  = 0.15f;

    // Impact
    float m_impact     = 0.0f;

    void stepAnt();
    float sampleProjection(float offset);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
