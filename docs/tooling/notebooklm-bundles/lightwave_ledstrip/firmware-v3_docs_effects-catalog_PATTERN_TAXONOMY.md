# LightwaveOS Effects -- Rendering Pattern Taxonomy

**Version:** 1.0.0
**Generated:** 2026-02-21 08:54:38

---

## Classification System

Every effect in the LightwaveOS library is classified along four orthogonal axes:

1. **Spatial Rendering Pattern** -- How the effect generates its visual output across the LED strip
2. **Colour Derivation Method** -- How the effect determines which colours to display
3. **Audio Coupling Mode** -- How (if at all) the effect responds to audio input
4. **Chroma Migration Pattern** -- Historical classification of how chroma-to-hue conversion was implemented

All effects adhere to the **centre-origin** constraint: visual patterns radiate from LED 79/80 outward
(or converge inward to 79/80). No linear sweeps are permitted.

## Spatial Rendering Patterns

### Field Computations

Continuous field computations including noise, reaction-diffusion, fire simulation, or coupled oscillators.

**Implementation markers:** `inoise8, perlin, reaction-diffusion, HeatColor, kuramoto`

**Centre-origin compliant:** Yes

**Effects using this pattern (36):**

| ID | Name | Class |
|----|------|-------|
| 0 | [Fire](EFFECTS_INVENTORY.md) | `FireEffect` |
| 34 | [LGP Aurora Borealis](EFFECTS_INVENTORY.md) | `LGPAuroraBorealisEffect` |
| 36 | [LGP Plasma Membrane](EFFECTS_INVENTORY.md) | `LGPPlasmaMembraneEffect` |
| 37 | [LGP Neural Network](EFFECTS_INVENTORY.md) | `LGPNeuralNetworkEffect` |
| 38 | [LGP Crystalline Growth](EFFECTS_INVENTORY.md) | `LGPCrystallineGrowthEffect` |
| 39 | [LGP Fluid Dynamics](EFFECTS_INVENTORY.md) | `LGPFluidDynamicsEffect` |
| 41 | [LGP Gravitational Lensing](EFFECTS_INVENTORY.md) | `LGPGravitationalLensingEffect` |
| 77 | [LGP Perlin Veil](EFFECTS_INVENTORY.md) | `LGPPerlinVeilEffect` |
| 78 | [LGP Perlin Shocklines](EFFECTS_INVENTORY.md) | `LGPPerlinShocklinesEffect` |
| 79 | [LGP Perlin Caustics](EFFECTS_INVENTORY.md) | `LGPPerlinCausticsEffect` |
| 80 | [LGP Perlin Interference Weave](EFFECTS_INVENTORY.md) | `LGPPerlinInterferenceWeaveEffect` |
| 81 | [LGP Perlin Veil Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinVeilAmbientEffect` |
| 82 | [LGP Perlin Shocklines Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinShocklinesAmbientEffect` |
| 83 | [LGP Perlin Caustics Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinCausticsAmbientEffect` |
| 84 | [LGP Perlin Interference Weave Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinInterferenceWeaveAmbientEffect` |
| 85 | [Perlin Test: FastLED](EFFECTS_INVENTORY.md) | `LGPPerlinBackendFastLEDEffect` |
| 86 | [Perlin Test: Emotiscope2 Full](EFFECTS_INVENTORY.md) | `LGPPerlinBackendEmotiscopeFullEffect` |
| 87 | [Perlin Test: Emotiscope2 Quarter](EFFECTS_INVENTORY.md) | `LGPPerlinBackendEmotiscopeQuarterEffect` |
| 88 | [BPM Enhanced](EFFECTS_INVENTORY.md) | `BPMEnhancedEffect` |
| 89 | [Breathing Enhanced](EFFECTS_INVENTORY.md) | `BreathingEnhancedEffect` |
| 90 | [LGP Chevron Waves Enhanced](EFFECTS_INVENTORY.md) | `ChevronWavesEnhancedEffect` |
| 92 | [LGP Photonic Crystal Enhanced](EFFECTS_INVENTORY.md) | `LGPPhotonicCrystalEnhancedEffect` |
| 95 | [LGP Star Burst Enhanced](EFFECTS_INVENTORY.md) | `LGPStarBurstEnhancedEffect` |
| 96 | [LGP Wave Collision Enhanced](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEnhancedEffect` |
| 123 | [Kuramoto Transport](EFFECTS_INVENTORY.md) | `KuramotoTransportEffect` |
| 136 | [LGP Talbot Carpet](EFFECTS_INVENTORY.md) | `LGPTalbotCarpetEffect` |
| 137 | [LGP Airy Comet](EFFECTS_INVENTORY.md) | `LGPAiryCometEffect` |
| 138 | [LGP Moire Cathedral](EFFECTS_INVENTORY.md) | `LGPMoireCathedralEffect` |
| 139 | [LGP Living Glyph](EFFECTS_INVENTORY.md) | `LGPSuperformulaGlyphEffect` |
| 140 | [LGP Spirograph Crown](EFFECTS_INVENTORY.md) | `LGPSpirographCrownEffect` |
| 141 | [LGP Rose Bloom](EFFECTS_INVENTORY.md) | `LGPRoseBloomEffect` |
| 142 | [LGP Harmonograph Halo](EFFECTS_INVENTORY.md) | `LGPHarmonographHaloEffect` |
| 143 | [LGP Rule 30 Cathedral](EFFECTS_INVENTORY.md) | `LGPRule30CathedralEffect` |
| 144 | [LGP Langton Highway](EFFECTS_INVENTORY.md) | `LGPLangtonHighwayEffect` |
| 145 | [LGP Cymatic Ladder](EFFECTS_INVENTORY.md) | `LGPCymaticLadderEffect` |
| 146 | [LGP Mach Diamonds](EFFECTS_INVENTORY.md) | `LGPMachDiamondsEffect` |

### Radial Expansion

LED values computed as a function of distance from centre points (LED 79/80), propagating outward.

**Implementation markers:** `centerPairDistance, SET_CENTER_PAIR, abs(i - center)`

**Centre-origin compliant:** Yes

**Effects using this pattern (109):**

| ID | Name | Class |
|----|------|-------|
| 1 | [Ocean](EFFECTS_INVENTORY.md) | `OceanEffect` |
| 5 | [Juggle](EFFECTS_INVENTORY.md) | `JuggleEffect` |
| 6 | [BPM](EFFECTS_INVENTORY.md) | `BPMEffect` |
| 7 | [Wave](EFFECTS_INVENTORY.md) | `WaveAmbientEffect` |
| 8 | [Ripple](EFFECTS_INVENTORY.md) | `RippleEffect` |
| 9 | [Heartbeat](EFFECTS_INVENTORY.md) | `HeartbeatEffect` |
| 10 | [Interference](EFFECTS_INVENTORY.md) | `InterferenceEffect` |
| 11 | [Breathing](EFFECTS_INVENTORY.md) | `BreathingEffect` |
| 13 | [LGP Box Wave](EFFECTS_INVENTORY.md) | `LGPBoxWaveEffect` |
| 14 | [LGP Holographic](EFFECTS_INVENTORY.md) | `LGPHolographicEffect` |
| 16 | [LGP Interference Scanner](EFFECTS_INVENTORY.md) | `LGPInterferenceScannerEffect` |
| 17 | [LGP Wave Collision](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEffect` |
| 18 | [LGP Diamond Lattice](EFFECTS_INVENTORY.md) | `LGPDiamondLatticeEffect` |
| 20 | [LGP Spiral Vortex](EFFECTS_INVENTORY.md) | `LGPSpiralVortexEffect` |
| 21 | [LGP Sierpinski](EFFECTS_INVENTORY.md) | `LGPSierpinskiEffect` |
| 22 | [LGP Chevron Waves](EFFECTS_INVENTORY.md) | `ChevronWavesEffect` |
| 23 | [LGP Concentric Rings](EFFECTS_INVENTORY.md) | `LGPConcentricRingsEffect` |
| 24 | [LGP Star Burst](EFFECTS_INVENTORY.md) | `LGPStarBurstEffect` |
| 25 | [LGP Mesh Network](EFFECTS_INVENTORY.md) | `LGPMeshNetworkEffect` |
| 26 | [LGP Moire Curtains](EFFECTS_INVENTORY.md) | `LGPMoireCurtainsEffect` |
| 27 | [LGP Radial Ripple](EFFECTS_INVENTORY.md) | `LGPRadialRippleEffect` |
| 28 | [LGP Holographic Vortex](EFFECTS_INVENTORY.md) | `LGPHolographicVortexEffect` |
| 29 | [LGP Evanescent Drift](EFFECTS_INVENTORY.md) | `LGPEvanescentDriftEffect` |
| 30 | [LGP Chromatic Shear](EFFECTS_INVENTORY.md) | `LGPChromaticShearEffect` |
| 31 | [LGP Modal Cavity](EFFECTS_INVENTORY.md) | `LGPModalCavityEffect` |
| 32 | [LGP Fresnel Zones](EFFECTS_INVENTORY.md) | `LGPFresnelZonesEffect` |
| 33 | [LGP Photonic Crystal](EFFECTS_INVENTORY.md) | `LGPPhotonicCrystalEffect` |
| 39 | [LGP Fluid Dynamics](EFFECTS_INVENTORY.md) | `LGPFluidDynamicsEffect` |
| 42 | [LGP Time Crystal](EFFECTS_INVENTORY.md) | `LGPTimeCrystalEffect` |
| 49 | [LGP Evanescent Skin](EFFECTS_INVENTORY.md) | `LGPEvanescentSkinEffect` |
| 50 | [LGP Color Temperature](EFFECTS_INVENTORY.md) | `LGPColorTemperatureEffect` |
| 51 | [LGP RGB Prism](EFFECTS_INVENTORY.md) | `LGPRGBPrismEffect` |
| 52 | [LGP Complementary Mixing](EFFECTS_INVENTORY.md) | `LGPComplementaryMixingEffect` |
| 53 | [LGP Quantum Colors](EFFECTS_INVENTORY.md) | `LGPQuantumColorsEffect` |
| 54 | [LGP Doppler Shift](EFFECTS_INVENTORY.md) | `LGPDopplerShiftEffect` |
| 55 | [LGP Color Accelerator](EFFECTS_INVENTORY.md) | `LGPColorAcceleratorEffect` |
| 56 | [LGP DNA Helix](EFFECTS_INVENTORY.md) | `LGPDNAHelixEffect` |
| 57 | [LGP Phase Transition](EFFECTS_INVENTORY.md) | `LGPPhaseTransitionEffect` |
| 58 | [LGP Chromatic Aberration](EFFECTS_INVENTORY.md) | `LGPChromaticAberrationEffect` |
| 59 | [LGP Perceptual Blend](EFFECTS_INVENTORY.md) | `LGPPerceptualBlendEffect` |
| 60 | [LGP Chladni Harmonics](EFFECTS_INVENTORY.md) | `LGPChladniHarmonicsEffect` |
| 61 | [LGP Gravitational Wave Chirp](EFFECTS_INVENTORY.md) | `LGPGravitationalWaveChirpEffect` |
| 62 | [LGP Quantum Entanglement](EFFECTS_INVENTORY.md) | `LGPQuantumEntanglementEffect` |
| 63 | [LGP Mycelial Network](EFFECTS_INVENTORY.md) | `LGPMycelialNetworkEffect` |
| 64 | [LGP Riley Dissonance](EFFECTS_INVENTORY.md) | `LGPRileyDissonanceEffect` |
| 65 | [LGP Chromatic Lens](EFFECTS_INVENTORY.md) | `LGPChromaticLensEffect` |
| 66 | [LGP Chromatic Pulse](EFFECTS_INVENTORY.md) | `LGPChromaticPulseEffect` |
| 67 | [LGP Chromatic Interference](EFFECTS_INVENTORY.md) | `ChromaticInterferenceEffect` |
| 68 | [Audio Test](EFFECTS_INVENTORY.md) | `LGPAudioTestEffect` |
| 69 | [Beat Pulse](EFFECTS_INVENTORY.md) | `LGPBeatPulseEffect` |
| 70 | [Spectrum Bars](EFFECTS_INVENTORY.md) | `LGPSpectrumBarsEffect` |
| 71 | [Bass Breath](EFFECTS_INVENTORY.md) | `LGPBassBreathEffect` |
| 73 | [Audio Bloom](EFFECTS_INVENTORY.md) | `AudioBloomEffect` |
| 74 | [LGP Star Burst (Narrative)](EFFECTS_INVENTORY.md) | `LGPStarBurstNarrativeEffect` |
| 75 | [Chord Glow](EFFECTS_INVENTORY.md) | `LGPChordGlowEffect` |
| 76 | [Wave Reactive](EFFECTS_INVENTORY.md) | `WaveReactiveEffect` |
| 77 | [LGP Perlin Veil](EFFECTS_INVENTORY.md) | `LGPPerlinVeilEffect` |
| 78 | [LGP Perlin Shocklines](EFFECTS_INVENTORY.md) | `LGPPerlinShocklinesEffect` |
| 79 | [LGP Perlin Caustics](EFFECTS_INVENTORY.md) | `LGPPerlinCausticsEffect` |
| 80 | [LGP Perlin Interference Weave](EFFECTS_INVENTORY.md) | `LGPPerlinInterferenceWeaveEffect` |
| 81 | [LGP Perlin Veil Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinVeilAmbientEffect` |
| 82 | [LGP Perlin Shocklines Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinShocklinesAmbientEffect` |
| 83 | [LGP Perlin Caustics Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinCausticsAmbientEffect` |
| 84 | [LGP Perlin Interference Weave Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinInterferenceWeaveAmbientEffect` |
| 85 | [Perlin Test: FastLED](EFFECTS_INVENTORY.md) | `LGPPerlinBackendFastLEDEffect` |
| 86 | [Perlin Test: Emotiscope2 Full](EFFECTS_INVENTORY.md) | `LGPPerlinBackendEmotiscopeFullEffect` |
| 87 | [Perlin Test: Emotiscope2 Quarter](EFFECTS_INVENTORY.md) | `LGPPerlinBackendEmotiscopeQuarterEffect` |
| 88 | [BPM Enhanced](EFFECTS_INVENTORY.md) | `BPMEnhancedEffect` |
| 89 | [Breathing Enhanced](EFFECTS_INVENTORY.md) | `BreathingEnhancedEffect` |
| 90 | [LGP Chevron Waves Enhanced](EFFECTS_INVENTORY.md) | `ChevronWavesEnhancedEffect` |
| 91 | [LGP Interference Scanner Enhanced](EFFECTS_INVENTORY.md) | `LGPInterferenceScannerEnhancedEffect` |
| 92 | [LGP Photonic Crystal Enhanced](EFFECTS_INVENTORY.md) | `LGPPhotonicCrystalEnhancedEffect` |
| 95 | [LGP Star Burst Enhanced](EFFECTS_INVENTORY.md) | `LGPStarBurstEnhancedEffect` |
| 96 | [LGP Wave Collision Enhanced](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEnhancedEffect` |
| 97 | [Ripple Enhanced](EFFECTS_INVENTORY.md) | `RippleEnhancedEffect` |
| 99 | [Audio Waveform Parity](EFFECTS_INVENTORY.md) | `TrinityTestEffect` |
| 100 | [LGP Holographic Auto-Cycle](EFFECTS_INVENTORY.md) | `LGPHolographicAutoCycleEffect` |
| 101 | [ES Analog (Ref)](EFFECTS_INVENTORY.md) | `EsAnalogRefEffect` |
| 102 | [ES Spectrum (Ref)](EFFECTS_INVENTORY.md) | `EsSpectrumRefEffect` |
| 103 | [ES Octave (Ref)](EFFECTS_INVENTORY.md) | `EsOctaveRefEffect` |
| 104 | [ES Bloom (Ref)](EFFECTS_INVENTORY.md) | `EsBloomRefEffect` |
| 105 | [ES Waveform (Ref)](EFFECTS_INVENTORY.md) | `EsWaveformRefEffect` |
| 106 | [Ripple (ES tuned)](EFFECTS_INVENTORY.md) | `RippleEsTunedEffect` |
| 107 | [Heartbeat (ES tuned)](EFFECTS_INVENTORY.md) | `HeartbeatEsTunedEffect` |
| 108 | [LGP Holographic (ES tuned)](EFFECTS_INVENTORY.md) | `LGPHolographicEsTunedEffect` |
| 109 | [SB Waveform (Ref)](EFFECTS_INVENTORY.md) | `SbWaveform310RefEffect` |
| 110 | [Beat Pulse (Stack)](EFFECTS_INVENTORY.md) | `BeatPulseStackEffect` |
| 111 | [Beat Pulse (Shockwave)](EFFECTS_INVENTORY.md) | `BeatPulseShockwaveEffect` |
| 113 | [Beat Pulse (Void)](EFFECTS_INVENTORY.md) | `BeatPulseVoidEffect` |
| 114 | [Beat Pulse (Resonant)](EFFECTS_INVENTORY.md) | `BeatPulseResonantEffect` |
| 115 | [Beat Pulse (Ripple)](EFFECTS_INVENTORY.md) | `BeatPulseRippleEffect` |
| 116 | [Beat Pulse (Shockwave Cascade)](EFFECTS_INVENTORY.md) | `BeatPulseShockwaveCascadeEffect` |
| 117 | [Beat Pulse (Spectral)](EFFECTS_INVENTORY.md) | `BeatPulseSpectralEffect` |
| 118 | [Beat Pulse (Spectral Pulse)](EFFECTS_INVENTORY.md) | `BeatPulseSpectralPulseEffect` |
| 119 | [Beat Pulse (Breathe)](EFFECTS_INVENTORY.md) | `BeatPulseBreatheEffect` |
| 121 | [Beat Pulse (Bloom)](EFFECTS_INVENTORY.md) | `BeatPulseBloomEffect` |
| 122 | [Bloom (Parity)](EFFECTS_INVENTORY.md) | `BloomParityEffect` |
| 124 | [LGP Opal Film](EFFECTS_INVENTORY.md) | `LGPOpalFilmEffect` |
| 125 | [LGP Grating Scan](EFFECTS_INVENTORY.md) | `LGPGratingScanEffect` |
| 126 | [LGP Stress Glass](EFFECTS_INVENTORY.md) | `LGPStressGlassEffect` |
| 127 | [LGP Moire Silk](EFFECTS_INVENTORY.md) | `LGPMoireSilkEffect` |
| 128 | [LGP Caustic Shards](EFFECTS_INVENTORY.md) | `LGPCausticShardsEffect` |
| 129 | [LGP Parallax Depth](EFFECTS_INVENTORY.md) | `LGPParallaxDepthEffect` |
| 130 | [LGP Stress Glass (Melt)](EFFECTS_INVENTORY.md) | `LGPStressGlassMeltEffect` |
| 131 | [LGP Grating Scan (Breakup)](EFFECTS_INVENTORY.md) | `LGPGratingScanBreakupEffect` |
| 132 | [LGP Water Caustics](EFFECTS_INVENTORY.md) | `LGPWaterCausticsEffect` |
| 133 | [LGP Schlieren Flow](EFFECTS_INVENTORY.md) | `LGPSchlierenFlowEffect` |
| 134 | [LGP Reaction Diffusion](EFFECTS_INVENTORY.md) | `LGPReactionDiffusionEffect` |
| 135 | [LGP RD Triangle](EFFECTS_INVENTORY.md) | `LGPReactionDiffusionTriangleEffect` |

### Wave Propagation

Wavefronts expanding or contracting from/to centre with sinusoidal or triangular profiles.

**Implementation markers:** `sinf(distFromCenter), sin8(), wave equation`

**Centre-origin compliant:** Yes

**Effects using this pattern (75):**

| ID | Name | Class |
|----|------|-------|
| 1 | [Ocean](EFFECTS_INVENTORY.md) | `OceanEffect` |
| 2 | [Plasma](EFFECTS_INVENTORY.md) | `PlasmaEffect` |
| 6 | [BPM](EFFECTS_INVENTORY.md) | `BPMEffect` |
| 7 | [Wave](EFFECTS_INVENTORY.md) | `WaveAmbientEffect` |
| 10 | [Interference](EFFECTS_INVENTORY.md) | `InterferenceEffect` |
| 11 | [Breathing](EFFECTS_INVENTORY.md) | `BreathingEffect` |
| 12 | [Pulse](EFFECTS_INVENTORY.md) | `PulseEffect` |
| 13 | [LGP Box Wave](EFFECTS_INVENTORY.md) | `LGPBoxWaveEffect` |
| 14 | [LGP Holographic](EFFECTS_INVENTORY.md) | `LGPHolographicEffect` |
| 15 | [LGP Modal Resonance](EFFECTS_INVENTORY.md) | `ModalResonanceEffect` |
| 16 | [LGP Interference Scanner](EFFECTS_INVENTORY.md) | `LGPInterferenceScannerEffect` |
| 17 | [LGP Wave Collision](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEffect` |
| 18 | [LGP Diamond Lattice](EFFECTS_INVENTORY.md) | `LGPDiamondLatticeEffect` |
| 19 | [LGP Hexagonal Grid](EFFECTS_INVENTORY.md) | `LGPHexagonalGridEffect` |
| 20 | [LGP Spiral Vortex](EFFECTS_INVENTORY.md) | `LGPSpiralVortexEffect` |
| 21 | [LGP Sierpinski](EFFECTS_INVENTORY.md) | `LGPSierpinskiEffect` |
| 22 | [LGP Chevron Waves](EFFECTS_INVENTORY.md) | `ChevronWavesEffect` |
| 23 | [LGP Concentric Rings](EFFECTS_INVENTORY.md) | `LGPConcentricRingsEffect` |
| 24 | [LGP Star Burst](EFFECTS_INVENTORY.md) | `LGPStarBurstEffect` |
| 25 | [LGP Mesh Network](EFFECTS_INVENTORY.md) | `LGPMeshNetworkEffect` |
| 26 | [LGP Moire Curtains](EFFECTS_INVENTORY.md) | `LGPMoireCurtainsEffect` |
| 27 | [LGP Radial Ripple](EFFECTS_INVENTORY.md) | `LGPRadialRippleEffect` |
| 28 | [LGP Holographic Vortex](EFFECTS_INVENTORY.md) | `LGPHolographicVortexEffect` |
| 29 | [LGP Evanescent Drift](EFFECTS_INVENTORY.md) | `LGPEvanescentDriftEffect` |
| 31 | [LGP Modal Cavity](EFFECTS_INVENTORY.md) | `LGPModalCavityEffect` |
| 33 | [LGP Photonic Crystal](EFFECTS_INVENTORY.md) | `LGPPhotonicCrystalEffect` |
| 35 | [LGP Bioluminescent Waves](EFFECTS_INVENTORY.md) | `LGPBioluminescentWavesEffect` |
| 39 | [LGP Fluid Dynamics](EFFECTS_INVENTORY.md) | `LGPFluidDynamicsEffect` |
| 42 | [LGP Time Crystal](EFFECTS_INVENTORY.md) | `LGPTimeCrystalEffect` |
| 46 | [LGP Caustic Fan](EFFECTS_INVENTORY.md) | `LGPCausticFanEffect` |
| 47 | [LGP Birefringent Shear](EFFECTS_INVENTORY.md) | `LGPBirefringentShearEffect` |
| 49 | [LGP Evanescent Skin](EFFECTS_INVENTORY.md) | `LGPEvanescentSkinEffect` |
| 51 | [LGP RGB Prism](EFFECTS_INVENTORY.md) | `LGPRGBPrismEffect` |
| 53 | [LGP Quantum Colors](EFFECTS_INVENTORY.md) | `LGPQuantumColorsEffect` |
| 56 | [LGP DNA Helix](EFFECTS_INVENTORY.md) | `LGPDNAHelixEffect` |
| 57 | [LGP Phase Transition](EFFECTS_INVENTORY.md) | `LGPPhaseTransitionEffect` |
| 58 | [LGP Chromatic Aberration](EFFECTS_INVENTORY.md) | `LGPChromaticAberrationEffect` |
| 59 | [LGP Perceptual Blend](EFFECTS_INVENTORY.md) | `LGPPerceptualBlendEffect` |
| 60 | [LGP Chladni Harmonics](EFFECTS_INVENTORY.md) | `LGPChladniHarmonicsEffect` |
| 61 | [LGP Gravitational Wave Chirp](EFFECTS_INVENTORY.md) | `LGPGravitationalWaveChirpEffect` |
| 62 | [LGP Quantum Entanglement](EFFECTS_INVENTORY.md) | `LGPQuantumEntanglementEffect` |
| 63 | [LGP Mycelial Network](EFFECTS_INVENTORY.md) | `LGPMycelialNetworkEffect` |
| 64 | [LGP Riley Dissonance](EFFECTS_INVENTORY.md) | `LGPRileyDissonanceEffect` |
| 65 | [LGP Chromatic Lens](EFFECTS_INVENTORY.md) | `LGPChromaticLensEffect` |
| 66 | [LGP Chromatic Pulse](EFFECTS_INVENTORY.md) | `LGPChromaticPulseEffect` |
| 67 | [LGP Chromatic Interference](EFFECTS_INVENTORY.md) | `ChromaticInterferenceEffect` |
| 68 | [Audio Test](EFFECTS_INVENTORY.md) | `LGPAudioTestEffect` |
| 70 | [Spectrum Bars](EFFECTS_INVENTORY.md) | `LGPSpectrumBarsEffect` |
| 71 | [Bass Breath](EFFECTS_INVENTORY.md) | `LGPBassBreathEffect` |
| 74 | [LGP Star Burst (Narrative)](EFFECTS_INVENTORY.md) | `LGPStarBurstNarrativeEffect` |
| 75 | [Chord Glow](EFFECTS_INVENTORY.md) | `LGPChordGlowEffect` |
| 76 | [Wave Reactive](EFFECTS_INVENTORY.md) | `WaveReactiveEffect` |
| 77 | [LGP Perlin Veil](EFFECTS_INVENTORY.md) | `LGPPerlinVeilEffect` |
| 81 | [LGP Perlin Veil Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinVeilAmbientEffect` |
| 83 | [LGP Perlin Caustics Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinCausticsAmbientEffect` |
| 84 | [LGP Perlin Interference Weave Ambient](EFFECTS_INVENTORY.md) | `LGPPerlinInterferenceWeaveAmbientEffect` |
| 88 | [BPM Enhanced](EFFECTS_INVENTORY.md) | `BPMEnhancedEffect` |
| 89 | [Breathing Enhanced](EFFECTS_INVENTORY.md) | `BreathingEnhancedEffect` |
| 90 | [LGP Chevron Waves Enhanced](EFFECTS_INVENTORY.md) | `ChevronWavesEnhancedEffect` |
| 91 | [LGP Interference Scanner Enhanced](EFFECTS_INVENTORY.md) | `LGPInterferenceScannerEnhancedEffect` |
| 92 | [LGP Photonic Crystal Enhanced](EFFECTS_INVENTORY.md) | `LGPPhotonicCrystalEnhancedEffect` |
| 95 | [LGP Star Burst Enhanced](EFFECTS_INVENTORY.md) | `LGPStarBurstEnhancedEffect` |
| 96 | [LGP Wave Collision Enhanced](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEnhancedEffect` |
| 100 | [LGP Holographic Auto-Cycle](EFFECTS_INVENTORY.md) | `LGPHolographicAutoCycleEffect` |
| 108 | [LGP Holographic (ES tuned)](EFFECTS_INVENTORY.md) | `LGPHolographicEsTunedEffect` |
| 117 | [Beat Pulse (Spectral)](EFFECTS_INVENTORY.md) | `BeatPulseSpectralEffect` |
| 118 | [Beat Pulse (Spectral Pulse)](EFFECTS_INVENTORY.md) | `BeatPulseSpectralPulseEffect` |
| 124 | [LGP Opal Film](EFFECTS_INVENTORY.md) | `LGPOpalFilmEffect` |
| 126 | [LGP Stress Glass](EFFECTS_INVENTORY.md) | `LGPStressGlassEffect` |
| 127 | [LGP Moire Silk](EFFECTS_INVENTORY.md) | `LGPMoireSilkEffect` |
| 128 | [LGP Caustic Shards](EFFECTS_INVENTORY.md) | `LGPCausticShardsEffect` |
| 129 | [LGP Parallax Depth](EFFECTS_INVENTORY.md) | `LGPParallaxDepthEffect` |
| 130 | [LGP Stress Glass (Melt)](EFFECTS_INVENTORY.md) | `LGPStressGlassMeltEffect` |
| 132 | [LGP Water Caustics](EFFECTS_INVENTORY.md) | `LGPWaterCausticsEffect` |
| 133 | [LGP Schlieren Flow](EFFECTS_INVENTORY.md) | `LGPSchlierenFlowEffect` |

### Particle Systems

Discrete particle-like elements with position/velocity state, bouncing or moving independently.

**Implementation markers:** `velocity, position[], particle`

**Centre-origin compliant:** Yes

**Effects using this pattern (9):**

| ID | Name | Class |
|----|------|-------|
| 3 | [Confetti](EFFECTS_INVENTORY.md) | `ConfettiEffect` |
| 4 | [Sinelon](EFFECTS_INVENTORY.md) | `SinelonEffect` |
| 40 | [LGP Quantum Tunneling](EFFECTS_INVENTORY.md) | `LGPQuantumTunnelingEffect` |
| 43 | [LGP Soliton Waves](EFFECTS_INVENTORY.md) | `LGPSolitonWavesEffect` |
| 44 | [LGP Metamaterial Cloak](EFFECTS_INVENTORY.md) | `LGPMetamaterialCloakEffect` |
| 45 | [LGP GRIN Cloak](EFFECTS_INVENTORY.md) | `LGPGrinCloakEffect` |
| 48 | [LGP Anisotropic Cloak](EFFECTS_INVENTORY.md) | `LGPAnisotropicCloakEffect` |
| 94 | [LGP Spectrum Detail Enhanced](EFFECTS_INVENTORY.md) | `LGPSpectrumDetailEnhancedEffect` |
| 123 | [Kuramoto Transport](EFFECTS_INVENTORY.md) | `KuramotoTransportEffect` |

### Interference

Superposition of multiple wave sources creating interference patterns, moire, or standing waves.

**Implementation markers:** `moire, superposition, standing wave`

**Centre-origin compliant:** Yes

**Effects using this pattern (29):**

| ID | Name | Class |
|----|------|-------|
| 13 | [LGP Box Wave](EFFECTS_INVENTORY.md) | `LGPBoxWaveEffect` |
| 17 | [LGP Wave Collision](EFFECTS_INVENTORY.md) | `LGPWaveCollisionEffect` |
| 23 | [LGP Concentric Rings](EFFECTS_INVENTORY.md) | `LGPConcentricRingsEffect` |
| 26 | [LGP Moire Curtains](EFFECTS_INVENTORY.md) | `LGPMoireCurtainsEffect` |
| 60 | [LGP Chladni Harmonics](EFFECTS_INVENTORY.md) | `LGPChladniHarmonicsEffect` |
| 62 | [LGP Quantum Entanglement](EFFECTS_INVENTORY.md) | `LGPQuantumEntanglementEffect` |
| 120 | [Beat Pulse (LGP Interference)](EFFECTS_INVENTORY.md) | `BeatPulseLGPInterferenceEffect` |
| 127 | [LGP Moire Silk](EFFECTS_INVENTORY.md) | `LGPMoireSilkEffect` |
| 136 | [LGP Talbot Carpet](EFFECTS_INVENTORY.md) | `LGPTalbotCarpetEffect` |
| 137 | [LGP Airy Comet](EFFECTS_INVENTORY.md) | `LGPAiryCometEffect` |
| 138 | [LGP Moire Cathedral](EFFECTS_INVENTORY.md) | `LGPMoireCathedralEffect` |
| 139 | [LGP Living Glyph](EFFECTS_INVENTORY.md) | `LGPSuperformulaGlyphEffect` |
| 140 | [LGP Spirograph Crown](EFFECTS_INVENTORY.md) | `LGPSpirographCrownEffect` |
| 141 | [LGP Rose Bloom](EFFECTS_INVENTORY.md) | `LGPRoseBloomEffect` |
| 142 | [LGP Harmonograph Halo](EFFECTS_INVENTORY.md) | `LGPHarmonographHaloEffect` |
| 143 | [LGP Rule 30 Cathedral](EFFECTS_INVENTORY.md) | `LGPRule30CathedralEffect` |
| 144 | [LGP Langton Highway](EFFECTS_INVENTORY.md) | `LGPLangtonHighwayEffect` |
| 145 | [LGP Cymatic Ladder](EFFECTS_INVENTORY.md) | `LGPCymaticLadderEffect` |
| 146 | [LGP Mach Diamonds](EFFECTS_INVENTORY.md) | `LGPMachDiamondsEffect` |
| 152 | [LGP Flux Rift](EFFECTS_INVENTORY.md) | `LGPFluxRiftEffect` |
| 153 | [LGP Beat Prism](EFFECTS_INVENTORY.md) | `LGPBeatPrismEffect` |
| 154 | [LGP Harmonic Tide](EFFECTS_INVENTORY.md) | `LGPHarmonicTideEffect` |
| 155 | [LGP Bass Quake](EFFECTS_INVENTORY.md) | `LGPBassQuakeEffect` |
| 156 | [LGP Treble Net](EFFECTS_INVENTORY.md) | `LGPTrebleNetEffect` |
| 157 | [LGP Rhythmic Gate](EFFECTS_INVENTORY.md) | `LGPRhythmicGateEffect` |
| 158 | [LGP Spectral Knot](EFFECTS_INVENTORY.md) | `LGPSpectralKnotEffect` |
| 159 | [LGP Saliency Bloom](EFFECTS_INVENTORY.md) | `LGPSaliencyBloomEffect` |
| 160 | [LGP Transient Lattice](EFFECTS_INVENTORY.md) | `LGPTransientLatticeEffect` |
| 161 | [LGP Wavelet Mirror](EFFECTS_INVENTORY.md) | `LGPWaveletMirrorEffect` |

### Geometric

Mathematical geometric shapes including fractals, superformulae, spirographs, cellular automata.

**Implementation markers:** `fractal, superformula, spirograph, rule30, IFS`

**Centre-origin compliant:** Yes

**Effects using this pattern (18):**

| ID | Name | Class |
|----|------|-------|
| 21 | [LGP Sierpinski](EFFECTS_INVENTORY.md) | `LGPSierpinskiEffect` |
| 63 | [LGP Mycelial Network](EFFECTS_INVENTORY.md) | `LGPMycelialNetworkEffect` |
| 136 | [LGP Talbot Carpet](EFFECTS_INVENTORY.md) | `LGPTalbotCarpetEffect` |
| 137 | [LGP Airy Comet](EFFECTS_INVENTORY.md) | `LGPAiryCometEffect` |
| 138 | [LGP Moire Cathedral](EFFECTS_INVENTORY.md) | `LGPMoireCathedralEffect` |
| 139 | [LGP Living Glyph](EFFECTS_INVENTORY.md) | `LGPSuperformulaGlyphEffect` |
| 140 | [LGP Spirograph Crown](EFFECTS_INVENTORY.md) | `LGPSpirographCrownEffect` |
| 141 | [LGP Rose Bloom](EFFECTS_INVENTORY.md) | `LGPRoseBloomEffect` |
| 142 | [LGP Harmonograph Halo](EFFECTS_INVENTORY.md) | `LGPHarmonographHaloEffect` |
| 143 | [LGP Rule 30 Cathedral](EFFECTS_INVENTORY.md) | `LGPRule30CathedralEffect` |
| 144 | [LGP Langton Highway](EFFECTS_INVENTORY.md) | `LGPLangtonHighwayEffect` |
| 145 | [LGP Cymatic Ladder](EFFECTS_INVENTORY.md) | `LGPCymaticLadderEffect` |
| 146 | [LGP Mach Diamonds](EFFECTS_INVENTORY.md) | `LGPMachDiamondsEffect` |
| 147 | [Chimera Crown](EFFECTS_INVENTORY.md) | `LGPChimeraCrownEffect` |
| 148 | [Catastrophe Caustics](EFFECTS_INVENTORY.md) | `LGPCatastropheCausticsEffect` |
| 149 | [Hyperbolic Portal](EFFECTS_INVENTORY.md) | `LGPHyperbolicPortalEffect` |
| 150 | [Lorenz Ribbon](EFFECTS_INVENTORY.md) | `LGPLorenzRibbonEffect` |
| 151 | [IFS Botanical Relic](EFFECTS_INVENTORY.md) | `LGPIFSBioRelicEffect` |

### Waveform Visualisation

Direct rendering of audio waveform samples or spectrum data onto LED positions.

**Implementation markers:** `samples[], waveform, waveBuf`

**Centre-origin compliant:** Yes

**Effects using this pattern (15):**

| ID | Name | Class |
|----|------|-------|
| 72 | [Audio Waveform](EFFECTS_INVENTORY.md) | `AudioWaveformEffect` |
| 93 | [LGP Spectrum Detail](EFFECTS_INVENTORY.md) | `LGPSpectrumDetailEffect` |
| 98 | [Audio Bloom Parity](EFFECTS_INVENTORY.md) | `SnapwaveLinearEffect` |
| 105 | [ES Waveform (Ref)](EFFECTS_INVENTORY.md) | `EsWaveformRefEffect` |
| 109 | [SB Waveform (Ref)](EFFECTS_INVENTORY.md) | `SbWaveform310RefEffect` |
| 152 | [LGP Flux Rift](EFFECTS_INVENTORY.md) | `LGPFluxRiftEffect` |
| 153 | [LGP Beat Prism](EFFECTS_INVENTORY.md) | `LGPBeatPrismEffect` |
| 154 | [LGP Harmonic Tide](EFFECTS_INVENTORY.md) | `LGPHarmonicTideEffect` |
| 155 | [LGP Bass Quake](EFFECTS_INVENTORY.md) | `LGPBassQuakeEffect` |
| 156 | [LGP Treble Net](EFFECTS_INVENTORY.md) | `LGPTrebleNetEffect` |
| 157 | [LGP Rhythmic Gate](EFFECTS_INVENTORY.md) | `LGPRhythmicGateEffect` |
| 158 | [LGP Spectral Knot](EFFECTS_INVENTORY.md) | `LGPSpectralKnotEffect` |
| 159 | [LGP Saliency Bloom](EFFECTS_INVENTORY.md) | `LGPSaliencyBloomEffect` |
| 160 | [LGP Transient Lattice](EFFECTS_INVENTORY.md) | `LGPTransientLatticeEffect` |
| 161 | [LGP Wavelet Mirror](EFFECTS_INVENTORY.md) | `LGPWaveletMirrorEffect` |

## Colour Derivation Methods

### Circular Chroma (Smoothed)

Weighted circular weighted mean of 12 chroma bins with circular EMA temporal smoothing

**Function:** [`circularChromaHueSmoothed()`](MATH_APPENDIX.md#circularchromahuesmoothed)

**Effects using this method (33):**

| ID | Name |
|----|------|
| 5 | Juggle |
| 8 | Ripple |
| 16 | LGP Interference Scanner |
| 17 | LGP Wave Collision |
| 22 | LGP Chevron Waves |
| 24 | LGP Star Burst |
| 33 | LGP Photonic Crystal |
| 69 | Beat Pulse |
| 70 | Spectrum Bars |
| 71 | Bass Breath |
| 72 | Audio Waveform |
| 74 | LGP Star Burst (Narrative) |
| 80 | LGP Perlin Interference Weave |
| 88 | BPM Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 97 | Ripple Enhanced |
| 106 | Ripple (ES tuned) |
| 107 | Heartbeat (ES tuned) |
| 108 | LGP Holographic (ES tuned) |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

### Chroma Hue (Instantaneous)

Chroma-derived hue using pack-specific functions (e.g. selectMusicalHue, dominantNoteFromBins)

**Function:** [`selectMusicalHue() / dominantNoteFromBins()`](MATH_APPENDIX.md#selectmusicalhue / dominantnotefrombins)

**Effects using this method (15):**

- **BPM** (ID 6)
- **Breathing** (ID 11)
- **LGP Chromatic Lens** (ID 65)
- **LGP Chromatic Pulse** (ID 66)
- **LGP Chromatic Interference** (ID 67)
- **Audio Bloom** (ID 73)
- **Chord Glow** (ID 75)
- **Breathing Enhanced** (ID 89)
- **LGP Spectrum Detail** (ID 93)
- **Audio Bloom Parity** (ID 98)
- **ES Octave (Ref)** (ID 103)
- **ES Bloom (Ref)** (ID 104)
- **SB Waveform (Ref)** (ID 109)
- **Bloom (Parity)** (ID 122)
- **LGP Parallax Depth** (ID 129)

### Palette Position

LED position or computed value mapped to palette index

**Function:** [`ctx.palette.getColor(position)`](MATH_APPENDIX.md#ctx.palette.getcolor(position))

**Effects using this method (152):**

| ID | Name |
|----|------|
| 1 | Ocean |
| 2 | Plasma |
| 3 | Confetti |
| 4 | Sinelon |
| 5 | Juggle |
| 6 | BPM |
| 7 | Wave |
| 8 | Ripple |
| 9 | Heartbeat |
| 10 | Interference |
| 11 | Breathing |
| 12 | Pulse |
| 13 | LGP Box Wave |
| 14 | LGP Holographic |
| 15 | LGP Modal Resonance |
| 16 | LGP Interference Scanner |
| 17 | LGP Wave Collision |
| 18 | LGP Diamond Lattice |
| 19 | LGP Hexagonal Grid |
| 20 | LGP Spiral Vortex |
| 21 | LGP Sierpinski |
| 22 | LGP Chevron Waves |
| 23 | LGP Concentric Rings |
| 24 | LGP Star Burst |
| 25 | LGP Mesh Network |
| 26 | LGP Moire Curtains |
| 27 | LGP Radial Ripple |
| 28 | LGP Holographic Vortex |
| 29 | LGP Evanescent Drift |
| 30 | LGP Chromatic Shear |
| 31 | LGP Modal Cavity |
| 32 | LGP Fresnel Zones |
| 33 | LGP Photonic Crystal |
| 34 | LGP Aurora Borealis |
| 36 | LGP Plasma Membrane |
| 38 | LGP Crystalline Growth |
| 39 | LGP Fluid Dynamics |
| 40 | LGP Quantum Tunneling |
| 41 | LGP Gravitational Lensing |
| 42 | LGP Time Crystal |
| 43 | LGP Soliton Waves |
| 44 | LGP Metamaterial Cloak |
| 45 | LGP GRIN Cloak |
| 46 | LGP Caustic Fan |
| 47 | LGP Birefringent Shear |
| 48 | LGP Anisotropic Cloak |
| 49 | LGP Evanescent Skin |
| 51 | LGP RGB Prism |
| 52 | LGP Complementary Mixing |
| 53 | LGP Quantum Colors |
| 54 | LGP Doppler Shift |
| 55 | LGP Color Accelerator |
| 56 | LGP DNA Helix |
| 57 | LGP Phase Transition |
| 58 | LGP Chromatic Aberration |
| 59 | LGP Perceptual Blend |
| 60 | LGP Chladni Harmonics |
| 61 | LGP Gravitational Wave Chirp |
| 62 | LGP Quantum Entanglement |
| 63 | LGP Mycelial Network |
| 64 | LGP Riley Dissonance |
| 65 | LGP Chromatic Lens |
| 66 | LGP Chromatic Pulse |
| 67 | LGP Chromatic Interference |
| 68 | Audio Test |
| 69 | Beat Pulse |
| 70 | Spectrum Bars |
| 71 | Bass Breath |
| 72 | Audio Waveform |
| 73 | Audio Bloom |
| 74 | LGP Star Burst (Narrative) |
| 75 | Chord Glow |
| 76 | Wave Reactive |
| 77 | LGP Perlin Veil |
| 78 | LGP Perlin Shocklines |
| 79 | LGP Perlin Caustics |
| 80 | LGP Perlin Interference Weave |
| 81 | LGP Perlin Veil Ambient |
| 82 | LGP Perlin Shocklines Ambient |
| 83 | LGP Perlin Caustics Ambient |
| 84 | LGP Perlin Interference Weave Ambient |
| 85 | Perlin Test: FastLED |
| 86 | Perlin Test: Emotiscope2 Full |
| 87 | Perlin Test: Emotiscope2 Quarter |
| 88 | BPM Enhanced |
| 89 | Breathing Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 93 | LGP Spectrum Detail |
| 94 | LGP Spectrum Detail Enhanced |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 97 | Ripple Enhanced |
| 98 | Audio Bloom Parity |
| 100 | LGP Holographic Auto-Cycle |
| 101 | ES Analog (Ref) |
| 102 | ES Spectrum (Ref) |
| 105 | ES Waveform (Ref) |
| 106 | Ripple (ES tuned) |
| 107 | Heartbeat (ES tuned) |
| 108 | LGP Holographic (ES tuned) |
| 110 | Beat Pulse (Stack) |
| 111 | Beat Pulse (Shockwave) |
| 113 | Beat Pulse (Void) |
| 114 | Beat Pulse (Resonant) |
| 115 | Beat Pulse (Ripple) |
| 116 | Beat Pulse (Shockwave Cascade) |
| 117 | Beat Pulse (Spectral) |
| 118 | Beat Pulse (Spectral Pulse) |
| 119 | Beat Pulse (Breathe) |
| 120 | Beat Pulse (LGP Interference) |
| 121 | Beat Pulse (Bloom) |
| 122 | Bloom (Parity) |
| 123 | Kuramoto Transport |
| 125 | LGP Grating Scan |
| 126 | LGP Stress Glass |
| 127 | LGP Moire Silk |
| 128 | LGP Caustic Shards |
| 129 | LGP Parallax Depth |
| 130 | LGP Stress Glass (Melt) |
| 131 | LGP Grating Scan (Breakup) |
| 132 | LGP Water Caustics |
| 133 | LGP Schlieren Flow |
| 134 | LGP Reaction Diffusion |
| 135 | LGP RD Triangle |
| 136 | LGP Talbot Carpet |
| 137 | LGP Airy Comet |
| 138 | LGP Moire Cathedral |
| 139 | LGP Living Glyph |
| 140 | LGP Spirograph Crown |
| 141 | LGP Rose Bloom |
| 142 | LGP Harmonograph Halo |
| 143 | LGP Rule 30 Cathedral |
| 144 | LGP Langton Highway |
| 145 | LGP Cymatic Ladder |
| 146 | LGP Mach Diamonds |
| 147 | Chimera Crown |
| 148 | Catastrophe Caustics |
| 149 | Hyperbolic Portal |
| 150 | Lorenz Ribbon |
| 151 | IFS Botanical Relic |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

### Global Hue

Uses ctx.gHue (auto-incrementing global hue)

**Function:** [`ctx.gHue`](MATH_APPENDIX.md#ctx.ghue)

**Effects using this method (123):**

| ID | Name |
|----|------|
| 1 | Ocean |
| 2 | Plasma |
| 3 | Confetti |
| 4 | Sinelon |
| 5 | Juggle |
| 6 | BPM |
| 8 | Ripple |
| 9 | Heartbeat |
| 10 | Interference |
| 11 | Breathing |
| 12 | Pulse |
| 13 | LGP Box Wave |
| 14 | LGP Holographic |
| 15 | LGP Modal Resonance |
| 16 | LGP Interference Scanner |
| 17 | LGP Wave Collision |
| 18 | LGP Diamond Lattice |
| 19 | LGP Hexagonal Grid |
| 20 | LGP Spiral Vortex |
| 21 | LGP Sierpinski |
| 22 | LGP Chevron Waves |
| 23 | LGP Concentric Rings |
| 24 | LGP Star Burst |
| 25 | LGP Mesh Network |
| 26 | LGP Moire Curtains |
| 27 | LGP Radial Ripple |
| 28 | LGP Holographic Vortex |
| 29 | LGP Evanescent Drift |
| 31 | LGP Modal Cavity |
| 32 | LGP Fresnel Zones |
| 33 | LGP Photonic Crystal |
| 38 | LGP Crystalline Growth |
| 39 | LGP Fluid Dynamics |
| 40 | LGP Quantum Tunneling |
| 41 | LGP Gravitational Lensing |
| 42 | LGP Time Crystal |
| 43 | LGP Soliton Waves |
| 44 | LGP Metamaterial Cloak |
| 45 | LGP GRIN Cloak |
| 46 | LGP Caustic Fan |
| 47 | LGP Birefringent Shear |
| 48 | LGP Anisotropic Cloak |
| 49 | LGP Evanescent Skin |
| 52 | LGP Complementary Mixing |
| 53 | LGP Quantum Colors |
| 54 | LGP Doppler Shift |
| 56 | LGP DNA Helix |
| 57 | LGP Phase Transition |
| 60 | LGP Chladni Harmonics |
| 61 | LGP Gravitational Wave Chirp |
| 62 | LGP Quantum Entanglement |
| 63 | LGP Mycelial Network |
| 64 | LGP Riley Dissonance |
| 65 | LGP Chromatic Lens |
| 66 | LGP Chromatic Pulse |
| 67 | LGP Chromatic Interference |
| 68 | Audio Test |
| 70 | Spectrum Bars |
| 73 | Audio Bloom |
| 74 | LGP Star Burst (Narrative) |
| 75 | Chord Glow |
| 77 | LGP Perlin Veil |
| 78 | LGP Perlin Shocklines |
| 79 | LGP Perlin Caustics |
| 80 | LGP Perlin Interference Weave |
| 85 | Perlin Test: FastLED |
| 86 | Perlin Test: Emotiscope2 Full |
| 87 | Perlin Test: Emotiscope2 Quarter |
| 88 | BPM Enhanced |
| 89 | Breathing Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 93 | LGP Spectrum Detail |
| 94 | LGP Spectrum Detail Enhanced |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 97 | Ripple Enhanced |
| 98 | Audio Bloom Parity |
| 100 | LGP Holographic Auto-Cycle |
| 106 | Ripple (ES tuned) |
| 109 | SB Waveform (Ref) |
| 121 | Beat Pulse (Bloom) |
| 122 | Bloom (Parity) |
| 123 | Kuramoto Transport |
| 124 | LGP Opal Film |
| 125 | LGP Grating Scan |
| 126 | LGP Stress Glass |
| 127 | LGP Moire Silk |
| 128 | LGP Caustic Shards |
| 129 | LGP Parallax Depth |
| 130 | LGP Stress Glass (Melt) |
| 131 | LGP Grating Scan (Breakup) |
| 132 | LGP Water Caustics |
| 133 | LGP Schlieren Flow |
| 134 | LGP Reaction Diffusion |
| 135 | LGP RD Triangle |
| 136 | LGP Talbot Carpet |
| 137 | LGP Airy Comet |
| 138 | LGP Moire Cathedral |
| 139 | LGP Living Glyph |
| 140 | LGP Spirograph Crown |
| 141 | LGP Rose Bloom |
| 142 | LGP Harmonograph Halo |
| 143 | LGP Rule 30 Cathedral |
| 144 | LGP Langton Highway |
| 145 | LGP Cymatic Ladder |
| 146 | LGP Mach Diamonds |
| 147 | Chimera Crown |
| 148 | Catastrophe Caustics |
| 149 | Hyperbolic Portal |
| 150 | Lorenz Ribbon |
| 151 | IFS Botanical Relic |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

### Static Colour

Hardcoded CHSV or CRGB values

**Function:** [`CHSV(...) / CRGB(...)`](MATH_APPENDIX.md#chsv(...) / crgb(...))

**Effects using this method (4):**

- **LGP Bioluminescent Waves** (ID 35)
- **LGP Neural Network** (ID 37)
- **LGP Color Temperature** (ID 50)
- **Audio Waveform Parity** (ID 99)

### Heat Colour Map

Temperature-to-colour mapping via FastLED HeatColor()

**Function:** [`HeatColor()`](MATH_APPENDIX.md#heatcolor)

**Effects using this method (1):**

- **Fire** (ID 0)

## Audio Coupling Modes

Effects that respond to audio input via the ControlBus system. See [Audio Feature Access](MATH_APPENDIX.md#audio-feature-access) in the Math Appendix for accessor documentation.

### RMS Intensity

Audio RMS level drives brightness or effect radius

**ControlBus field:** `rms / smoothRms / ctx.audio.rms()`

**Effects using this mode (28):**

| ID | Name |
|----|------|
| 5 | Juggle |
| 7 | Wave |
| 11 | Breathing |
| 68 | Audio Test |
| 72 | Audio Waveform |
| 74 | LGP Star Burst (Narrative) |
| 76 | Wave Reactive |
| 77 | LGP Perlin Veil |
| 78 | LGP Perlin Shocklines |
| 89 | Breathing Enhanced |
| 98 | Audio Bloom Parity |
| 99 | Audio Waveform Parity |
| 101 | ES Analog (Ref) |
| 105 | ES Waveform (Ref) |
| 106 | Ripple (ES tuned) |
| 109 | SB Waveform (Ref) |
| 121 | Beat Pulse (Bloom) |
| 123 | Kuramoto Transport |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

### Beat Trigger

Beat detection events trigger discrete visual events

**ControlBus field:** `beatStrength / onBeat / BeatPulseCore`

**Effects using this mode (44):**

| ID | Name |
|----|------|
| 5 | Juggle |
| 6 | BPM |
| 11 | Breathing |
| 68 | Audio Test |
| 69 | Beat Pulse |
| 70 | Spectrum Bars |
| 71 | Bass Breath |
| 73 | Audio Bloom |
| 77 | LGP Perlin Veil |
| 78 | LGP Perlin Shocklines |
| 80 | LGP Perlin Interference Weave |
| 88 | BPM Enhanced |
| 89 | Breathing Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 99 | Audio Waveform Parity |
| 100 | LGP Holographic Auto-Cycle |
| 106 | Ripple (ES tuned) |
| 107 | Heartbeat (ES tuned) |
| 108 | LGP Holographic (ES tuned) |
| 110 | Beat Pulse (Stack) |
| 111 | Beat Pulse (Shockwave) |
| 113 | Beat Pulse (Void) |
| 114 | Beat Pulse (Resonant) |
| 115 | Beat Pulse (Ripple) |
| 116 | Beat Pulse (Shockwave Cascade) |
| 117 | Beat Pulse (Spectral) |
| 118 | Beat Pulse (Spectral Pulse) |
| 119 | Beat Pulse (Breathe) |
| 120 | Beat Pulse (LGP Interference) |
| 121 | Beat Pulse (Bloom) |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

### Frequency Band

Specific frequency bands drive specific visual parameters

**ControlBus field:** `heavy_bands / bins64Adaptive / bins64`

**Effects using this mode (19):**

| ID | Name |
|----|------|
| 5 | Juggle |
| 6 | BPM |
| 16 | LGP Interference Scanner |
| 17 | LGP Wave Collision |
| 22 | LGP Chevron Waves |
| 24 | LGP Star Burst |
| 33 | LGP Photonic Crystal |
| 73 | Audio Bloom |
| 79 | LGP Perlin Caustics |
| 88 | BPM Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 93 | LGP Spectrum Detail |
| 94 | LGP Spectrum Detail Enhanced |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 102 | ES Spectrum (Ref) |
| 106 | Ripple (ES tuned) |

### Chroma Hue

Chroma bins drive palette hue selection

**ControlBus field:** `chroma / heavy_chroma`

**Effects using this mode (44):**

| ID | Name |
|----|------|
| 5 | Juggle |
| 6 | BPM |
| 8 | Ripple |
| 11 | Breathing |
| 16 | LGP Interference Scanner |
| 17 | LGP Wave Collision |
| 22 | LGP Chevron Waves |
| 24 | LGP Star Burst |
| 33 | LGP Photonic Crystal |
| 69 | Beat Pulse |
| 70 | Spectrum Bars |
| 71 | Bass Breath |
| 72 | Audio Waveform |
| 73 | Audio Bloom |
| 74 | LGP Star Burst (Narrative) |
| 75 | Chord Glow |
| 80 | LGP Perlin Interference Weave |
| 88 | BPM Enhanced |
| 89 | Breathing Enhanced |
| 90 | LGP Chevron Waves Enhanced |
| 91 | LGP Interference Scanner Enhanced |
| 92 | LGP Photonic Crystal Enhanced |
| 93 | LGP Spectrum Detail |
| 95 | LGP Star Burst Enhanced |
| 96 | LGP Wave Collision Enhanced |
| 97 | Ripple Enhanced |
| 98 | Audio Bloom Parity |
| 103 | ES Octave (Ref) |
| 104 | ES Bloom (Ref) |
| 106 | Ripple (ES tuned) |
| 107 | Heartbeat (ES tuned) |
| 108 | LGP Holographic (ES tuned) |
| 109 | SB Waveform (Ref) |
| 122 | Bloom (Parity) |
| 152 | LGP Flux Rift |
| 153 | LGP Beat Prism |
| 154 | LGP Harmonic Tide |
| 155 | LGP Bass Quake |
| 156 | LGP Treble Net |
| 157 | LGP Rhythmic Gate |
| 158 | LGP Spectral Knot |
| 159 | LGP Saliency Bloom |
| 160 | LGP Transient Lattice |
| 161 | LGP Wavelet Mirror |

## Migration Patterns (Historical)

The chroma-to-hue conversion code underwent a systematic migration from ad-hoc inline implementations to the centralised [ChromaUtils](MATH_APPENDIX.md#chroma-processing-functions) library. Four distinct patterns were identified and migrated.

### Pattern A: Static Utility Functions

Duplicated `dominantChromaBin12()` + `chromaBinToHue()` static functions.

**Status:** migrated

**Before:**

```cpp
dominantChromaBin12(chroma) -> chromaBinToHue(bin)
```

**After:**

```cpp
effects::chroma::circularChromaHueSmoothed(chroma, m_chromaAngle, dt, tau)
```

**Affected effects (6):**

- [Ripple](EFFECTS_INVENTORY.md) (ID 8)
- [LGP Beat Pulse](EFFECTS_INVENTORY.md) (ID 69)
- [LGP Spectrum Bars](EFFECTS_INVENTORY.md) (ID 70)
- [LGP Bass Breath](EFFECTS_INVENTORY.md) (ID 71)
- [Ripple Enhanced](EFFECTS_INVENTORY.md) (ID 97)
- [Ripple (ES tuned)](EFFECTS_INVENTORY.md) (ID 106)

### Pattern B: Linear EMA on Circular Quantity

Applied standard (linear) exponential moving average to a circular palette hue, causing wrap-around artefacts at the 0/255 boundary.

**Status:** migrated

**Before:**

```cpp
m_dominantBinSmooth = m_dominantBinSmooth * 0.88f + dominantBin * 0.12f; hue = m_dominantBinSmooth * 21.25f
```

**After:**

```cpp
effects::chroma::circularChromaHueSmoothed(chroma, m_chromaAngle, dt, tau)
```

**Affected effects (4):**

- [LGP Wave Collision](EFFECTS_INVENTORY.md) (ID 17)
- [LGP Chevron Waves](EFFECTS_INVENTORY.md) (ID 22)
- [LGP Chevron Waves Enhanced](EFFECTS_INVENTORY.md) (ID 90)
- [LGP Wave Collision Enhanced](EFFECTS_INVENTORY.md) (ID 96)

### Pattern C: Raw Inline Argmax

Inline argmax over 12 chroma bins with direct bin-to-hue mapping. Produced jarring hue jumps when two bins had similar magnitudes.

**Status:** migrated

**Before:**

```cpp
maxBin = argmax(chroma); hue = maxBin * 21
```

**After:**

```cpp
effects::chroma::circularChromaHueSmoothed(chroma, m_chromaAngle, dt, tau)
```

**Affected effects (6):**

- [Juggle](EFFECTS_INVENTORY.md) (ID 5)
- [LGP Star Burst](EFFECTS_INVENTORY.md) (ID 24)
- [LGP Photonic Crystal](EFFECTS_INVENTORY.md) (ID 33)
- [BPM Enhanced](EFFECTS_INVENTORY.md) (ID 88)
- [LGP Photonic Crystal Enhanced](EFFECTS_INVENTORY.md) (ID 92)
- [LGP Star Burst Enhanced](EFFECTS_INVENTORY.md) (ID 95)

### Pattern D: Experimental Pack (Schmitt Trigger)

Schmitt trigger hysteresis combined with `circularChromaHueSmoothed()`. Native implementation, not a migration.

**Status:** native_implementation

**Before:**

```cpp
N/A (built with circular chroma fix incorporated)
```

**After:**

```cpp
selectMusicalHue() + smoothHue() + dominantNoteFromBins() using kCos/kSin
```

**Affected effects (10):**

- [LGP Flux Rift](EFFECTS_INVENTORY.md) (ID 152)
- [LGP Beat Prism](EFFECTS_INVENTORY.md) (ID 153)
- [LGP Harmonic Tide](EFFECTS_INVENTORY.md) (ID 154)
- [LGP Bass Quake](EFFECTS_INVENTORY.md) (ID 155)
- [LGP Treble Net](EFFECTS_INVENTORY.md) (ID 156)
- [LGP Rhythmic Gate](EFFECTS_INVENTORY.md) (ID 157)
- [LGP Spectral Knot](EFFECTS_INVENTORY.md) (ID 158)
- [LGP Saliency Bloom](EFFECTS_INVENTORY.md) (ID 159)
- [LGP Transient Lattice](EFFECTS_INVENTORY.md) (ID 160)
- [LGP Wavelet Mirror](EFFECTS_INVENTORY.md) (ID 161)

## Cross-Reference Matrix

Sparse matrix showing which spatial rendering pattern(s) each effect uses.

| ID | Name | Field | Radial | Wave | Particle | Interf. | Geom. | Waveform |
|----|------|--- | --- | --- | --- | --- | --- | ---|
| 0 | Fire | X |  |  |  |  |  |  |
| 1 | Ocean |  | X | X |  |  |  |  |
| 2 | Plasma |  |  | X |  |  |  |  |
| 3 | Confetti |  |  |  | X |  |  |  |
| 4 | Sinelon |  |  |  | X |  |  |  |
| 5 | Juggle |  | X |  |  |  |  |  |
| 6 | BPM |  | X | X |  |  |  |  |
| 7 | Wave |  | X | X |  |  |  |  |
| 8 | Ripple |  | X |  |  |  |  |  |
| 9 | Heartbeat |  | X |  |  |  |  |  |
| 10 | Interference |  | X | X |  |  |  |  |
| 11 | Breathing |  | X | X |  |  |  |  |
| 12 | Pulse |  |  | X |  |  |  |  |
| 13 | LGP Box Wave |  | X | X |  | X |  |  |
| 14 | LGP Holographic |  | X | X |  |  |  |  |
| 15 | LGP Modal Resonance |  |  | X |  |  |  |  |
| 16 | LGP Interference Scanner |  | X | X |  |  |  |  |
| 17 | LGP Wave Collision |  | X | X |  | X |  |  |
| 18 | LGP Diamond Lattice |  | X | X |  |  |  |  |
| 19 | LGP Hexagonal Grid |  |  | X |  |  |  |  |
| 20 | LGP Spiral Vortex |  | X | X |  |  |  |  |
| 21 | LGP Sierpinski |  | X | X |  |  | X |  |
| 22 | LGP Chevron Waves |  | X | X |  |  |  |  |
| 23 | LGP Concentric Rings |  | X | X |  | X |  |  |
| 24 | LGP Star Burst |  | X | X |  |  |  |  |
| 25 | LGP Mesh Network |  | X | X |  |  |  |  |
| 26 | LGP Moire Curtains |  | X | X |  | X |  |  |
| 27 | LGP Radial Ripple |  | X | X |  |  |  |  |
| 28 | LGP Holographic Vortex |  | X | X |  |  |  |  |
| 29 | LGP Evanescent Drift |  | X | X |  |  |  |  |
| 30 | LGP Chromatic Shear |  | X |  |  |  |  |  |
| 31 | LGP Modal Cavity |  | X | X |  |  |  |  |
| 32 | LGP Fresnel Zones |  | X |  |  |  |  |  |
| 33 | LGP Photonic Crystal |  | X | X |  |  |  |  |
| 34 | LGP Aurora Borealis | X |  |  |  |  |  |  |
| 35 | LGP Bioluminescent Waves |  |  | X |  |  |  |  |
| 36 | LGP Plasma Membrane | X |  |  |  |  |  |  |
| 37 | LGP Neural Network | X |  |  |  |  |  |  |
| 38 | LGP Crystalline Growth | X |  |  |  |  |  |  |
| 39 | LGP Fluid Dynamics | X | X | X |  |  |  |  |
| 40 | LGP Quantum Tunneling |  |  |  | X |  |  |  |
| 41 | LGP Gravitational Lensing | X |  |  |  |  |  |  |
| 42 | LGP Time Crystal |  | X | X |  |  |  |  |
| 43 | LGP Soliton Waves |  |  |  | X |  |  |  |
| 44 | LGP Metamaterial Cloak |  |  |  | X |  |  |  |
| 45 | LGP GRIN Cloak |  |  |  | X |  |  |  |
| 46 | LGP Caustic Fan |  |  | X |  |  |  |  |
| 47 | LGP Birefringent Shear |  |  | X |  |  |  |  |
| 48 | LGP Anisotropic Cloak |  |  |  | X |  |  |  |
| 49 | LGP Evanescent Skin |  | X | X |  |  |  |  |
| 50 | LGP Color Temperature |  | X |  |  |  |  |  |
| 51 | LGP RGB Prism |  | X | X |  |  |  |  |
| 52 | LGP Complementary Mixing |  | X |  |  |  |  |  |
| 53 | LGP Quantum Colors |  | X | X |  |  |  |  |
| 54 | LGP Doppler Shift |  | X |  |  |  |  |  |
| 55 | LGP Color Accelerator |  | X |  |  |  |  |  |
| 56 | LGP DNA Helix |  | X | X |  |  |  |  |
| 57 | LGP Phase Transition |  | X | X |  |  |  |  |
| 58 | LGP Chromatic Aberration |  | X | X |  |  |  |  |
| 59 | LGP Perceptual Blend |  | X | X |  |  |  |  |
| 60 | LGP Chladni Harmonics |  | X | X |  | X |  |  |
| 61 | LGP Gravitational Wave Chirp |  | X | X |  |  |  |  |
| 62 | LGP Quantum Entanglement |  | X | X |  | X |  |  |
| 63 | LGP Mycelial Network |  | X | X |  |  | X |  |
| 64 | LGP Riley Dissonance |  | X | X |  |  |  |  |
| 65 | LGP Chromatic Lens |  | X | X |  |  |  |  |
| 66 | LGP Chromatic Pulse |  | X | X |  |  |  |  |
| 67 | LGP Chromatic Interference |  | X | X |  |  |  |  |
| 68 | Audio Test |  | X | X |  |  |  |  |
| 69 | Beat Pulse |  | X |  |  |  |  |  |
| 70 | Spectrum Bars |  | X | X |  |  |  |  |
| 71 | Bass Breath |  | X | X |  |  |  |  |
| 72 | Audio Waveform |  |  |  |  |  |  | X |
| 73 | Audio Bloom |  | X |  |  |  |  |  |
| 74 | LGP Star Burst (Narrative) |  | X | X |  |  |  |  |
| 75 | Chord Glow |  | X | X |  |  |  |  |
| 76 | Wave Reactive |  | X | X |  |  |  |  |
| 77 | LGP Perlin Veil | X | X | X |  |  |  |  |
| 78 | LGP Perlin Shocklines | X | X |  |  |  |  |  |
| 79 | LGP Perlin Caustics | X | X |  |  |  |  |  |
| 80 | LGP Perlin Interference Weave | X | X |  |  |  |  |  |
| 81 | LGP Perlin Veil Ambient | X | X | X |  |  |  |  |
| 82 | LGP Perlin Shocklines Ambient | X | X |  |  |  |  |  |
| 83 | LGP Perlin Caustics Ambient | X | X | X |  |  |  |  |
| 84 | LGP Perlin Interference Weave Ambient | X | X | X |  |  |  |  |
| 85 | Perlin Test: FastLED | X | X |  |  |  |  |  |
| 86 | Perlin Test: Emotiscope2 Full | X | X |  |  |  |  |  |
| 87 | Perlin Test: Emotiscope2 Quarter | X | X |  |  |  |  |  |
| 88 | BPM Enhanced | X | X | X |  |  |  |  |
| 89 | Breathing Enhanced | X | X | X |  |  |  |  |
| 90 | LGP Chevron Waves Enhanced | X | X | X |  |  |  |  |
| 91 | LGP Interference Scanner Enhanced |  | X | X |  |  |  |  |
| 92 | LGP Photonic Crystal Enhanced | X | X | X |  |  |  |  |
| 93 | LGP Spectrum Detail |  |  |  |  |  |  | X |
| 94 | LGP Spectrum Detail Enhanced |  |  |  | X |  |  |  |
| 95 | LGP Star Burst Enhanced | X | X | X |  |  |  |  |
| 96 | LGP Wave Collision Enhanced | X | X | X |  |  |  |  |
| 97 | Ripple Enhanced |  | X |  |  |  |  |  |
| 98 | Audio Bloom Parity |  |  |  |  |  |  | X |
| 99 | Audio Waveform Parity |  | X |  |  |  |  |  |
| 100 | LGP Holographic Auto-Cycle |  | X | X |  |  |  |  |
| 101 | ES Analog (Ref) |  | X |  |  |  |  |  |
| 102 | ES Spectrum (Ref) |  | X |  |  |  |  |  |
| 103 | ES Octave (Ref) |  | X |  |  |  |  |  |
| 104 | ES Bloom (Ref) |  | X |  |  |  |  |  |
| 105 | ES Waveform (Ref) |  | X |  |  |  |  | X |
| 106 | Ripple (ES tuned) |  | X |  |  |  |  |  |
| 107 | Heartbeat (ES tuned) |  | X |  |  |  |  |  |
| 108 | LGP Holographic (ES tuned) |  | X | X |  |  |  |  |
| 109 | SB Waveform (Ref) |  | X |  |  |  |  | X |
| 110 | Beat Pulse (Stack) |  | X |  |  |  |  |  |
| 111 | Beat Pulse (Shockwave) |  | X |  |  |  |  |  |
| 113 | Beat Pulse (Void) |  | X |  |  |  |  |  |
| 114 | Beat Pulse (Resonant) |  | X |  |  |  |  |  |
| 115 | Beat Pulse (Ripple) |  | X |  |  |  |  |  |
| 116 | Beat Pulse (Shockwave Cascade) |  | X |  |  |  |  |  |
| 117 | Beat Pulse (Spectral) |  | X | X |  |  |  |  |
| 118 | Beat Pulse (Spectral Pulse) |  | X | X |  |  |  |  |
| 119 | Beat Pulse (Breathe) |  | X |  |  |  |  |  |
| 120 | Beat Pulse (LGP Interference) |  |  |  |  | X |  |  |
| 121 | Beat Pulse (Bloom) |  | X |  |  |  |  |  |
| 122 | Bloom (Parity) |  | X |  |  |  |  |  |
| 123 | Kuramoto Transport | X |  |  | X |  |  |  |
| 124 | LGP Opal Film |  | X | X |  |  |  |  |
| 125 | LGP Grating Scan |  | X |  |  |  |  |  |
| 126 | LGP Stress Glass |  | X | X |  |  |  |  |
| 127 | LGP Moire Silk |  | X | X |  | X |  |  |
| 128 | LGP Caustic Shards |  | X | X |  |  |  |  |
| 129 | LGP Parallax Depth |  | X | X |  |  |  |  |
| 130 | LGP Stress Glass (Melt) |  | X | X |  |  |  |  |
| 131 | LGP Grating Scan (Breakup) |  | X |  |  |  |  |  |
| 132 | LGP Water Caustics |  | X | X |  |  |  |  |
| 133 | LGP Schlieren Flow |  | X | X |  |  |  |  |
| 134 | LGP Reaction Diffusion |  | X |  |  |  |  |  |
| 135 | LGP RD Triangle |  | X |  |  |  |  |  |
| 136 | LGP Talbot Carpet | X |  |  |  | X | X |  |
| 137 | LGP Airy Comet | X |  |  |  | X | X |  |
| 138 | LGP Moire Cathedral | X |  |  |  | X | X |  |
| 139 | LGP Living Glyph | X |  |  |  | X | X |  |
| 140 | LGP Spirograph Crown | X |  |  |  | X | X |  |
| 141 | LGP Rose Bloom | X |  |  |  | X | X |  |
| 142 | LGP Harmonograph Halo | X |  |  |  | X | X |  |
| 143 | LGP Rule 30 Cathedral | X |  |  |  | X | X |  |
| 144 | LGP Langton Highway | X |  |  |  | X | X |  |
| 145 | LGP Cymatic Ladder | X |  |  |  | X | X |  |
| 146 | LGP Mach Diamonds | X |  |  |  | X | X |  |
| 147 | Chimera Crown |  |  |  |  |  | X |  |
| 148 | Catastrophe Caustics |  |  |  |  |  | X |  |
| 149 | Hyperbolic Portal |  |  |  |  |  | X |  |
| 150 | Lorenz Ribbon |  |  |  |  |  | X |  |
| 151 | IFS Botanical Relic |  |  |  |  |  | X |  |
| 152 | LGP Flux Rift |  |  |  |  | X |  | X |
| 153 | LGP Beat Prism |  |  |  |  | X |  | X |
| 154 | LGP Harmonic Tide |  |  |  |  | X |  | X |
| 155 | LGP Bass Quake |  |  |  |  | X |  | X |
| 156 | LGP Treble Net |  |  |  |  | X |  | X |
| 157 | LGP Rhythmic Gate |  |  |  |  | X |  | X |
| 158 | LGP Spectral Knot |  |  |  |  | X |  | X |
| 159 | LGP Saliency Bloom |  |  |  |  | X |  | X |
| 160 | LGP Transient Lattice |  |  |  |  | X |  | X |
| 161 | LGP Wavelet Mirror |  |  |  |  | X |  | X |
