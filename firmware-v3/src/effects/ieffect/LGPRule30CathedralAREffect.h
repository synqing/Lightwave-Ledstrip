/**
 * @file LGPRule30CathedralAREffect.h
 * @brief Rule 30 Cathedral (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C0D (EID_LGP_RULE30_CATHEDRAL_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed CA buffers: cells[160] + next[160].
 * Rule 30 CA: new = l ^ (c | r)
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPRule30CathedralAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RULE30_CATHEDRAL_AR;

    LGPRule30CathedralAREffect();
    ~LGPRule30CathedralAREffect() override;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint8_t kMaxZones = 4;
#ifndef NATIVE_BUILD
    // PSRAM storage for CA state
    struct Rule30Psram {
        uint8_t cells[kMaxZones][160];
        uint8_t next[kMaxZones][160];
    };
    Rule30Psram* m_ps = nullptr;
#else
    uint8_t m_cells[kMaxZones][160];
    uint8_t m_next[kMaxZones][160];
#endif

    float m_t[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_stepAccum[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Single-stage smoothed audio
    float m_bass[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Asymmetric max followers
    float m_bassMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};

    // Impact
    float m_impact[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    void seedCA(int z);
    void stepCA(int z);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
