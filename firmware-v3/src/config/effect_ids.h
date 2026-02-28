/**
 * @file effect_ids.h
 * @brief Stable namespaced effect IDs -- single source of truth
 *
 * Each effect has a uint16_t ID assigned once at creation and NEVER changed.
 * The high byte encodes family membership, the low byte is the sequence
 * within that family. IDs are never reused, even when effects are removed.
 *
 * Structure: [FAMILY : 8 bits][SEQUENCE : 8 bits]
 *
 * This file is auto-generated from inventory.json by gen_effect_ids.py.
 * Do not edit manually -- regenerate when adding new effects.
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <stdint.h>

namespace lightwaveos {

// ============================================================================
// Type Alias
// ============================================================================

using EffectId = uint16_t;
constexpr EffectId INVALID_EFFECT_ID = 0xFFFF;

// ============================================================================
// Family Block Constants
// ============================================================================

constexpr uint8_t FAMILY_CORE = 0x01;  // Core / Classic
constexpr uint8_t FAMILY_INTERFERENCE = 0x02;  // LGP Interference
constexpr uint8_t FAMILY_GEOMETRIC = 0x03;  // LGP Geometric
constexpr uint8_t FAMILY_ADVANCED_OPTICAL = 0x04;  // LGP Advanced Optical
constexpr uint8_t FAMILY_ORGANIC = 0x05;  // LGP Organic
constexpr uint8_t FAMILY_QUANTUM = 0x06;  // LGP Quantum
constexpr uint8_t FAMILY_COLOUR_MIXING = 0x07;  // LGP Colour Mixing
constexpr uint8_t FAMILY_NOVEL_PHYSICS = 0x08;  // LGP Novel Physics
constexpr uint8_t FAMILY_CHROMATIC = 0x09;  // LGP Chromatic
constexpr uint8_t FAMILY_AUDIO_REACTIVE = 0x0A;  // Audio Reactive
constexpr uint8_t FAMILY_PERLIN_REACTIVE = 0x0B;  // Perlin Reactive
constexpr uint8_t FAMILY_PERLIN_AMBIENT = 0x0C;  // Perlin Ambient
constexpr uint8_t FAMILY_PERLIN_TEST = 0x0D;  // Perlin Backend Test
constexpr uint8_t FAMILY_ENHANCED_AUDIO = 0x0E;  // Enhanced Audio-Reactive
constexpr uint8_t FAMILY_DIAGNOSTIC = 0x0F;  // Diagnostic
constexpr uint8_t FAMILY_AUTO_CYCLE = 0x10;  // Palette Auto-Cycle
constexpr uint8_t FAMILY_ES_REFERENCE = 0x11;  // ES v1.1 Reference
constexpr uint8_t FAMILY_ES_TUNED = 0x12;  // ES Tuned
constexpr uint8_t FAMILY_SB_REFERENCE = 0x13;  // SensoryBridge Reference
constexpr uint8_t FAMILY_BEAT_PULSE = 0x14;  // Beat Pulse Family
constexpr uint8_t FAMILY_TRANSPORT = 0x15;  // Transport / Parity
constexpr uint8_t FAMILY_HOLOGRAPHIC_VAR = 0x16;  // Holographic Variants
constexpr uint8_t FAMILY_REACTION_DIFFUSION = 0x17;  // Reaction Diffusion
constexpr uint8_t FAMILY_SHAPE_BANGERS = 0x18;  // Shape Bangers Pack
constexpr uint8_t FAMILY_HOLY_SHIT_BANGERS = 0x19;  // Holy Shit Bangers Pack
constexpr uint8_t FAMILY_EXPERIMENTAL_AUDIO = 0x1A;  // Experimental Audio Pack
constexpr uint8_t FAMILY_SHOWPIECE_PACK3 = 0x1B;  // Showpiece Pack 3

// ============================================================================
// Reserved Family Blocks
// ============================================================================

constexpr uint8_t FAMILY_RESERVED_START = 0x1C;  // 0x1C-0xEF reserved for expansion
constexpr uint8_t FAMILY_OTA_USER       = 0xF0;  // OTA-provisioned / user-uploaded effects

// ============================================================================
// Effect ID Constants
// ============================================================================
// Format: EID_<EFFECT_NAME> = 0xFFSS
//   FF = family byte, SS = sequence within family
//
// These constants are the SINGLE SOURCE OF TRUTH for effect identity.
// They are declared in effect class headers as:
//   static constexpr EffectId kId = EID_FIRE;
// ============================================================================

// --- Core / Classic (0x01xx) ---
constexpr EffectId EID_FIRE                                = 0x0100;  // old   0: Fire
constexpr EffectId EID_OCEAN                               = 0x0101;  // old   1: Ocean
constexpr EffectId EID_PLASMA                              = 0x0102;  // old   2: Plasma
constexpr EffectId EID_CONFETTI                            = 0x0103;  // old   3: Confetti
constexpr EffectId EID_SINELON                             = 0x0104;  // old   4: Sinelon
constexpr EffectId EID_JUGGLE                              = 0x0105;  // old   5: Juggle
constexpr EffectId EID_BPM                                 = 0x0106;  // old   6: BPM
constexpr EffectId EID_WAVE_AMBIENT                        = 0x0107;  // old   7: Wave
constexpr EffectId EID_RIPPLE                              = 0x0108;  // old   8: Ripple
constexpr EffectId EID_HEARTBEAT                           = 0x0109;  // old   9: Heartbeat
constexpr EffectId EID_INTERFERENCE                        = 0x010A;  // old  10: Interference
constexpr EffectId EID_BREATHING                           = 0x010B;  // old  11: Breathing
constexpr EffectId EID_PULSE                               = 0x010C;  // old  12: Pulse

// --- LGP Interference (0x02xx) ---
constexpr EffectId EID_LGP_BOX_WAVE                        = 0x0200;  // old  13: LGP Box Wave
constexpr EffectId EID_LGP_HOLOGRAPHIC                     = 0x0201;  // old  14: LGP Holographic
constexpr EffectId EID_MODAL_RESONANCE                     = 0x0202;  // old  15: LGP Modal Resonance
constexpr EffectId EID_LGP_INTERFERENCE_SCANNER            = 0x0203;  // old  16: LGP Interference Scanner
constexpr EffectId EID_LGP_WAVE_COLLISION                  = 0x0204;  // old  17: LGP Wave Collision

// --- LGP Geometric (0x03xx) ---
constexpr EffectId EID_LGP_DIAMOND_LATTICE                 = 0x0300;  // old  18: LGP Diamond Lattice
constexpr EffectId EID_LGP_HEXAGONAL_GRID                  = 0x0301;  // old  19: LGP Hexagonal Grid
constexpr EffectId EID_LGP_SPIRAL_VORTEX                   = 0x0302;  // old  20: LGP Spiral Vortex
constexpr EffectId EID_LGP_SIERPINSKI                      = 0x0303;  // old  21: LGP Sierpinski
constexpr EffectId EID_CHEVRON_WAVES                       = 0x0304;  // old  22: LGP Chevron Waves
constexpr EffectId EID_LGP_CONCENTRIC_RINGS                = 0x0305;  // old  23: LGP Concentric Rings
constexpr EffectId EID_LGP_STAR_BURST                      = 0x0306;  // old  24: LGP Star Burst
constexpr EffectId EID_LGP_MESH_NETWORK                    = 0x0307;  // old  25: LGP Mesh Network

// --- LGP Advanced Optical (0x04xx) ---
constexpr EffectId EID_LGP_MOIRE_CURTAINS                  = 0x0400;  // old  26: LGP Moire Curtains
constexpr EffectId EID_LGP_RADIAL_RIPPLE                   = 0x0401;  // old  27: LGP Radial Ripple
constexpr EffectId EID_LGP_HOLOGRAPHIC_VORTEX              = 0x0402;  // old  28: LGP Holographic Vortex
constexpr EffectId EID_LGP_EVANESCENT_DRIFT                = 0x0403;  // old  29: LGP Evanescent Drift
constexpr EffectId EID_LGP_CHROMATIC_SHEAR                 = 0x0404;  // old  30: LGP Chromatic Shear
constexpr EffectId EID_LGP_MODAL_CAVITY                    = 0x0405;  // old  31: LGP Modal Cavity
constexpr EffectId EID_LGP_FRESNEL_ZONES                   = 0x0406;  // old  32: LGP Fresnel Zones
constexpr EffectId EID_LGP_PHOTONIC_CRYSTAL                = 0x0407;  // old  33: LGP Photonic Crystal

// --- LGP Organic (0x05xx) ---
constexpr EffectId EID_LGP_AURORA_BOREALIS                 = 0x0500;  // old  34: LGP Aurora Borealis
constexpr EffectId EID_LGP_BIOLUMINESCENT_WAVES            = 0x0501;  // old  35: LGP Bioluminescent Waves
constexpr EffectId EID_LGP_PLASMA_MEMBRANE                 = 0x0502;  // old  36: LGP Plasma Membrane
constexpr EffectId EID_LGP_NEURAL_NETWORK                  = 0x0503;  // old  37: LGP Neural Network
constexpr EffectId EID_LGP_CRYSTALLINE_GROWTH              = 0x0504;  // old  38: LGP Crystalline Growth
constexpr EffectId EID_LGP_FLUID_DYNAMICS                  = 0x0505;  // old  39: LGP Fluid Dynamics

// --- LGP Quantum (0x06xx) ---
constexpr EffectId EID_LGP_QUANTUM_TUNNELING               = 0x0600;  // old  40: LGP Quantum Tunneling
constexpr EffectId EID_LGP_GRAVITATIONAL_LENSING           = 0x0601;  // old  41: LGP Gravitational Lensing
constexpr EffectId EID_LGP_TIME_CRYSTAL                    = 0x0602;  // old  42: LGP Time Crystal
constexpr EffectId EID_LGP_SOLITON_WAVES                   = 0x0603;  // old  43: LGP Soliton Waves
constexpr EffectId EID_LGP_METAMATERIAL_CLOAK              = 0x0604;  // old  44: LGP Metamaterial Cloak
constexpr EffectId EID_LGP_GRIN_CLOAK                      = 0x0605;  // old  45: LGP GRIN Cloak
constexpr EffectId EID_LGP_CAUSTIC_FAN                     = 0x0606;  // old  46: LGP Caustic Fan
constexpr EffectId EID_LGP_BIREFRINGENT_SHEAR              = 0x0607;  // old  47: LGP Birefringent Shear
constexpr EffectId EID_LGP_ANISOTROPIC_CLOAK               = 0x0608;  // old  48: LGP Anisotropic Cloak
constexpr EffectId EID_LGP_EVANESCENT_SKIN                 = 0x0609;  // old  49: LGP Evanescent Skin

// --- LGP Colour Mixing (0x07xx) ---
constexpr EffectId EID_LGP_COLOR_TEMPERATURE               = 0x0700;  // old  50: LGP Color Temperature
constexpr EffectId EID_LGP_RGB_PRISM                        = 0x0701;  // old  51: LGP RGB Prism
constexpr EffectId EID_LGP_COMPLEMENTARY_MIXING            = 0x0702;  // old  52: LGP Complementary Mixing
constexpr EffectId EID_LGP_QUANTUM_COLORS                  = 0x0703;  // old  53: LGP Quantum Colors
constexpr EffectId EID_LGP_DOPPLER_SHIFT                   = 0x0704;  // old  54: LGP Doppler Shift
constexpr EffectId EID_LGP_COLOR_ACCELERATOR               = 0x0705;  // old  55: LGP Color Accelerator
constexpr EffectId EID_LGP_DNA_HELIX                        = 0x0706;  // old  56: LGP DNA Helix
constexpr EffectId EID_LGP_PHASE_TRANSITION                = 0x0707;  // old  57: LGP Phase Transition
constexpr EffectId EID_LGP_CHROMATIC_ABERRATION            = 0x0708;  // old  58: LGP Chromatic Aberration
constexpr EffectId EID_LGP_PERCEPTUAL_BLEND                = 0x0709;  // old  59: LGP Perceptual Blend

// --- LGP Novel Physics (0x08xx) ---
constexpr EffectId EID_LGP_CHLADNI_HARMONICS               = 0x0800;  // old  60: LGP Chladni Harmonics
constexpr EffectId EID_LGP_GRAVITATIONAL_WAVE_CHIRP        = 0x0801;  // old  61: LGP Gravitational Wave Chirp
constexpr EffectId EID_LGP_QUANTUM_ENTANGLEMENT            = 0x0802;  // old  62: LGP Quantum Entanglement
constexpr EffectId EID_LGP_MYCELIAL_NETWORK                = 0x0803;  // old  63: LGP Mycelial Network
constexpr EffectId EID_LGP_RILEY_DISSONANCE                = 0x0804;  // old  64: LGP Riley Dissonance

// --- LGP Chromatic (0x09xx) ---
constexpr EffectId EID_LGP_CHROMATIC_LENS                  = 0x0900;  // old  65: LGP Chromatic Lens
constexpr EffectId EID_LGP_CHROMATIC_PULSE                 = 0x0901;  // old  66: LGP Chromatic Pulse
constexpr EffectId EID_CHROMATIC_INTERFERENCE              = 0x0902;  // old  67: LGP Chromatic Interference

// --- Audio Reactive (0x0Axx) ---
constexpr EffectId EID_LGP_AUDIO_TEST                      = 0x0A00;  // old  68: Audio Test
constexpr EffectId EID_LGP_BEAT_PULSE                      = 0x0A01;  // old  69: Beat Pulse
constexpr EffectId EID_LGP_SPECTRUM_BARS                   = 0x0A02;  // old  70: Spectrum Bars
constexpr EffectId EID_LGP_BASS_BREATH                     = 0x0A03;  // old  71: Bass Breath
constexpr EffectId EID_AUDIO_WAVEFORM                      = 0x0A04;  // old  72: Audio Waveform
constexpr EffectId EID_AUDIO_BLOOM                         = 0x0A05;  // old  73: Audio Bloom
constexpr EffectId EID_LGP_STAR_BURST_NARRATIVE            = 0x0A06;  // old  74: LGP Star Burst (Narrative)
constexpr EffectId EID_LGP_CHORD_GLOW                      = 0x0A07;  // old  75: Chord Glow
constexpr EffectId EID_WAVE_REACTIVE                       = 0x0A08;  // old  76: Wave Reactive

// --- Perlin Reactive (0x0Bxx) ---
constexpr EffectId EID_LGP_PERLIN_VEIL                     = 0x0B00;  // old  77: LGP Perlin Veil
constexpr EffectId EID_LGP_PERLIN_SHOCKLINES               = 0x0B01;  // old  78: LGP Perlin Shocklines
constexpr EffectId EID_LGP_PERLIN_CAUSTICS                 = 0x0B02;  // old  79: LGP Perlin Caustics
constexpr EffectId EID_LGP_PERLIN_INTERFERENCE_WEAVE       = 0x0B03;  // old  80: LGP Perlin Interference Weave

// --- Perlin Ambient (0x0Cxx) ---
constexpr EffectId EID_LGP_PERLIN_VEIL_AMBIENT             = 0x0C00;  // old  81: LGP Perlin Veil Ambient
constexpr EffectId EID_LGP_PERLIN_SHOCKLINES_AMBIENT       = 0x0C01;  // old  82: LGP Perlin Shocklines Ambient
constexpr EffectId EID_LGP_PERLIN_CAUSTICS_AMBIENT         = 0x0C02;  // old  83: LGP Perlin Caustics Ambient
constexpr EffectId EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT = 0x0C03;  // old  84: LGP Perlin Interference Weave Ambient

// --- Perlin Backend Test (0x0Dxx) ---
constexpr EffectId EID_LGP_PERLIN_BACKEND_FAST_LED         = 0x0D00;  // old  85: Perlin Test: FastLED
constexpr EffectId EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL  = 0x0D01;  // old  86: Perlin Test: Emotiscope2 Full
constexpr EffectId EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER = 0x0D02;  // old  87: Perlin Test: Emotiscope2 Quarter

// --- Enhanced Audio-Reactive (0x0Exx) ---
constexpr EffectId EID_BPM_ENHANCED                        = 0x0E00;  // old  88: BPM Enhanced
constexpr EffectId EID_BREATHING_ENHANCED                  = 0x0E01;  // old  89: Breathing Enhanced
constexpr EffectId EID_CHEVRON_WAVES_ENHANCED              = 0x0E02;  // old  90: LGP Chevron Waves Enhanced
constexpr EffectId EID_LGP_INTERFERENCE_SCANNER_ENHANCED   = 0x0E03;  // old  91: LGP Interference Scanner Enhanced
constexpr EffectId EID_LGP_PHOTONIC_CRYSTAL_ENHANCED       = 0x0E04;  // old  92: LGP Photonic Crystal Enhanced
constexpr EffectId EID_LGP_SPECTRUM_DETAIL                 = 0x0E05;  // old  93: LGP Spectrum Detail
constexpr EffectId EID_LGP_SPECTRUM_DETAIL_ENHANCED        = 0x0E06;  // old  94: LGP Spectrum Detail Enhanced
constexpr EffectId EID_LGP_STAR_BURST_ENHANCED             = 0x0E07;  // old  95: LGP Star Burst Enhanced
constexpr EffectId EID_LGP_WAVE_COLLISION_ENHANCED         = 0x0E08;  // old  96: LGP Wave Collision Enhanced
constexpr EffectId EID_RIPPLE_ENHANCED                     = 0x0E09;  // old  97: Ripple Enhanced
constexpr EffectId EID_SNAPWAVE_LINEAR                     = 0x0E0A;  // old  98: Audio Bloom Parity

// --- Diagnostic (0x0Fxx) ---
constexpr EffectId EID_TRINITY_TEST                        = 0x0F00;  // old  99: Audio Waveform Parity

// --- Palette Auto-Cycle (0x10xx) ---
constexpr EffectId EID_LGP_HOLOGRAPHIC_AUTO_CYCLE          = 0x1000;  // old 100: LGP Holographic Auto-Cycle

// --- ES v1.1 Reference (0x11xx) ---
constexpr EffectId EID_ES_ANALOG                           = 0x1100;  // old 101: ES Analog (Ref)
constexpr EffectId EID_ES_SPECTRUM                         = 0x1101;  // old 102: ES Spectrum (Ref)
constexpr EffectId EID_ES_OCTAVE                           = 0x1102;  // old 103: ES Octave (Ref)
constexpr EffectId EID_ES_BLOOM                            = 0x1103;  // old 104: ES Bloom (Ref)
constexpr EffectId EID_ES_WAVEFORM                         = 0x1104;  // old 105: ES Waveform (Ref)

// --- ES Tuned (0x12xx) ---
constexpr EffectId EID_RIPPLE_ES_TUNED                     = 0x1200;  // old 106: Ripple (ES tuned)
constexpr EffectId EID_HEARTBEAT_ES_TUNED                  = 0x1201;  // old 107: Heartbeat (ES tuned)
constexpr EffectId EID_LGP_HOLOGRAPHIC_ES_TUNED            = 0x1202;  // old 108: LGP Holographic (ES tuned)

// --- SensoryBridge Reference (0x13xx) ---
constexpr EffectId EID_SB_WAVEFORM310                      = 0x1300;  // old 109: SB Waveform (Ref)

// --- Beat Pulse Family (0x14xx) ---
constexpr EffectId EID_BEAT_PULSE_STACK                    = 0x1400;  // old 110: Beat Pulse (Stack)
constexpr EffectId EID_BEAT_PULSE_SHOCKWAVE                = 0x1401;  // old 111: Beat Pulse (Shockwave)
constexpr EffectId EID_RETIRED_112                         = 0x1402;  // old 112: Beat Pulse (Shockwave In) // RETIRED
constexpr EffectId EID_BEAT_PULSE_VOID                     = 0x1403;  // old 113: Beat Pulse (Void)
constexpr EffectId EID_BEAT_PULSE_RESONANT                 = 0x1404;  // old 114: Beat Pulse (Resonant)
constexpr EffectId EID_BEAT_PULSE_RIPPLE                   = 0x1405;  // old 115: Beat Pulse (Ripple)
constexpr EffectId EID_BEAT_PULSE_SHOCKWAVE_CASCADE        = 0x1406;  // old 116: Beat Pulse (Shockwave Cascade)
constexpr EffectId EID_BEAT_PULSE_SPECTRAL                 = 0x1407;  // old 117: Beat Pulse (Spectral)
constexpr EffectId EID_BEAT_PULSE_SPECTRAL_PULSE           = 0x1408;  // old 118: Beat Pulse (Spectral Pulse)
constexpr EffectId EID_BEAT_PULSE_BREATHE                  = 0x1409;  // old 119: Beat Pulse (Breathe)
constexpr EffectId EID_BEAT_PULSE_LGP_INTERFERENCE         = 0x140A;  // old 120: Beat Pulse (LGP Interference)
constexpr EffectId EID_BEAT_PULSE_BLOOM                    = 0x140B;  // old 121: Beat Pulse (Bloom)

// --- Transport / Parity (0x15xx) ---
constexpr EffectId EID_BLOOM_PARITY                        = 0x1500;  // old 122: Bloom (Parity)
constexpr EffectId EID_KURAMOTO_TRANSPORT                  = 0x1501;  // old 123: Kuramoto Transport

// --- Holographic Variants (0x16xx) ---
constexpr EffectId EID_LGP_OPAL_FILM                       = 0x1600;  // old 124: LGP Opal Film
constexpr EffectId EID_LGP_GRATING_SCAN                    = 0x1601;  // old 125: LGP Grating Scan
constexpr EffectId EID_LGP_STRESS_GLASS                    = 0x1602;  // old 126: LGP Stress Glass
constexpr EffectId EID_LGP_MOIRE_SILK                      = 0x1603;  // old 127: LGP Moire Silk
constexpr EffectId EID_LGP_CAUSTIC_SHARDS                  = 0x1604;  // old 128: LGP Caustic Shards
constexpr EffectId EID_LGP_PARALLAX_DEPTH                  = 0x1605;  // old 129: LGP Parallax Depth
constexpr EffectId EID_LGP_STRESS_GLASS_MELT               = 0x1606;  // old 130: LGP Stress Glass (Melt)
constexpr EffectId EID_LGP_GRATING_SCAN_BREAKUP            = 0x1607;  // old 131: LGP Grating Scan (Breakup)
constexpr EffectId EID_LGP_WATER_CAUSTICS                  = 0x1608;  // old 132: LGP Water Caustics
constexpr EffectId EID_LGP_SCHLIEREN_FLOW                  = 0x1609;  // old 133: LGP Schlieren Flow

// --- Reaction Diffusion (0x17xx) ---
constexpr EffectId EID_LGP_REACTION_DIFFUSION              = 0x1700;  // old 134: LGP Reaction Diffusion
constexpr EffectId EID_LGP_REACTION_DIFFUSION_TRIANGLE     = 0x1701;  // old 135: LGP RD Triangle

// --- Shape Bangers Pack (0x18xx) ---
constexpr EffectId EID_LGP_TALBOT_CARPET                   = 0x1800;  // old 136: LGP Talbot Carpet
constexpr EffectId EID_LGP_AIRY_COMET                      = 0x1801;  // old 137: LGP Airy Comet
constexpr EffectId EID_LGP_MOIRE_CATHEDRAL                 = 0x1802;  // old 138: LGP Moire Cathedral
constexpr EffectId EID_LGP_SUPERFORMULA_GLYPH              = 0x1803;  // old 139: LGP Living Glyph
constexpr EffectId EID_LGP_SPIROGRAPH_CROWN                = 0x1804;  // old 140: LGP Spirograph Crown
constexpr EffectId EID_LGP_ROSE_BLOOM                      = 0x1805;  // old 141: LGP Rose Bloom
constexpr EffectId EID_LGP_HARMONOGRAPH_HALO               = 0x1806;  // old 142: LGP Harmonograph Halo
constexpr EffectId EID_LGP_RULE30_CATHEDRAL                = 0x1807;  // old 143: LGP Rule 30 Cathedral
constexpr EffectId EID_LGP_LANGTON_HIGHWAY                 = 0x1808;  // old 144: LGP Langton Highway
constexpr EffectId EID_LGP_CYMATIC_LADDER                  = 0x1809;  // old 145: LGP Cymatic Ladder
constexpr EffectId EID_LGP_MACH_DIAMONDS                   = 0x180A;  // old 146: LGP Mach Diamonds

// --- Holy Shit Bangers Pack (0x19xx) ---
constexpr EffectId EID_LGP_CHIMERA_CROWN                   = 0x1900;  // old 147: Chimera Crown
constexpr EffectId EID_LGP_CATASTROPHE_CAUSTICS            = 0x1901;  // old 148: Catastrophe Caustics
constexpr EffectId EID_LGP_HYPERBOLIC_PORTAL               = 0x1902;  // old 149: Hyperbolic Portal
constexpr EffectId EID_LGP_LORENZ_RIBBON                   = 0x1903;  // old 150: Lorenz Ribbon
constexpr EffectId EID_LGP_IFS_BIO_RELIC                    = 0x1904;  // old 151: IFS Botanical Relic

// --- Experimental Audio Pack (0x1Axx) ---
constexpr EffectId EID_LGP_FLUX_RIFT                       = 0x1A00;  // old 152: LGP Flux Rift
constexpr EffectId EID_LGP_BEAT_PRISM                      = 0x1A01;  // old 153: LGP Beat Prism
constexpr EffectId EID_LGP_HARMONIC_TIDE                   = 0x1A02;  // old 154: LGP Harmonic Tide
constexpr EffectId EID_LGP_BASS_QUAKE                      = 0x1A03;  // old 155: LGP Bass Quake
constexpr EffectId EID_LGP_TREBLE_NET                      = 0x1A04;  // old 156: LGP Treble Net
constexpr EffectId EID_LGP_RHYTHMIC_GATE                   = 0x1A05;  // old 157: LGP Rhythmic Gate
constexpr EffectId EID_LGP_SPECTRAL_KNOT                   = 0x1A06;  // old 158: LGP Spectral Knot
constexpr EffectId EID_LGP_SALIENCY_BLOOM                  = 0x1A07;  // old 159: LGP Saliency Bloom
constexpr EffectId EID_LGP_TRANSIENT_LATTICE               = 0x1A08;  // old 160: LGP Transient Lattice
constexpr EffectId EID_LGP_WAVELET_MIRROR                  = 0x1A09;  // old 161: LGP Wavelet Mirror

// --- Showpiece Pack 3 (0x1Bxx) ---
constexpr EffectId EID_LGP_TIME_REVERSAL_MIRROR            = 0x1B00;  // Time-Reversal Mirror base
constexpr EffectId EID_LGP_KDV_SOLITON_PAIR               = 0x1B01;  // KdV Soliton Pair
constexpr EffectId EID_LGP_GOLD_CODE_SPECKLE               = 0x1B02;  // Gold Code Speckle
constexpr EffectId EID_LGP_QUASICRYSTAL_LATTICE            = 0x1B03;  // Quasicrystal Lattice
constexpr EffectId EID_LGP_FRESNEL_CAUSTIC_SWEEP           = 0x1B04;  // Fresnel Caustic Sweep
constexpr EffectId EID_LGP_TIME_REVERSAL_MIRROR_AR         = 0x1B05;  // Time-Reversal Mirror (Audio-Reactive)
constexpr EffectId EID_LGP_TIME_REVERSAL_MIRROR_MOD1       = 0x1B06;  // Time-Reversal Mirror Mod1
constexpr EffectId EID_LGP_TIME_REVERSAL_MIRROR_MOD2       = 0x1B07;  // Time-Reversal Mirror Mod2
constexpr EffectId EID_LGP_TIME_REVERSAL_MIRROR_MOD3       = 0x1B08;  // Time-Reversal Mirror Mod3

// Total: 171 IDs assigned (170 active, 1 retired)

// ============================================================================
// Migration: Old Sequential ID -> New Stable ID
// ============================================================================
// Used during NVS persistence migration (version 1 -> version 2).
// Index = old uint8_t effect ID, value = new EffectId.
// Retired slots map to INVALID_EFFECT_ID.
// ============================================================================

constexpr EffectId OLD_TO_NEW[162] = {
    EID_FIRE,                                // old   0
    EID_OCEAN,                               // old   1
    EID_PLASMA,                              // old   2
    EID_CONFETTI,                            // old   3
    EID_SINELON,                             // old   4
    EID_JUGGLE,                              // old   5
    EID_BPM,                                 // old   6
    EID_WAVE_AMBIENT,                        // old   7
    EID_RIPPLE,                              // old   8
    EID_HEARTBEAT,                           // old   9
    EID_INTERFERENCE,                        // old  10
    EID_BREATHING,                           // old  11
    EID_PULSE,                               // old  12
    EID_LGP_BOX_WAVE,                        // old  13
    EID_LGP_HOLOGRAPHIC,                     // old  14
    EID_MODAL_RESONANCE,                     // old  15
    EID_LGP_INTERFERENCE_SCANNER,            // old  16
    EID_LGP_WAVE_COLLISION,                  // old  17
    EID_LGP_DIAMOND_LATTICE,                 // old  18
    EID_LGP_HEXAGONAL_GRID,                  // old  19
    EID_LGP_SPIRAL_VORTEX,                   // old  20
    EID_LGP_SIERPINSKI,                      // old  21
    EID_CHEVRON_WAVES,                       // old  22
    EID_LGP_CONCENTRIC_RINGS,                // old  23
    EID_LGP_STAR_BURST,                      // old  24
    EID_LGP_MESH_NETWORK,                    // old  25
    EID_LGP_MOIRE_CURTAINS,                  // old  26
    EID_LGP_RADIAL_RIPPLE,                   // old  27
    EID_LGP_HOLOGRAPHIC_VORTEX,              // old  28
    EID_LGP_EVANESCENT_DRIFT,                // old  29
    EID_LGP_CHROMATIC_SHEAR,                 // old  30
    EID_LGP_MODAL_CAVITY,                    // old  31
    EID_LGP_FRESNEL_ZONES,                   // old  32
    EID_LGP_PHOTONIC_CRYSTAL,                // old  33
    EID_LGP_AURORA_BOREALIS,                 // old  34
    EID_LGP_BIOLUMINESCENT_WAVES,            // old  35
    EID_LGP_PLASMA_MEMBRANE,                 // old  36
    EID_LGP_NEURAL_NETWORK,                  // old  37
    EID_LGP_CRYSTALLINE_GROWTH,              // old  38
    EID_LGP_FLUID_DYNAMICS,                  // old  39
    EID_LGP_QUANTUM_TUNNELING,               // old  40
    EID_LGP_GRAVITATIONAL_LENSING,           // old  41
    EID_LGP_TIME_CRYSTAL,                    // old  42
    EID_LGP_SOLITON_WAVES,                   // old  43
    EID_LGP_METAMATERIAL_CLOAK,              // old  44
    EID_LGP_GRIN_CLOAK,                      // old  45
    EID_LGP_CAUSTIC_FAN,                     // old  46
    EID_LGP_BIREFRINGENT_SHEAR,              // old  47
    EID_LGP_ANISOTROPIC_CLOAK,               // old  48
    EID_LGP_EVANESCENT_SKIN,                 // old  49
    EID_LGP_COLOR_TEMPERATURE,               // old  50
    EID_LGP_RGB_PRISM,                        // old  51
    EID_LGP_COMPLEMENTARY_MIXING,            // old  52
    EID_LGP_QUANTUM_COLORS,                  // old  53
    EID_LGP_DOPPLER_SHIFT,                   // old  54
    EID_LGP_COLOR_ACCELERATOR,               // old  55
    EID_LGP_DNA_HELIX,                        // old  56
    EID_LGP_PHASE_TRANSITION,                // old  57
    EID_LGP_CHROMATIC_ABERRATION,            // old  58
    EID_LGP_PERCEPTUAL_BLEND,                // old  59
    EID_LGP_CHLADNI_HARMONICS,               // old  60
    EID_LGP_GRAVITATIONAL_WAVE_CHIRP,        // old  61
    EID_LGP_QUANTUM_ENTANGLEMENT,            // old  62
    EID_LGP_MYCELIAL_NETWORK,                // old  63
    EID_LGP_RILEY_DISSONANCE,                // old  64
    EID_LGP_CHROMATIC_LENS,                  // old  65
    EID_LGP_CHROMATIC_PULSE,                 // old  66
    EID_CHROMATIC_INTERFERENCE,              // old  67
    EID_LGP_AUDIO_TEST,                      // old  68
    EID_LGP_BEAT_PULSE,                      // old  69
    EID_LGP_SPECTRUM_BARS,                   // old  70
    EID_LGP_BASS_BREATH,                     // old  71
    EID_AUDIO_WAVEFORM,                      // old  72
    EID_AUDIO_BLOOM,                         // old  73
    EID_LGP_STAR_BURST_NARRATIVE,            // old  74
    EID_LGP_CHORD_GLOW,                      // old  75
    EID_WAVE_REACTIVE,                       // old  76
    EID_LGP_PERLIN_VEIL,                     // old  77
    EID_LGP_PERLIN_SHOCKLINES,               // old  78
    EID_LGP_PERLIN_CAUSTICS,                 // old  79
    EID_LGP_PERLIN_INTERFERENCE_WEAVE,       // old  80
    EID_LGP_PERLIN_VEIL_AMBIENT,             // old  81
    EID_LGP_PERLIN_SHOCKLINES_AMBIENT,       // old  82
    EID_LGP_PERLIN_CAUSTICS_AMBIENT,         // old  83
    EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT, // old  84
    EID_LGP_PERLIN_BACKEND_FAST_LED,         // old  85
    EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL,  // old  86
    EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER, // old  87
    EID_BPM_ENHANCED,                        // old  88
    EID_BREATHING_ENHANCED,                  // old  89
    EID_CHEVRON_WAVES_ENHANCED,              // old  90
    EID_LGP_INTERFERENCE_SCANNER_ENHANCED,   // old  91
    EID_LGP_PHOTONIC_CRYSTAL_ENHANCED,       // old  92
    EID_LGP_SPECTRUM_DETAIL,                 // old  93
    EID_LGP_SPECTRUM_DETAIL_ENHANCED,        // old  94
    EID_LGP_STAR_BURST_ENHANCED,             // old  95
    EID_LGP_WAVE_COLLISION_ENHANCED,         // old  96
    EID_RIPPLE_ENHANCED,                     // old  97
    EID_SNAPWAVE_LINEAR,                     // old  98
    EID_TRINITY_TEST,                        // old  99
    EID_LGP_HOLOGRAPHIC_AUTO_CYCLE,          // old 100
    EID_ES_ANALOG,                           // old 101
    EID_ES_SPECTRUM,                         // old 102
    EID_ES_OCTAVE,                           // old 103
    EID_ES_BLOOM,                            // old 104
    EID_ES_WAVEFORM,                         // old 105
    EID_RIPPLE_ES_TUNED,                     // old 106
    EID_HEARTBEAT_ES_TUNED,                  // old 107
    EID_LGP_HOLOGRAPHIC_ES_TUNED,            // old 108
    EID_SB_WAVEFORM310,                      // old 109
    EID_BEAT_PULSE_STACK,                    // old 110
    EID_BEAT_PULSE_SHOCKWAVE,                // old 111
    INVALID_EFFECT_ID,  // old 112: RETIRED
    EID_BEAT_PULSE_VOID,                     // old 113
    EID_BEAT_PULSE_RESONANT,                 // old 114
    EID_BEAT_PULSE_RIPPLE,                   // old 115
    EID_BEAT_PULSE_SHOCKWAVE_CASCADE,        // old 116
    EID_BEAT_PULSE_SPECTRAL,                 // old 117
    EID_BEAT_PULSE_SPECTRAL_PULSE,           // old 118
    EID_BEAT_PULSE_BREATHE,                  // old 119
    EID_BEAT_PULSE_LGP_INTERFERENCE,         // old 120
    EID_BEAT_PULSE_BLOOM,                    // old 121
    EID_BLOOM_PARITY,                        // old 122
    EID_KURAMOTO_TRANSPORT,                  // old 123
    EID_LGP_OPAL_FILM,                       // old 124
    EID_LGP_GRATING_SCAN,                    // old 125
    EID_LGP_STRESS_GLASS,                    // old 126
    EID_LGP_MOIRE_SILK,                      // old 127
    EID_LGP_CAUSTIC_SHARDS,                  // old 128
    EID_LGP_PARALLAX_DEPTH,                  // old 129
    EID_LGP_STRESS_GLASS_MELT,               // old 130
    EID_LGP_GRATING_SCAN_BREAKUP,            // old 131
    EID_LGP_WATER_CAUSTICS,                  // old 132
    EID_LGP_SCHLIEREN_FLOW,                  // old 133
    EID_LGP_REACTION_DIFFUSION,              // old 134
    EID_LGP_REACTION_DIFFUSION_TRIANGLE,     // old 135
    EID_LGP_TALBOT_CARPET,                   // old 136
    EID_LGP_AIRY_COMET,                      // old 137
    EID_LGP_MOIRE_CATHEDRAL,                 // old 138
    EID_LGP_SUPERFORMULA_GLYPH,              // old 139
    EID_LGP_SPIROGRAPH_CROWN,                // old 140
    EID_LGP_ROSE_BLOOM,                      // old 141
    EID_LGP_HARMONOGRAPH_HALO,               // old 142
    EID_LGP_RULE30_CATHEDRAL,                // old 143
    EID_LGP_LANGTON_HIGHWAY,                 // old 144
    EID_LGP_CYMATIC_LADDER,                  // old 145
    EID_LGP_MACH_DIAMONDS,                   // old 146
    EID_LGP_CHIMERA_CROWN,                   // old 147
    EID_LGP_CATASTROPHE_CAUSTICS,            // old 148
    EID_LGP_HYPERBOLIC_PORTAL,               // old 149
    EID_LGP_LORENZ_RIBBON,                   // old 150
    EID_LGP_IFS_BIO_RELIC,                    // old 151
    EID_LGP_FLUX_RIFT,                       // old 152
    EID_LGP_BEAT_PRISM,                      // old 153
    EID_LGP_HARMONIC_TIDE,                   // old 154
    EID_LGP_BASS_QUAKE,                      // old 155
    EID_LGP_TREBLE_NET,                      // old 156
    EID_LGP_RHYTHMIC_GATE,                   // old 157
    EID_LGP_SPECTRAL_KNOT,                   // old 158
    EID_LGP_SALIENCY_BLOOM,                  // old 159
    EID_LGP_TRANSIENT_LATTICE,               // old 160
    EID_LGP_WAVELET_MIRROR                  // old 161
};

constexpr uint16_t OLD_ID_COUNT = sizeof(OLD_TO_NEW) / sizeof(EffectId);

// ============================================================================
// Migration Helpers
// ============================================================================

/**
 * @brief Convert old sequential uint8_t ID to new stable EffectId
 * @param oldId Old sequential effect ID (0-161)
 * @return New EffectId, or INVALID_EFFECT_ID if out of range or retired
 */
inline EffectId oldIdToNew(uint8_t oldId) {
    if (oldId >= OLD_ID_COUNT) return INVALID_EFFECT_ID;
    return OLD_TO_NEW[oldId];
}

/**
 * @brief Find old sequential index for a new EffectId (reverse lookup)
 * @param newId New stable EffectId
 * @return Old sequential index (0-161), or 0xFF if not found
 *
 * Linear scan -- only used during migration, not in hot paths.
 */
inline uint8_t newIdToOldIndex(EffectId newId) {
    for (uint16_t i = 0; i < OLD_ID_COUNT; i++) {
        if (OLD_TO_NEW[i] == newId) return static_cast<uint8_t>(i);
    }
    return 0xFF;
}

/**
 * @brief Extract family byte from an EffectId
 * @param id Effect ID
 * @return Family byte (high 8 bits)
 */
inline uint8_t effectFamily(EffectId id) {
    return static_cast<uint8_t>(id >> 8);
}

/**
 * @brief Extract sequence number from an EffectId
 * @param id Effect ID
 * @return Sequence within family (low 8 bits)
 */
inline uint8_t effectSequence(EffectId id) {
    return static_cast<uint8_t>(id & 0xFF);
}

/**
 * @brief Check if an effect belongs to a late-pack family that needs a hard silence gate.
 *
 * Effects in Shape Bangers (0x18), Holy Shit Bangers (0x19), and Experimental Audio (0x1A)
 * were designed for audio reactivity and produce visual noise in silence. The silence gate
 * fades them to black when no audio activity is detected.
 */
inline bool needsSilenceGate(EffectId id) {
    return effectFamily(id) >= FAMILY_SHAPE_BANGERS;
}

/**
 * @brief Check if an effect uses additive blending that requires tone mapping.
 *
 * Only effects that accumulate colour via += on the LED buffer can produce
 * values that clip to white.  Non-additive effects skip the tone mapper
 * entirely for sharper colour and ~3 ms/frame savings.
 *
 * Unlike needsSilenceGate() this cannot use a family-range check because
 * the additive effects span multiple families.  Explicit ID list instead.
 *
 * NOTE: When adding new effects that use additive blending (ctx.leds[i] += ...),
 * add their EID here to enable washout prevention.
 */
inline bool needsToneMap(EffectId id) {
    switch (id) {
        case EID_AUDIO_BLOOM:                   // 0x0A05 — pulse += warmBoost
        case EID_LGP_STAR_BURST_NARRATIVE:      // 0x0A06 — multi-layer += c
        case EID_LGP_SPECTRUM_DETAIL:           // 0x0E05 — additive spectrum bars
        case EID_LGP_SPECTRUM_DETAIL_ENHANCED:  // 0x0E06 — additive spectrum bars
        case EID_SNAPWAVE_LINEAR:               // 0x0E0A — pulse += fadedColor
        case EID_TRINITY_TEST:                  // 0x0F00 — additive white flash
        case EID_LGP_GRAVITATIONAL_LENSING:     // 0x0601 — particle += palette
        case EID_LGP_TIME_REVERSAL_MIRROR:      // 0x1B00 — tuned with tone map
        case EID_LGP_TIME_REVERSAL_MIRROR_AR:   // 0x1B05
        case EID_LGP_TIME_REVERSAL_MIRROR_MOD1: // 0x1B06
        case EID_LGP_TIME_REVERSAL_MIRROR_MOD2: // 0x1B07
        case EID_LGP_TIME_REVERSAL_MIRROR_MOD3: // 0x1B08
            return true;
        default:
            return false;
    }
}

} // namespace lightwaveos
