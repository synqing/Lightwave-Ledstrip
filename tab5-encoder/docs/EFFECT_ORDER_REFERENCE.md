# Effect Order Reference

Canonical reference for the effect cycling pipeline between Tab5 encoder and K1 (LightwaveOS firmware-v3).

**Last updated:** 2026-03-03
**DISPLAY_ORDER count:** 162 active effects (7 removed/commented out)
**Registered effects:** 166 (3 radial variants + LGP Spectrum Detail not in display order)
**Source of truth:** `firmware-v3/src/config/display_order.h`

---

## 1. Overview -- End-to-End Pipeline

The effect cycling pipeline works as follows:

```
Tab5 Encoder 0       WebSocket              K1 Firmware
   (rotate)     -->  nextEffect/     -->  WsEffectsCommands.cpp
                     prevEffect           handleNextEffect()
                                               |
                                     getNextDisplay(current)
                                               |
                                     DISPLAY_ORDER[] lookup
                                               |
                                     setEffect(newId)
                                               |
                                     broadcastStatus()  -->  Tab5 receives
                                                             effect.changed
```

1. **Physical encoder** on Tab5 produces delta (positive = clockwise, negative = anticlockwise).
2. **Tab5 main.cpp** calculates delta from raw encoder value, caps at 8 steps, sends `nextEffect` (clockwise) or `prevEffect` (anticlockwise) N times via WebSocket.
3. **K1 WebSocket handler** (`WsEffectsCommands.cpp`) calls `getNextDisplay()` or `getPrevDisplay()` using the current EffectId.
4. **Navigation helpers** (`display_order.h`) perform a linear scan of the `DISPLAY_ORDER[]` array to find the current effect's index, then return the next/previous entry with wrap-around.
5. **K1 sets the new effect** and broadcasts a status update so all connected clients (Tab5, iOS app) sync.

---

## 2. DISPLAY_ORDER Canonical Sequence

The `DISPLAY_ORDER[]` array in `firmware-v3/src/config/display_order.h` defines the exact cycling order. Effects not in this array still exist and can be set directly by ID, but they are skipped when cycling via encoder.

### ID Format

Effect IDs use a `uint16_t` with structure `[FAMILY:8][SEQUENCE:8]` (e.g., `0x0100` = Core family, sequence 0 = Fire).

| Idx | Effect ID | Hex    | Name                                  | Family                    | Audio |
|-----|-----------|--------|---------------------------------------|---------------------------|-------|
|   0 | EID_FIRE                          | 0x0100 | Fire                                  | Core / Classic            | No    |
|   1 | EID_OCEAN                         | 0x0101 | Ocean                                 | Core / Classic            | No    |
|   2 | EID_PLASMA                        | 0x0102 | Plasma                                | Core / Classic            | No    |
|   3 | EID_CONFETTI                      | 0x0103 | Confetti                              | Core / Classic            | No    |
|   4 | EID_SINELON                       | 0x0104 | Sinelon                               | Core / Classic            | No    |
|   5 | EID_JUGGLE                        | 0x0105 | Juggle                                | Core / Classic            | Yes   |
|   6 | EID_BPM                           | 0x0106 | BPM                                   | Core / Classic            | Yes   |
|   7 | EID_WAVE_AMBIENT                  | 0x0107 | Wave                                  | Core / Classic            | Yes   |
|   8 | EID_RIPPLE                        | 0x0108 | Ripple                                | Core / Classic            | Yes   |
|   9 | EID_HEARTBEAT                     | 0x0109 | Heartbeat                             | Core / Classic            | Yes   |
|  10 | EID_INTERFERENCE                  | 0x010A | Interference                          | Core / Classic            | No    |
|  11 | EID_BREATHING                     | 0x010B | Breathing                             | Core / Classic            | Yes   |
|  12 | EID_PULSE                         | 0x010C | Pulse                                 | Core / Classic            | No    |
|  13 | EID_LGP_BOX_WAVE                  | 0x0200 | LGP Box Wave                          | LGP Interference          | No    |
|  14 | EID_LGP_HOLOGRAPHIC               | 0x0201 | LGP Holographic                       | LGP Interference          | No    |
|  15 | EID_MODAL_RESONANCE               | 0x0202 | LGP Modal Resonance                   | LGP Interference          | No    |
|  16 | EID_LGP_INTERFERENCE_SCANNER      | 0x0203 | LGP Interference Scanner              | LGP Interference          | Yes   |
|  17 | EID_LGP_WAVE_COLLISION            | 0x0204 | LGP Wave Collision                    | LGP Interference          | Yes   |
|  18 | EID_LGP_DIAMOND_LATTICE           | 0x0300 | LGP Diamond Lattice                   | LGP Geometric             | No    |
|  19 | EID_LGP_HEXAGONAL_GRID            | 0x0301 | LGP Hexagonal Grid                    | LGP Geometric             | No    |
|  20 | EID_LGP_SPIRAL_VORTEX             | 0x0302 | LGP Spiral Vortex                     | LGP Geometric             | No    |
|  21 | EID_LGP_SIERPINSKI                | 0x0303 | LGP Sierpinski                        | LGP Geometric             | No    |
|  22 | EID_CHEVRON_WAVES                 | 0x0304 | LGP Chevron Waves                     | LGP Geometric             | Yes   |
|  23 | EID_LGP_CONCENTRIC_RINGS          | 0x0305 | LGP Concentric Rings                  | LGP Geometric             | No    |
|  24 | EID_LGP_STAR_BURST                | 0x0306 | LGP Star Burst                        | LGP Geometric             | Yes   |
|  25 | EID_LGP_MESH_NETWORK              | 0x0307 | LGP Mesh Network                      | LGP Geometric             | No    |
|  26 | EID_LGP_MOIRE_CURTAINS            | 0x0400 | LGP Moire Curtains                    | LGP Advanced Optical      | No    |
|  27 | EID_LGP_RADIAL_RIPPLE             | 0x0401 | LGP Radial Ripple                     | LGP Advanced Optical      | No    |
|  28 | EID_LGP_HOLOGRAPHIC_VORTEX        | 0x0402 | LGP Holographic Vortex                | LGP Advanced Optical      | No    |
|  29 | EID_LGP_EVANESCENT_DRIFT          | 0x0403 | LGP Evanescent Drift                  | LGP Advanced Optical      | No    |
|  30 | EID_LGP_CHROMATIC_SHEAR           | 0x0404 | LGP Chromatic Shear                   | LGP Advanced Optical      | No    |
|  31 | EID_LGP_MODAL_CAVITY              | 0x0405 | LGP Modal Cavity                      | LGP Advanced Optical      | No    |
|  32 | EID_LGP_FRESNEL_ZONES             | 0x0406 | LGP Fresnel Zones                     | LGP Advanced Optical      | No    |
|  33 | EID_LGP_PHOTONIC_CRYSTAL          | 0x0407 | LGP Photonic Crystal                  | LGP Advanced Optical      | Yes   |
|  34 | EID_LGP_AURORA_BOREALIS           | 0x0500 | LGP Aurora Borealis                   | LGP Organic               | No    |
|  35 | EID_LGP_BIOLUMINESCENT_WAVES      | 0x0501 | LGP Bioluminescent Waves              | LGP Organic               | No    |
|  36 | EID_LGP_PLASMA_MEMBRANE           | 0x0502 | LGP Plasma Membrane                   | LGP Organic               | No    |
|  37 | EID_LGP_NEURAL_NETWORK            | 0x0503 | LGP Neural Network                    | LGP Organic               | No    |
|  38 | EID_LGP_CRYSTALLINE_GROWTH        | 0x0504 | LGP Crystalline Growth                | LGP Organic               | No    |
|  39 | EID_LGP_FLUID_DYNAMICS            | 0x0505 | LGP Fluid Dynamics                    | LGP Organic               | No    |
|  40 | EID_LGP_QUANTUM_TUNNELING         | 0x0600 | LGP Quantum Tunneling                 | LGP Quantum               | No    |
|  41 | EID_LGP_GRAVITATIONAL_LENSING     | 0x0601 | LGP Gravitational Lensing             | LGP Quantum               | No    |
|  42 | EID_LGP_TIME_CRYSTAL              | 0x0602 | LGP Time Crystal                      | LGP Quantum               | No    |
|  43 | EID_LGP_SOLITON_WAVES             | 0x0603 | LGP Soliton Waves                     | LGP Quantum               | No    |
|  44 | EID_LGP_METAMATERIAL_CLOAK        | 0x0604 | LGP Metamaterial Cloak                | LGP Quantum               | No    |
|  45 | EID_LGP_GRIN_CLOAK                | 0x0605 | LGP GRIN Cloak                        | LGP Quantum               | No    |
|  46 | EID_LGP_CAUSTIC_FAN               | 0x0606 | LGP Caustic Fan                       | LGP Quantum               | No    |
|  47 | EID_LGP_BIREFRINGENT_SHEAR        | 0x0607 | LGP Birefringent Shear                | LGP Quantum               | No    |
|  48 | EID_LGP_ANISOTROPIC_CLOAK         | 0x0608 | LGP Anisotropic Cloak                 | LGP Quantum               | No    |
|  49 | EID_LGP_EVANESCENT_SKIN           | 0x0609 | LGP Evanescent Skin                   | LGP Quantum               | No    |
|  50 | EID_LGP_COLOR_TEMPERATURE         | 0x0700 | LGP Colour Temperature                | LGP Colour Mixing         | No    |
|  51 | EID_LGP_RGB_PRISM                 | 0x0701 | LGP RGB Prism                         | LGP Colour Mixing         | No    |
|  52 | EID_LGP_COMPLEMENTARY_MIXING      | 0x0702 | LGP Complementary Mixing              | LGP Colour Mixing         | No    |
|  53 | EID_LGP_QUANTUM_COLORS            | 0x0703 | LGP Quantum Colours                   | LGP Colour Mixing         | No    |
|  54 | EID_LGP_DOPPLER_SHIFT             | 0x0704 | LGP Doppler Shift                     | LGP Colour Mixing         | No    |
|  55 | EID_LGP_COLOR_ACCELERATOR         | 0x0705 | LGP Colour Accelerator                | LGP Colour Mixing         | No    |
|  56 | EID_LGP_DNA_HELIX                 | 0x0706 | LGP DNA Helix                         | LGP Colour Mixing         | No    |
|  57 | EID_LGP_PHASE_TRANSITION          | 0x0707 | LGP Phase Transition                  | LGP Colour Mixing         | No    |
|  58 | EID_LGP_CHROMATIC_ABERRATION      | 0x0708 | LGP Chromatic Aberration              | LGP Colour Mixing         | No    |
|  59 | EID_LGP_PERCEPTUAL_BLEND          | 0x0709 | LGP Perceptual Blend                  | LGP Colour Mixing         | No    |
|  60 | EID_LGP_CHLADNI_HARMONICS         | 0x0800 | LGP Chladni Harmonics                 | LGP Novel Physics         | No    |
|  61 | EID_LGP_GRAVITATIONAL_WAVE_CHIRP  | 0x0801 | LGP Gravitational Wave Chirp          | LGP Novel Physics         | No    |
|  62 | EID_LGP_QUANTUM_ENTANGLEMENT      | 0x0802 | LGP Quantum Entanglement              | LGP Novel Physics         | No    |
|  63 | EID_LGP_MYCELIAL_NETWORK          | 0x0803 | LGP Mycelial Network                  | LGP Novel Physics         | No    |
|  64 | EID_LGP_RILEY_DISSONANCE          | 0x0804 | LGP Riley Dissonance                  | LGP Novel Physics         | No    |
|  65 | EID_LGP_CHROMATIC_LENS            | 0x0900 | LGP Chromatic Lens                    | LGP Chromatic             | No    |
|  66 | EID_LGP_CHROMATIC_PULSE           | 0x0901 | LGP Chromatic Pulse                   | LGP Chromatic             | No    |
|  67 | EID_CHROMATIC_INTERFERENCE         | 0x0902 | LGP Chromatic Interference            | LGP Chromatic             | No    |
|  68 | EID_LGP_AUDIO_TEST                | 0x0A00 | Audio Test                            | Audio Reactive            | Yes   |
|  69 | EID_LGP_BEAT_PULSE                | 0x0A01 | Beat Pulse                            | Audio Reactive            | Yes   |
|  70 | EID_LGP_SPECTRUM_BARS             | 0x0A02 | Spectrum Bars                         | Audio Reactive            | Yes   |
|  71 | EID_LGP_BASS_BREATH               | 0x0A03 | Bass Breath                           | Audio Reactive            | Yes   |
|  72 | EID_AUDIO_WAVEFORM                | 0x0A04 | Audio Waveform                        | Audio Reactive            | Yes   |
|  73 | EID_AUDIO_BLOOM                   | 0x0A05 | Audio Bloom                           | Audio Reactive            | Yes   |
|  74 | EID_LGP_STAR_BURST_NARRATIVE      | 0x0A06 | LGP Star Burst (Narrative)            | Audio Reactive            | Yes   |
|  75 | EID_LGP_CHORD_GLOW                | 0x0A07 | Chord Glow                            | Audio Reactive            | Yes   |
|  76 | EID_WAVE_REACTIVE                 | 0x0A08 | Wave Reactive                         | Audio Reactive            | Yes   |
|  77 | EID_LGP_PERLIN_VEIL               | 0x0B00 | LGP Perlin Veil                       | Perlin Reactive           | Yes   |
|  78 | EID_LGP_PERLIN_SHOCKLINES         | 0x0B01 | LGP Perlin Shocklines                 | Perlin Reactive           | Yes   |
|  79 | EID_LGP_PERLIN_CAUSTICS           | 0x0B02 | LGP Perlin Caustics                   | Perlin Reactive           | Yes   |
|  80 | EID_LGP_PERLIN_INTERFERENCE_WEAVE | 0x0B03 | LGP Perlin Interference Weave         | Perlin Reactive           | Yes   |
|  81 | EID_LGP_PERLIN_VEIL_AMBIENT       | 0x0C00 | LGP Perlin Veil Ambient               | Perlin Ambient            | No    |
|  82 | EID_LGP_PERLIN_SHOCKLINES_AMBIENT | 0x0C01 | LGP Perlin Shocklines Ambient         | Perlin Ambient            | No    |
|  83 | EID_LGP_PERLIN_CAUSTICS_AMBIENT   | 0x0C02 | LGP Perlin Caustics Ambient           | Perlin Ambient            | No    |
|  84 | EID_LGP_PERLIN_INTERFERENCE_WEAVE_AMBIENT | 0x0C03 | LGP Perlin IW Ambient         | Perlin Ambient            | No    |
|  85 | EID_LGP_PERLIN_BACKEND_FAST_LED   | 0x0D00 | Perlin Test: FastLED                  | Perlin Backend Test       | No    |
|  86 | EID_LGP_PERLIN_BACKEND_EMOTISCOPE_FULL | 0x0D01 | Perlin Test: Emotiscope2 Full    | Perlin Backend Test       | No    |
|  87 | EID_LGP_PERLIN_BACKEND_EMOTISCOPE_QUARTER | 0x0D02 | Perlin Test: Emotiscope2 Quarter | Perlin Backend Test    | No    |
|  88 | EID_BPM_ENHANCED                  | 0x0E00 | BPM Enhanced                          | Enhanced Audio-Reactive   | Yes   |
|  89 | EID_BREATHING_ENHANCED            | 0x0E01 | Breathing Enhanced                    | Enhanced Audio-Reactive   | Yes   |
|  90 | EID_CHEVRON_WAVES_ENHANCED        | 0x0E02 | LGP Chevron Waves Enhanced            | Enhanced Audio-Reactive   | Yes   |
|  91 | EID_LGP_INTERFERENCE_SCANNER_ENHANCED | 0x0E03 | LGP Interference Scanner Enhanced | Enhanced Audio-Reactive | Yes   |
|  92 | EID_LGP_PHOTONIC_CRYSTAL_ENHANCED | 0x0E04 | LGP Photonic Crystal Enhanced         | Enhanced Audio-Reactive   | Yes   |
|  93 | ~~EID_LGP_SPECTRUM_DETAIL~~       | ~~0x0E05~~ | ~~LGP Spectrum Detail~~            | ~~Enhanced Audio-Reactive~~ | ~~Yes~~ |
|  94 | EID_LGP_SPECTRUM_DETAIL_ENHANCED  | 0x0E06 | LGP Spectrum Detail Enhanced          | Enhanced Audio-Reactive   | Yes   |
|  95 | EID_LGP_STAR_BURST_ENHANCED       | 0x0E07 | LGP Star Burst Enhanced               | Enhanced Audio-Reactive   | Yes   |
|  96 | EID_LGP_WAVE_COLLISION_ENHANCED   | 0x0E08 | LGP Wave Collision Enhanced           | Enhanced Audio-Reactive   | Yes   |
|  97 | EID_RIPPLE_ENHANCED               | 0x0E09 | Ripple Enhanced                       | Enhanced Audio-Reactive   | Yes   |
|  98 | EID_SNAPWAVE_LINEAR               | 0x0E0A | Audio Bloom Parity                    | Enhanced Audio-Reactive   | Yes   |
|  99 | ~~EID_TRINITY_TEST~~              | ~~0x0F00~~ | ~~Audio Waveform Parity~~          | ~~Diagnostic~~            | ~~Yes~~ |
| 100 | EID_LGP_HOLOGRAPHIC_AUTO_CYCLE    | 0x1000 | LGP Holographic Auto-Cycle            | Palette Auto-Cycle        | Yes   |
| 101 | ~~EID_ES_ANALOG~~                 | ~~0x1100~~ | ~~ES Analog (Ref)~~                | ~~ES v1.1 Reference~~     | ~~Yes~~ |
| 102 | ~~EID_ES_SPECTRUM~~               | ~~0x1101~~ | ~~ES Spectrum (Ref)~~              | ~~ES v1.1 Reference~~     | ~~Yes~~ |
| 103 | ~~EID_ES_OCTAVE~~                 | ~~0x1102~~ | ~~ES Octave (Ref)~~                | ~~ES v1.1 Reference~~     | ~~Yes~~ |
| 104 | ~~EID_ES_BLOOM~~                  | ~~0x1103~~ | ~~ES Bloom (Ref)~~                 | ~~ES v1.1 Reference~~     | ~~Yes~~ |
| 105 | ~~EID_ES_WAVEFORM~~               | ~~0x1104~~ | ~~ES Waveform (Ref)~~              | ~~ES v1.1 Reference~~     | ~~Yes~~ |
| 106 | EID_RIPPLE_ES_TUNED               | 0x1200 | Ripple (ES tuned)                     | ES Tuned                  | Yes   |
| 107 | EID_HEARTBEAT_ES_TUNED            | 0x1201 | Heartbeat (ES tuned)                  | ES Tuned                  | Yes   |
| 108 | EID_LGP_HOLOGRAPHIC_ES_TUNED      | 0x1202 | LGP Holographic (ES tuned)            | ES Tuned                  | Yes   |
| 109 | EID_SB_WAVEFORM310                | 0x1300 | SB Waveform (Ref)                     | SensoryBridge Reference   | Yes   |
| 110 | EID_BEAT_PULSE_STACK              | 0x1400 | Beat Pulse (Stack)                    | Beat Pulse Family         | Yes   |
| 111 | EID_BEAT_PULSE_SHOCKWAVE          | 0x1401 | Beat Pulse (Shockwave)                | Beat Pulse Family         | Yes   |
| 112 | EID_BEAT_PULSE_VOID               | 0x1403 | Beat Pulse (Void)                     | Beat Pulse Family         | Yes   |
| 113 | EID_BEAT_PULSE_RESONANT           | 0x1404 | Beat Pulse (Resonant)                 | Beat Pulse Family         | Yes   |
| 114 | EID_BEAT_PULSE_RIPPLE             | 0x1405 | Beat Pulse (Ripple)                   | Beat Pulse Family         | Yes   |
| 115 | EID_BEAT_PULSE_SHOCKWAVE_CASCADE  | 0x1406 | Beat Pulse (Shockwave Cascade)        | Beat Pulse Family         | Yes   |
| 116 | EID_BEAT_PULSE_SPECTRAL           | 0x1407 | Beat Pulse (Spectral)                 | Beat Pulse Family         | Yes   |
| 117 | EID_BEAT_PULSE_SPECTRAL_PULSE     | 0x1408 | Beat Pulse (Spectral Pulse)           | Beat Pulse Family         | Yes   |
| 118 | EID_BEAT_PULSE_BREATHE            | 0x1409 | Beat Pulse (Breathe)                  | Beat Pulse Family         | Yes   |
| 119 | EID_BEAT_PULSE_LGP_INTERFERENCE   | 0x140A | Beat Pulse (LGP Interference)         | Beat Pulse Family         | Yes   |
| 120 | EID_BEAT_PULSE_BLOOM              | 0x140B | Beat Pulse (Bloom)                    | Beat Pulse Family         | Yes   |
| 121 | EID_BLOOM_PARITY                  | 0x1500 | Bloom (Parity)                        | Transport / Parity        | Yes   |
| 122 | EID_KURAMOTO_TRANSPORT            | 0x1501 | Kuramoto Transport                    | Transport / Parity        | Yes   |
| 123 | EID_LGP_OPAL_FILM                | 0x1600 | LGP Opal Film                         | Holographic Variants      | No    |
| 124 | EID_LGP_GRATING_SCAN             | 0x1601 | LGP Grating Scan                      | Holographic Variants      | No    |
| 125 | EID_LGP_STRESS_GLASS             | 0x1602 | LGP Stress Glass                      | Holographic Variants      | No    |
| 126 | EID_LGP_MOIRE_SILK               | 0x1603 | LGP Moire Silk                        | Holographic Variants      | No    |
| 127 | EID_LGP_CAUSTIC_SHARDS           | 0x1604 | LGP Caustic Shards                    | Holographic Variants      | No    |
| 128 | EID_LGP_PARALLAX_DEPTH           | 0x1605 | LGP Parallax Depth                    | Holographic Variants      | No    |
| 129 | EID_LGP_STRESS_GLASS_MELT        | 0x1606 | LGP Stress Glass (Melt)               | Holographic Variants      | No    |
| 130 | EID_LGP_GRATING_SCAN_BREAKUP     | 0x1607 | LGP Grating Scan (Breakup)            | Holographic Variants      | No    |
| 131 | EID_LGP_WATER_CAUSTICS           | 0x1608 | LGP Water Caustics                    | Holographic Variants      | No    |
| 132 | EID_LGP_SCHLIEREN_FLOW           | 0x1609 | LGP Schlieren Flow                    | Holographic Variants      | No    |
| 133 | EID_LGP_REACTION_DIFFUSION       | 0x1700 | LGP Reaction Diffusion                | Reaction Diffusion        | No    |
| 134 | EID_LGP_REACTION_DIFFUSION_TRIANGLE | 0x1701 | LGP RD Triangle                    | Reaction Diffusion        | No    |
| 135 | EID_LGP_TALBOT_CARPET            | 0x1800 | LGP Talbot Carpet                     | Shape Bangers Pack        | No    |
| 136 | EID_LGP_AIRY_COMET               | 0x1801 | LGP Airy Comet                        | Shape Bangers Pack        | No    |
| 137 | EID_LGP_MOIRE_CATHEDRAL          | 0x1802 | LGP Moire Cathedral                   | Shape Bangers Pack        | No    |
| 138 | EID_LGP_SUPERFORMULA_GLYPH       | 0x1803 | LGP Living Glyph                      | Shape Bangers Pack        | No    |
| 139 | EID_LGP_SPIROGRAPH_CROWN         | 0x1804 | LGP Spirograph Crown                  | Shape Bangers Pack        | No    |
| 140 | EID_LGP_ROSE_BLOOM               | 0x1805 | LGP Rose Bloom                        | Shape Bangers Pack        | No    |
| 141 | EID_LGP_HARMONOGRAPH_HALO        | 0x1806 | LGP Harmonograph Halo                 | Shape Bangers Pack        | No    |
| 142 | EID_LGP_RULE30_CATHEDRAL         | 0x1807 | LGP Rule 30 Cathedral                 | Shape Bangers Pack        | No    |
| 143 | EID_LGP_LANGTON_HIGHWAY          | 0x1808 | LGP Langton Highway                   | Shape Bangers Pack        | No    |
| 144 | EID_LGP_CYMATIC_LADDER           | 0x1809 | LGP Cymatic Ladder                    | Shape Bangers Pack        | No    |
| 145 | EID_LGP_MACH_DIAMONDS            | 0x180A | LGP Mach Diamonds                     | Shape Bangers Pack        | No    |
| 146 | EID_LGP_CHIMERA_CROWN            | 0x1900 | Chimera Crown                         | Holy Shit Bangers Pack    | No    |
| 147 | EID_LGP_CATASTROPHE_CAUSTICS     | 0x1901 | Catastrophe Caustics                  | Holy Shit Bangers Pack    | No    |
| 148 | EID_LGP_HYPERBOLIC_PORTAL        | 0x1902 | Hyperbolic Portal                     | Holy Shit Bangers Pack    | No    |
| 149 | EID_LGP_LORENZ_RIBBON            | 0x1903 | Lorenz Ribbon                         | Holy Shit Bangers Pack    | No    |
| 150 | EID_LGP_IFS_BIO_RELIC            | 0x1904 | IFS Botanical Relic                   | Holy Shit Bangers Pack    | No    |
| 151 | EID_LGP_FLUX_RIFT                | 0x1A00 | LGP Flux Rift                         | Experimental Audio Pack   | Yes   |
| 152 | EID_LGP_BEAT_PRISM               | 0x1A01 | LGP Beat Prism                        | Experimental Audio Pack   | Yes   |
| 153 | EID_LGP_HARMONIC_TIDE            | 0x1A02 | LGP Harmonic Tide                     | Experimental Audio Pack   | Yes   |
| 154 | EID_LGP_BASS_QUAKE               | 0x1A03 | LGP Bass Quake                        | Experimental Audio Pack   | Yes   |
| 155 | EID_LGP_TREBLE_NET               | 0x1A04 | LGP Treble Net                        | Experimental Audio Pack   | Yes   |
| 156 | EID_LGP_RHYTHMIC_GATE            | 0x1A05 | LGP Rhythmic Gate                     | Experimental Audio Pack   | Yes   |
| 157 | EID_LGP_SPECTRAL_KNOT            | 0x1A06 | LGP Spectral Knot                     | Experimental Audio Pack   | Yes   |
| 158 | EID_LGP_SALIENCY_BLOOM           | 0x1A07 | LGP Saliency Bloom                    | Experimental Audio Pack   | Yes   |
| 159 | EID_LGP_TRANSIENT_LATTICE        | 0x1A08 | LGP Transient Lattice                 | Experimental Audio Pack   | Yes   |
| 160 | EID_LGP_WAVELET_MIRROR           | 0x1A09 | LGP Wavelet Mirror                    | Experimental Audio Pack   | Yes   |
| 161 | EID_LGP_TIME_REVERSAL_MIRROR     | 0x1B00 | Time-Reversal Mirror                  | Showpiece Pack 3          | No    |
| 162 | EID_LGP_KDV_SOLITON_PAIR         | 0x1B01 | KdV Soliton Pair                      | Showpiece Pack 3          | No    |
| 163 | EID_LGP_GOLD_CODE_SPECKLE        | 0x1B02 | Gold Code Speckle                     | Showpiece Pack 3          | No    |
| 164 | EID_LGP_QUASICRYSTAL_LATTICE     | 0x1B03 | Quasicrystal Lattice                  | Showpiece Pack 3          | No    |
| 165 | EID_LGP_FRESNEL_CAUSTIC_SWEEP    | 0x1B04 | Fresnel Caustic Sweep                 | Showpiece Pack 3          | No    |
| 166 | EID_LGP_TIME_REVERSAL_MIRROR_AR  | 0x1B05 | Time-Reversal Mirror AR               | Showpiece Pack 3          | Yes   |
| 167 | EID_LGP_TIME_REVERSAL_MIRROR_MOD1| 0x1B06 | Time-Reversal Mirror Mod1             | Showpiece Pack 3          | No    |
| 168 | EID_LGP_TIME_REVERSAL_MIRROR_MOD2| 0x1B07 | Time-Reversal Mirror Mod2             | Showpiece Pack 3          | No    |
| 169 | EID_LGP_TIME_REVERSAL_MIRROR_MOD3| 0x1B08 | Time-Reversal Mirror Mod3             | Showpiece Pack 3          | No    |

**Note:** Indices 93, 99, 101-105 are struck through -- these entries are commented out in `display_order.h` and skipped during cycling. The actual array has 162 contiguous entries.

---

## 3. How nextEffect / prevEffect Works

### K1 Algorithm (display_order.h)

```cpp
// getNextDisplay(current):
//   1. Linear scan DISPLAY_ORDER[] to find index of current EffectId
//   2. If not found, return DISPLAY_ORDER[0] (reset to first)
//   3. Return DISPLAY_ORDER[(index + 1) % DISPLAY_COUNT]

// getPrevDisplay(current):
//   1. Linear scan DISPLAY_ORDER[] to find index of current EffectId
//   2. If not found, return DISPLAY_ORDER[DISPLAY_COUNT - 1] (jump to last)
//   3. Return DISPLAY_ORDER[(index + DISPLAY_COUNT - 1) % DISPLAY_COUNT]
```

**Key behaviours:**
- **Wrap-around:** Cycling past the last effect (index 161) wraps to the first (index 0), and vice versa.
- **Unknown current effect:** If the current effect is not in `DISPLAY_ORDER` (e.g., a radial variant or a retired effect set directly by ID), `nextEffect` resets to the first effect (Fire), and `prevEffect` resets to the last (Time-Reversal Mirror Mod3).
- **DISPLAY_COUNT** is computed at compile time: `sizeof(DISPLAY_ORDER) / sizeof(EffectId)` = **162**.

### K1 WebSocket Handler (WsEffectsCommands.cpp)

```cpp
static void handleNextEffect(AsyncWebSocketClient* client, JsonDocument& doc,
                              const WebServerContext& ctx) {
    EffectId current = ctx.renderer->getCurrentEffect();
    EffectId next = lightwaveos::getNextDisplay(current);
    ctx.actorSystem.setEffect(next);
    if (ctx.broadcastStatus) ctx.broadcastStatus();
}
```

The handler is stateless -- it reads the current effect from the renderer, computes the next, sets it, and broadcasts. No delta accumulation occurs on the K1 side.

---

## 4. Tab5 Encoder 0 Behaviour

### Delta Calculation (main.cpp, line ~1127)

```cpp
case 0: {
    // Calculate delta from raw encoder value (0-255 range)
    int32_t delta = (int32_t)value - (int32_t)s_prevEncoderValues[0];

    // Handle 8-bit wrap-around
    if (delta > 128) delta -= 256;
    else if (delta < -128) delta += 256;

    bool forward = (delta > 0);
    int32_t steps = forward ? delta : -delta;
    if (steps > 8) steps = 8;  // Cap to prevent WS flooding

    for (int32_t i = 0; i < steps; i++) {
        if (forward) g_wsClient.sendNextEffect();
        else         g_wsClient.sendPrevEffect();
    }
    break;
}
```

**Key behaviours:**
- The encoder produces raw 8-bit values (0-255). Delta is calculated from the previous value with wrap-around correction.
- **Step cap:** Maximum 8 steps per encoder update to prevent WebSocket flooding.
- **No rate limiting on nextEffect/prevEffect:** Unlike other parameters (brightness, speed, palette) which use `canSend()` throttling, effect cycling sends individual `nextEffect`/`prevEffect` messages for each step. This means rapid rotation can queue up to 8 messages per encoder poll.
- **No local display order knowledge:** Tab5 does NOT have a copy of `DISPLAY_ORDER[]`. It blindly sends next/prev commands and waits for K1's `effect.changed` broadcast to learn the new effect name/ID.

### WebSocket Messages Sent

```json
// Clockwise rotation (forward):
{"type": "nextEffect"}

// Anticlockwise rotation (backward):
{"type": "prevEffect"}
```

These messages carry no payload -- the K1 handles all state.

---

## 5. Registry vs Display Order

There are two distinct orderings:

### Registry Order (REST API / `effects.list`)

The `RendererActor::m_registry[]` array holds effects in the order they were registered in `CoreEffects.cpp::registerAllEffects()`. This is what the REST API endpoint `/api/v1/effects` returns. It includes **all 166 registered effects**, including effects not in the display cycling order.

- Accessed by: REST API (`GET /api/v1/effects`), WebSocket `effects.list` command
- Order: Registration order (follows `CoreEffects.cpp` sequence)
- Count: 166 (as of 2026-03-03)

### Display Order (Cycling)

The `DISPLAY_ORDER[]` constexpr array in `display_order.h` defines the cycling order for encoder/UI navigation. It is a curated subset of the registry.

- Accessed by: `nextEffect`/`prevEffect`, serial cycling, any UI cycling
- Order: Hand-curated in `display_order.h`
- Count: 162

### Effects in Registry but NOT in Display Order

| Effect ID | Hex    | Name                          | Reason Not in Display Order |
|-----------|--------|-------------------------------|-----------------------------|
| EID_LGP_SPECTRUM_DETAIL        | 0x0E05 | LGP Spectrum Detail           | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_TRINITY_TEST               | 0x0F00 | Audio Waveform Parity         | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_ES_ANALOG                  | 0x1100 | ES Analog (Ref)               | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_ES_SPECTRUM                | 0x1101 | ES Spectrum (Ref)             | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_ES_OCTAVE                  | 0x1102 | ES Octave (Ref)               | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_ES_BLOOM                   | 0x1103 | ES Bloom (Ref)                | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_ES_WAVEFORM                | 0x1104 | ES Waveform (Ref)             | Dead/broken, removed 2026-03-02 (also de-registered) |
| EID_LGP_BIREFRINGENT_SHEAR_RADIAL | 0x060A | Birefringent Shear (Radial) | New radial variant, not yet added to display order |
| EID_LGP_SOLITON_WAVES_RADIAL     | 0x060B | Soliton Waves (Radial)     | New radial variant, not yet added to display order |
| EID_LGP_NEURAL_NETWORK_RADIAL    | 0x0506 | Neural Network (Radial)    | New radial variant, not yet added to display order |
| EID_RETIRED_112                   | 0x1402 | Beat Pulse (Shockwave In)  | Retired effect, ID preserved but never registered |

**Note:** The 7 dead/broken effects are both commented out of `DISPLAY_ORDER` AND de-registered from `CoreEffects.cpp` (registration code commented out). They exist only as ID constants in `effect_ids.h` to prevent ID reuse. The 3 radial variants are fully registered and renderable but deliberately excluded from cycling -- they can only be activated by direct ID set.

---

## 6. Retired / Removed Effects

### Commented Out of DISPLAY_ORDER (2026-03-02)

| Display Index | Effect ID | Hex    | Name                     | Reason |
|---------------|-----------|--------|--------------------------|--------|
| 93            | EID_LGP_SPECTRUM_DETAIL  | 0x0E05 | LGP Spectrum Detail      | Dead/broken -- uses raw `bins64[]` Goertzel API, not ported to PipelineCore |
| 99            | EID_TRINITY_TEST         | 0x0F00 | Audio Waveform Parity    | Dead/broken -- Trinity data flow diagnostic, no longer functional |
| 101           | EID_ES_ANALOG            | 0x1100 | ES Analog (Ref)          | Dead/broken -- ESv11 reference, OOB zone access caused RMT crash |
| 102           | EID_ES_SPECTRUM          | 0x1101 | ES Spectrum (Ref)        | Dead/broken -- ESv11 reference, requires Goertzel backend |
| 103           | EID_ES_OCTAVE            | 0x1102 | ES Octave (Ref)          | Dead/broken -- ESv11 reference, OOB zone access |
| 104           | EID_ES_BLOOM             | 0x1103 | ES Bloom (Ref)           | Dead/broken -- ESv11 reference, requires Goertzel backend |
| 105           | EID_ES_WAVEFORM          | 0x1104 | ES Waveform (Ref)        | Dead/broken -- ESv11 reference, requires Goertzel backend |

### Retired ID (never in DISPLAY_ORDER)

| Effect ID | Hex    | Name                       | Reason |
|-----------|--------|----------------------------|--------|
| EID_RETIRED_112 | 0x1402 | Beat Pulse (Shockwave In) | Retired -- inward shockwave variant superseded by Beat Pulse (Resonant). ID preserved in `effect_ids.h` as `EID_RETIRED_112` to prevent reuse. Still in `PATTERN_METADATA` and `REACTIVE_EFFECT_IDS` arrays but not registered or in display order. |

---

## 7. Troubleshooting

### Effect Not Cycling (Stuck on One Effect)

1. **Check WebSocket connection:** Tab5 debug log should show `[ENC0-DBG] val=... prev=... delta=... wsConn=1`. If `wsConn=0`, the WebSocket is disconnected and all commands are dropped silently.
2. **Check delta:** If delta is always 0, the encoder hardware is not registering rotation. Verify I2C communication with the M5ROTATE8 unit.
3. **Check K1 serial log:** Look for effect change messages. If commands arrive but the effect does not change, the renderer may be stuck.

### Effect Not in Display Order

If you set an effect directly by ID (e.g., via REST API `effects.setCurrent` with a radial variant ID like `0x060A`), and then try to cycle with the encoder:
- **nextEffect** will jump to `DISPLAY_ORDER[0]` (Fire, 0x0100)
- **prevEffect** will jump to `DISPLAY_ORDER[161]` (Time-Reversal Mirror Mod3, 0x1B08)
- This is because `displayIndexOf()` returns `DISPLAY_COUNT` for unknown IDs, triggering the fallback.

### Registry Order Does Not Match Display Order

This is **by design**. The REST API returns effects in registration order (as they appear in `CoreEffects.cpp`), which matches the legacy sequential ID order. The display cycling order is a curated sequence in `display_order.h`. These are intentionally independent.

### Effect Order Appears Corrupted When Cycling

Potential causes:
1. **Multiple rapid nextEffect/prevEffect messages:** The Tab5 caps at 8 steps per encoder update, but if the encoder is turned very quickly, multiple updates can queue before K1 processes them. Each `nextEffect` is processed independently and sequentially, so the result should be deterministic. However, if WebSocket messages arrive out of order or are duplicated, the effect may appear to skip around.
2. **WebSocket message reordering:** The underlying TCP connection should preserve order, but if the connection drops and reconnects mid-rotation, queued messages may be lost.
3. **Stale status broadcast:** After each effect change, K1 broadcasts status. If Tab5 receives an older broadcast after a newer one (unlikely with TCP), the displayed effect name may flicker.
4. **NVS persistence race:** K1 persists the current effect to NVS flash. If the device reboots mid-cycle, it will resume from the last persisted effect, which may not be the last one displayed.

### Wrap-Around Edge Cases

- **Index 161 + 1** wraps to index 0 (Fire).
- **Index 0 - 1** wraps to index 161 (Time-Reversal Mirror Mod3).
- The modular arithmetic `(idx + DISPLAY_COUNT - 1) % DISPLAY_COUNT` handles this correctly for all values.
- `DISPLAY_COUNT` is 162 (compile-time constant).

### Adding a New Effect to the Cycling Order

1. Define the EffectId constant in `firmware-v3/src/config/effect_ids.h`
2. Register the effect in `firmware-v3/src/effects/CoreEffects.cpp`
3. Add metadata in `firmware-v3/src/effects/PatternRegistry.cpp`
4. Add the EffectId to the desired position in `DISPLAY_ORDER[]` in `firmware-v3/src/config/display_order.h`
5. If audio-reactive, add to `REACTIVE_EFFECT_IDS[]` in `PatternRegistry.cpp`
6. `DISPLAY_COUNT` updates automatically (computed from array size)

---

## Source Files

| File | Purpose |
|------|---------|
| `firmware-v3/src/config/display_order.h` | Display cycling order, navigation helpers |
| `firmware-v3/src/config/effect_ids.h` | Stable EffectId constants, family blocks |
| `firmware-v3/src/effects/CoreEffects.cpp` | Effect registration (determines registry order) |
| `firmware-v3/src/effects/PatternRegistry.cpp` | Effect metadata, audio-reactive flags |
| `firmware-v3/src/network/webserver/ws/WsEffectsCommands.cpp` | nextEffect/prevEffect WS handlers |
| `firmware-v3/src/network/webserver/handlers/EffectHandlers.cpp` | REST API effect list handler |
| `tab5-encoder/src/network/WebSocketClient.cpp` | sendNextEffect/sendPrevEffect implementation |
| `tab5-encoder/src/main.cpp` | Encoder 0 delta calculation and dispatch |
