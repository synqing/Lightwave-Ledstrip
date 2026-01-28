/**
 * @file LGPHolographicAutoCycleEffect.cpp
 * @brief LGP Holographic Auto-Cycle effect implementation
 *
 * Rendering math is identical to LGPHolographicEffect.cpp.
 * The key difference is internal palette management with auto-cycling.
 */

#include "LGPHolographicAutoCycleEffect.h"
#include "../CoreEffects.h"
#include "../../palettes/Palettes_Master.h"
#include <FastLED.h>
#include <cmath>

namespace lightwaveos {
namespace effects {
namespace ieffect {

// The 20 hand-selected palette IDs (source of truth, never mutated)
static constexpr uint8_t PALETTE_IDS[LGPHolographicAutoCycleEffect::PLAYLIST_SIZE] = {
     0,  1, 10, 11, 12, 13, 15, 16, 17, 18,
    20, 23, 25, 31, 33, 36, 43, 44, 57, 61
};

static constexpr float TWO_PI_F = 6.283185307f;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LGPHolographicAutoCycleEffect::LGPHolographicAutoCycleEffect()
    : m_phase1(0.0f)
    , m_phase2(0.0f)
    , m_phase3(0.0f)
    , m_activePalette()
    , m_targetPalette()
    , m_lastCycleCount(0)
    , m_cyclesSinceChange(0)
    , m_playlistIndex(0)
    , m_isTransitioning(false)
{
    memcpy(m_playlist, PALETTE_IDS, PLAYLIST_SIZE);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool LGPHolographicAutoCycleEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;

    // Reset rendering phases
    m_phase1 = 0.0f;
    m_phase2 = 0.0f;
    m_phase3 = 0.0f;

    // Reset cycle tracking
    m_lastCycleCount = 0;
    m_cyclesSinceChange = 0;

    // Shuffle and start from first palette
    shufflePlaylist();
    m_playlistIndex = 0;

    // Load first palette as both active and target (no transition on start)
    loadPaletteFromId(m_playlist[0], m_activePalette);
    m_targetPalette = m_activePalette;
    m_isTransitioning = false;

    return true;
}

void LGPHolographicAutoCycleEffect::render(plugins::EffectContext& ctx) {
    // ===== Phase advancement (identical to LGPHolographicEffect) =====
    float speedNorm = ctx.speed / 50.0f;
    float intensityNorm = ctx.brightness / 255.0f;

    m_phase1 += speedNorm * 0.02f;
    m_phase2 += speedNorm * 0.03f;
    m_phase3 += speedNorm * 0.05f;

    // ===== Cycle detection =====
    // A "cycle" is one full 2*PI rotation of m_phase1
    int32_t currentCycleCount = (int32_t)floorf(m_phase1 / TWO_PI_F);

    if (currentCycleCount > m_lastCycleCount) {
        m_lastCycleCount = currentCycleCount;
        m_cyclesSinceChange++;

        if (m_cyclesSinceChange >= 2) {
            // 2 rotations completed on this palette -- advance
            m_cyclesSinceChange = 0;
            advancePalette();
        }
    }

    // ===== Palette crossfade =====
    if (m_isTransitioning) {
        nblendPaletteTowardPalette(m_activePalette, m_targetPalette, 24);

        // Check convergence: all 16 palette entries must match
        if (m_activePalette == m_targetPalette) {
            m_isTransitioning = false;
        }
    }

    // ===== LED rendering (identical math to LGPHolographicEffect) =====
    const int numLayers = 4;

    for (int i = 0; i < STRIP_LENGTH; i++) {
        float dist = (float)centerPairDistance((uint16_t)i);

        float layerSum = 0.0f;

        // Layer 1 - Slow, wide pattern
        layerSum += sinf(dist * 0.05f + m_phase1);

        // Layer 2 - Medium pattern
        layerSum += sinf(dist * 0.15f + m_phase2) * 0.7f;

        // Layer 3 - Fast, tight pattern
        layerSum += sinf(dist * 0.3f + m_phase3) * 0.5f;

        // Layer 4 - Very fast shimmer
        layerSum += sinf(dist * 0.6f - m_phase1 * 3.0f) * 0.3f;

        // Normalize
        layerSum = layerSum / (float)numLayers;
        layerSum = tanhf(layerSum);

        uint8_t brightness = (uint8_t)(128.0f + 127.0f * layerSum * intensityNorm);

        // Chromatic dispersion effect
        uint8_t paletteIndex1 = (uint8_t)((dist * 0.5f) + (layerSum * 20.0f));
        int paletteIndex2 = (int)(128.0f - (dist * 0.5f) - (layerSum * 20.0f));

        // Use internal auto-cycled palette instead of ctx.palette
        ctx.leds[i] = ColorFromPalette(m_activePalette,
                                       (uint8_t)(ctx.gHue + paletteIndex1),
                                       brightness, LINEARBLEND);
        if (i + STRIP_LENGTH < ctx.ledCount) {
            ctx.leds[i + STRIP_LENGTH] = ColorFromPalette(m_activePalette,
                                                          (uint8_t)(ctx.gHue + (uint8_t)paletteIndex2),
                                                          brightness, LINEARBLEND);
        }
    }
}

void LGPHolographicAutoCycleEffect::cleanup() {
    // No dynamic resources to free
}

const plugins::EffectMetadata& LGPHolographicAutoCycleEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "LGP Holo Auto-Cycle",
        "Holographic interference with auto-cycling palettes",
        plugins::EffectCategory::QUANTUM,
        1
    };
    return meta;
}

// ---------------------------------------------------------------------------
// Internal: Palette Management
// ---------------------------------------------------------------------------

void LGPHolographicAutoCycleEffect::shufflePlaylist() {
    // Reset playlist from source IDs
    memcpy(m_playlist, PALETTE_IDS, PLAYLIST_SIZE);

    // Fisher-Yates shuffle using FastLED's random8()
    for (uint8_t i = PLAYLIST_SIZE - 1; i > 0; i--) {
        uint8_t j = random8(i + 1);  // random in [0, i]
        uint8_t tmp = m_playlist[i];
        m_playlist[i] = m_playlist[j];
        m_playlist[j] = tmp;
    }
}

void LGPHolographicAutoCycleEffect::advancePalette() {
    m_playlistIndex++;

    // If we've exhausted the playlist, reshuffle
    if (m_playlistIndex >= PLAYLIST_SIZE) {
        // Save last palette ID to avoid immediate repeat after reshuffle
        uint8_t lastPaletteId = m_playlist[PLAYLIST_SIZE - 1];

        shufflePlaylist();
        m_playlistIndex = 0;

        // If first palette after shuffle matches last before shuffle, swap to avoid repeat
        if (m_playlist[0] == lastPaletteId && PLAYLIST_SIZE > 1) {
            uint8_t swapIdx = 1 + random8(PLAYLIST_SIZE - 1);
            uint8_t tmp = m_playlist[0];
            m_playlist[0] = m_playlist[swapIdx];
            m_playlist[swapIdx] = tmp;
        }
    }

    // Load the new target palette and begin crossfade
    loadPaletteFromId(m_playlist[m_playlistIndex], m_targetPalette);
    m_isTransitioning = true;
}

void LGPHolographicAutoCycleEffect::loadPaletteFromId(uint8_t paletteId, CRGBPalette16& dest) {
    uint8_t safeId = palettes::validatePaletteId(paletteId);
    dest = palettes::gMasterPalettes[safeId];
}

} // namespace ieffect
} // namespace effects
} // namespace lightwaveos
