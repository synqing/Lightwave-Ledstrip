/**
 * @file LGPHolyShitBangersPack.h
 * @brief 5 GOD-TIER bangers: Chimera Crown, Catastrophe Caustics, Hyperbolic Portal,
 *        Lorenz Ribbon, IFS Botanical Relic.
 *
 * Rules:
 * - Centre-origin symmetry is sacred.
 * - Dual-strip is LOCKED (no wing rivalry / no alternating dominance).
 * - Brightness carries structure; hue stays coherent (colourist-grade).
 *
 * Integration:
 * - Add this .h/.cpp in your effects folder.
 * - Register each class in your effect registry/factory with new IDs.
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"

#include "../../config/effect_ids.h"

#ifndef NATIVE_BUILD
#include <esp_heap_caps.h>
#endif

namespace lightwaveos {
namespace effects {
namespace ieffect {

static constexpr uint16_t STRIP_LENGTH = 160;

// 1) Chimera Crown — synced + unsynced domains coexist (Kuramoto-ish, nonlocal)
class LGPChimeraCrownEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CHIMERA_CROWN;
    LGPChimeraCrownEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    struct ChimeraPsram {
        float theta[STRIP_LENGTH];
        float omega[STRIP_LENGTH];
        float Rlocal[STRIP_LENGTH];
    };
    ChimeraPsram* m_ps = nullptr;
    float m_t;
};

// 2) Catastrophe Caustics — 1D ray envelope histogram (focus pull, cusp filaments)
class LGPCatastropheCausticsEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CATASTROPHE_CAUSTICS;
    LGPCatastropheCausticsEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    struct CausticsPsram {
        float I[STRIP_LENGTH];
    };
    CausticsPsram* m_ps = nullptr;
    float m_t;
};

// 3) Hyperbolic Portal — Poincaré-style edge densification (atanh blow-up)
class LGPHyperbolicPortalEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HYPERBOLIC_PORTAL;
    LGPHyperbolicPortalEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 4) Lorenz Ribbon — strange attractor trail → 1D distance field glow
class LGPLorenzRibbonEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_LORENZ_RIBBON;
    LGPLorenzRibbonEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    static constexpr int TRAIL_N = 96;
    struct LorenzPsram {
        float trail[TRAIL_N];
    };
    LorenzPsram* m_ps = nullptr;
    float m_x, m_y, m_z;
    uint8_t m_head;
    float m_t;
};

// 5) IFS Botanical Relic — Barnsley-style IFS, mirrored for relic symmetry
class LGPIFSBioRelicEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_IFS_BIO_RELIC;
    LGPIFSBioRelicEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    struct IFSPsram {
        float hist[STRIP_LENGTH];
    };
    IFSPsram* m_ps = nullptr;
    float m_px, m_py;
    uint32_t m_rng;
    float m_t;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
