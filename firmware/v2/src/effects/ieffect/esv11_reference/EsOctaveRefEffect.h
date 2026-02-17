/**
 * @file EsOctaveRefEffect.h
 * @brief ES v1.1 "Octave" reference show (chromagram strip).
 *
 * Per-zone PSRAM state: ZoneComposer reuses one instance across up to 4
 * zones, so all temporal state is indexed by ctx.zoneId.
 * Follower coefficients are dt-corrected for frame-rate independence.
 */

#pragma once

#include "../../../plugins/api/IEffect.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos::effects::ieffect::esv11_reference {

class EsOctaveRefEffect : public plugins::IEffect {
public:
    EsOctaveRefEffect() = default;
    ~EsOctaveRefEffect() override = default;

    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    static constexpr uint8_t kMaxZones = 4;

    // Follower time constants derived from per-frame alphas at 60 FPS:
    //   attack alpha 0.25 -> tau = -1 / (60 * ln(1 - 0.25)) ~= 0.058 s
    //   decay  alpha 0.03 -> tau = -1 / (60 * ln(1 - 0.03)) ~= 0.547 s
    static constexpr float kMaxFollowerAttackTau = 0.058f;
    static constexpr float kMaxFollowerDecayTau  = 0.547f;

    // PSRAM-allocated per-zone state (>64 bytes total)
    struct PsramData {
        float chromaSmooth[kMaxZones][12];
        float maxFollower[kMaxZones];
    };
    PsramData* m_ps = nullptr;
};

} // namespace lightwaveos::effects::ieffect::esv11_reference
