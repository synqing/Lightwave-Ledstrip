/**
 * @file LGPLangtonHighwayAREffect.h
 * @brief Langton Highway (5-Layer AR) — REWRITTEN
 *
 * Effect ID: 0x1C0E (EID_LGP_LANGTON_HIGHWAY_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 *
 * Langton's ant on 64x64 grid, projected to 1D via drifting diagonal slice.
 * PSRAM-allocated per-zone grid.
 *
 * Per-zone state: ZoneComposer reuses one instance across up to kMaxZones
 * zones. The ant grid itself, ant state, scalars and followers are ALL
 * dimensioned [kMaxZones]. Each zone runs an independent Langton's ant so
 * assigning the same effect to multiple zones produces independent CA
 * evolutions, not a single shared grid whose ant advances N times per frame.
 * See forensic audit P1-09.
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
    // Per-zone state dimensioning. kMaxZones=4 matches existing exemplars
    // and leaves headroom for 0xFF fallback to slot 0.
    static constexpr uint8_t kMaxZones = 4;

    // Langton's ant grid dimensions
    static constexpr uint8_t W = 64;
    static constexpr uint8_t H = 64;
    static constexpr uint16_t kGridBytes = static_cast<uint16_t>(W) * H; // 4096 B

    // Per-zone PSRAM grid (contiguous block of kMaxZones * W * H bytes).
    // Access zone z's grid as  m_grid + z * kGridBytes.
    // Total: 4 * 4096 = 16 KB in SPIRAM (was 4 KB single-zone).
    uint8_t* m_grid = nullptr;

    // Per-zone ant state
    int8_t m_antX[kMaxZones]   = {32, 32, 32, 32};
    int8_t m_antY[kMaxZones]   = {32, 32, 32, 32};
    int8_t m_antDir[kMaxZones] = {0, 0, 0, 0};

    float m_antStepAccum[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_sliceOffset[kMaxZones]  = {0.0f, 0.0f, 0.0f, 0.0f};

    // Per-zone smoothed audio
    float m_bass[kMaxZones]        = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Per-zone asymmetric max followers
    float m_bassMax[kMaxZones]     = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones]   = {0.15f, 0.15f, 0.15f, 0.15f};

    // Per-zone impact
    float m_impact[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};

    // Zone-aware helpers (operate on zone z's grid slice)
    void stepAnt(int z);
    float sampleProjection(int z, float offset);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
