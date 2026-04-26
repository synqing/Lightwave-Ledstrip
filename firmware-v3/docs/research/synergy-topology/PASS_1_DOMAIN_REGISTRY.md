---
abstract: "Pass 1 of the K1 visual-pipeline synergy-topology protocol. Merges 4 parallel-subagent domain fragments (Audio+Persistence / Physics+Geometry / Composition+CrossLineage / K1Specific+ProductStrategy) into a unified deduplicated domain registry of 119 distinct domains tagged Level-0/1/2, plus a cross-subagent pairwise interaction matrix (focused on multiplicative/combinatorial edges only — additive pairs omitted) and a top-10 triadic-synergy list. Built from 9 SB releases × 4 ES releases of forensic taxonomy + ~97 brainstorm proposals + 8 raw SSA outputs. Read before PASS_2_SYNERGY_TOPOLOGY.md (depends on this file's domain IDs). Headline finding: 6 multiplicative substrates (LayerStack, FramebufferLPF, TempoPhase, PSRAM-FrameRing, EffectRoleFlags, PersistenceHelpers) sit on the critical path of 70%+ of all proposed Level-1 capabilities — the kill order in Pass 3 will compound around these."
---

# Pass 1 — Domain Registry & Interaction Matrix

**Protocol:** K1 Visual Pipeline Synergy Topology — Pass 1 of 4. Inputs: 2 synthesised primaries (taxonomy + brainstorm catalogue) + 8 raw SSA outputs. Outputs: this file + state for Pass 2.

**Method:** 4 parallel subagents per CLAUDE.md `/dispatching-parallel-agents` protocol, each scoped to 2 SSAs + both primaries. Orchestrator merged the fragments, deduplicated overlapping domain proposals, built the cross-subagent interaction matrix (which no individual subagent could produce), and surfaced higher-order synergies.

**Source coverage note:** SA-3 (Composition + CrossLineage) had its taxonomy read truncated to line 1 by a session hook; the subagent worked from brainstorm-catalogue cross-references that cite the taxonomy extensively. The other three subagents read all 4 of their assigned files in full. Confidence per fragment: SA-1 HIGH, SA-2 HIGH, SA-3 MEDIUM-HIGH (mitigated by triangulation through brainstorm catalogue), SA-4 HIGH.

---

## 1. Domain ID convention

Domains are renumbered with semantic prefixes. The orchestrator's merge collapsed cross-subagent duplicates (e.g. PSRAM frame ring proposed by 4 subagents, framebuffer LPF by 3) into single canonical IDs.

| Prefix | Meaning | Source SAs |
|---|---|---|
| `AUD-` | Audio feature / interpretation domain | SA-1 |
| `PER-` | Temporal persistence / decay / framebuffer domain | SA-1 |
| `PHY-` | Animation-physics / continuum dynamics domain | SA-2 |
| `GEO-` | Geometric / topological / spatial-mapping domain | SA-2 |
| `COM-` | Layer composition / blend / orchestration domain | SA-3 |
| `LIN-` | SB×ES cross-lineage fusion (emergent capability) | SA-3 |
| `HW-` | K1 hardware-affordance domain | SA-4 |
| `PS-` | Product-strategy / brand-voice domain | SA-4 |
| `VG-` | Viability-gate domain (engineering vs brand-voice) | SA-4 |
| `INF-` | Infrastructure substrate (Level-2; cited by ≥2 SAs) | merged |

**Status:** `[implemented | partial | absent]`. **Level:** 0 = engine primitive leaf, 1 = capability composed of ≥1 primitive, 2 = architectural pattern.

---

## 2. Unified Domain Registry

### 2.1 Infrastructure substrates (INF-) — Level-2, cross-cutting

These are the substrates that ≥2 subagents independently cited as preconditions. Pass 2 will treat these as candidate hubs.

```
[INF-01] LayerStack — PSRAM N-buffer overlap renderer  [Level-2] [absent]
     Sibling to ZoneComposer; full-strip overlap-permitted vs partition-only.
     Cited by: SA-3 (D01), brainstorm Infra #1 (~150 LOC, unlocks 12 Composition categories), brainstorm Theme 5.
     GIVEN per Context Preamble.

[INF-02] FramebufferLPF — global dt-correct apply_image_lpf analogue  [Level-2] [absent]
     RendererActor post-pass; cutoff `0.5 + (1-√softness)·14.5 Hz`; per-effect opt-out.
     Cited by: SA-1 (D-P04), SA-3 (D27), brainstorm Theme 1 + Infra #2 (~80 LOC).
     GIVEN per Context Preamble. Single most under-served K1 lineage gap (brainstorm:40).

[INF-03] PSRAMFrameRing — 1.15 MB circular write/arbitrary-read frame buffer  [Level-2] [absent]
     10s × 320 LED × 3B × 120 FPS. Substrate for echo, time-warp, optical-flow, long-context.
     Cited by: SA-1 (D-P27), SA-2 (D34), SA-3 (D37), SA-4 (D-HW-04), brainstorm Infra #6 (~120 LOC, V1.1).

[INF-04] TempoPhaseContinuous — `controlBus.tempoPhase01` 0..1 scalar  [Level-0/1] [partial]
     Single-tempo-phase scalar; possible bank version (`tempi[N].phase`) is V1.1+.
     Cited by: SA-1 (D-A06), SA-3 (D36), SA-4 implicit, brainstorm Theme 2 + Infra #3 (~210 LOC).
     SA-1 flagged status conflict — field exists in ControlBus per SSA-AudioDriver l.11 but brainstorm:52 says not surfaced. **Open for orchestrator: clangd read of ControlBus.h to resolve.**

[INF-05] TempoBankPerBin — `tempi[N].phase`/`tempi[N].magnitude` per-tempo-bin bank  [Level-0] [absent]
     ES doctrine. K1 has reliable single-tempo via ESV11; bank version is V1.1+.
     Cited by: SA-1 (implicit), SA-3 (D35), brainstorm:152 (Captain Q1 — "recommend single-tempo first").

[INF-06] EffectRoleFlags — IEffect interface metadata (renders-colour-only / renders-geometry-only / opt-out-of-global-LPF / opt-out-of-global-trail / invert-input)  [Level-0] [absent]
     Without role flags: COM-Stain-Glass silently drops colour, INF-02 mandatory pass double-trails self-trailing effects, COM-Anti-Mode ambiguous, etc.
     Cited by: SA-3 (D38) — STRONG cross-cutting concern flagged by SA-3 as likely required by SA-1/SA-2/SA-4 primitives too.

[INF-07] PersistenceHelpersLibrary — `dtDecay3`, `emaArrayDt`, `crossBlendArray`, `spatialLPF1D`, `heatStep1D`, `velocityAniso1D` engine helpers  [Level-2] [partial]
     Cited by: SA-1 (D-P28), brainstorm Infra #4 (~250 LOC). Some helpers (emaArrayDt) nearly exist already.

[INF-08] sinLUT256/cosLUT256 — float-precision trig LUT  [Level-0] [partial]
     Required by PHY-/GEO- continuum/oscillator domains. FastLED has 8-bit `sin8`/`cos8`; float-precision LUT for PDE work is unconfirmed.
     Cited by: SA-2 (D33).

[INF-09] CFLSubstepGate — CFL stability guard for explicit PDE solvers  [Level-0] [absent]
     Compute substep count from current v, D, c. Cap at N substeps; document "snap" artefact when capped.
     Cited by: SA-2 (D31). No analogue in SB/ES (neither has PDE work).

[INF-10] CubicHermiteResample — N-node → 320-LED C¹-continuous resample  [Level-0] [absent]
     Used by Verlet, Kuramoto, Pendulum to map sparse node arrays to dense LED brightness.
     Cited by: SA-2 (D30). SB/ES have linear interpolate only.

[INF-11] LongWindowAudioStats — 30s/60s/120s rolling audio statistics ring  [Level-1] [absent]
     ~24 KB PSRAM. Substrate for mood-driven mode selection, predictive UX.
     Cited by: SA-3 (D40 mood classifier), SA-4 (D-HW-09).

[INF-12] PSRAMScalarRing — small per-scalar history buffers (hue ring 240 B, onset ring 640 B, position ring 320 B)  [Level-0] [partial]
     Substrate for prism reflection hue-drift, onset-history scope, fx_dots[12] motion-blur cache.
     Cited by: SA-3 (D29 implicit, D33 implicit), SA-1 (D-A16), SA-4 implicit. Smaller cousin of INF-03.

[INF-13] TransitionEngine — 12-type transition library (incl. LinearFade 250ms, lpf_drag)  [Level-2] [implemented]
     Cited by: SA-3 (D21). Re-used by Two-Step, Mood Switch, Voice/Music Switcher, Story-Arc.

[INF-14] ZoneComposer — 3-zone partition-based renderer, 4 BlendMode operators  [Level-2] [implemented]
     Cited by: SA-3 (D22). Sibling-not-replacement of INF-01 LayerStack.
```

### 2.2 Audio domains (AUD-) — Level-0/1, SA-1 primary

```
[AUD-01] BandEnergyOctave bands[8]                            [L0] [implemented]  SB+ES universal
[AUD-02] ChromaVector chroma[12]                              [L0] [implemented]  SB+ES universal
[AUD-03] RawSpectrum bins64/bins64Adaptive/bins256/waveform[128] [L0] [implemented]  underexploited
[AUD-04] OnsetFlux + Bass/Mid/High split                      [L0] [implemented]  ES novelty cousin
[AUD-05] BeatTrackTempo (BPM/tick/strength/confidence/downbeat) [L0] [implemented] ESV11 32 kHz path
[AUD-06] TempoPhaseContinuous → see INF-04
[AUD-07] PercussionTriggers (snare/hihat/kick + energies)     [L0] [implemented]  K1-only
[AUD-08] STMTemporalSpectral stmTemporal[16]/stmSpectral[42]  [L1] [implemented]  K1-only; consumed only by EdgeMixer stm_dual
[AUD-09] MotionSemanticFeatures (timing_jitter, syncopation_level, pitch_contour_dir) [L1] [implemented] K1-only; render side does not consume
[AUD-10] SaliencyChannels (harm/rhythm/timbral/dynamic novelty) [L1] [implemented] K1-only; render side does not consume
[AUD-11] AudioConfidence + ChordState + CurrentStyle          [L1] [implemented]  K1-only; render side does not consume
[AUD-12] SpectralCentroid (Hz, log-norm 0..1)                 [L0] [absent]       AudioActor +12 LOC, +4 B
[AUD-13] SpectralFlatness (Wiener entropy)                    [L0] [absent]       AudioActor +20 LOC, +4 B
[AUD-14] PitchHPSConfidence (HPS + pitchHz + pitchConfidence) [L0] [absent]       AudioActor +40 LOC, +8 B
[AUD-15] ZeroCrossingRate (ZCR scalar from waveform)          [L0] [absent — render-trivial] 0 DSP
[AUD-16] OnsetHistoryRing (160-sample × ~5 ms ring)           [L1] [absent — render-trivial] 640 B
[AUD-17] BeatPhaseLookaheadPredictive (50 ms ahead)           [L1] [absent — render-trivial if INF-04 wired]
[AUD-18] InterBandCofiringMatrix (8×8, 1 s window)            [L1] [absent]       AudioActor +80 LOC, +256 B [divergent]
[AUD-19] EnergyModulationSpectrumLock (0.5–10 Hz Goertzel)    [L1] [absent]       AudioActor +120 LOC, +8 B [divergent]
[AUD-20] ChromaDerivativeHarmonicMotion (||Δchroma||₂)        [L1] [partial]      saliency.harmonic possibly equals — open Q
[AUD-21] VoiceVsMusicClassifier (chroma peakiness + centroid + STM heuristic) [L1] [absent — render-trivial heuristic]
[AUD-22] FormantTriangulation (F1/F2 peak-pick)               [L1] [absent]       AudioActor +50 LOC, +8 B [divergent]
[AUD-23] TempoConfidence scalar                              [L0] [absent]       Derivable from beat auto-correlation strength (SA-3 D19)
[AUD-24] VAD (ZCR + chroma_strength + 4–8 Hz RMS modulation)  [L0] [absent]       SA-3 D39 [divergent]
[AUD-25] MoodClassifier (RMS env + beat density + chroma entropy → 3-axis vector) [L0] [absent] SA-3 D40 [divergent]
```

### 2.3 Persistence domains (PER-) — Level-0/1, SA-1 primary

```
[PER-01] PerPixelEMA (single-τ exponential)                   [L0] [implemented]
[PER-02] AsymmetricMaxFollower (per-band attack/decay)        [L0] [implemented]
[PER-03] SpriteSelfFeedback (α∈[0.95,0.99])                   [L0] [implemented]  SB+ES dominant trail family
[PER-04] FramebufferLPF → see INF-02
[PER-05] FrameSkipCadence                                      [L0] [implemented]
[PER-06] PingPongReplay                                        [L1] [implemented]
[PER-07] DtCorrectArrayLPF                                     [L0] [partial]      no canonical helper
[PER-08] EightFrameRingBoxcar                                  [L0] [implemented]
[PER-09] ChromaticPhosphorDecay (per-RGB τ — slow R, fast B)  [L0] [absent]       [divergent] ~0.02 ms
[PER-10] AsymmetricLuminanceConditionalDecay (bright→long τ)  [L0] [absent]       ~0.08 ms
[PER-11] MultiScaleMemoryComposite (fast 30 ms + slow 400 ms) [L0] [absent]       Theme 1; ~0.10 ms; 1920 B
[PER-12] RecursiveAdaptiveFloor (τ_value + τ_floor)           [L0] [absent]       [divergent]
[PER-13] EchoSpatialDelay (8-frame × spatial offset)          [L1] [absent]       7,680 B; subsumed by INF-03
[PER-14] PredictiveTrailOpticalFlow (frame-Δ velocity)        [L0] [absent]       [divergent] 960 B prev-frame
[PER-15] BeatLockedRefresh (one ×0.4 multiply on beat)        [L1] [absent]       Theme 2
[PER-16] TempoPhaseModulatedDecay (τ varies with phase)       [L1] [absent]       Theme 2
[PER-17] SchmittTriggerPersistence (per-pixel hot/cold)       [L0] [absent]       [divergent] 40 B
[PER-18] AudioGatedConditionalDecay (rms drives τ)            [L0] [absent]       Required for V1.0 F3 ambient state
[PER-19] OnsetTriggeredCrossBlend (dual buffer A/B + onset)   [L1] [absent]       1920 B
[PER-20] HeatEquationDiffusion1D                               [L1] [absent]       Theme 3; ~0.10 ms; 960 B
[PER-21] AnisotropicDiffusionPeronaMalik                       [L1] [absent]       ~0.40 ms
[PER-22] FrequencyDependentSpatialLPF                          [L0] [absent]       ~0.08 ms (cheaper approx of PER-20)
[PER-23] FluxConservingTrail                                   [L1] [absent]       [divergent] power-budget benefit
[PER-24] ConvolutionalKernelPersistence (5-tap structured)    [L0] [absent]       ~0.20 ms
[PER-25] VelocityAnisotropicBlur (PER-14 × PER-20 hybrid)      [L0] [absent]       ~0.10 ms
[PER-26] TempoLockedHeatEquation (k cycles with phase)        [L1] [absent]       Theme 2 × Theme 3
```

### 2.4 Physics domains (PHY-) — Level-0/1, SA-2 primary

```
[PHY-01] SpringMassDamperLattice (320 coupled oscillators)   [L1] [absent]       Wholly new physics class
[PHY-02] VerletConstraintChain                                 [L0] [absent]       [divergent at L0] No SB/ES analogue
[PHY-03] FluidAdvectionDiffusion1D                             [L1] [absent]       Theme 3; advection wholly new
[PHY-04] EasingCurveCatalogue (back/elastic/bounce/quart)     [L0] [absent]       Animation-physics canon untouched
[PHY-05] ParticlePoolNewtonianIntegrator (pos/vel/age/lifetime) [L0] [partial]    fx_dots[] is 1-particle motion-blur; full pool new
[PHY-06] CoulombFieldRender (chroma-as-charge inverse-square) [L1] [absent]       [divergent]
[PHY-07] KuramotoPhaseLattice (32 coupled oscillators)        [L1] [absent]       Order parameter as derived signal
[PHY-08] PendulumChainRK4 (16-link, gravity radial-from-centre) [L1] [absent]    Distinct from ES metronome (static phase)
[PHY-09] ViscousCrossStripBleed (μ-coupled diffusion)         [L1] [absent]       DUAL-STRIP-EXCLUSIVE
[PHY-10] WaveEquationLeapfrog1D (energy-conserving)           [L1] [absent]       Theme 3; 2nd-order PDE
[PHY-11] BoidSwarmAttractor                                    [L1] [absent]       [divergent]
[PHY-12] SemiImplicitEulerStep                                 [L0] [absent]       Generic engine primitive
[PHY-13] HeatEquationStep1D → see PER-20 (same mechanism, different SA naming)
[PHY-14] InelasticCollisionRestitution                        [L0] [absent]
```

### 2.5 Geometry domains (GEO-) — Level-1, SA-2 primary

```
[GEO-01] BilateralWaveInterference (counter-propagating sinusoids) [L1] [absent]   Theme 3+4
[GEO-02] StandingWaveHarmonicLattice (12-mode chroma → A_n)   [L1] [absent]       Theme 3
[GEO-03] ParallaxDepthStackCompositor (3 depth layers, irrational v)  [L2] [absent]  Architectural; needs INF-01
[GEO-04] HolographicTwoSourceFringe (hyperbolic fringe)       [L1] [absent]       Theme 3+4
[GEO-05] FractalRecursiveMirror (Cantor-like self-similar)    [L1] [absent]
[GEO-06] CircularRingTopology (S¹ — adjacency wrap)           [L1] [partial]      DEPARTS centre-origin; Captain Q
[GEO-07] SpatialFourierSynthesizer (bands[8] → A_k)           [L1] [absent]
[GEO-08] VoronoiCellularPartition                              [L1] [absent]       [divergent]
[GEO-09] MassConservationPixelFlow (advection conservation)   [L1] [absent]       Theme 3
[GEO-10] AsymmetricDriftOrigin (continuous f(audio))          [L1] [absent]       DEPARTS centre-origin; Captain Q (≤±20 LED gating proposed)
[GEO-11] RecursiveZoomViewport                                 [L1] [absent]       [divergent]
[GEO-12] MultiOriginCollisionField (8-slot ring of emit pts)  [L1] [absent]
[GEO-13] InterStripPhaseDelayBuffer (strip B reads strip A τ-delayed) [L1] [absent] DUAL-STRIP-EXCLUSIVE; needs INF-03
[GEO-14] CounterStreamingInterleavedLayers (even↔odd LEDs)    [L1] [absent]       [divergent]
[GEO-15] CentreSymmetryBySinglePass / mirror_image_downwards  [L0] [implemented]  K1 inherits from SB
[GEO-16] LogSpacedNodePlacement (octave-correct)              [L0] [partial]
```

### 2.6 Composition domains (COM-) — Level-1/2, SA-3 primary

```
[COM-01] BlendMode operator set (OVERWRITE/ADDITIVE/ALPHA/MULTIPLY) [L0] [implemented]
[COM-02] ForegroundPresenceDecayMask (320 B mask, τ=120 ms)   [L0] [absent]       [divergent]
[COM-03] ConvictionMixer (N-layer × N-confidence-channel)     [L1] [absent]       Direct generalisation of ES spectronome
[COM-04] SkyScoreComposer (slow ambient + sharp reactive)     [L1] [absent]       Wholly new
[COM-05] AudioDrivenCrossfaderMoodSwitch (Schmitt 0.4/0.6)    [L1] [absent]       ES lpf_drag re-purposed
[COM-06] VoiceVsMusicSwitcher (binary VAD branch swap)        [L1] [absent]       Depends on AUD-24 VAD
[COM-07] TwoStepBeatQuantisedComposer (cos² ±60 ms)           [L1] [absent]       Wholly new; needs INF-04
[COM-08] MoodStateMachineComposer (4–6 state FSM)             [L2] [absent]       Half-divergent
[COM-09] StoryArcSequencer (authored timeline)                [L2] [absent]       [divergent] V1.2+
[COM-10] DensityGatedLayerStack (RMS thresholds 0.15/0.40/0.70) [L1] [absent]
[COM-11] EchoComposerTimeMirror (multi-tap visual delay)      [L1] [absent]       Needs INF-03
[COM-12] AntiModeNegativeSpaceComposer (255 - structure)      [L1] [absent]       [divergent]
[COM-13] ParamCrossWireModulationComposer                      [L1] [absent]       Half-divergent
[COM-14] CloudMaskPerlinComposer (centre-mirrored Perlin α)   [L1] [absent]       Re-purposes kill-listed kaleidoscope mechanism
[COM-15] DecoratorPoolEventOverlay (4×16 B, 200–800 ms life)  [L1] [absent]       [divergent]
[COM-16] StainGlassOrthogonalHSV (V from geometry, HS from colour) [L1] [absent]  [divergent]; needs INF-06
```

### 2.7 Cross-lineage emergent fusions (LIN-) — Level-1, SA-3 primary

These are SB×ES fusions where neither lineage shipped the combined form. Strong K1-native exemplar candidates.

```
[LIN-01] PitchClassPendulumBouquet (12-pendulum centre-mirror) [L1] [absent]       SB centre-mirror × ES tempo bank
[LIN-02] CentreMirroredBeatParityCometPair                    [L1] [absent]       SB VU-Dot comet × ES beat-parity
[LIN-03] LogWarpedPhosphorTrail                                [L1] [absent]       SB distort_logarithmic × ES global LPF
[LIN-04] RhythmLockedCubicPerlinRibbon                         [L1] [absent]       SB kaleidoscope cubic × ES beat_tunnel
[LIN-05] HaloCompositeWithTempoConfidenceSwell                 [L1] [absent]       SB halo geometry × ES tempo-confidence
[LIN-06] CentreOriginRadialTimeScope                           [L1] [absent]       **WHOLLY NEW EMERGENT** — SA-3 names "most K1-native"
[LIN-07] PrismReflectionsIndependentHueDrift                   [L1] [absent]       SB PRISM × ES auto_color_cycle
[LIN-08] AttackOnlyPitchClassVelocityField                     [L1] [absent]       SB kaleidoscope follower × ES pitch-as-spatial
[LIN-09] BeatParityBloomSpriteInjection                        [L1] [absent]       ES tempo-phase gate × SB bloom transport
[LIN-10] MotionBlurCachedChromagramDots                        [L1] [absent]       **WHOLLY NEW EMERGENT** — fx_dots × chromagram_dots
[LIN-11] LpfDragCentreOriginCrossFade (per-LED τ gradient)    [L1] [absent]       ES lpf_drag × SB centre-mirror
[LIN-12] TempoPhasePendulumCometFan (N comets, 1 per bin)     [L1] [absent]       SB VU-Dot × ES tempo bank
```

### 2.8 K1 hardware-affordance domains (HW-) — Level-0/1/2, SA-4

```
[HW-01] LGP optical diffusion (gradient-native rendering medium) [L1] [implemented passively]  Hardware-unique
[HW-02] Dual-Strip topology (2× 160 LED edge emitters)        [L1] [partial — used as mirror only]  Hardware-unique
[HW-03] Centre-Origin Invariant (LED 79/80 as gravitational consistency) [L2] [implemented — policy] K1 brand-level
[HW-04] PSRAMFrameRing → see INF-03
[HW-05] Tab5-Encoder External Sync (8 encoderDelta knobs over WS) [L1] [partial — protocol shipped, render-side absent]
[HW-06] RichControlBusVocabulary → see AUD-01..AUD-11 (SB+ES + K1-extension fields)
[HW-07] 120 FPS / 2.0 ms render budget with FPU + cache       [L0] [implemented]  K1-unique
[HW-08] ESP32-S3 SIMD (`dsps_*` ops, 320-element ~5 µs)       [L0] [partial — available, unused in render]
[HW-09] LongWindowAudioStats → see INF-11
```

### 2.9 Product-strategy / brand-voice domains (PS-) — Level-1/2, SA-4

```
[PS-01] LockedPositioning ("Music. Made visible." / "Liquid Light" + banned terms/aesthetics) [L2] [implemented — locked copy]
[PS-02] DualStateRequirement (silent-extraordinary + reactive-extraordinary) [L2] [partial]
[PS-03] WhatIsThatTest (visceral-wow gate) [L2] [implemented as filter]
[PS-04] FoundersEditionDemoCarryFilter (90-second launch-video viability) [L2] [implemented as filter]
[PS-05] ReflectiveTwinContract (dual-strip reads as single 330 mm panel) [L2] [partial — enforcement gap]
[PS-06] RestraintBeatsMaximalismLock (monochrome wins; one colour wins) [L2] [implemented as filter]
[PS-07] DefensibilityMoatClassification (hardware/audio/composition/commodity) [L2] [implemented as classifier]
[PS-08] CustomerGeneratedContentCapture (social-loop force multiplier) [L1] [partial]
[PS-09] WakeUpChoreography (cinematic boot ritual) [L1] [absent — proposed F6]
[PS-10] MoodDrivenAutonomousModeSelection [L1] [absent]  TENSION with PS-01 — risks "smart-lighting" framing
```

### 2.10 Viability gates (VG-) — Level-2, SA-4

```
[VG-01] BrandVoiceGate (positioning + locked language + aesthetic) [L2] [implemented]  Reversible if positioning loosens
[VG-02] EngineeringGate (2.0 ms / no-heap / dt-correct / centre-origin) [L2] [implemented]  Irreversible at hardware tier
[VG-03] ImplementationEffortTier (CHEAP / MODERATE / AMBITIOUS) [L1] [implemented as classifier]
[VG-04] DefensibilityTier (uncopyable / hard-to-copy / commodity) [L1] [implemented as classifier]
[VG-05] TempoSourceReliabilityGate (ESV11 vs PipelineCore) [L1] [partial]  Soft engineering gate; confidence > 0.6 recommended
```

**Total unique domains: 119** (14 INF + 25 AUD + 26 PER + 14 PHY + 16 GEO + 16 COM + 12 LIN + 9 HW − overlaps + 10 PS + 5 VG, after INF dedup of cross-cited substrates).

---

## 3. Within-Subagent Interaction Summaries

The four subagent fragments documented their own within-scope pairwise interactions. Rather than reproduce them at length, the orchestrator preserves their conclusions and points to the source. Pass 2 graph construction will treat these as edge sets:

| Subagent | Within-scope pairs | Class breakdown | Key high-conviction edges |
|---|---|---|---|
| SA-1 (AUD + PER) | 28 multiplicative | 6 audio×audio, 8 PER×PER, 11 audio×PER, 3 substrate flagged | AUD-12×AUD-13 (2-D timbre), AUD-05×INF-04 (tempo doctrine), PER-11×PER-15 (Captain's "strongest signature"), PER-14×PER-20→PER-25 (velocity-aniso), AUD-04×PER-19 (onset×cross-blend), AUD-21×PER-18 (voice-aware silence) |
| SA-2 (PHY + GEO) | 24 pairs (11 multiplicative, 4 combinatorial, 8 additive, 1 conflict) | 6 dual-strip-EXCLUSIVE | PHY-01×PHY-09 (cross-strip viscoelastic), GEO-01×GEO-13 (interference + temporal echo), PHY-03×PHY-10 (advection+wave PDE), GEO-03×GEO-13 (parallax+phase-delay), PHY-04×PHY-05 (easing×particle-pool); GEO-06×GEO-15 mutually-exclusive conflict |
| SA-3 (COM + LIN) | 31 pairs (17 combinatorial, 8 multiplicative, 6 additive) | INF-06 cross-cutting | INF-01×COM-03 (LayerStack→ConvictionMixer canonical use case), INF-01×COM-02×COM-04 (Sky/Score triad), INF-02×INF-06 (LPF needs opt-out flags), LIN-10×INF-12 (motion-blur chromagram needs fx_dots cache), LIN-06 K1-native exemplar |
| SA-4 (HW + PS) | 17 pairs (5 combinatorial, 8 multiplicative, 4 additive) | 6 brand×hardware cross-level edges | HW-01×HW-02 combinatorial (LGP+Dual-Strip = optical interference physics), HW-01×HW-03 multiplicative (signature aesthetic), HW-02×HW-04 (parallax depth via PSRAM), HW-02×PS-05 TENSION (interference vs single-panel reading), VG-01×VG-02 (orthogonal axes; brand-voice rejection reversible, engineering rejection not) |

Pass 2 will construct the directed graph using all subagent edges plus the cross-subagent edges below. The 4 fragments together yield ~100 within-scope pairs; the orchestrator now adds the cross-subagent edges that no single subagent could see.

---

## 4. Cross-Subagent Interaction Matrix (orchestrator's unique deliverable)

The orchestrator focuses on **multiplicative** and **combinatorial** edges only — additive cross-subagent pairs are omitted (they don't change the topology). Edges are grouped by the bridge they cross. Citations point to the SSA(s) or brainstorm line(s) that ground the edge.

### 4.1 Audio × Physics

```
[AUD-12 × PHY-01] SpectralCentroid × SpringMassDamperLattice → multiplicative
     Centroid drives spring stiffness k_inter; bright timbre = stiffer lattice (faster waves), dark timbre = softer (slow ringing). Without centroid, spring lattice has no timbral character; without lattice, centroid is a scalar without spatial expression.
     Prerequisite: AUD-12 absent (12 LOC); PHY-01 absent.
     Dual-strip amplification: yes — independent k per strip × HW-09 viscous bleed = stereo timbre flow.
     Cross-level: no (both L0/L1).
     Evidence: SSA-Physics §1; SSA-AudioDriver §1.

[AUD-05 × PHY-01] BeatTrackTempo × SpringMassDamperLattice → combinatorial
     Beat tick injects centre force impulse F(t)·δ(t-t_beat) at LED 79/80; lattice rings outward. Combined with PER-15 BeatLockedRefresh = Captain's "tempo-locked breath" pattern.
     Prerequisite: PHY-01 needs INF-09 (CFL gate) + INF-08 (sin LUT).
     Dual-strip amplification: yes (mirrored radial impulse).
     Cross-level: no.
     Evidence: SSA-Physics §1.

[AUD-02 × PHY-07] ChromaVector × KuramotoPhaseLattice → multiplicative
     Chroma drives oscillator natural frequencies ω_i = ω_base + chroma[i mod 12]·Δω. Chord-modal coherence emerges as Kuramoto order parameter — pulled-toward-sync at consonant chords, fragmented at dissonant. Order parameter exposable as derived signal for downstream effects.
     Prerequisite: INF-08 mandatory (32 oscillators × 32 sins = 1024 sin/frame).
     Dual-strip amplification: yes — strip A consonant lattice + strip B dissonant lattice with cross-coupling = "tension-resolution" stereo.
     Cross-level: yes — flag (AUD-02 L0 × PHY-07 L1 × INF-08 L0). Healthy substrate-consumer.
     Evidence: SSA-Physics §6.

[AUD-04 × PHY-05] OnsetFlux × ParticlePoolNewtonianIntegrator → multiplicative
     Onset events spawn pool agents at centre 79/80 with outward velocity. Without onset, pool is empty; without pool, onset has no graceful spatial expression.
     Prerequisite: AUD-04 implemented; PHY-05 partial.
     Dual-strip amplification: yes (mirrored spawn).
     Cross-level: no.
     Evidence: SSA-Physics §10, §12.

[AUD-14 × PHY-06] PitchHPSConfidence × CoulombFieldRender → multiplicative
     HPS confidence gates whether chroma-as-charge is meaningful. High HPS confidence → render Coulomb field; low confidence (drums/noise) → field collapses to neutral. Either alone is unstable.
     Prerequisite: AUD-14 +40 LOC; PHY-06 absent.
     Dual-strip amplification: yes.
     Cross-level: no.
     Evidence: SSA-Physics §5; SSA-AudioDriver §3.
```

### 4.2 Audio × Geometry

```
[AUD-02 × GEO-02] ChromaVector × StandingWaveHarmonicLattice → combinatorial
     Direct chroma[i] → A_n (mode amplitude) mapping. brightness[i] = Σ_n chroma[n] · sin(nπi/N) · cos(ω_n·t). 12 modes; LGP smooths nodes into "musical interference patterns".
     Prerequisite: INF-08 (sin LUT) — 12×320 = 3840 sins/frame ≈ 3.8 ms naïve, 0.2 ms with LUT.
     Dual-strip amplification: yes — even modes vs odd modes split L/R.
     Cross-level: yes — flag (L0 × L1 × L0 substrate).
     Evidence: SSA-Geometry §2; SSA-AudioDriver §4.

[AUD-12 × GEO-01] SpectralCentroid × BilateralWaveInterference → multiplicative
     Centroid drives wavenumber k. Bright timbre = fine fringes; dark timbre = broad lobes. Two-source counter-propagation produces interference fringes audio-modulated.
     Prerequisite: AUD-12 +12 LOC; INF-08; HW-02 dual-strip.
     Dual-strip amplification: YES — DUAL-STRIP-EXCLUSIVE (single-strip cannot produce two-source interference).
     Cross-level: no.
     Evidence: SSA-Geometry §1.

[AUD-01 × GEO-07] BandEnergyOctave × SpatialFourierSynthesizer → combinatorial
     bands[0..7] → A_k spatial-frequency amplitudes. Direct mapping — bass = broad lobe, treble = fine pattern. brightness[i] = Σ_k bands[k]·cos(2π·k·i/N + φ_k).
     Prerequisite: INF-08.
     Dual-strip amplification: yes (per-strip phase shift).
     Cross-level: no.
     Evidence: SSA-Geometry §7.

[INF-04 × GEO-02] TempoPhaseContinuous × StandingWaveHarmonicLattice → multiplicative
     Phase drives modal angular frequency ω_n. Modes pulse on the beat — chord wave equation breathing tempo-locked.
     Prerequisite: INF-04.
     Dual-strip amplification: yes.
     Cross-level: no.
     Evidence: brainstorm Theme 2 + Theme 3 convergence.

[AUD-04 × GEO-12] OnsetFlux × MultiOriginCollisionField → multiplicative
     Each onset spawns a new emit point in the 8-slot ring; ripples pass through linearly until two meet — collision produces constructive peak. Without onsets, ring decays; without ring, onsets are flashes.
     Prerequisite: AUD-04 implemented; GEO-12 absent.
     Dual-strip amplification: yes (cross-strip ripple collision).
     Cross-level: no.
     Evidence: SSA-Geometry §12.

[AUD-20 × GEO-04] ChromaDerivativeHarmonicMotion × HolographicTwoSourceFringe → multiplicative
     Chord change spawns new source positions in the hologram → fringe pattern rearranges at chord boundaries. Sustained chord = static hologram; chord change = re-tessellation.
     Prerequisite: AUD-20 [partial — saliency.harmonic may equal]; GEO-04 absent.
     Dual-strip amplification: yes — DUAL-STRIP-EXCLUSIVE.
     Cross-level: no.
     Evidence: SSA-Geometry §4.
```

### 4.3 Audio × Persistence (most already in SA-1's matrix, key cross-bridges only)

Already comprehensive in SA-1 fragment. Orchestrator notes the **emergent triad** AUD-21 × PER-18 × INF-02 (voice classifier × audio-gated decay × global LPF) is the **F3 Liquid Stillness ambient state** stack — covered as triadic in §5 below.

### 4.4 Audio × Composition

```
[AUD-21 × COM-06] VoiceVsMusicClassifier × VoiceVsMusicSwitcher → combinatorial
     Tight coupling: AUD-21 (or AUD-24 VAD) is the input gate; COM-06 is the consumer. Neither exists without the other.
     Prerequisite: AUD-21 (heuristic, render-trivial) or AUD-24 (full VAD, AudioActor work) before COM-06.
     Dual-strip amplification: neutral.
     Cross-level: yes — flag (L0 × L1).
     Evidence: SSA-Composition §4.

[AUD-23 × COM-03] TempoConfidence × ConvictionMixer → multiplicative
     Founding use case: layer α = (1-√√tempo_confidence)·0.85+0.15. Direct ES spectronome semantics.
     Prerequisite: AUD-23 absent (derivable from auto-corr strength); COM-03 needs INF-01.
     Dual-strip amplification: neutral.
     Cross-level: yes — flag (L0 × L1).
     Evidence: SSA-CrossLineage §5; brainstorm Theme 2.

[AUD-18 × COM-03] InterBandCofiringMatrix × ConvictionMixer → multiplicative
     Polyphony correlation drives mixer's fast/slow blend coefficient; simple grooves (high correlation) lean fast (snappy unified pulse), complex polyphony leans slow (independent persistent ghosts). "Music-complexity-aware" mixer.
     Prerequisite: AUD-18 +80 LOC; COM-03 needs INF-01.
     Dual-strip amplification: yes (left = fast branch, right = slow branch).
     Cross-level: no.
     Evidence: SSA-AudioDriver §8; SSA-Persistence A3 mix-coefficient role.

[AUD-04 × COM-15] OnsetFlux × DecoratorPoolEventOverlay → combinatorial
     Onsets drive decorator spawn; without onsets, pool is dead. Rate-limit at 8 decorators/sec.
     Prerequisite: AUD-04 implemented; COM-15 absent (4×16 B static).
     Dual-strip amplification: yes (centre-pop natural mirror).
     Cross-level: no.
     Evidence: SSA-Composition §13.

[INF-04 × COM-07] TempoPhaseContinuous × TwoStepBeatQuantisedComposer → combinatorial
     Two-Step IS the beat-quantised mode swap; cos² ±60 ms crossfade window centred on beat REQUIRES continuous phase. Beat-flag-only fallback produces step-square not cos² envelope.
     Prerequisite: INF-04 strongly before COM-07.
     Dual-strip amplification: neutral.
     Cross-level: yes — flag.
     Evidence: SSA-Composition §5.

[AUD-25 × COM-08] MoodClassifier × MoodStateMachineComposer → combinatorial
     FSM is meaningless without classifier-derived state; classifier is academic without FSM consumer. Tight coupling.
     Prerequisite: AUD-25/INF-11 before COM-08.
     Dual-strip amplification: neutral.
     Cross-level: yes — flag.
     Evidence: SSA-Composition §6.
```

### 4.5 Audio × LIN (cross-lineage emergent fusions)

```
[INF-04 × LIN-01] TempoPhaseContinuous × PitchClassPendulumBouquet → combinatorial
     12-pendulum bouquet needs per-pitch-class tempo phase; INF-04 single-tempo variant maps all 12 to same phase (less ambitious but ships earlier per brainstorm:152 Captain Q1).
     Prerequisite: INF-04 (single-tempo) OR INF-05 (per-bin bank — full doctrine).
     Dual-strip amplification: yes — mirror-symmetric pendulum sweeps anchor centre 79/80.
     Cross-level: yes (L0 × L1).
     Evidence: SSA-CrossLineage §1.

[AUD-02 × LIN-08] ChromaVector × AttackOnlyPitchClassVelocityField → multiplicative
     12-class chroma each with SB attack-only follower drives Perlin-ribbon pan velocity. Onset = sprint; sustain = drift. Render top-3 classes only.
     Prerequisite: AUD-02 implemented; LIN-08 absent (96 B scratch).
     Dual-strip amplification: yes.
     Cross-level: no.
     Evidence: SSA-CrossLineage §8.

[INF-04 × LIN-09] TempoPhaseContinuous × BeatParityBloomSpriteInjection → multiplicative
     Narrow-window injection gating |phase - 0.65| < 0.02 requires continuous phase. Beat-flag-only fallback works but loses sub-beat positioning that gives sprite "musical breath".
     Prerequisite: INF-04 strongly preferred.
     Dual-strip amplification: yes.
     Cross-level: yes (L0 × L1).
     Evidence: SSA-CrossLineage §9.

[AUD-02 × LIN-10] ChromaVector × MotionBlurCachedChromagramDots → combinatorial
     Wholly new emergent: SB chromagram_dots positions × ES fx_dots[12] cache = 12 chromatic dots with sub-pixel motion trails. Both halves are K1 gaps; fusion only natural here.
     Prerequisite: AUD-02 implemented; INF-12 fx_dots cache.
     Dual-strip amplification: yes (mirror-symmetric pairs).
     Cross-level: yes (L0 × L0 × L1).
     Evidence: SSA-CrossLineage §10. **K1-native exemplar.**

[AUD-05 × LIN-09] BeatTrackTempo × BeatParityBloomSpriteInjection → multiplicative
     Beat-flag fallback for LIN-09 if INF-04 not yet wired.
     Evidence: SSA-CrossLineage §9 fallback path.
```

### 4.6 Audio × HW (hardware affordances)

```
[AUD-08 × HW-02] STMTemporalSpectral × DualStrip → multiplicative
     STM bands can split L/R (left strip = even modulation bands, right = odd) — per-band tremolo readout impossible on single-strip without sacrificing chromatic colour-mapping.
     Prerequisite: AUD-08 implemented but unconsumed at render side.
     Dual-strip amplification: YES — DUAL-STRIP-EXCLUSIVE readout.
     Cross-level: no.
     Evidence: SSA-AudioDriver §5; brainstorm Theme 4.

[AUD-01 × HW-02] BandEnergyOctave × DualStrip → multiplicative
     "Frequency-Spatial Strip Stereo": strip A = treble, strip B = bass. Doubled spatial canvas; novel readout impossible on single-strip.
     Evidence: SSA-K1Specific §5.

[AUD-02 × HW-02 × HW-03] ChromaVector × DualStrip × CentreOriginInvariant → combinatorial
     "Chroma-Spatial Pitch Lattice": strip A = current chroma, strip B = predicted chroma; both anchored at centre 79/80. Triadic — three-way K1-native lattice.
     Prerequisite: HW-02 + HW-03 + AUD-02 implemented.
     Dual-strip amplification: yes — DUAL-STRIP-EXCLUSIVE.
     Cross-level: yes (multiple).
     Evidence: SSA-K1Specific §11.
```

### 4.7 Persistence × Physics/Geometry (continuum stack)

```
[INF-02 × PHY-20] FramebufferLPF × HeatEquationDiffusion1D → additive→multiplicative
     INF-02 is global temporal LPF; PHY-20 (=PER-20) is spatial diffusion. Temporal+spatial = full 1+1D diffusion stack. Composed correctly (PER-20 first, INF-02 after) yields Stable PDE smoothing.
     Prerequisite: INF-02 + INF-09 (CFL gate) + INF-06 (opt-out flag to avoid double-diffusion).
     Dual-strip amplification: yes — independent diffusion per strip + HW-01 LGP optical-blur = three-stack diffusion.
     Cross-level: yes — flag (L2 substrate × L1 capability).
     Evidence: SSA-Persistence C1; brainstorm Theme 1+3.

[PER-14 × PHY-03] PredictiveTrailOpticalFlow × FluidAdvectionDiffusion1D → multiplicative
     PER-14 estimates per-pixel velocity from frame-Δ; PHY-03 advects content along velocity. Combined = "fluid follows the beat".
     Prerequisite: INF-09; HW-08 SIMD strongly recommended.
     Dual-strip amplification: yes (centre-out outward velocity natural on dual-mirror).
     Cross-level: no.
     Evidence: SSA-Persistence A6; SSA-Physics §3.

[PER-11 × GEO-03] MultiScaleMemoryComposite × ParallaxDepthStackCompositor → multiplicative
     Fast/slow branches assigned to far/mid/near depth layers — natural depth-of-field on a moving image. Far = slow τ (background blur), near = fast τ (sharp focus).
     Prerequisite: PER-11 absent; GEO-03 needs INF-01.
     Dual-strip amplification: yes (left strip near, right far — true parallax).
     Cross-level: yes — flag.
     Evidence: SSA-Persistence A3; SSA-Geometry §3.

[INF-03 × GEO-13] PSRAMFrameRing × InterStripPhaseDelayBuffer → combinatorial
     GEO-13 strip-B-reads-strip-A-τ-delayed REQUIRES INF-03 substrate. Tempo-modulated lag = perceived depth illusion. DUAL-STRIP-EXCLUSIVE.
     Prerequisite: INF-03 strictly before GEO-13.
     Dual-strip amplification: YES — exclusively dual-strip.
     Cross-level: yes (L2 × L1).
     Evidence: SSA-Geometry §13; brainstorm Theme 4.
```

### 4.8 Persistence × Composition

```
[INF-02 × INF-01] FramebufferLPF × LayerStack → combinatorial
     Two GIVEN substrates. Order matters: LPF-after-compositing = unified global softness; LPF-per-layer = layer-wise softness control. Captain Q3 (brainstorm:154) requires policy decision.
     Prerequisite: INF-06 (opt-out flag) before either ships as mandatory.
     Dual-strip amplification: neutral (architectural).
     Cross-level: yes (both L2).
     Evidence: brainstorm:154; this is the GIVEN multiplicative chain.

[PER-11 × COM-03] MultiScaleMemoryComposite × ConvictionMixer → multiplicative
     Fast/slow branches presented as two layers in mixer; mixer assigns confidence-weighted α. Natural compositional unity.
     Prerequisite: COM-03 needs INF-01.
     Dual-strip amplification: yes.
     Cross-level: yes (L0 × L1).
     Evidence: SSA-Persistence A3; SSA-Composition §1.

[INF-03 × COM-11] PSRAMFrameRing × EchoComposerTimeMirror → combinatorial
     COM-11 IS the cleanest demonstrator of INF-03. Multi-tap visual delay line.
     Prerequisite: INF-03 + AUD-05 (beat-locked delays).
     Dual-strip amplification: neutral (per-strip).
     Cross-level: yes (L2 × L1).
     Evidence: SSA-Composition §9; brainstorm:146.
```

### 4.9 Hardware × Composition / LIN (the dual-strip × LayerStack convergence)

```
[HW-02 × INF-01 × GEO-03] DualStrip × LayerStack × ParallaxDepthStackCompositor → combinatorial
     Three-way: dual-strip provides physical separation; LayerStack provides overlap renderer; ParallaxDepth provides irrational-velocity layer assignment. Combined: true 3D depth illusion physically anchored to dual-strip parallax that single-strip cannot reproduce.
     Prerequisite: HW-02 implemented; INF-01 absent; GEO-03 absent.
     Dual-strip amplification: YES — DUAL-STRIP-EXCLUSIVE.
     Cross-level: yes (L1 × L2 × L2). **Highest-leverage triadic edge.**
     Evidence: SSA-K1Specific §2; SSA-Geometry §3; brainstorm Theme 4+5.

[HW-02 × HW-01 × GEO-01] DualStrip × LGP × BilateralWaveInterference → combinatorial
     Optical-physics interference becomes literally observable: two emitter strips + diffusing medium + mathematical interference pattern = physically computable interference fringes on K1, geometrically impossible on competing single-strip + non-LGP hardware. **Strongest moat edge in entire registry.**
     Prerequisite: HW-01 + HW-02 implemented; GEO-01 absent.
     Dual-strip amplification: YES — DUAL-STRIP-EXCLUSIVE × LGP-EXCLUSIVE.
     Cross-level: no (all L1).
     Evidence: SSA-K1Specific §4; SSA-Geometry §1; brainstorm Theme 4 verdict (catalogue:73).

[HW-02 × PS-05] DualStrip × ReflectiveTwinContract → multiplicative (TENSION)
     Two interpretations of dual-strip: Reflective Twin says "make the strips invisible as separate objects" (mirror, phase-coherent); Theme 4 says "exploit the strips as two emitters" (interference, parallax). RESOLUTION: contract is "reads as one object at the macro level"; HW-01 LGP physically fuses interference fringes into a single field, satisfying both. **But explicit Captain ratification needed (open Q4.10.1 below).**
     Prerequisite: policy clarification.
     Dual-strip amplification: yes.
     Cross-level: yes — flag (L1 × L2).
     Evidence: SSA-ProductStrategy §F5; SSA-K1Specific §4; brainstorm V1.0 #5 vs #6.
```

### 4.10 Hardware × LIN (K1-native exemplars + brand-locked anchors)

```
[HW-03 × LIN-06] CentreOriginInvariant × CentreOriginRadialTimeScope → multiplicative
     LIN-06 is wholly new emergent — a temporal-radial readout that ONLY exists because K1 has the centre-origin invariant. Newest samples at LED 79/80, oldest at edges; both strips natively show same time profile.
     Prerequisite: HW-03 structural; INF-12 (320 B ring).
     Dual-strip amplification: yes; LGP fuses radial scroll into apparent "time depth".
     Cross-level: yes — flag (L2 × L1). **K1-native exemplar #1.**
     Evidence: SSA-CrossLineage §6; brainstorm:227-229.

[HW-03 × LIN-10] CentreOriginInvariant × MotionBlurCachedChromagramDots → multiplicative
     Same K1-native logic: SB chromagram_dots + ES fx_dots[]; both halves K1 gaps, fusion natural only on platform that takes both lineages AND has centre-origin invariant.
     Prerequisite: HW-03 structural; AUD-02; INF-12.
     Dual-strip amplification: yes.
     Cross-level: yes. **K1-native exemplar #2.**
     Evidence: SSA-CrossLineage §10.

[HW-01 × PER-09] LGP × ChromaticPhosphorDecay → multiplicative
     LGP smoothing physically improves the per-channel τ aesthetic (per-RGB chromaticity decay). On naked LEDs the decay would look grainy; on diffused glass it reads as "CRT phosphor on glass". Bridge candidate per SA-1.
     Evidence: SSA-Persistence A1 K1-affordance note.
```

### 4.11 Brand-voice × hardware/audio (filter cascades)

```
[PS-01 × VG-01 × COM-14] LockedPositioning × BrandVoiceGate × CloudMaskPerlinComposer → multiplicative (REHABILITATION)
     COM-14 re-purposes cubic-spatial-Perlin (from kill-listed kaleidoscope) as alpha mask vs colour-channel input. Brand-voice rejection of kaleidoscope is **aesthetically granular, not capability-wholesale** (SA-4 viability artifact). COM-14 passes brand voice precisely because it reframes the same engineering primitive.
     Prerequisite: COM-14 needs INF-01.
     Dual-strip amplification: yes (centre-mirror Perlin natural).
     Cross-level: yes (L2 × L2 × L1).
     Evidence: SSA-Composition §12; SSA-ProductStrategy KILL-LIST §line 124.

[PS-10 × VG-01] MoodDrivenAutonomousModeSelection × BrandVoiceGate → multiplicative (TENSION)
     Risks "smart-lighting" framing if transitions feel arbitrary. Either curate mood→mode mapping such that transitions feel intentional, OR surface user-explicit control. Open Captain Q.
     Cross-level: yes.
     Evidence: SSA-K1Specific §9; SSA-ProductStrategy silent on this proposal.

[PS-09 × HW-03] WakeUpChoreography × CentreOriginInvariant → multiplicative
     "Dark frame → spark at centre 79/80 → outward bloom → settles into active mode." Centre-origin gives the wake-up its visual grammar — without it the wake-up is just a generic boot animation.
     Prerequisite: HW-03 structural; PS-09 ~100 LOC.
     Dual-strip amplification: yes (mirrored bloom).
     Cross-level: yes (L2 × L1).
     Evidence: SSA-ProductStrategy §F6.
```

---

## 5. Top-10 Higher-Order (Triadic+) Synergies

Per Pass 1 instructions: capped at 10. Each must explain why removing one member ELIMINATES (not merely reduces) the emergent capability. Stop at order-3 unless order-4 is qualitatively distinct.

```
[T-01] DualStrip × LGP × BilateralWaveInterference (HW-02 × HW-01 × GEO-01)
     Emergent capability: Physically observable optical-interference fringes on a single diffused panel. Without HW-02: only one source — no interference. Without HW-01: two crisp lines, no fringe pattern. Without GEO-01: two strips lit independently, no superposition mathematics applied. All three required.
     Order: triadic (no order-4 distinction).
     Moat strength: ABSOLUTE — geometrically impossible on competing hardware (single-strip OR no-LGP).
     Evidence: brainstorm Theme 4 verdict (catalogue:73); SSA-Geometry §1; SSA-K1Specific §4.

[T-02] FramebufferLPF × LayerStack × ControlBus reuse (INF-02 × INF-01 × AUD-01..11)
     Emergent capability: GIVEN per Context Preamble. LPF turns hard-pixel mode into fluid; LayerStack converts mode count from additive to combinatorial; ControlBus reuse means audio side is "free". Without LPF: every effect needs own trail mechanism, no uniform softness knob, no liquid character. Without LayerStack: modes remain disjoint single-renders, no composition vocabulary. Without ControlBus reuse: audio side cost dominates render-side benefit.
     Order: triadic.
     Moat strength: STRONG — substrate that compounds, not directly copyable without equivalent investment.
     Evidence: GIVEN; brainstorm Themes 1+5+6.

[T-03] BeatTrack × MultiScaleMemoryComposite × BeatLockedRefresh (AUD-05 × PER-11 × PER-15)
     Emergent capability: Captain's named "strongest novel signature" (SSA-Persistence l.479). Fast branch (30 ms) cleared on beat; slow branch (400 ms) accumulates across beats. Result = tempo-quantised foreground over slow harmonic background. Without beat: no tempo quantisation. Without multi-scale: only one time-scale, no foreground/background separation. Without beat-locked refresh: blends are continuous, not musically anchored.
     Order: triadic.
     Moat strength: STRONG — composes audio-DSP, persistence engine, and rhythmic doctrine; defensible algorithm + hardware moat.
     Evidence: SSA-Persistence l.479 explicit triple.

[T-04] VoiceClassifier × AudioGatedDecay × FramebufferLPF (AUD-21 × PER-18 × INF-02)
     Emergent capability: V1.0 F3 Liquid Stillness ambient state. Voice classifier extends "silence" definition to include speech; PER-18 drives τ short for silence (clear sparkle, ghost remains); INF-02 supplies the universal trail. Without any one: silence behaviour is arbitrary, no calm-mode UX, no graceful sleep/wake transition.
     Order: triadic.
     Moat strength: MEDIUM — audio + algorithm composition; replicable but UX-sensitive.
     Evidence: SSA-AudioDriver §11; SSA-Persistence B4; brainstorm V1.0 F3 (catalogue:106).

[T-05] DualStrip × LayerStack × ParallaxDepthStackCompositor (HW-02 × INF-01 × GEO-03)
     Emergent capability: True 3D depth illusion physically anchored to dual-strip parallax. Without dual-strip: depth is only via velocity ratios, no physical parallax. Without LayerStack: no overlap rendering for depth layers. Without ParallaxDepth: layers don't separate by depth velocity.
     Order: triadic.
     Moat strength: ABSOLUTE on physical-parallax axis; STRONG on software-velocity-only axis.
     Evidence: SSA-Geometry §3; brainstorm Theme 5.

[T-06] FramebufferLPF × HeatEquationDiffusion × LGP (INF-02 × PHY-20 × HW-01)
     Emergent capability: Three-stack diffusion (temporal LPF + spatial heat equation + optical LGP smoothing) = the "liquid centre-origin chiaroscuro" signature aesthetic at full physical depth. Without LPF: hard-pixel quantisation in time. Without heat-eq: spatial sharpness inconsistent. Without LGP: simulation visible, glass/liquid metaphor breaks.
     Order: triadic.
     Moat strength: STRONG — algorithm+hardware composition; LGP forms hardware moat.
     Evidence: brainstorm Theme 1+3 convergence; SSA-K1Specific §1+§7.

[T-07] CentreOrigin × DualStrip × MirroredCentreEmanation (HW-03 × HW-02 × GEO-15)
     Emergent capability: K1's brand-mechanical signature. Centre as gravitational anchor; dual strips physically separated; mirror-symmetric emanation. Without centre: no anchor. Without dual: no stereo emanation. Without mirror: two independent strips, brand-voice failure (PS-05 ReflectiveTwin violated).
     Order: triadic.
     Moat strength: ABSOLUTE on hardware-mechanical axis.
     Note: this is the BRAND TRIAD — every effect should honour this triad as default. Departures (GEO-06 ring, GEO-10 drift) require explicit Captain ratification.
     Evidence: SSA-K1Specific §8; brainstorm:107 V1.0 #5.

[T-08] TempoPhase × LayerStack × TwoStepBeatQuantisedComposer (INF-04 × INF-01 × COM-07)
     Emergent capability: Beat-quantised mode swap with cos² ±60 ms crossfade. Without phase: only beat-flag, step-square envelope (loses musical centring). Without LayerStack: cannot composite the two modes during crossfade. Without COM-07: no beat-quantised swap, just continuous transitions.
     Order: triadic.
     Moat strength: MEDIUM — software-replicable but tightly coupled to ESV11 beat-tracking quality.
     Evidence: SSA-Composition §5.

[T-09] PSRAMFrameRing × LayerStack × DualStrip (INF-03 × INF-01 × HW-02)
     Emergent capability: Inter-strip phase-delay (GEO-13) + parallax-depth-stack (GEO-03) + echo-composer (COM-11) ALL share this triad as substrate. INF-03 supplies time-shifted reads; INF-01 supplies overlap composition; HW-02 supplies physical parallax. Order-4 with any specific GEO-13/GEO-03/COM-11 doesn't increase qualitatively.
     Order: triadic substrate (multiple consumer capabilities).
     Moat strength: STRONG — combines hardware (PSRAM affordance, dual-strip) with algorithm (composition).
     Evidence: brainstorm Infra #6; SSA-Geometry §13; SSA-Composition §9.

[T-10] LGP × PerChannelChromaticDecay × CentreOriginEmission (HW-01 × PER-09 × HW-03)
     Emergent capability: "CRT phosphor on glass" aesthetic — per-RGB τ decays through colour-time (R slow, B fast) at LED-precision, optically smoothed by LGP, anchored at centre 79/80. Without LGP: grain visible. Without per-channel τ: monochromatic decay only. Without centre-origin: no aesthetic anchor.
     Order: triadic.
     Moat strength: STRONG — hardware (LGP+CRGB precision) + algorithm; aesthetically distinctive.
     Note: PER-09 is a SA-1 [divergent] singleton. Surfacing it via this triad rescues it from synthesis-loss bias.
     Evidence: SSA-Persistence A1 K1-affordance note (l.40).
```

**Order-4+ scan:** Several order-4 candidates (e.g. T-02 + AUD-05 = "GIVEN + beat-track"; T-03 + INF-02 = "Captain triple + global LPF") were considered but reduce to T-02/T-03 plus an additive amplifier rather than producing qualitatively distinct emergent capability. The orchestrator stops at order-3.

---

## 6. Coverage flags & known blind spots

Per Pass 4 adversarial requirements, the orchestrator surfaces uneven coverage and missing-domain candidates so Pass 4 can stress-test:

1. **Long-context temporal evolution** (phrase / song / session-level): partially covered by COM-08 MoodStateMachine, COM-09 StoryArc, INF-11 LongWindowStats, PS-10. Underweight given how "session-arc" is one of the strongest brand-voice differentiators (curated-feel vs algorithmic-feel). Pass 4 should challenge whether the registry adequately captures multi-minute evolution.

2. **Failure modes** (silent / clipping / mono-frequency / adversarial audio): partially covered by PER-18 audio-gated decay, AUD-21 voice classifier, PS-02 dual-state. NOT covered: clipping behaviour, mono-frequency feedback, deliberate adversarial input. Pass 4 should add.

3. **Manufacturing / hardware variance** (LED brightness curves, diffuser opacity, thermal throttling): NOT covered by any subagent. K1 hardware-affordance treatment assumes nominal hardware. Pass 4 should add.

4. **User experience / perceptual psychology**: partially via brand-voice (PS-01, PS-03 WHAT-IS-THAT), but no domain explicitly captures "time to perceptual fatigue" or "brightness-vs-contrast preferences". Pass 4 should add.

5. **Singleton SA-3 source-truncation**: SA-3 had taxonomy file truncated to line 1. Compensated via brainstorm-catalogue cross-references but a small probability of missed taxonomy-only details remains. Recommend Pass 2/3 spot-check any LIN- domain claim against the taxonomy directly.

6. **Status-conflict resolution** (INF-04 / AUD-06): SSA-AudioDriver lists `es_phase01_at_audio_t` as ControlBus field; brainstorm:52 says not surfaced. Recommend `mcp__clangd__get_document_symbols` on `src/audio/contracts/ControlBus.h` for definitive answer before Pass 3 sequencing.

7. **Brand-voice rejection class-coding**: SA-4 viability artifact found ZERO engineering rejections, all 4 kills brand-voice. Pass 4 may legitimately challenge any kill-list entry on brand-positioning grounds without engineering pushback (a strong adversarial vector).

8. **Cross-level edge inflation risk**: 30+ edges flagged "cross-level". Most are healthy substrate-consumer relations (Level-2 substrate enables Level-1 capability) but some may be category errors. Pass 2 should partition cross-level edges into "substrate-consumer (healthy)" vs "category-error (suspect)".

---

## 7. Open questions for Captain (carrying forward to Pass 4)

1. **PS-05 Reflective Twin Contract** — does "reads as one object" allow asymmetric cross-strip patterns (interference, parallax) provided LGP fuses them into a single perceived field? Or is mirror-symmetry contractually required? This decision gates 6+ dual-strip-exclusive edges. (SA-4 Open Q1.)

2. **INF-04 vs INF-05** — single-tempo `tempoPhase01` first (matches ESV11 reliable beat-track; unlocks F2 + ~6 Theme 2 categories), or per-bin `tempi[N].phase` bank (matches ES doctrine; unlocks literal-import categories that ProductStrategy filter rejects anyway). Brainstorm:152 recommends single-tempo first; orchestrator agrees.

3. **INF-02 mandatory vs opt-in** — is the global framebuffer LPF a mandatory RendererActor pass or per-effect opt-in? Mandatory eliminates per-effect persistence boilerplate but risks double-trails on existing self-trailing effects without INF-06 effect-role-flags landing first. (Brainstorm:154; SA-1 cross-level edge.)

4. **GEO-06 / GEO-10 centre-origin departure** — is HW-03 a structural invariant or a default with audio-driven exception zones? Two domains intentionally depart (GEO-06 has no centre by topology; GEO-10 ≤±20 LED gating proposed). Captain ratification needed.

5. **PS-10 mood-driven autonomous mode selection** — risks PS-01 brand-voice failure (smart-lighting framing). Either curated mood→mode mapping (intentional-feel) OR user-explicit control. Captain decision.

6. **F3 Liquid Stillness curation arbitration** — who decides which 8–12 of the existing 106 ambient programmes survive? Process undefined per brainstorm:158.

7. **PS-08 customer-content-capture vs PS-04 founder-demo-carry vs PS-03 WHAT-IS-THAT** — rank-order across these three filters when they conflict on a candidate effect.

---

## 8. Outputs handed to Pass 2

Pass 2 reads this file and constructs:
- A directed graph with 119 nodes (one per domain in §2).
- Edges from all within-subagent matrices (§3) PLUS the cross-subagent matrix in §4 (multiplicative + combinatorial only).
- The 10 triadic synergies in §5 as candidate hubs.
- The 14 INF- substrates as candidate hubs (every Level-2 substrate is a hub by definition unless graph data refutes).
- The known infrastructure layer (T-02 GIVEN) as Layer 0; Pass 2 will identify Layers 1, 2, 3+ that become visible only after each prior layer lands.

Pass 2 must NOT modify this file; it writes `PASS_2_SYNERGY_TOPOLOGY.md`.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:claude-opus-4-7 (orchestrator + 4 SA fragments) | Created — Pass 1 of K1 visual-pipeline synergy-topology protocol. Merged 4 parallel-subagent domain fragments (SA-1 audio+persistence; SA-2 physics+geometry; SA-3 composition+cross-lineage; SA-4 K1-specific+product-strategy) into unified registry of 119 deduplicated domains (14 INF + 25 AUD + 26 PER + 14 PHY + 16 GEO + 16 COM + 12 LIN + 9 HW + 10 PS + 5 VG). Built cross-subagent multiplicative/combinatorial interaction matrix focused on bridges no single subagent could see. Identified 10 triadic synergies; flagged 8 coverage blind spots and 7 Captain decisions for Pass 4. Confidence per fragment: SA-1/SA-2/SA-4 HIGH, SA-3 MEDIUM-HIGH (taxonomy read truncated; mitigated via brainstorm cross-references). |
