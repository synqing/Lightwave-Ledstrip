/**
 * @file LGPIFSBioRelicAREffect.h
 * @brief IFS Botanical Relic (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C13 (EID_LGP_IFS_BIO_RELIC_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed per-zone histogram buffer (160 floats x kMaxZones).
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
    static constexpr uint8_t kMaxZones = 4;

#ifndef NATIVE_BUILD
    struct IFSPsram {
        float hist[kMaxZones][STRIP_LENGTH];
    };
    IFSPsram* m_ps = nullptr;
#else
    float m_hist[kMaxZones][STRIP_LENGTH];
#endif

    float m_px[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_py[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_t[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t m_rng[kMaxZones] = {0xBADC0DEu, 0xBADC0DEu, 0xBADC0DEu, 0xBADC0DEu};

    // Single-stage smoothed audio
    float m_bass[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Asymmetric max followers
    float m_bassMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones] = {0.15f, 0.15f, 0.15f, 0.15f};

    // Impact
    float m_impact[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
