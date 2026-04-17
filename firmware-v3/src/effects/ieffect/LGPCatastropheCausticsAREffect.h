/**
 * @file LGPCatastropheCausticsAREffect.h
 * @brief Catastrophe Caustics (5-Layer AR) -- REWRITTEN
 *
 * Effect ID: 0x1C10 (EID_LGP_CATASTROPHE_CAUSTICS_AR)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 * PSRAM-backed per-zone intensity histogram buffer (160 floats x kMaxZones).
 * Ray-envelope histogram with catastrophe optics.
 *
 * Per-zone state: ZoneComposer reuses one instance across up to kMaxZones
 * zones. ALL temporal state (scalars + histogram accumulator) is dimensioned
 * [kMaxZones] and indexed by ctx.zoneId with bounds-check fallback to 0.
 * Without this, zones stomp each other's caustic histograms. See forensic
 * audit P1-09.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPCatastropheCausticsAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CATASTROPHE_CAUSTICS_AR;

    LGPCatastropheCausticsAREffect();
    ~LGPCatastropheCausticsAREffect() override;

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
    // (Snapwave, Bloom, Es*) — slightly oversized versus MAX_ZONES=3 for
    // defensive 0xFF fallback.
    static constexpr uint8_t kMaxZones = 4;

    // Per-zone scalars
    float m_t[kMaxZones]           = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_bass[kMaxZones]        = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_bassMax[kMaxZones]     = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones]   = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_impact[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};

#ifndef NATIVE_BUILD
    // PSRAM allocation for per-zone intensity histograms.
    // Each zone needs its own 160-float accumulator — zones' ray projections
    // must not mix. Size: 160 * 4 * 4 = 2,560 B in SPIRAM (was 640 B single).
    struct CausticsPsram {
        float I[kMaxZones][160];  // Per-zone ray-envelope intensity accumulators
    };
    CausticsPsram* m_ps = nullptr;
#else
    // NATIVE_BUILD fallback (testing only)
    float m_I[kMaxZones][160];
#endif
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
