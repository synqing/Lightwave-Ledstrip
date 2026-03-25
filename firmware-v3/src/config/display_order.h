/**
 * @file display_order.h
 * @brief Display ordering for effect selection UI
 *
 * Decouples the order in which effects appear in menus/cycling from their
 * stable EffectId. This list can be reordered, pruned, or extended
 * without touching effect identity or persistence.
 *
 * Initially mirrors the legacy sequential order (0-161), minus retired slots.
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include "effect_ids.h"
#include "features.h"

namespace lightwaveos {

// ============================================================================
// Display Order
// ============================================================================
// This array defines the order in which effects appear when cycling via
// encoder, serial, WebSocket nextEffect/prevEffect, or any UI.
//
// To reorder effects in the UI, simply rearrange entries here.
// To hide an effect from cycling, remove it from this list.
// The effect still exists and can be set directly by ID.
// ============================================================================

constexpr EffectId DISPLAY_ORDER[] = {
    // Core / Classic
    EID_FIRE,                                //   0
    EID_OCEAN,                               //   1
    EID_PLASMA,                              //   2
    EID_CONFETTI,                            //   3
    EID_SINELON,                             //   4
    EID_JUGGLE,                              //   5
    EID_BPM,                                 //   6
    EID_WAVE_AMBIENT,                        //   7
    EID_RIPPLE,                              //   8
    EID_HEARTBEAT,                           //   9
    EID_INTERFERENCE,                        //  10
    EID_BREATHING,                           //  11
    EID_PULSE,                               //  12

    // LGP Interference
    EID_LGP_BOX_WAVE,                        //  13
    EID_LGP_HOLOGRAPHIC,                     //  14
    EID_MODAL_RESONANCE,                     //  15
    EID_LGP_INTERFERENCE_SCANNER,            //  16
    EID_LGP_WAVE_COLLISION,                  //  17

    // LGP Geometric
    EID_LGP_DIAMOND_LATTICE,                 //  18
    EID_LGP_HEXAGONAL_GRID,                  //  19
    EID_LGP_SPIRAL_VORTEX,                   //  20
    EID_LGP_SIERPINSKI,                      //  21
    EID_CHEVRON_WAVES,                       //  22
    EID_LGP_CONCENTRIC_RINGS,               //  23
    EID_LGP_STAR_BURST,                      //  24
    EID_LGP_MESH_NETWORK,                    //  25

    // LGP Advanced Optical
    EID_LGP_MOIRE_CURTAINS,                  //  26
    EID_LGP_RADIAL_RIPPLE,                   //  27
    EID_LGP_HOLOGRAPHIC_VORTEX,              //  28
    EID_LGP_EVANESCENT_DRIFT,                //  29
    EID_LGP_CHROMATIC_SHEAR,                 //  30
    EID_LGP_MODAL_CAVITY,                    //  31
    EID_LGP_FRESNEL_ZONES,                   //  32
    EID_LGP_PHOTONIC_CRYSTAL,                //  33

    // LGP Organic
    EID_LGP_AURORA_BOREALIS,                 //  34
    EID_LGP_BIOLUMINESCENT_WAVES,            //  35
    EID_LGP_PLASMA_MEMBRANE,                 //  36
    EID_LGP_NEURAL_NETWORK,                  //  37
    EID_LGP_NEURAL_NETWORK_RADIAL,           //  37b (radial variant)
    EID_LGP_CRYSTALLINE_GROWTH,              //  38
    EID_LGP_FLUID_DYNAMICS,                  //  39

    // LGP Quantum
    EID_LGP_QUANTUM_TUNNELING,               //  40
    EID_LGP_GRAVITATIONAL_LENSING,           //  41
    EID_LGP_TIME_CRYSTAL,                    //  42
    EID_LGP_SOLITON_WAVES,                   //  43
    EID_LGP_SOLITON_WAVES_RADIAL,            //  43b (radial variant)
    EID_LGP_METAMATERIAL_CLOAK,              //  44
    EID_LGP_GRIN_CLOAK,                      //  45
    EID_LGP_CAUSTIC_FAN,                     //  46
    EID_LGP_BIREFRINGENT_SHEAR,              //  47
    EID_LGP_BIREFRINGENT_SHEAR_RADIAL,       //  47b (radial variant)
    EID_LGP_ANISOTROPIC_CLOAK,               //  48
    EID_LGP_EVANESCENT_SKIN,                 //  49

    // LGP Colour Mixing
    EID_LGP_COLOR_TEMPERATURE,               //  50
    EID_LGP_RGB_PRISM,                       //  51
    EID_LGP_COMPLEMENTARY_MIXING,            //  52
    EID_LGP_QUANTUM_COLORS,                  //  53
    EID_LGP_DOPPLER_SHIFT,                   //  54
    EID_LGP_COLOR_ACCELERATOR,               //  55
    EID_LGP_DNA_HELIX,                       //  56
    EID_LGP_PHASE_TRANSITION,                //  57
    EID_LGP_CHROMATIC_ABERRATION,            //  58
    EID_LGP_PERCEPTUAL_BLEND,                //  59

    // LGP Novel Physics
    EID_LGP_CHLADNI_HARMONICS,               //  60
    EID_LGP_GRAVITATIONAL_WAVE_CHIRP,        //  61
    EID_LGP_QUANTUM_ENTANGLEMENT,            //  62
    EID_LGP_MYCELIAL_NETWORK,                //  63
    EID_LGP_RILEY_DISSONANCE,                //  64

    // LGP Chromatic
    EID_LGP_CHROMATIC_LENS,                  //  65
    EID_LGP_CHROMATIC_PULSE,                 //  66
    EID_CHROMATIC_INTERFERENCE,              //  67

    // Audio Reactive
    EID_LGP_AUDIO_TEST,                      //  68
    EID_LGP_BEAT_PULSE,                      //  69
    EID_LGP_SPECTRUM_BARS,                   //  70
    EID_LGP_BASS_BREATH,                     //  71
    EID_AUDIO_WAVEFORM,                      //  72
    EID_AUDIO_BLOOM,                         //  73
    EID_LGP_STAR_BURST_NARRATIVE,            //  74
    EID_LGP_CHORD_GLOW,                      //  75
    EID_WAVE_REACTIVE,                       //  76

    // Perlin Reactive
    EID_LGP_PERLIN_VEIL,                     //  77
    EID_LGP_PERLIN_SHOCKLINES,              //  78
    EID_LGP_PERLIN_CAUSTICS,                 //  79
    EID_LGP_PERLIN_INTERFERENCE_WEAVE,       //  80

    // Perlin Ambient
    EID_LGP_PERLIN_VEIL_AMBIENT,             //  81
    EID_LGP_PERLIN_SHOCKLINES_AMBIENT,       //  82
    EID_LGP_PERLIN_CAUSTICS_AMBIENT,         //  83
    EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT, // 84

    // Perlin Backend Test
    EID_LGP_PERLIN_BACKEND_FAST_LED,         //  85
    EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL,  //  86
    EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER, // 87

    // Enhanced Audio-Reactive
    EID_BPM_ENHANCED,                        //  88
    EID_BREATHING_ENHANCED,                  //  89
    EID_CHEVRON_WAVES_ENHANCED,              //  90
    EID_LGP_INTERFERENCE_SCANNER_ENHANCED,   //  91
    EID_LGP_PHOTONIC_CRYSTAL_ENHANCED,       //  92
    // EID_LGP_SPECTRUM_DETAIL,              //  93 - REMOVED (dead/broken 2026-03-02)
    EID_LGP_SPECTRUM_DETAIL_ENHANCED,        //  94
    EID_LGP_STAR_BURST_ENHANCED,             //  95
    EID_LGP_WAVE_COLLISION_ENHANCED,         //  96
    EID_RIPPLE_ENHANCED,                     //  97
    EID_SNAPWAVE_LINEAR,                     //  98

    // Diagnostic
    // EID_TRINITY_TEST,                     //  99 - REMOVED (dead/broken 2026-03-02)

    // Palette Auto-Cycle
    EID_LGP_HOLOGRAPHIC_AUTO_CYCLE,          // 100

    // ES v1.1 Reference
    // EID_ES_ANALOG,                        // 101 - REMOVED (dead/broken 2026-03-02)
    // EID_ES_SPECTRUM,                      // 102 - REMOVED (dead/broken 2026-03-02)
    // EID_ES_OCTAVE,                        // 103 - REMOVED (dead/broken 2026-03-02)
    // EID_ES_BLOOM,                         // 104 - REMOVED (dead/broken 2026-03-02)
    // EID_ES_WAVEFORM,                      // 105 - REMOVED (dead/broken 2026-03-02)

    // ES Tuned
    EID_RIPPLE_ES_TUNED,                     // 106
    EID_HEARTBEAT_ES_TUNED,                  // 107
    EID_LGP_HOLOGRAPHIC_ES_TUNED,            // 108

    // SensoryBridge Reference
    EID_SB_WAVEFORM310,                      // 109

    // Beat Pulse Family (retired ID 112 excluded)
    EID_BEAT_PULSE_STACK,                    // 110
    EID_BEAT_PULSE_SHOCKWAVE,                // 111
    EID_BEAT_PULSE_VOID,                     // 113
    EID_BEAT_PULSE_RESONANT,                 // 114
    EID_BEAT_PULSE_RIPPLE,                   // 115
    EID_BEAT_PULSE_SHOCKWAVE_CASCADE,        // 116
    EID_BEAT_PULSE_SPECTRAL,                 // 117
    EID_BEAT_PULSE_SPECTRAL_PULSE,           // 118
    EID_BEAT_PULSE_BREATHE,                  // 119
    EID_BEAT_PULSE_LGP_INTERFERENCE,         // 120
    EID_BEAT_PULSE_BLOOM,                    // 121

    // Transport / Parity
    EID_BLOOM_PARITY,                        // 122
    EID_KURAMOTO_TRANSPORT,                  // 123

    // Holographic Variants
    EID_LGP_OPAL_FILM,                      // 124
    EID_LGP_GRATING_SCAN,                    // 125
    EID_LGP_STRESS_GLASS,                    // 126
    EID_LGP_MOIRE_SILK,                      // 127
    EID_LGP_CAUSTIC_SHARDS,                  // 128
    EID_LGP_PARALLAX_DEPTH,                  // 129
    EID_LGP_STRESS_GLASS_MELT,              // 130
    EID_LGP_GRATING_SCAN_BREAKUP,            // 131
    EID_LGP_WATER_CAUSTICS,                  // 132
    EID_LGP_SCHLIEREN_FLOW,                  // 133

    // Reaction Diffusion
    EID_LGP_REACTION_DIFFUSION,              // 134
    EID_LGP_REACTION_DIFFUSION_TRIANGLE,     // 135

    // Shape Bangers Pack
    EID_LGP_TALBOT_CARPET,                   // 136
    EID_LGP_AIRY_COMET,                      // 137
    EID_LGP_MOIRE_CATHEDRAL,                 // 138
    EID_LGP_SUPERFORMULA_GLYPH,              // 139
    EID_LGP_SPIROGRAPH_CROWN,                // 140
    EID_LGP_ROSE_BLOOM,                      // 141
    EID_LGP_HARMONOGRAPH_HALO,               // 142
    EID_LGP_RULE30_CATHEDRAL,                // 143
    EID_LGP_LANGTON_HIGHWAY,                 // 144
    EID_LGP_CYMATIC_LADDER,                  // 145
    EID_LGP_MACH_DIAMONDS,                   // 146

    // Holy Shit Bangers Pack
    EID_LGP_CHIMERA_CROWN,                   // 147
    EID_LGP_CATASTROPHE_CAUSTICS,            // 148
    EID_LGP_HYPERBOLIC_PORTAL,               // 149
    EID_LGP_LORENZ_RIBBON,                   // 150
    EID_LGP_IFS_BIO_RELIC,                   // 151

    // Experimental Audio Pack
    EID_LGP_FLUX_RIFT,                       // 152
    EID_LGP_BEAT_PRISM,                      // 153
    EID_LGP_HARMONIC_TIDE,                   // 154
    EID_LGP_BASS_QUAKE,                      // 155
    EID_LGP_TREBLE_NET,                      // 156
    EID_LGP_RHYTHMIC_GATE,                   // 157
    EID_LGP_SPECTRAL_KNOT,                   // 158
    EID_LGP_SALIENCY_BLOOM,                  // 159
    EID_LGP_TRANSIENT_LATTICE,               // 160
    EID_LGP_WAVELET_MIRROR,                  // 161

    // Showpiece Pack 3
    EID_LGP_TIME_REVERSAL_MIRROR,            // 162
    EID_LGP_KDV_SOLITON_PAIR,               // 163
    EID_LGP_GOLD_CODE_SPECKLE,              // 164
    EID_LGP_QUASICRYSTAL_LATTICE,           // 165
    EID_LGP_FRESNEL_CAUSTIC_SWEEP,          // 166
    EID_LGP_TIME_REVERSAL_MIRROR_AR,        // 167
    EID_LGP_TIME_REVERSAL_MIRROR_MOD1,      // 168
    EID_LGP_TIME_REVERSAL_MIRROR_MOD2,      // 169
    EID_LGP_TIME_REVERSAL_MIRROR_MOD3,      // 170

    // 5-Layer Audio-Reactive Variants (0x1Cxx)
    EID_LGP_MACH_DIAMONDS_AR,               // 1C05
    EID_LGP_SPIROGRAPH_CROWN_AR,            // 1C0A
    EID_LGP_WATER_CAUSTICS_AR,              // 1C00
    EID_LGP_SCHLIEREN_FLOW_AR,              // 1C01
#if FEATURE_AR_1C_EXPERIMENTAL
    EID_LGP_TALBOT_CARPET_AR,               // 1C02
#endif
    EID_LGP_AIRY_COMET_AR,                  // 1C03
    EID_LGP_CYMATIC_LADDER_AR,              // 1C04
    EID_LGP_REACTION_DIFFUSION_AR,          // 1C06
    EID_LGP_RD_TRIANGLE_AR,                 // 1C07
    EID_LGP_MOIRE_CATHEDRAL_AR,             // 1C08
    EID_LGP_SUPERFORMULA_GLYPH_AR,          // 1C09
    EID_LGP_ROSE_BLOOM_AR,                  // 1C0B
    EID_LGP_HARMONOGRAPH_HALO_AR,           // 1C0C
    EID_LGP_RULE30_CATHEDRAL_AR,            // 1C0D
    EID_LGP_LANGTON_HIGHWAY_AR,             // 1C0E
#if FEATURE_AR_1C_EXPERIMENTAL
    EID_LGP_CHIMERA_CROWN_AR,               // 1C0F
#endif
    EID_LGP_CATASTROPHE_CAUSTICS_AR,        // 1C10
    EID_LGP_HYPERBOLIC_PORTAL_AR,           // 1C11
#if FEATURE_AR_1C_EXPERIMENTAL
    EID_LGP_LORENZ_RIBBON_AR,               // 1C12
#endif
    EID_LGP_IFS_BIO_RELIC_AR,               // 1C13

    // K1 Lightwave Parity
    EID_SB_K1_BLOOM,                         // K1 Bloom parity
    EID_SB_K1_WAVEFORM,                      // K1 Waveform parity
    EID_SB_K1_WAVEFORM_HYBRID,               // K1 Waveform Hybrid (SB colour)
    EID_SB_K1_WAVEFORM_HARMONIC,             // K1 Waveform Harmonic (pitch-mapped)
    EID_SB_K1_BLOOM_V2,                      // K1 Bloom V2 (Mean energy)
    EID_SB_K1_BLOOM_V2_MAXBIN,               // K1 Bloom V2 (Max Bin energy)
    EID_SB_K1_BLOOM_V2_RENORM,               // K1 Bloom V2 (Re-normalized energy)
    EID_SB_K1_BLOOM_V2_BEAT_PULSE,           // K1 Bloom V2 (Beat Pulse)
    EID_SB_K1_BLOOM_V2_COLOR_HISTORY,        // K1 Bloom V2 (Color History)
    EID_SB_K1_BLOOM_V2_SPECTRAL_DELTA,       // K1 Bloom V2 (Spectral Delta)
    EID_SB_K1_BLOOM_V2_BASS_TREBLE,          // K1 Bloom V2 (Bass-Treble Split)
    EID_SB_K1_BLOOM_V2_EXPONENTIAL,          // K1 Bloom V2 (Exponential Energy)
    EID_SB_K1_BLOOM_V2_SPECTRAL_SPREAD,      // K1 Bloom V2 (Spectral Spread)

    EID_LGP_FRESNEL_CAUSTIC_REACTIVE,        // Fresnel Caustic Reactive (tension/release)

    // SB Waveform Oscilloscope — removed from display order pending algorithmic
    // redesign. Effects remain registered (accessible via API/serial) for dev.
    // See .claude/handoff.md for context: SB 3.0.0 waveform was dead code,
    // activity-gated waveform data makes raw time-domain display difficult.
    // EID_SB_WAVEFORM_OSCILLOSCOPE,         // 0x130C — dev only
    // EID_SB_WAVEFORM_OSCILLOSCOPE_BRIGHT,  // 0x130D — dev only

    // Oscilloscope design exploration — 5 approaches for A/B evaluation
    EID_SB_SPECTRAL_ENVELOPE,                // A: bands[8] → spatial envelope
    EID_SB_FULL_SPECTRUM,                    // B: bins64 → high-res spectrum
    EID_SB_RAW_WAVEFORM_SCOPE,              // C: time-domain + heavy processing
    EID_SB_RECONSTRUCTED_WAVEFORM,           // D: spectral → synthetic wave
    EID_SB_SPECTRAL_BEAT_PULSE,              // E: spectrum + beat expansion

    // Onset-Driven Effects (0x1E)
    EID_LGP_BEAT_PRISM_ONSET,                // 1E00: Beat Prism (onset-driven)
    EID_LGP_BEAT_PRISM_ONSET_DRIFT,          // 1E01: Outward Drift
    EID_LGP_BEAT_PRISM_ONSET_ADVECT,         // 1E02: Pressure Advection
    EID_LGP_BEAT_PRISM_ONSET_ROTATE,         // 1E03: Rotating Facets
    EID_LGP_BEAT_PRISM_ONSET_IGNITE,         // 1E04: Progressive Ignition
};

/// Number of effects in the display order (excludes retired slots)
constexpr uint16_t DISPLAY_COUNT = sizeof(DISPLAY_ORDER) / sizeof(EffectId);

// ============================================================================
// Display Navigation Helpers
// ============================================================================

/**
 * @brief Find the display index for a given EffectId
 * @param id Effect ID to find
 * @return Display index (0-based), or DISPLAY_COUNT if not found
 *
 * Linear scan -- display list is small enough that this is negligible.
 */
inline uint16_t displayIndexOf(EffectId id) {
    for (uint16_t i = 0; i < DISPLAY_COUNT; i++) {
        if (DISPLAY_ORDER[i] == id) return i;
    }
    return DISPLAY_COUNT;
}

/**
 * @brief Get the next effect in display order (wraps around)
 * @param current Current effect ID
 * @return Next effect ID in display order, or first if current not found
 */
inline EffectId getNextDisplay(EffectId current) {
    uint16_t idx = displayIndexOf(current);
    if (idx >= DISPLAY_COUNT) return DISPLAY_ORDER[0];
    return DISPLAY_ORDER[(idx + 1) % DISPLAY_COUNT];
}

/**
 * @brief Get the previous effect in display order (wraps around)
 * @param current Current effect ID
 * @return Previous effect ID in display order, or last if current not found
 */
inline EffectId getPrevDisplay(EffectId current) {
    uint16_t idx = displayIndexOf(current);
    if (idx >= DISPLAY_COUNT) return DISPLAY_ORDER[DISPLAY_COUNT - 1];
    return DISPLAY_ORDER[(idx + DISPLAY_COUNT - 1) % DISPLAY_COUNT];
}

} // namespace lightwaveos
