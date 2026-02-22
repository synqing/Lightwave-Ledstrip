/**
 * @file LGPShapeBangersPack.h
 * @brief 11 shape-driven LGP effects pack (1D, centre-origin, dual-strip)
 *
 * Design rules:
 * - Centre-origin illusion first: patterns are symmetric about the mid point.
 * - No wing rivalry: Strip B is a phase-locked copy of Strip A (no alternating dominance).
 * - Palette-friendly: hue is coherent and slow; contrast comes from luminance.
 *
 * Integration:
 * - Drop this into the same folder as your other LGP effects.
 * - Add it to your build.
 * - Register each class in your effect registry with new IDs.
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

static constexpr uint16_t BANGERS_STRIP_LENGTH = 160;

// 1) Talbot Carpet (optical self-imaging vibes)
class LGPTalbotCarpetEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_TALBOT_CARPET;
    LGPTalbotCarpetEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 2) Airy Comet (self-accelerating lobe + trailing lobes)
class LGPAiryCometEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_AIRY_COMET;
    LGPAiryCometEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 3) Moire Cathedral (beat interference into arches)
class LGPMoireCathedralEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MOIRE_CATHEDRAL;
    LGPMoireCathedralEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 4) Superformula Living Glyph (morphing sigils)
class LGPSuperformulaGlyphEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SUPERFORMULA_GLYPH;
    LGPSuperformulaGlyphEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 5) Spirograph Crown (hypotrochoid crown loops)
class LGPSpirographCrownEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_SPIROGRAPH_CROWN;
    LGPSpirographCrownEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 6) Rose Bloom (petal engine)
class LGPRoseBloomEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_ROSE_BLOOM;
    LGPRoseBloomEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 7) Harmonograph Halo (Lissajous aura orbitals)
class LGPHarmonographHaloEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_HARMONOGRAPH_HALO;
    LGPHarmonographHaloEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 8) Rule 30 Cathedral (triangles + chaos textile)
class LGPRule30CathedralEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_RULE30_CATHEDRAL;
    LGPRule30CathedralEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
#ifndef NATIVE_BUILD
    struct Rule30Psram {
        uint8_t cells[BANGERS_STRIP_LENGTH];
        uint8_t next[BANGERS_STRIP_LENGTH];
    };
    Rule30Psram* m_ps = nullptr;
#else
    void* m_ps = nullptr;
#endif
    uint32_t m_step;
};

// 9) Langton Highway (emergent order reveal)
class LGPLangtonHighwayEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_LANGTON_HIGHWAY;
    LGPLangtonHighwayEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    static constexpr uint8_t W = 64;
    static constexpr uint8_t H = 64;

private:
    // ⚠️ PSRAM-ALLOCATED — 4KB grid MUST NOT live in DRAM
    uint8_t* m_grid = nullptr;  // PSRAM: H*W bytes, accessed as m_grid[y * W + x]
    uint8_t  m_x;
    uint8_t  m_y;
    uint8_t  m_dir;      // 0=up,1=right,2=down,3=left
    uint32_t m_steps;
    uint8_t  m_scan;     // simple projection offset
};

// 10) Cymatic Ladder (standing wave sculpture)
class LGPCymaticLadderEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_CYMATIC_LADDER;
    LGPCymaticLadderEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

// 11) Mach Diamonds (shock diamond jewellery)
class LGPMachDiamondsEffect final : public plugins::IEffect {
public:
    static constexpr lightwaveos::EffectId kId = lightwaveos::EID_LGP_MACH_DIAMONDS;
    LGPMachDiamondsEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;
private:
    float m_t;
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
