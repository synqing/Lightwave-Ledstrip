/**
 * @file EsSpectrumRefEffect.h
 * @brief ES v1.1 "Spectrum" reference show (64-bin spectrum strip).
 *
 * Per-zone PSRAM state: ZoneComposer reuses one instance across up to 4 zones,
 * setting ctx.zoneId before each render(). All temporal state is zone-indexed
 * to prevent cross-zone contamination.
 *
 * dt-corrected follower coefficients for frame-rate independence.
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#include "../../../config/effect_ids.h"
#endif

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsSpectrumRefEffect : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_ES_SPECTRUM;

    EsSpectrumRefEffect() = default;
    ~EsSpectrumRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t kMaxZones = 4;
    static constexpr uint8_t kBinCount = 64;

    // Follower time constants (derived from per-frame alphas at 60 fps):
    //   attack alpha 0.30 → tau = -1 / (60 * ln(1 - 0.30)) ≈ 0.047 s
    //   decay  alpha 0.02 → tau = -1 / (60 * ln(1 - 0.02)) ≈ 0.825 s
    static constexpr float kFollowerAttackTau = 0.047f;
    static constexpr float kFollowerDecayTau  = 0.825f;

    // PSRAM-allocated — large buffers MUST NOT live in DRAM (see MEMORY_ALLOCATION.md)
    struct PsramData {
        float binsSmooth[kMaxZones][kBinCount];
        float maxFollower[kMaxZones];
    };
    PsramData* m_ps = nullptr;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference
