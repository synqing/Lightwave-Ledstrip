# LightwaveOS Effects Inventory

**Version:** 1.0.0
**Generated:** 2026-02-21 08:54:38

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total effect slots | 162 |
| Registered (active) | 161 |
| Removed slots | 1 (IDs: 112) |
| Audio-reactive | 70 |
| Non-audio-reactive | 92 |
| Effect families | 9 |

### Chroma Migration Summary

| Migration Pattern | Count | Status |
|-------------------|-------|--------|
| A | 6 | Migrated -- static utility functions replaced by [`circularChromaHueSmoothed()`](MATH_APPENDIX.md#circularchromahuesmoothed) |
| B | 4 | Migrated -- linear EMA on circular quantity replaced by [`circularEma()`](MATH_APPENDIX.md#circularema) |
| C | 6 | Migrated -- raw inline argmax replaced by [`circularChromaHue()`](MATH_APPENDIX.md#circularchromahue) |
| D | 10 | Native implementation -- Schmitt trigger with [`circularChromaHueSmoothed()`](MATH_APPENDIX.md#circularchromahuesmoothed) |
| additional_user | 7 | Additional effects using ChromaUtils |
| none | 129 | No chroma processing |

## Effects by Family

### ADVANCED_OPTICAL (26 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 26 | LGP Moire Curtains | `LGPMoireCurtainsEffect` | No | -- | Yes |
| 27 | LGP Radial Ripple | `LGPRadialRippleEffect` | No | -- | Yes |
| 28 | LGP Holographic Vortex | `LGPHolographicVortexEffect` | No | -- | Yes |
| 29 | LGP Evanescent Drift | `LGPEvanescentDriftEffect` | No | -- | Yes |
| 30 | LGP Chromatic Shear | `LGPChromaticShearEffect` | No | -- | Yes |
| 31 | LGP Modal Cavity | `LGPModalCavityEffect` | No | -- | Yes |
| 32 | LGP Fresnel Zones | `LGPFresnelZonesEffect` | No | -- | Yes |
| 33 | LGP Photonic Crystal | `LGPPhotonicCrystalEffect` | Yes | Pattern C | Yes |
| 65 | LGP Chromatic Lens | `LGPChromaticLensEffect` | No | -- | Yes |
| 66 | LGP Chromatic Pulse | `LGPChromaticPulseEffect` | No | -- | Yes |
| 67 | LGP Chromatic Interference | `ChromaticInterferenceEffect` | No | -- | Yes |
| 79 | LGP Perlin Caustics | `LGPPerlinCausticsEffect` | Yes | -- | Yes |
| 80 | LGP Perlin Interference Weave | `LGPPerlinInterferenceWeaveEffect` | Yes | Pattern additional_user | Yes |
| 83 | LGP Perlin Caustics Ambient | `LGPPerlinCausticsAmbientEffect` | No | -- | Yes |
| 84 | LGP Perlin Interference Weave Ambient | `LGPPerlinInterferenceWeaveAmbientEffect` | No | -- | Yes |
| 92 | LGP Photonic Crystal Enhanced | `LGPPhotonicCrystalEnhancedEffect` | Yes | Pattern C | Yes |
| 124 | LGP Opal Film | `LGPOpalFilmEffect` | No | -- | Yes |
| 125 | LGP Grating Scan | `LGPGratingScanEffect` | No | -- | Yes |
| 126 | LGP Stress Glass | `LGPStressGlassEffect` | No | -- | Yes |
| 127 | LGP Moire Silk | `LGPMoireSilkEffect` | No | -- | Yes |
| 128 | LGP Caustic Shards | `LGPCausticShardsEffect` | No | -- | Yes |
| 129 | LGP Parallax Depth | `LGPParallaxDepthEffect` | No | -- | Yes |
| 130 | LGP Stress Glass (Melt) | `LGPStressGlassMeltEffect` | No | -- | Yes |
| 131 | LGP Grating Scan (Breakup) | `LGPGratingScanBreakupEffect` | No | -- | Yes |
| 132 | LGP Water Caustics | `LGPWaterCausticsEffect` | No | -- | Yes |
| 133 | LGP Schlieren Flow | `LGPSchlierenFlowEffect` | No | -- | Yes |

### COLOR_MIXING (10 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 50 | LGP Color Temperature | `LGPColorTemperatureEffect` | No | -- | Yes |
| 51 | LGP RGB Prism | `LGPRGBPrismEffect` | No | -- | Yes |
| 52 | LGP Complementary Mixing | `LGPComplementaryMixingEffect` | No | -- | Yes |
| 53 | LGP Quantum Colors | `LGPQuantumColorsEffect` | No | -- | Yes |
| 54 | LGP Doppler Shift | `LGPDopplerShiftEffect` | No | -- | Yes |
| 55 | LGP Color Accelerator | `LGPColorAcceleratorEffect` | No | -- | Yes |
| 56 | LGP DNA Helix | `LGPDNAHelixEffect` | No | -- | Yes |
| 57 | LGP Phase Transition | `LGPPhaseTransitionEffect` | No | -- | Yes |
| 58 | LGP Chromatic Aberration | `LGPChromaticAberrationEffect` | No | -- | Yes |
| 59 | LGP Perceptual Blend | `LGPPerceptualBlendEffect` | No | -- | Yes |

### FLUID_PLASMA (55 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 0 | Fire | `FireEffect` | No | -- | Yes |
| 1 | Ocean | `OceanEffect` | No | -- | Yes |
| 2 | Plasma | `PlasmaEffect` | No | -- | Yes |
| 3 | Confetti | `ConfettiEffect` | No | -- | Yes |
| 7 | Wave | `WaveAmbientEffect` | Yes | -- | Yes |
| 8 | Ripple | `RippleEffect` | Yes | Pattern A | Yes |
| 9 | Heartbeat | `HeartbeatEffect` | Yes | -- | Yes |
| 11 | Breathing | `BreathingEffect` | Yes | -- | Yes |
| 12 | Pulse | `PulseEffect` | No | -- | Yes |
| 68 | Audio Test | `LGPAudioTestEffect` | Yes | -- | Yes |
| 69 | Beat Pulse | `LGPBeatPulseEffect` | Yes | Pattern A | Yes |
| 70 | Spectrum Bars | `LGPSpectrumBarsEffect` | Yes | Pattern A | Yes |
| 71 | Bass Breath | `LGPBassBreathEffect` | Yes | Pattern A | Yes |
| 72 | Audio Waveform | `AudioWaveformEffect` | Yes | Pattern additional_user | Yes |
| 73 | Audio Bloom | `AudioBloomEffect` | Yes | -- | Yes |
| 75 | Chord Glow | `LGPChordGlowEffect` | Yes | -- | Yes |
| 76 | Wave Reactive | `WaveReactiveEffect` | Yes | -- | Yes |
| 77 | LGP Perlin Veil | `LGPPerlinVeilEffect` | Yes | -- | Yes |
| 78 | LGP Perlin Shocklines | `LGPPerlinShocklinesEffect` | Yes | -- | Yes |
| 81 | LGP Perlin Veil Ambient | `LGPPerlinVeilAmbientEffect` | No | -- | Yes |
| 82 | LGP Perlin Shocklines Ambient | `LGPPerlinShocklinesAmbientEffect` | No | -- | Yes |
| 85 | Perlin Test: FastLED | `LGPPerlinBackendFastLEDEffect` | No | -- | Yes |
| 86 | Perlin Test: Emotiscope2 Full | `LGPPerlinBackendEmotiscopeFullEffect` | No | -- | Yes |
| 87 | Perlin Test: Emotiscope2 Quarter | `LGPPerlinBackendEmotiscopeQuarterEffect` | No | -- | Yes |
| 89 | Breathing Enhanced | `BreathingEnhancedEffect` | Yes | -- | Yes |
| 93 | LGP Spectrum Detail | `LGPSpectrumDetailEffect` | Yes | -- | Yes |
| 94 | LGP Spectrum Detail Enhanced | `LGPSpectrumDetailEnhancedEffect` | Yes | -- | Yes |
| 97 | Ripple Enhanced | `RippleEnhancedEffect` | Yes | Pattern A | Yes |
| 98 | Audio Bloom Parity | `SnapwaveLinearEffect` | Yes | -- | Yes |
| 99 | Audio Waveform Parity | `TrinityTestEffect` | Yes | -- | Yes |
| 102 | ES Spectrum (Ref) | `EsSpectrumRefEffect` | Yes | -- | Yes |
| 103 | ES Octave (Ref) | `EsOctaveRefEffect` | Yes | -- | Yes |
| 104 | ES Bloom (Ref) | `EsBloomRefEffect` | Yes | -- | Yes |
| 105 | ES Waveform (Ref) | `EsWaveformRefEffect` | Yes | -- | Yes |
| 106 | Ripple (ES tuned) | `RippleEsTunedEffect` | Yes | Pattern A | Yes |
| 107 | Heartbeat (ES tuned) | `HeartbeatEsTunedEffect` | Yes | Pattern additional_user | Yes |
| 109 | SB Waveform (Ref) | `SbWaveform310RefEffect` | Yes | -- | Yes |
| 110 | Beat Pulse (Stack) | `BeatPulseStackEffect` | Yes | -- | Yes |
| 111 | Beat Pulse (Shockwave) | `BeatPulseShockwaveEffect` | Yes | -- | Yes |
| 112 | Beat Pulse (Shockwave In) (REMOVED) | `None` | Yes | -- | Yes |
| 113 | Beat Pulse (Void) | `BeatPulseVoidEffect` | Yes | -- | Yes |
| 114 | Beat Pulse (Resonant) | `BeatPulseResonantEffect` | Yes | -- | Yes |
| 115 | Beat Pulse (Ripple) | `BeatPulseRippleEffect` | Yes | -- | Yes |
| 116 | Beat Pulse (Shockwave Cascade) | `BeatPulseShockwaveCascadeEffect` | Yes | -- | Yes |
| 117 | Beat Pulse (Spectral) | `BeatPulseSpectralEffect` | Yes | -- | Yes |
| 118 | Beat Pulse (Spectral Pulse) | `BeatPulseSpectralPulseEffect` | Yes | -- | Yes |
| 119 | Beat Pulse (Breathe) | `BeatPulseBreatheEffect` | Yes | -- | Yes |
| 120 | Beat Pulse (LGP Interference) | `BeatPulseLGPInterferenceEffect` | Yes | -- | Yes |
| 121 | Beat Pulse (Bloom) | `BeatPulseBloomEffect` | Yes | -- | Yes |
| 122 | Bloom (Parity) | `BloomParityEffect` | Yes | -- | Yes |
| 123 | Kuramoto Transport | `KuramotoTransportEffect` | Yes | -- | Yes |
| 152 | LGP Flux Rift | `LGPFluxRiftEffect` | Yes | Pattern D | Yes |
| 154 | LGP Harmonic Tide | `LGPHarmonicTideEffect` | Yes | Pattern D | Yes |
| 155 | LGP Bass Quake | `LGPBassQuakeEffect` | Yes | Pattern D | Yes |
| 159 | LGP Saliency Bloom | `LGPSaliencyBloomEffect` | Yes | Pattern D | Yes |

### GEOMETRIC (25 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 4 | Sinelon | `SinelonEffect` | No | -- | Yes |
| 5 | Juggle | `JuggleEffect` | Yes | Pattern C | Yes |
| 6 | BPM | `BPMEffect` | Yes | -- | Yes |
| 18 | LGP Diamond Lattice | `LGPDiamondLatticeEffect` | No | -- | Yes |
| 19 | LGP Hexagonal Grid | `LGPHexagonalGridEffect` | No | -- | Yes |
| 20 | LGP Spiral Vortex | `LGPSpiralVortexEffect` | No | -- | Yes |
| 21 | LGP Sierpinski | `LGPSierpinskiEffect` | No | -- | Yes |
| 22 | LGP Chevron Waves | `ChevronWavesEffect` | Yes | Pattern B | Yes |
| 23 | LGP Concentric Rings | `LGPConcentricRingsEffect` | No | -- | Yes |
| 24 | LGP Star Burst | `LGPStarBurstEffect` | Yes | Pattern C | Yes |
| 25 | LGP Mesh Network | `LGPMeshNetworkEffect` | No | -- | Yes |
| 74 | LGP Star Burst (Narrative) | `LGPStarBurstNarrativeEffect` | Yes | Pattern additional_user | Yes |
| 88 | BPM Enhanced | `BPMEnhancedEffect` | Yes | Pattern C | Yes |
| 90 | LGP Chevron Waves Enhanced | `ChevronWavesEnhancedEffect` | Yes | Pattern B | Yes |
| 95 | LGP Star Burst Enhanced | `LGPStarBurstEnhancedEffect` | Yes | Pattern C | Yes |
| 101 | ES Analog (Ref) | `EsAnalogRefEffect` | Yes | -- | Yes |
| 139 | LGP Living Glyph | `LGPSuperformulaGlyphEffect` | No | -- | Yes |
| 140 | LGP Spirograph Crown | `LGPSpirographCrownEffect` | No | -- | Yes |
| 141 | LGP Rose Bloom | `LGPRoseBloomEffect` | No | -- | Yes |
| 142 | LGP Harmonograph Halo | `LGPHarmonographHaloEffect` | No | -- | Yes |
| 149 | Hyperbolic Portal | `LGPHyperbolicPortalEffect` | No | -- | Yes |
| 153 | LGP Beat Prism | `LGPBeatPrismEffect` | Yes | Pattern D | Yes |
| 157 | LGP Rhythmic Gate | `LGPRhythmicGateEffect` | Yes | Pattern D | Yes |
| 158 | LGP Spectral Knot | `LGPSpectralKnotEffect` | Yes | Pattern D | Yes |
| 161 | LGP Wavelet Mirror | `LGPWaveletMirrorEffect` | Yes | Pattern D | Yes |

### INTERFERENCE (17 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 10 | Interference | `InterferenceEffect` | No | -- | Yes |
| 13 | LGP Box Wave | `LGPBoxWaveEffect` | No | -- | Yes |
| 14 | LGP Holographic | `LGPHolographicEffect` | No | -- | Yes |
| 15 | LGP Modal Resonance | `ModalResonanceEffect` | No | -- | Yes |
| 16 | LGP Interference Scanner | `LGPInterferenceScannerEffect` | Yes | Pattern additional_user | Yes |
| 17 | LGP Wave Collision | `LGPWaveCollisionEffect` | Yes | Pattern B | Yes |
| 91 | LGP Interference Scanner Enhanced | `LGPInterferenceScannerEnhancedEffect` | Yes | Pattern additional_user | Yes |
| 96 | LGP Wave Collision Enhanced | `LGPWaveCollisionEnhancedEffect` | Yes | Pattern B | Yes |
| 100 | LGP Holographic Auto-Cycle | `LGPHolographicAutoCycleEffect` | Yes | -- | Yes |
| 108 | LGP Holographic (ES tuned) | `LGPHolographicEsTunedEffect` | Yes | Pattern additional_user | Yes |
| 136 | LGP Talbot Carpet | `LGPTalbotCarpetEffect` | No | -- | Yes |
| 137 | LGP Airy Comet | `LGPAiryCometEffect` | No | -- | Yes |
| 138 | LGP Moire Cathedral | `LGPMoireCathedralEffect` | No | -- | Yes |
| 145 | LGP Cymatic Ladder | `LGPCymaticLadderEffect` | No | -- | Yes |
| 146 | LGP Mach Diamonds | `LGPMachDiamondsEffect` | No | -- | Yes |
| 156 | LGP Treble Net | `LGPTrebleNetEffect` | Yes | Pattern D | Yes |
| 160 | LGP Transient Lattice | `LGPTransientLatticeEffect` | Yes | Pattern D | Yes |

### MATHEMATICAL (2 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 147 | Chimera Crown | `LGPChimeraCrownEffect` | No | -- | Yes |
| 150 | Lorenz Ribbon | `LGPLorenzRibbonEffect` | No | -- | Yes |

### NOVEL_PHYSICS (9 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 60 | LGP Chladni Harmonics | `LGPChladniHarmonicsEffect` | No | -- | Yes |
| 61 | LGP Gravitational Wave Chirp | `LGPGravitationalWaveChirpEffect` | No | -- | Yes |
| 62 | LGP Quantum Entanglement | `LGPQuantumEntanglementEffect` | No | -- | Yes |
| 63 | LGP Mycelial Network | `LGPMycelialNetworkEffect` | No | -- | Yes |
| 64 | LGP Riley Dissonance | `LGPRileyDissonanceEffect` | No | -- | Yes |
| 134 | LGP Reaction Diffusion | `LGPReactionDiffusionEffect` | No | -- | Yes |
| 135 | LGP RD Triangle | `LGPReactionDiffusionTriangleEffect` | No | -- | Yes |
| 143 | LGP Rule 30 Cathedral | `LGPRule30CathedralEffect` | No | -- | Yes |
| 144 | LGP Langton Highway | `LGPLangtonHighwayEffect` | No | -- | Yes |

### ORGANIC (7 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 34 | LGP Aurora Borealis | `LGPAuroraBorealisEffect` | No | -- | Yes |
| 35 | LGP Bioluminescent Waves | `LGPBioluminescentWavesEffect` | No | -- | Yes |
| 36 | LGP Plasma Membrane | `LGPPlasmaMembraneEffect` | No | -- | Yes |
| 37 | LGP Neural Network | `LGPNeuralNetworkEffect` | No | -- | Yes |
| 38 | LGP Crystalline Growth | `LGPCrystallineGrowthEffect` | No | -- | Yes |
| 39 | LGP Fluid Dynamics | `LGPFluidDynamicsEffect` | No | -- | Yes |
| 151 | IFS Botanical Relic | `LGPIFSBioRelicEffect` | No | -- | Yes |

### QUANTUM (11 effects)

| ID | Name | Class | Audio | Chroma | Centre-Origin |
|----|------|-------|-------|--------|---------------|
| 40 | LGP Quantum Tunneling | `LGPQuantumTunnelingEffect` | No | -- | Yes |
| 41 | LGP Gravitational Lensing | `LGPGravitationalLensingEffect` | No | -- | Yes |
| 42 | LGP Time Crystal | `LGPTimeCrystalEffect` | No | -- | Yes |
| 43 | LGP Soliton Waves | `LGPSolitonWavesEffect` | No | -- | Yes |
| 44 | LGP Metamaterial Cloak | `LGPMetamaterialCloakEffect` | No | -- | Yes |
| 45 | LGP GRIN Cloak | `LGPGrinCloakEffect` | No | -- | Yes |
| 46 | LGP Caustic Fan | `LGPCausticFanEffect` | No | -- | Yes |
| 47 | LGP Birefringent Shear | `LGPBirefringentShearEffect` | No | -- | Yes |
| 48 | LGP Anisotropic Cloak | `LGPAnisotropicCloakEffect` | No | -- | Yes |
| 49 | LGP Evanescent Skin | `LGPEvanescentSkinEffect` | No | -- | Yes |
| 148 | Catastrophe Caustics | `LGPCatastropheCausticsEffect` | No | -- | Yes |

## Audio-Reactive Effects

Effects that respond to audio input via the [ControlBus](MATH_APPENDIX.md#audio-feature-access) system.

### Effects by Migration Pattern

#### Pattern A

See [Pattern Taxonomy -- Pattern A](PATTERN_TAXONOMY.md#pattern-a-static-utility-functions) for migration details.

- **Ripple** (ID 8, `RippleEffect`) -- uses ChromaUtils: Yes
- **Beat Pulse** (ID 69, `LGPBeatPulseEffect`) -- uses ChromaUtils: Yes
- **Spectrum Bars** (ID 70, `LGPSpectrumBarsEffect`) -- uses ChromaUtils: Yes
- **Bass Breath** (ID 71, `LGPBassBreathEffect`) -- uses ChromaUtils: Yes
- **Ripple Enhanced** (ID 97, `RippleEnhancedEffect`) -- uses ChromaUtils: Yes
- **Ripple (ES tuned)** (ID 106, `RippleEsTunedEffect`) -- uses ChromaUtils: Yes

#### Pattern B

See [Pattern Taxonomy -- Pattern B](PATTERN_TAXONOMY.md#pattern-b-linear-ema-on-circular-quantity) for migration details.

- **LGP Wave Collision** (ID 17, `LGPWaveCollisionEffect`) -- uses ChromaUtils: Yes
- **LGP Chevron Waves** (ID 22, `ChevronWavesEffect`) -- uses ChromaUtils: Yes
- **LGP Chevron Waves Enhanced** (ID 90, `ChevronWavesEnhancedEffect`) -- uses ChromaUtils: Yes
- **LGP Wave Collision Enhanced** (ID 96, `LGPWaveCollisionEnhancedEffect`) -- uses ChromaUtils: Yes

#### Pattern C

See [Pattern Taxonomy -- Pattern C](PATTERN_TAXONOMY.md#pattern-c-raw-inline-argmax) for migration details.

- **Juggle** (ID 5, `JuggleEffect`) -- uses ChromaUtils: Yes
- **LGP Star Burst** (ID 24, `LGPStarBurstEffect`) -- uses ChromaUtils: Yes
- **LGP Photonic Crystal** (ID 33, `LGPPhotonicCrystalEffect`) -- uses ChromaUtils: Yes
- **BPM Enhanced** (ID 88, `BPMEnhancedEffect`) -- uses ChromaUtils: Yes
- **LGP Photonic Crystal Enhanced** (ID 92, `LGPPhotonicCrystalEnhancedEffect`) -- uses ChromaUtils: Yes
- **LGP Star Burst Enhanced** (ID 95, `LGPStarBurstEnhancedEffect`) -- uses ChromaUtils: Yes

#### Pattern D

See [Pattern Taxonomy -- Pattern D](PATTERN_TAXONOMY.md#pattern-d-experimental-pack-schmitt-trigger) for migration details.

- **LGP Flux Rift** (ID 152, `LGPFluxRiftEffect`) -- uses ChromaUtils: Yes
- **LGP Beat Prism** (ID 153, `LGPBeatPrismEffect`) -- uses ChromaUtils: Yes
- **LGP Harmonic Tide** (ID 154, `LGPHarmonicTideEffect`) -- uses ChromaUtils: Yes
- **LGP Bass Quake** (ID 155, `LGPBassQuakeEffect`) -- uses ChromaUtils: Yes
- **LGP Treble Net** (ID 156, `LGPTrebleNetEffect`) -- uses ChromaUtils: Yes
- **LGP Rhythmic Gate** (ID 157, `LGPRhythmicGateEffect`) -- uses ChromaUtils: Yes
- **LGP Spectral Knot** (ID 158, `LGPSpectralKnotEffect`) -- uses ChromaUtils: Yes
- **LGP Saliency Bloom** (ID 159, `LGPSaliencyBloomEffect`) -- uses ChromaUtils: Yes
- **LGP Transient Lattice** (ID 160, `LGPTransientLatticeEffect`) -- uses ChromaUtils: Yes
- **LGP Wavelet Mirror** (ID 161, `LGPWaveletMirrorEffect`) -- uses ChromaUtils: Yes

#### Additional ChromaUtils Users

- **LGP Interference Scanner** (ID 16, `LGPInterferenceScannerEffect`)
- **Audio Waveform** (ID 72, `AudioWaveformEffect`)
- **LGP Star Burst (Narrative)** (ID 74, `LGPStarBurstNarrativeEffect`)
- **LGP Perlin Interference Weave** (ID 80, `LGPPerlinInterferenceWeaveEffect`)
- **LGP Interference Scanner Enhanced** (ID 91, `LGPInterferenceScannerEnhancedEffect`)
- **Heartbeat (ES tuned)** (ID 107, `HeartbeatEsTunedEffect`)
- **LGP Holographic (ES tuned)** (ID 108, `LGPHolographicEsTunedEffect`)

## Complete Effects Table (Alphabetical)

| ID | Name | Class | Family | Audio | Chroma | Files |
|----|------|-------|--------|-------|--------|-------|
| 73 | Audio Bloom | `AudioBloomEffect` | FLUID_PLASMA | Yes | -- | `AudioBloomEffect.h`, `AudioBloomEffect.cpp` |
| 98 | Audio Bloom Parity | `SnapwaveLinearEffect` | FLUID_PLASMA | Yes | -- | `SnapwaveLinearEffect.h`, `SnapwaveLinearEffect.cpp` |
| 68 | Audio Test | `LGPAudioTestEffect` | FLUID_PLASMA | Yes | -- | `LGPAudioTestEffect.h`, `LGPAudioTestEffect.cpp` |
| 72 | Audio Waveform | `AudioWaveformEffect` | FLUID_PLASMA | Yes | Pattern additional_user | `AudioWaveformEffect.h`, `AudioWaveformEffect.cpp` |
| 99 | Audio Waveform Parity | `TrinityTestEffect` | FLUID_PLASMA | Yes | -- | `TrinityTestEffect.h`, `TrinityTestEffect.cpp` |
| 71 | Bass Breath | `LGPBassBreathEffect` | FLUID_PLASMA | Yes | Pattern A | `LGPBassBreathEffect.h`, `LGPBassBreathEffect.cpp` |
| 69 | Beat Pulse | `LGPBeatPulseEffect` | FLUID_PLASMA | Yes | Pattern A | `LGPBeatPulseEffect.h`, `LGPBeatPulseEffect.cpp` |
| 121 | Beat Pulse (Bloom) | `BeatPulseBloomEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseBloomEffect.h`, `BeatPulseBloomEffect.cpp` |
| 119 | Beat Pulse (Breathe) | `BeatPulseBreatheEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseBreatheEffect.h`, `BeatPulseBreatheEffect.cpp` |
| 120 | Beat Pulse (LGP Interference) | `BeatPulseLGPInterferenceEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseLGPInterferenceEffect.h`, `BeatPulseLGPInterferenceEffect.cpp` |
| 114 | Beat Pulse (Resonant) | `BeatPulseResonantEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseResonantEffect.h`, `BeatPulseResonantEffect.cpp` |
| 115 | Beat Pulse (Ripple) | `BeatPulseRippleEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseRippleEffect.h`, `BeatPulseRippleEffect.cpp` |
| 116 | Beat Pulse (Shockwave Cascade) | `BeatPulseShockwaveCascadeEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseShockwaveCascadeEffect.h`, `BeatPulseShockwaveCascadeEffect.cpp` |
| 112 | Beat Pulse (Shockwave In) **(REMOVED)** | `None` | FLUID_PLASMA | Yes | -- | `--`, `--` |
| 111 | Beat Pulse (Shockwave) | `BeatPulseShockwaveEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseShockwaveEffect.h`, `BeatPulseShockwaveEffect.cpp` |
| 118 | Beat Pulse (Spectral Pulse) | `BeatPulseSpectralPulseEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseSpectralPulseEffect.h`, `BeatPulseSpectralPulseEffect.cpp` |
| 117 | Beat Pulse (Spectral) | `BeatPulseSpectralEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseSpectralEffect.h`, `BeatPulseSpectralEffect.cpp` |
| 110 | Beat Pulse (Stack) | `BeatPulseStackEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseStackEffect.h`, `BeatPulseStackEffect.cpp` |
| 113 | Beat Pulse (Void) | `BeatPulseVoidEffect` | FLUID_PLASMA | Yes | -- | `BeatPulseVoidEffect.h`, `BeatPulseVoidEffect.cpp` |
| 122 | Bloom (Parity) | `BloomParityEffect` | FLUID_PLASMA | Yes | -- | `BloomParityEffect.h`, `BloomParityEffect.cpp` |
| 6 | BPM | `BPMEffect` | GEOMETRIC | Yes | -- | `BPMEffect.h`, `BPMEffect.cpp` |
| 88 | BPM Enhanced | `BPMEnhancedEffect` | GEOMETRIC | Yes | Pattern C | `BPMEnhancedEffect.h`, `BPMEnhancedEffect.cpp` |
| 11 | Breathing | `BreathingEffect` | FLUID_PLASMA | Yes | -- | `BreathingEffect.h`, `BreathingEffect.cpp` |
| 89 | Breathing Enhanced | `BreathingEnhancedEffect` | FLUID_PLASMA | Yes | -- | `BreathingEnhancedEffect.h`, `BreathingEnhancedEffect.cpp` |
| 148 | Catastrophe Caustics | `LGPCatastropheCausticsEffect` | QUANTUM | No | -- | `LGPHolyShitBangersPack.h`, `LGPHolyShitBangersPack.cpp` |
| 147 | Chimera Crown | `LGPChimeraCrownEffect` | MATHEMATICAL | No | -- | `LGPHolyShitBangersPack.h`, `LGPHolyShitBangersPack.cpp` |
| 75 | Chord Glow | `LGPChordGlowEffect` | FLUID_PLASMA | Yes | -- | `LGPChordGlowEffect.h`, `LGPChordGlowEffect.cpp` |
| 3 | Confetti | `ConfettiEffect` | FLUID_PLASMA | No | -- | `ConfettiEffect.h`, `ConfettiEffect.cpp` |
| 101 | ES Analog (Ref) | `EsAnalogRefEffect` | GEOMETRIC | Yes | -- | `EsAnalogRefEffect.h`, `EsAnalogRefEffect.cpp` |
| 104 | ES Bloom (Ref) | `EsBloomRefEffect` | FLUID_PLASMA | Yes | -- | `EsBloomRefEffect.h`, `EsBloomRefEffect.cpp` |
| 103 | ES Octave (Ref) | `EsOctaveRefEffect` | FLUID_PLASMA | Yes | -- | `EsOctaveRefEffect.h`, `EsOctaveRefEffect.cpp` |
| 102 | ES Spectrum (Ref) | `EsSpectrumRefEffect` | FLUID_PLASMA | Yes | -- | `EsSpectrumRefEffect.h`, `EsSpectrumRefEffect.cpp` |
| 105 | ES Waveform (Ref) | `EsWaveformRefEffect` | FLUID_PLASMA | Yes | -- | `EsWaveformRefEffect.h`, `EsWaveformRefEffect.cpp` |
| 0 | Fire | `FireEffect` | FLUID_PLASMA | No | -- | `FireEffect.h`, `FireEffect.cpp` |
| 9 | Heartbeat | `HeartbeatEffect` | FLUID_PLASMA | Yes | -- | `HeartbeatEffect.h`, `HeartbeatEffect.cpp` |
| 107 | Heartbeat (ES tuned) | `HeartbeatEsTunedEffect` | FLUID_PLASMA | Yes | Pattern additional_user | `HeartbeatEsTunedEffect.h`, `HeartbeatEsTunedEffect.cpp` |
| 149 | Hyperbolic Portal | `LGPHyperbolicPortalEffect` | GEOMETRIC | No | -- | `LGPHolyShitBangersPack.h`, `LGPHolyShitBangersPack.cpp` |
| 151 | IFS Botanical Relic | `LGPIFSBioRelicEffect` | ORGANIC | No | -- | `LGPHolyShitBangersPack.h`, `LGPHolyShitBangersPack.cpp` |
| 10 | Interference | `InterferenceEffect` | INTERFERENCE | No | -- | `InterferenceEffect.h`, `InterferenceEffect.cpp` |
| 5 | Juggle | `JuggleEffect` | GEOMETRIC | Yes | Pattern C | `JuggleEffect.h`, `JuggleEffect.cpp` |
| 123 | Kuramoto Transport | `KuramotoTransportEffect` | FLUID_PLASMA | Yes | -- | `KuramotoTransportEffect.h`, `KuramotoTransportEffect.cpp` |
| 137 | LGP Airy Comet | `LGPAiryCometEffect` | INTERFERENCE | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 48 | LGP Anisotropic Cloak | `LGPAnisotropicCloakEffect` | QUANTUM | No | -- | `LGPAnisotropicCloakEffect.h`, `LGPAnisotropicCloakEffect.cpp` |
| 34 | LGP Aurora Borealis | `LGPAuroraBorealisEffect` | ORGANIC | No | -- | `LGPAuroraBorealisEffect.h`, `LGPAuroraBorealisEffect.cpp` |
| 155 | LGP Bass Quake | `LGPBassQuakeEffect` | FLUID_PLASMA | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 153 | LGP Beat Prism | `LGPBeatPrismEffect` | GEOMETRIC | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 35 | LGP Bioluminescent Waves | `LGPBioluminescentWavesEffect` | ORGANIC | No | -- | `LGPBioluminescentWavesEffect.h`, `LGPBioluminescentWavesEffect.cpp` |
| 47 | LGP Birefringent Shear | `LGPBirefringentShearEffect` | QUANTUM | No | -- | `LGPBirefringentShearEffect.h`, `LGPBirefringentShearEffect.cpp` |
| 13 | LGP Box Wave | `LGPBoxWaveEffect` | INTERFERENCE | No | -- | `LGPBoxWaveEffect.h`, `LGPBoxWaveEffect.cpp` |
| 46 | LGP Caustic Fan | `LGPCausticFanEffect` | QUANTUM | No | -- | `LGPCausticFanEffect.h`, `LGPCausticFanEffect.cpp` |
| 128 | LGP Caustic Shards | `LGPCausticShardsEffect` | ADVANCED_OPTICAL | No | -- | `LGPCausticShardsEffect.h`, `LGPCausticShardsEffect.cpp` |
| 22 | LGP Chevron Waves | `ChevronWavesEffect` | GEOMETRIC | Yes | Pattern B | `ChevronWavesEffect.h`, `ChevronWavesEffect.cpp` |
| 90 | LGP Chevron Waves Enhanced | `ChevronWavesEnhancedEffect` | GEOMETRIC | Yes | Pattern B | `ChevronWavesEffectEnhanced.h`, `ChevronWavesEffectEnhanced.cpp` |
| 60 | LGP Chladni Harmonics | `LGPChladniHarmonicsEffect` | NOVEL_PHYSICS | No | -- | `LGPChladniHarmonicsEffect.h`, `LGPChladniHarmonicsEffect.cpp` |
| 58 | LGP Chromatic Aberration | `LGPChromaticAberrationEffect` | COLOR_MIXING | No | -- | `LGPChromaticAberrationEffect.h`, `LGPChromaticAberrationEffect.cpp` |
| 67 | LGP Chromatic Interference | `ChromaticInterferenceEffect` | ADVANCED_OPTICAL | No | -- | `ChromaticInterferenceEffect.h`, `ChromaticInterferenceEffect.cpp` |
| 65 | LGP Chromatic Lens | `LGPChromaticLensEffect` | ADVANCED_OPTICAL | No | -- | `LGPChromaticLensEffect.h`, `LGPChromaticLensEffect.cpp` |
| 66 | LGP Chromatic Pulse | `LGPChromaticPulseEffect` | ADVANCED_OPTICAL | No | -- | `LGPChromaticPulseEffect.h`, `LGPChromaticPulseEffect.cpp` |
| 30 | LGP Chromatic Shear | `LGPChromaticShearEffect` | ADVANCED_OPTICAL | No | -- | `LGPChromaticShearEffect.h`, `LGPChromaticShearEffect.cpp` |
| 55 | LGP Color Accelerator | `LGPColorAcceleratorEffect` | COLOR_MIXING | No | -- | `LGPColorAcceleratorEffect.h`, `LGPColorAcceleratorEffect.cpp` |
| 50 | LGP Color Temperature | `LGPColorTemperatureEffect` | COLOR_MIXING | No | -- | `LGPColorTemperatureEffect.h`, `LGPColorTemperatureEffect.cpp` |
| 52 | LGP Complementary Mixing | `LGPComplementaryMixingEffect` | COLOR_MIXING | No | -- | `LGPComplementaryMixingEffect.h`, `LGPComplementaryMixingEffect.cpp` |
| 23 | LGP Concentric Rings | `LGPConcentricRingsEffect` | GEOMETRIC | No | -- | `LGPConcentricRingsEffect.h`, `LGPConcentricRingsEffect.cpp` |
| 38 | LGP Crystalline Growth | `LGPCrystallineGrowthEffect` | ORGANIC | No | -- | `LGPCrystallineGrowthEffect.h`, `LGPCrystallineGrowthEffect.cpp` |
| 145 | LGP Cymatic Ladder | `LGPCymaticLadderEffect` | INTERFERENCE | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 18 | LGP Diamond Lattice | `LGPDiamondLatticeEffect` | GEOMETRIC | No | -- | `LGPDiamondLatticeEffect.h`, `LGPDiamondLatticeEffect.cpp` |
| 56 | LGP DNA Helix | `LGPDNAHelixEffect` | COLOR_MIXING | No | -- | `LGPDNAHelixEffect.h`, `LGPDNAHelixEffect.cpp` |
| 54 | LGP Doppler Shift | `LGPDopplerShiftEffect` | COLOR_MIXING | No | -- | `LGPDopplerShiftEffect.h`, `LGPDopplerShiftEffect.cpp` |
| 29 | LGP Evanescent Drift | `LGPEvanescentDriftEffect` | ADVANCED_OPTICAL | No | -- | `LGPEvanescentDriftEffect.h`, `LGPEvanescentDriftEffect.cpp` |
| 49 | LGP Evanescent Skin | `LGPEvanescentSkinEffect` | QUANTUM | No | -- | `LGPEvanescentSkinEffect.h`, `LGPEvanescentSkinEffect.cpp` |
| 39 | LGP Fluid Dynamics | `LGPFluidDynamicsEffect` | ORGANIC | No | -- | `LGPFluidDynamicsEffect.h`, `LGPFluidDynamicsEffect.cpp` |
| 152 | LGP Flux Rift | `LGPFluxRiftEffect` | FLUID_PLASMA | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 32 | LGP Fresnel Zones | `LGPFresnelZonesEffect` | ADVANCED_OPTICAL | No | -- | `LGPFresnelZonesEffect.h`, `LGPFresnelZonesEffect.cpp` |
| 125 | LGP Grating Scan | `LGPGratingScanEffect` | ADVANCED_OPTICAL | No | -- | `LGPGratingScanEffect.h`, `LGPGratingScanEffect.cpp` |
| 131 | LGP Grating Scan (Breakup) | `LGPGratingScanBreakupEffect` | ADVANCED_OPTICAL | No | -- | `LGPGratingScanBreakupEffect.h`, `LGPGratingScanBreakupEffect.cpp` |
| 41 | LGP Gravitational Lensing | `LGPGravitationalLensingEffect` | QUANTUM | No | -- | `LGPGravitationalLensingEffect.h`, `LGPGravitationalLensingEffect.cpp` |
| 61 | LGP Gravitational Wave Chirp | `LGPGravitationalWaveChirpEffect` | NOVEL_PHYSICS | No | -- | `LGPGravitationalWaveChirpEffect.h`, `LGPGravitationalWaveChirpEffect.cpp` |
| 45 | LGP GRIN Cloak | `LGPGrinCloakEffect` | QUANTUM | No | -- | `LGPGrinCloakEffect.h`, `LGPGrinCloakEffect.cpp` |
| 154 | LGP Harmonic Tide | `LGPHarmonicTideEffect` | FLUID_PLASMA | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 142 | LGP Harmonograph Halo | `LGPHarmonographHaloEffect` | GEOMETRIC | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 19 | LGP Hexagonal Grid | `LGPHexagonalGridEffect` | GEOMETRIC | No | -- | `LGPHexagonalGridEffect.h`, `LGPHexagonalGridEffect.cpp` |
| 14 | LGP Holographic | `LGPHolographicEffect` | INTERFERENCE | No | -- | `LGPHolographicEffect.h`, `LGPHolographicEffect.cpp` |
| 108 | LGP Holographic (ES tuned) | `LGPHolographicEsTunedEffect` | INTERFERENCE | Yes | Pattern additional_user | `LGPHolographicEsTunedEffect.h`, `LGPHolographicEsTunedEffect.cpp` |
| 100 | LGP Holographic Auto-Cycle | `LGPHolographicAutoCycleEffect` | INTERFERENCE | Yes | -- | `LGPHolographicAutoCycleEffect.h`, `LGPHolographicAutoCycleEffect.cpp` |
| 28 | LGP Holographic Vortex | `LGPHolographicVortexEffect` | ADVANCED_OPTICAL | No | -- | `LGPHolographicVortexEffect.h`, `LGPHolographicVortexEffect.cpp` |
| 16 | LGP Interference Scanner | `LGPInterferenceScannerEffect` | INTERFERENCE | Yes | Pattern additional_user | `LGPInterferenceScannerEffect.h`, `LGPInterferenceScannerEffect.cpp` |
| 91 | LGP Interference Scanner Enhanced | `LGPInterferenceScannerEnhancedEffect` | INTERFERENCE | Yes | Pattern additional_user | `LGPInterferenceScannerEffectEnhanced.h`, `LGPInterferenceScannerEffectEnhanced.cpp` |
| 144 | LGP Langton Highway | `LGPLangtonHighwayEffect` | NOVEL_PHYSICS | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 139 | LGP Living Glyph | `LGPSuperformulaGlyphEffect` | GEOMETRIC | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 146 | LGP Mach Diamonds | `LGPMachDiamondsEffect` | INTERFERENCE | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 25 | LGP Mesh Network | `LGPMeshNetworkEffect` | GEOMETRIC | No | -- | `LGPMeshNetworkEffect.h`, `LGPMeshNetworkEffect.cpp` |
| 44 | LGP Metamaterial Cloak | `LGPMetamaterialCloakEffect` | QUANTUM | No | -- | `LGPMetamaterialCloakEffect.h`, `LGPMetamaterialCloakEffect.cpp` |
| 31 | LGP Modal Cavity | `LGPModalCavityEffect` | ADVANCED_OPTICAL | No | -- | `LGPModalCavityEffect.h`, `LGPModalCavityEffect.cpp` |
| 15 | LGP Modal Resonance | `ModalResonanceEffect` | INTERFERENCE | No | -- | `ModalResonanceEffect.h`, `ModalResonanceEffect.cpp` |
| 138 | LGP Moire Cathedral | `LGPMoireCathedralEffect` | INTERFERENCE | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 26 | LGP Moire Curtains | `LGPMoireCurtainsEffect` | ADVANCED_OPTICAL | No | -- | `LGPMoireCurtainsEffect.h`, `LGPMoireCurtainsEffect.cpp` |
| 127 | LGP Moire Silk | `LGPMoireSilkEffect` | ADVANCED_OPTICAL | No | -- | `LGPMoireSilkEffect.h`, `LGPMoireSilkEffect.cpp` |
| 63 | LGP Mycelial Network | `LGPMycelialNetworkEffect` | NOVEL_PHYSICS | No | -- | `LGPMycelialNetworkEffect.h`, `LGPMycelialNetworkEffect.cpp` |
| 37 | LGP Neural Network | `LGPNeuralNetworkEffect` | ORGANIC | No | -- | `LGPNeuralNetworkEffect.h`, `LGPNeuralNetworkEffect.cpp` |
| 124 | LGP Opal Film | `LGPOpalFilmEffect` | ADVANCED_OPTICAL | No | -- | `LGPOpalFilmEffect.h`, `LGPOpalFilmEffect.cpp` |
| 129 | LGP Parallax Depth | `LGPParallaxDepthEffect` | ADVANCED_OPTICAL | No | -- | `LGPParallaxDepthEffect.h`, `LGPParallaxDepthEffect.cpp` |
| 59 | LGP Perceptual Blend | `LGPPerceptualBlendEffect` | COLOR_MIXING | No | -- | `LGPPerceptualBlendEffect.h`, `LGPPerceptualBlendEffect.cpp` |
| 79 | LGP Perlin Caustics | `LGPPerlinCausticsEffect` | ADVANCED_OPTICAL | Yes | -- | `LGPPerlinCausticsEffect.h`, `LGPPerlinCausticsEffect.cpp` |
| 83 | LGP Perlin Caustics Ambient | `LGPPerlinCausticsAmbientEffect` | ADVANCED_OPTICAL | No | -- | `LGPPerlinCausticsAmbientEffect.h`, `LGPPerlinCausticsAmbientEffect.cpp` |
| 80 | LGP Perlin Interference Weave | `LGPPerlinInterferenceWeaveEffect` | ADVANCED_OPTICAL | Yes | Pattern additional_user | `LGPPerlinInterferenceWeaveEffect.h`, `LGPPerlinInterferenceWeaveEffect.cpp` |
| 84 | LGP Perlin Interference Weave Ambient | `LGPPerlinInterferenceWeaveAmbientEffect` | ADVANCED_OPTICAL | No | -- | `LGPPerlinInterferenceWeaveAmbientEffect.h`, `LGPPerlinInterferenceWeaveAmbientEffect.cpp` |
| 78 | LGP Perlin Shocklines | `LGPPerlinShocklinesEffect` | FLUID_PLASMA | Yes | -- | `LGPPerlinShocklinesEffect.h`, `LGPPerlinShocklinesEffect.cpp` |
| 82 | LGP Perlin Shocklines Ambient | `LGPPerlinShocklinesAmbientEffect` | FLUID_PLASMA | No | -- | `LGPPerlinShocklinesAmbientEffect.h`, `LGPPerlinShocklinesAmbientEffect.cpp` |
| 77 | LGP Perlin Veil | `LGPPerlinVeilEffect` | FLUID_PLASMA | Yes | -- | `LGPPerlinVeilEffect.h`, `LGPPerlinVeilEffect.cpp` |
| 81 | LGP Perlin Veil Ambient | `LGPPerlinVeilAmbientEffect` | FLUID_PLASMA | No | -- | `LGPPerlinVeilAmbientEffect.h`, `LGPPerlinVeilAmbientEffect.cpp` |
| 57 | LGP Phase Transition | `LGPPhaseTransitionEffect` | COLOR_MIXING | No | -- | `LGPPhaseTransitionEffect.h`, `LGPPhaseTransitionEffect.cpp` |
| 33 | LGP Photonic Crystal | `LGPPhotonicCrystalEffect` | ADVANCED_OPTICAL | Yes | Pattern C | `LGPPhotonicCrystalEffect.h`, `LGPPhotonicCrystalEffect.cpp` |
| 92 | LGP Photonic Crystal Enhanced | `LGPPhotonicCrystalEnhancedEffect` | ADVANCED_OPTICAL | Yes | Pattern C | `LGPPhotonicCrystalEffectEnhanced.h`, `LGPPhotonicCrystalEffectEnhanced.cpp` |
| 36 | LGP Plasma Membrane | `LGPPlasmaMembraneEffect` | ORGANIC | No | -- | `LGPPlasmaMembraneEffect.h`, `LGPPlasmaMembraneEffect.cpp` |
| 53 | LGP Quantum Colors | `LGPQuantumColorsEffect` | COLOR_MIXING | No | -- | `LGPQuantumColorsEffect.h`, `LGPQuantumColorsEffect.cpp` |
| 62 | LGP Quantum Entanglement | `LGPQuantumEntanglementEffect` | NOVEL_PHYSICS | No | -- | `LGPQuantumEntanglementEffect.h`, `LGPQuantumEntanglementEffect.cpp` |
| 40 | LGP Quantum Tunneling | `LGPQuantumTunnelingEffect` | QUANTUM | No | -- | `LGPQuantumTunnelingEffect.h`, `LGPQuantumTunnelingEffect.cpp` |
| 27 | LGP Radial Ripple | `LGPRadialRippleEffect` | ADVANCED_OPTICAL | No | -- | `LGPRadialRippleEffect.h`, `LGPRadialRippleEffect.cpp` |
| 135 | LGP RD Triangle | `LGPReactionDiffusionTriangleEffect` | NOVEL_PHYSICS | No | -- | `LGPReactionDiffusionTriangleEffect.h`, `LGPReactionDiffusionTriangleEffect.cpp` |
| 134 | LGP Reaction Diffusion | `LGPReactionDiffusionEffect` | NOVEL_PHYSICS | No | -- | `LGPReactionDiffusionEffect.h`, `LGPReactionDiffusionEffect.cpp` |
| 51 | LGP RGB Prism | `LGPRGBPrismEffect` | COLOR_MIXING | No | -- | `LGPRGBPrismEffect.h`, `LGPRGBPrismEffect.cpp` |
| 157 | LGP Rhythmic Gate | `LGPRhythmicGateEffect` | GEOMETRIC | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 64 | LGP Riley Dissonance | `LGPRileyDissonanceEffect` | NOVEL_PHYSICS | No | -- | `LGPRileyDissonanceEffect.h`, `LGPRileyDissonanceEffect.cpp` |
| 141 | LGP Rose Bloom | `LGPRoseBloomEffect` | GEOMETRIC | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 143 | LGP Rule 30 Cathedral | `LGPRule30CathedralEffect` | NOVEL_PHYSICS | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 159 | LGP Saliency Bloom | `LGPSaliencyBloomEffect` | FLUID_PLASMA | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 133 | LGP Schlieren Flow | `LGPSchlierenFlowEffect` | ADVANCED_OPTICAL | No | -- | `LGPSchlierenFlowEffect.h`, `LGPSchlierenFlowEffect.cpp` |
| 21 | LGP Sierpinski | `LGPSierpinskiEffect` | GEOMETRIC | No | -- | `LGPSierpinskiEffect.h`, `LGPSierpinskiEffect.cpp` |
| 43 | LGP Soliton Waves | `LGPSolitonWavesEffect` | QUANTUM | No | -- | `LGPSolitonWavesEffect.h`, `LGPSolitonWavesEffect.cpp` |
| 158 | LGP Spectral Knot | `LGPSpectralKnotEffect` | GEOMETRIC | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 93 | LGP Spectrum Detail | `LGPSpectrumDetailEffect` | FLUID_PLASMA | Yes | -- | `LGPSpectrumDetailEffect.h`, `LGPSpectrumDetailEffect.cpp` |
| 94 | LGP Spectrum Detail Enhanced | `LGPSpectrumDetailEnhancedEffect` | FLUID_PLASMA | Yes | -- | `LGPSpectrumDetailEnhancedEffect.h`, `LGPSpectrumDetailEnhancedEffect.cpp` |
| 20 | LGP Spiral Vortex | `LGPSpiralVortexEffect` | GEOMETRIC | No | -- | `LGPSpiralVortexEffect.h`, `LGPSpiralVortexEffect.cpp` |
| 140 | LGP Spirograph Crown | `LGPSpirographCrownEffect` | GEOMETRIC | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 24 | LGP Star Burst | `LGPStarBurstEffect` | GEOMETRIC | Yes | Pattern C | `LGPStarBurstEffect.h`, `LGPStarBurstEffect.cpp` |
| 74 | LGP Star Burst (Narrative) | `LGPStarBurstNarrativeEffect` | GEOMETRIC | Yes | Pattern additional_user | `LGPStarBurstNarrativeEffect.h`, `LGPStarBurstNarrativeEffect.cpp` |
| 95 | LGP Star Burst Enhanced | `LGPStarBurstEnhancedEffect` | GEOMETRIC | Yes | Pattern C | `LGPStarBurstEffectEnhanced.h`, `LGPStarBurstEffectEnhanced.cpp` |
| 126 | LGP Stress Glass | `LGPStressGlassEffect` | ADVANCED_OPTICAL | No | -- | `LGPStressGlassEffect.h`, `LGPStressGlassEffect.cpp` |
| 130 | LGP Stress Glass (Melt) | `LGPStressGlassMeltEffect` | ADVANCED_OPTICAL | No | -- | `LGPStressGlassMeltEffect.h`, `LGPStressGlassMeltEffect.cpp` |
| 136 | LGP Talbot Carpet | `LGPTalbotCarpetEffect` | INTERFERENCE | No | -- | `LGPShapeBangersPack.h`, `LGPShapeBangersPack.cpp` |
| 42 | LGP Time Crystal | `LGPTimeCrystalEffect` | QUANTUM | No | -- | `LGPTimeCrystalEffect.h`, `LGPTimeCrystalEffect.cpp` |
| 160 | LGP Transient Lattice | `LGPTransientLatticeEffect` | INTERFERENCE | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 156 | LGP Treble Net | `LGPTrebleNetEffect` | INTERFERENCE | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 132 | LGP Water Caustics | `LGPWaterCausticsEffect` | ADVANCED_OPTICAL | No | -- | `LGPWaterCausticsEffect.h`, `LGPWaterCausticsEffect.cpp` |
| 17 | LGP Wave Collision | `LGPWaveCollisionEffect` | INTERFERENCE | Yes | Pattern B | `LGPWaveCollisionEffect.h`, `LGPWaveCollisionEffect.cpp` |
| 96 | LGP Wave Collision Enhanced | `LGPWaveCollisionEnhancedEffect` | INTERFERENCE | Yes | Pattern B | `LGPWaveCollisionEffectEnhanced.h`, `LGPWaveCollisionEffectEnhanced.cpp` |
| 161 | LGP Wavelet Mirror | `LGPWaveletMirrorEffect` | GEOMETRIC | Yes | Pattern D | `LGPExperimentalAudioPack.h`, `LGPExperimentalAudioPack.cpp` |
| 150 | Lorenz Ribbon | `LGPLorenzRibbonEffect` | MATHEMATICAL | No | -- | `LGPHolyShitBangersPack.h`, `LGPHolyShitBangersPack.cpp` |
| 1 | Ocean | `OceanEffect` | FLUID_PLASMA | No | -- | `OceanEffect.h`, `OceanEffect.cpp` |
| 86 | Perlin Test: Emotiscope2 Full | `LGPPerlinBackendEmotiscopeFullEffect` | FLUID_PLASMA | No | -- | `LGPPerlinBackendEmotiscopeFullEffect.h`, `LGPPerlinBackendEmotiscopeFullEffect.cpp` |
| 87 | Perlin Test: Emotiscope2 Quarter | `LGPPerlinBackendEmotiscopeQuarterEffect` | FLUID_PLASMA | No | -- | `LGPPerlinBackendEmotiscopeQuarterEffect.h`, `LGPPerlinBackendEmotiscopeQuarterEffect.cpp` |
| 85 | Perlin Test: FastLED | `LGPPerlinBackendFastLEDEffect` | FLUID_PLASMA | No | -- | `LGPPerlinBackendFastLEDEffect.h`, `LGPPerlinBackendFastLEDEffect.cpp` |
| 2 | Plasma | `PlasmaEffect` | FLUID_PLASMA | No | -- | `PlasmaEffect.h`, `PlasmaEffect.cpp` |
| 12 | Pulse | `PulseEffect` | FLUID_PLASMA | No | -- | `PulseEffect.h`, `PulseEffect.cpp` |
| 8 | Ripple | `RippleEffect` | FLUID_PLASMA | Yes | Pattern A | `RippleEffect.h`, `RippleEffect.cpp` |
| 106 | Ripple (ES tuned) | `RippleEsTunedEffect` | FLUID_PLASMA | Yes | Pattern A | `RippleEsTunedEffect.h`, `RippleEsTunedEffect.cpp` |
| 97 | Ripple Enhanced | `RippleEnhancedEffect` | FLUID_PLASMA | Yes | Pattern A | `RippleEnhancedEffect.h`, `RippleEnhancedEffect.cpp` |
| 109 | SB Waveform (Ref) | `SbWaveform310RefEffect` | FLUID_PLASMA | Yes | -- | `SbWaveform310RefEffect.h`, `SbWaveform310RefEffect.cpp` |
| 4 | Sinelon | `SinelonEffect` | GEOMETRIC | No | -- | `SinelonEffect.h`, `SinelonEffect.cpp` |
| 70 | Spectrum Bars | `LGPSpectrumBarsEffect` | FLUID_PLASMA | Yes | Pattern A | `LGPSpectrumBarsEffect.h`, `LGPSpectrumBarsEffect.cpp` |
| 7 | Wave | `WaveAmbientEffect` | FLUID_PLASMA | Yes | -- | `WaveAmbientEffect.h`, `WaveAmbientEffect.cpp` |
| 76 | Wave Reactive | `WaveReactiveEffect` | FLUID_PLASMA | Yes | -- | `WaveReactiveEffect.h`, `WaveReactiveEffect.cpp` |

## Appendix: Effect ID Allocation Map

The following table shows the full ID allocation sequence. Gaps indicate unused or removed slots.

- **ID range:** 0 -- 161
- **Allocated:** 162
- **Gaps in sequence:** 0

```
ID    Effect Name
----  --------------------------------------------------
0     Fire
1     Ocean
2     Plasma
3     Confetti
4     Sinelon
5     Juggle
6     BPM
7     Wave
8     Ripple
9     Heartbeat
10    Interference
11    Breathing
12    Pulse
13    LGP Box Wave
14    LGP Holographic
15    LGP Modal Resonance
16    LGP Interference Scanner
17    LGP Wave Collision
18    LGP Diamond Lattice
19    LGP Hexagonal Grid
20    LGP Spiral Vortex
21    LGP Sierpinski
22    LGP Chevron Waves
23    LGP Concentric Rings
24    LGP Star Burst
25    LGP Mesh Network
26    LGP Moire Curtains
27    LGP Radial Ripple
28    LGP Holographic Vortex
29    LGP Evanescent Drift
30    LGP Chromatic Shear
31    LGP Modal Cavity
32    LGP Fresnel Zones
33    LGP Photonic Crystal
34    LGP Aurora Borealis
35    LGP Bioluminescent Waves
36    LGP Plasma Membrane
37    LGP Neural Network
38    LGP Crystalline Growth
39    LGP Fluid Dynamics
40    LGP Quantum Tunneling
41    LGP Gravitational Lensing
42    LGP Time Crystal
43    LGP Soliton Waves
44    LGP Metamaterial Cloak
45    LGP GRIN Cloak
46    LGP Caustic Fan
47    LGP Birefringent Shear
48    LGP Anisotropic Cloak
49    LGP Evanescent Skin
50    LGP Color Temperature
51    LGP RGB Prism
52    LGP Complementary Mixing
53    LGP Quantum Colors
54    LGP Doppler Shift
55    LGP Color Accelerator
56    LGP DNA Helix
57    LGP Phase Transition
58    LGP Chromatic Aberration
59    LGP Perceptual Blend
60    LGP Chladni Harmonics
61    LGP Gravitational Wave Chirp
62    LGP Quantum Entanglement
63    LGP Mycelial Network
64    LGP Riley Dissonance
65    LGP Chromatic Lens
66    LGP Chromatic Pulse
67    LGP Chromatic Interference
68    Audio Test
69    Beat Pulse
70    Spectrum Bars
71    Bass Breath
72    Audio Waveform
73    Audio Bloom
74    LGP Star Burst (Narrative)
75    Chord Glow
76    Wave Reactive
77    LGP Perlin Veil
78    LGP Perlin Shocklines
79    LGP Perlin Caustics
80    LGP Perlin Interference Weave
81    LGP Perlin Veil Ambient
82    LGP Perlin Shocklines Ambient
83    LGP Perlin Caustics Ambient
84    LGP Perlin Interference Weave Ambient
85    Perlin Test: FastLED
86    Perlin Test: Emotiscope2 Full
87    Perlin Test: Emotiscope2 Quarter
88    BPM Enhanced
89    Breathing Enhanced
90    LGP Chevron Waves Enhanced
91    LGP Interference Scanner Enhanced
92    LGP Photonic Crystal Enhanced
93    LGP Spectrum Detail
94    LGP Spectrum Detail Enhanced
95    LGP Star Burst Enhanced
96    LGP Wave Collision Enhanced
97    Ripple Enhanced
98    Audio Bloom Parity
99    Audio Waveform Parity
100   LGP Holographic Auto-Cycle
101   ES Analog (Ref)
102   ES Spectrum (Ref)
103   ES Octave (Ref)
104   ES Bloom (Ref)
105   ES Waveform (Ref)
106   Ripple (ES tuned)
107   Heartbeat (ES tuned)
108   LGP Holographic (ES tuned)
109   SB Waveform (Ref)
110   Beat Pulse (Stack)
111   Beat Pulse (Shockwave)
112   Beat Pulse (Shockwave In) [REMOVED]
113   Beat Pulse (Void)
114   Beat Pulse (Resonant)
115   Beat Pulse (Ripple)
116   Beat Pulse (Shockwave Cascade)
117   Beat Pulse (Spectral)
118   Beat Pulse (Spectral Pulse)
119   Beat Pulse (Breathe)
120   Beat Pulse (LGP Interference)
121   Beat Pulse (Bloom)
122   Bloom (Parity)
123   Kuramoto Transport
124   LGP Opal Film
125   LGP Grating Scan
126   LGP Stress Glass
127   LGP Moire Silk
128   LGP Caustic Shards
129   LGP Parallax Depth
130   LGP Stress Glass (Melt)
131   LGP Grating Scan (Breakup)
132   LGP Water Caustics
133   LGP Schlieren Flow
134   LGP Reaction Diffusion
135   LGP RD Triangle
136   LGP Talbot Carpet
137   LGP Airy Comet
138   LGP Moire Cathedral
139   LGP Living Glyph
140   LGP Spirograph Crown
141   LGP Rose Bloom
142   LGP Harmonograph Halo
143   LGP Rule 30 Cathedral
144   LGP Langton Highway
145   LGP Cymatic Ladder
146   LGP Mach Diamonds
147   Chimera Crown
148   Catastrophe Caustics
149   Hyperbolic Portal
150   Lorenz Ribbon
151   IFS Botanical Relic
152   LGP Flux Rift
153   LGP Beat Prism
154   LGP Harmonic Tide
155   LGP Bass Quake
156   LGP Treble Net
157   LGP Rhythmic Gate
158   LGP Spectral Knot
159   LGP Saliency Bloom
160   LGP Transient Lattice
161   LGP Wavelet Mirror
```
