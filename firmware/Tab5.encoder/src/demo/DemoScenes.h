/**
 * @file DemoScenes.h
 * @brief Curated scene definitions for Interactive Demo Mode
 *
 * Contains 8 hand-picked scenes optimized for "wow" moments during demos.
 * All effect and palette IDs are verified against actual registries.
 *
 * Verified: 2026-01-15 from PatternRegistry.cpp and Palettes_MasterData.cpp
 */

#pragma once

#include <stdint.h>

namespace demo {

// =============================================================================
// SCENE DEFINITION STRUCTURE
// =============================================================================

struct DemoScene {
    uint8_t effectId;       // Effect ID from PatternRegistry
    uint8_t paletteId;      // Palette ID from Palettes_Master
    uint8_t energy;         // 0-100% mapped to speed+intensity+complexity
    uint8_t flow;           // 0-100% mapped to mood+variation+fade
    uint8_t brightness;     // 0-100% direct mapping (floor at 30)
    uint8_t saturation;     // 0-100% palette saturation
    const char* name;       // Display name for UI
    const char* effectName; // Effect name for logging/debug
};

// =============================================================================
// CURATED SCENES (8 total)
// =============================================================================
// Verified against PatternRegistry.cpp and Palettes_MasterData.cpp

constexpr DemoScene DEMO_SCENES[] = {
    // 0: Ocean Depths - Signature scene (Reset target)
    // Organic, calming, showcases light-guide diffusion
    {
        .effectId = 35,       // LGP Bioluminescent Waves
        .paletteId = 57,      // Viridis
        .energy = 30,
        .flow = 70,
        .brightness = 75,
        .saturation = 80,
        .name = "Ocean Depths",
        .effectName = "LGP Bioluminescent Waves"
    },

    // 1: Neon Storm - High energy, vibrant
    {
        .effectId = 65,       // LGP Chromatic Lens
        .paletteId = 9,       // Pink Splash 07 (PAL_HIGH_SAT | PAL_VIVID)
        .energy = 80,
        .flow = 50,
        .brightness = 85,
        .saturation = 95,
        .name = "Neon Storm",
        .effectName = "LGP Chromatic Lens"
    },

    // 2: Forest Glow - Natural, gentle
    // Substituted: Benard Convection -> LGP Fluid Dynamics
    {
        .effectId = 39,       // LGP Fluid Dynamics
        .paletteId = 71,      // Earth (PAL_WARM | PAL_CALM)
        .energy = 40,
        .flow = 60,
        .brightness = 70,
        .saturation = 75,
        .name = "Forest Glow",
        .effectName = "LGP Fluid Dynamics"
    },

    // 3: Cosmos Dance - Mathematical beauty, hypnotic
    // Substituted: Mandelbrot Zoom -> LGP Gravitational Wave Chirp
    {
        .effectId = 61,       // LGP Gravitational Wave Chirp
        .paletteId = 58,      // Plasma
        .energy = 60,
        .flow = 40,
        .brightness = 80,
        .saturation = 85,
        .name = "Cosmos Dance",
        .effectName = "LGP Gravitational Wave Chirp"
    },

    // 4: Pulse Beat - Dynamic, rhythmic feel
    {
        .effectId = 43,       // LGP Soliton Waves
        .paletteId = 5,       // Analogous 1 (PAL_HIGH_SAT | PAL_VIVID)
        .energy = 90,
        .flow = 30,
        .brightness = 85,
        .saturation = 90,
        .name = "Pulse Beat",
        .effectName = "LGP Soliton Waves"
    },

    // 5: Calm Breath - Meditative, smooth
    {
        .effectId = 11,       // Breathing
        .paletteId = 64,      // Ocean (PAL_COOL | PAL_CALM)
        .energy = 20,
        .flow = 80,
        .brightness = 65,
        .saturation = 70,
        .name = "Calm Breath",
        .effectName = "Breathing"
    },

    // 6: Fire & Ice - Dramatic contrast
    // Substituted: Rayleigh-Taylor -> LGP Wave Collision
    {
        .effectId = 17,       // LGP Wave Collision
        .paletteId = 73,      // Split (diverging blue<->red)
        .energy = 70,
        .flow = 50,
        .brightness = 80,
        .saturation = 85,
        .name = "Fire & Ice",
        .effectName = "LGP Wave Collision"
    },

    // 7: Quantum Flow - Scientific beauty
    // Substituted: Gray-Scott -> LGP Photonic Crystal
    {
        .effectId = 33,       // LGP Photonic Crystal
        .paletteId = 58,      // Plasma
        .energy = 50,
        .flow = 60,
        .brightness = 75,
        .saturation = 80,
        .name = "Quantum Flow",
        .effectName = "LGP Photonic Crystal"
    }
};

constexpr uint8_t DEMO_SCENE_COUNT = sizeof(DEMO_SCENES) / sizeof(DemoScene);

// =============================================================================
// SIGNATURE SCENE (Reset Target)
// =============================================================================
// Ocean Depths is the brand-defining signature scene.
// Reset always returns to this scene for consistent "known-good" state.

constexpr uint8_t SIGNATURE_SCENE_INDEX = 0;  // Ocean Depths

inline const DemoScene& getSignatureScene() {
    return DEMO_SCENES[SIGNATURE_SCENE_INDEX];
}

// =============================================================================
// WATCH DEMO CHOREOGRAPHY
// =============================================================================
// 90-second choreographed showcase sequence

struct DemoChoreographyStep {
    uint8_t sceneIndex;     // Index into DEMO_SCENES
    uint16_t durationMs;    // How long to show this scene
    uint16_t transitionMs;  // Transition time to next scene (0 = instant)
};

constexpr DemoChoreographyStep WATCH_DEMO_SEQUENCE[] = {
    {0, 15000, 1000},   // 0-15s:  Ocean Depths (calm intro)
    {2, 10000, 1500},   // 15-25s: Forest Glow (build)
    {3, 10000, 1500},   // 25-35s: Cosmos Dance (continue build)
    {1, 10000, 1000},   // 35-45s: Neon Storm (peak energy)
    {4, 10000, 1000},   // 45-55s: Pulse Beat (maintain energy)
    {6, 10000, 1500},   // 55-65s: Fire & Ice (dramatic)
    {7, 10000, 1500},   // 65-75s: Quantum Flow (scientific beauty)
    {5, 15000, 2000},   // 75-90s: Calm Breath (resolution)
};

constexpr uint8_t WATCH_DEMO_STEP_COUNT = sizeof(WATCH_DEMO_SEQUENCE) / sizeof(DemoChoreographyStep);
constexpr uint32_t WATCH_DEMO_TOTAL_MS = 90000;  // 90 seconds

// =============================================================================
// PALETTE GROUPS (for Lane C)
// =============================================================================
// 8 curated palette selections for quick color changes

struct PaletteGroup {
    uint8_t paletteId;      // Palette ID from Palettes_Master
    const char* name;       // Display name
    uint8_t flags;          // Palette flags for categorization
};

// Flag definitions (from Palettes_Master.h)
constexpr uint8_t PAL_WARM        = 0x01;
constexpr uint8_t PAL_COOL        = 0x02;
constexpr uint8_t PAL_HIGH_SAT    = 0x04;
constexpr uint8_t PAL_CALM        = 0x10;
constexpr uint8_t PAL_VIVID       = 0x20;

constexpr PaletteGroup PALETTE_GROUPS[] = {
    {0,  "Warm",   PAL_WARM | PAL_VIVID},          // Sunset Real
    {64, "Cool",   PAL_COOL | PAL_CALM},           // Ocean
    {9,  "Neon",   PAL_HIGH_SAT | PAL_VIVID},      // Pink Splash 07
    {1,  "Calm",   PAL_COOL | PAL_CALM},           // Rivendell
    {5,  "Bold",   PAL_HIGH_SAT | PAL_VIVID},      // Analogous 1
    {60, "Soft",   PAL_WARM | PAL_CALM},           // Magma
    {24, "Fire",   PAL_WARM | PAL_VIVID},          // Fire
    {62, "Ice",    PAL_COOL | PAL_CALM},           // Abyss
};

constexpr uint8_t PALETTE_GROUP_COUNT = sizeof(PALETTE_GROUPS) / sizeof(PaletteGroup);

} // namespace demo
