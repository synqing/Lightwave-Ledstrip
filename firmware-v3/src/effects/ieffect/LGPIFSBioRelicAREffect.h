/**
 * @file LGPIFSBioRelicAREffect.h
 * @brief IFS Botanical Relic (5-Layer Audio-Reactive)
 *
 * Effect ID: 0x1C13 (EID_LGP_IFS_BIO_RELIC_AR)
 * Family: FIVE_LAYER_AR
 * Category: QUANTUM
 * Tags: CENTER_ORIGIN | DUAL_STRIP | PHYSICS | AUDIO_REACTIVE
 *
 * 5-layer composition model (NOT flat lerp):
 *   Bed       - RMS-driven relic luminance base (tau ~0.45s)
 *   Structure - points/frame + decay rate from flux + saliency (tau ~0.15s)
 *   Impact    - beat-triggered vein pulse (decay ~0.25s)
 *   Tonal     - museum-relic hue anchor from chord/root
 *   Memory    - accumulated vein glow persistence (decay ~0.95s)
 *
 * Composition: brightness = bed * veinGeom + impact + memory
 *
 * Barnsley fern IFS physics:
 *   4-transform IFS with classic probabilities (0.01, 0.85, 0.07, 0.07)
 *   Mirror x for centre-origin symmetry, project onto 160-LED histogram
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"
#include "AudioReactiveLowRiskPackHelpers.h"

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
    lowrisk_ar::Ar16Controls m_controls;
    lowrisk_ar::ArRuntimeState m_ar;

    float m_px  = 0.0f;
    float m_py  = 0.0f;
    float m_t   = 0.0f;
    uint32_t m_rng = 0xBADC0DEu;

    // 5-Layer composition state
    float m_bed       = 0.3f;
    float m_structure = 0.5f;
    float m_impact    = 0.0f;
    float m_memory    = 0.0f;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
