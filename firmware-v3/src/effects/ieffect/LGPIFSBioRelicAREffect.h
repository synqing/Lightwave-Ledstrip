/**
 * @file LGPIFSBioRelicAREffect.h
 * @brief IFS Botanical Relic (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C13 (EID_LGP_IFS_BIO_RELIC_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed histogram buffer (160 floats).
 * Barnsley fern IFS with 4-transform probabilities (0.01, 0.85, 0.07, 0.07).
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPIFSBioRelicAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_IFS_BIO_RELIC_AR;

    LGPIFSBioRelicAREffect();
    ~LGPIFSBioRelicAREffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
    uint8_t getParameterCount() const override;
    const plugins::EffectParameter* getParameter(uint8_t index) const override;
    bool setParameter(const char* name, float value) override;
    float getParameter(const char* name) const override;

private:
    static constexpr uint16_t STRIP_LENGTH = 160;

    struct IFSPsram {
        float hist[STRIP_LENGTH];
    };

    IFSPsram* m_ps = nullptr;

    float m_px  = 0.0f;
    float m_py  = 0.0f;
    float m_t   = 0.0f;
    uint32_t m_rng = 0xBADC0DEu;

    // Single-stage smoothed audio
    float m_bass       = 0.0f;
    float m_treble     = 0.0f;
    float m_chromaAngle = 0.0f;

    // Asymmetric max followers
    float m_bassMax    = 0.15f;
    float m_trebleMax  = 0.15f;

    // Impact
    float m_impact     = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
