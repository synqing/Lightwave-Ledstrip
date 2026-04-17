/**
 * @file LGPAiryCometAREffect.h
 * @brief LGP Airy Comet (5-Layer AR) — REWRITTEN
 *
 * Effect ID: EID_LGP_AIRY_COMET_AR (0x1C03)
 * Direct ControlBus reads, single-stage smoothing, max follower normalisation.
 *
 * Per-zone state: ZoneComposer reuses one instance across up to kMaxZones zones.
 * ALL temporal state is dimensioned [kMaxZones] and indexed by ctx.zoneId
 * (with bounds-check fallback to zone 0 for global render 0xFF).
 * Without this, a single effect driving multiple zones would advance its
 * smoothing/followers N times per frame, collapsing audio dynamics and
 * stacking motion. See forensic audit P1-09.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include "../../config/effect_ids.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPAiryCometAREffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_AIRY_COMET_AR;

    LGPAiryCometAREffect();
    ~LGPAiryCometAREffect() override = default;

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
    // (Snapwave, Bloom, Es*) — slightly oversized versus MAX_ZONES=3 to
    // tolerate future growth and defensive 0xFF fallback.
    static constexpr uint8_t kMaxZones = 4;

    // ---------------- Per-zone temporal state ----------------
    float m_t[kMaxZones]           = {0.0f, 0.0f, 0.0f, 0.0f};

    // Single-stage smoothed audio (per-zone)
    float m_bass[kMaxZones]        = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_treble[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_chromaAngle[kMaxZones] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Asymmetric max followers (per-zone)
    float m_bassMax[kMaxZones]     = {0.15f, 0.15f, 0.15f, 0.15f};
    float m_trebleMax[kMaxZones]   = {0.15f, 0.15f, 0.15f, 0.15f};

    // Impact (per-zone)
    float m_impact[kMaxZones]      = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
