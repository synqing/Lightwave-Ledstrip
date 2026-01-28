/**
 * @file LGPHolographicAutoCycleEffect.h
 * @brief LGP Holographic Auto-Cycle - Multi-layer interference with auto-cycling palettes
 *
 * Effect ID: 100
 * Family: INTERFERENCE
 * Tags: CENTER_ORIGIN | DUAL_STRIP | MOIRE | DEPTH | AUTO_PALETTE
 *
 * Renders the same holographic interference pattern as LGPHolographicEffect (ID 14)
 * but manages its own internal palette, randomly cycling through 20 hand-selected
 * palettes. Each palette plays for 2 full rotational cycles of the primary phase,
 * then smooth-crossfades to the next.
 *
 * Instance State:
 * - m_phase1/m_phase2/m_phase3: Layer phases (identical to LGPHolographicEffect)
 * - m_activePalette/m_targetPalette: Internal palette management
 * - m_playlist[]: Shuffled palette ID order
 */

#pragma once

#include "../../plugins/api/IEffect.h"
#include "../../plugins/api/EffectContext.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

class LGPHolographicAutoCycleEffect : public plugins::IEffect {
public:
    LGPHolographicAutoCycleEffect();
    ~LGPHolographicAutoCycleEffect() override = default;

    // IEffect interface
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

    // Public constant so static PALETTE_IDS array in .cpp can reference it
    static constexpr uint8_t PLAYLIST_SIZE = 20;

private:
    // ===== Rendering state (identical to LGPHolographicEffect) =====
    float m_phase1;
    float m_phase2;
    float m_phase3;

    // ===== Palette auto-cycle state =====
    CRGBPalette16 m_activePalette;      // Currently rendering palette (mutated during crossfade)
    CRGBPalette16 m_targetPalette;      // Target palette being blended toward

    // Cycle tracking
    int32_t m_lastCycleCount;            // floor(m_phase1 / TWO_PI) at last check
    uint8_t m_cyclesSinceChange;         // Counts rotations on current palette (0 or 1)

    // Playlist state
    uint8_t m_playlist[PLAYLIST_SIZE];   // Shuffled copy of palette IDs
    uint8_t m_playlistIndex;             // Current position in shuffled playlist

    // Crossfade state
    bool m_isTransitioning;              // True when blending active toward target

    // ===== Internal methods =====
    void shufflePlaylist();              // Fisher-Yates shuffle
    void advancePalette();               // Pick next palette and begin crossfade
    void loadPaletteFromId(uint8_t paletteId, CRGBPalette16& dest);
};

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
