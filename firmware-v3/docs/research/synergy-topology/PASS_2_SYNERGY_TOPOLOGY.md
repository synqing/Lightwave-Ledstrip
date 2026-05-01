---
abstract: "Pass 2 of the K1 visual-pipeline synergy-topology protocol. Reads PASS_1_DOMAIN_REGISTRY.md as input. Constructs the directed multiplicative-interaction graph from the 119-domain registry; identifies 8 platform-domain hubs (4 audio-side, 4 system-side) ranked by outbound multiplicative edge count; partitions the registry into 6 capability pillars (Composition, Persistence, Continuum-Dynamics, Cross-Lineage-Fusion, Audio-Interpretation, Dual-Strip-Geometry) plus 1 brand-meta pillar; identifies 5 bridge domains that connect otherwise isolated pillars (INF-04, INF-06, INF-08, AUD-02, HW-02); extracts 6 hidden synergy layers (Layer 0 = GIVEN, Layers 1–5 each visible only after the prior lands); proposes 6 external-field domain candidates (Reaction-Diffusion, Phase-Conjugate Holography, BeatEntropy, Kalman beat-phase prediction, JND perceptual floor, Distance-Field topology); and grades moat strength per layer (Layers 1–2 STRONG, Layer 3 ABSOLUTE on dual-strip subset, Layer 4–5 MEDIUM). Headline: every absolute moat in the registry routes through ≥1 of {LGP, Dual-Strip, Centre-Origin} — K1's hardware triad is the only inalienable moat axis."
---

# Pass 2 — Synergy Topology & Hidden Layer Extraction

**Protocol:** K1 Visual Pipeline Synergy Topology — Pass 2 of 4. Input: `PASS_1_DOMAIN_REGISTRY.md`. This pass executes sequentially in the orchestrator's main context. Output: this file + state for Pass 3.

**Scope:** Graph construction → hub identification → cluster detection → bridge domains → hidden-layer extraction → moat analysis → external-domain candidates. NOT a kill-order (that's Pass 3) and NOT an adversarial review (that's Pass 4).

---

## 1. Graph Construction

The directed graph has 119 nodes (one per Pass 1 §2 registry entry). Edges drawn ONLY from multiplicative + combinatorial interactions (additive edges omitted — they don't change topology). Direction `A → B` reads: "A must precede B AND A multiplicatively amplifies B's value." Bidirectional pairs (A↔B with mutual amplification, no strict precedence) drawn as unordered.

**Edge inventory (counted from Pass 1 §3 within-subagent + §4 cross-subagent matrices and §5 triadic decomposition):**

| Edge class | Count | Notes |
|---|---|---|
| Within-AUD pairs (audio×audio) | 6 multiplicative | SA-1 fragment |
| Within-PER pairs (persistence×persistence) | 8 multiplicative + 1 substrate | SA-1 fragment |
| Within-PHY×GEO pairs | 11 multiplicative + 4 combinatorial | SA-2 fragment |
| Within-COM/LIN pairs | 17 combinatorial + 8 multiplicative | SA-3 fragment |
| Within-HW/PS pairs | 5 combinatorial + 8 multiplicative | SA-4 fragment |
| Cross AUD×PHY | 5 cross-subagent | §4.1 |
| Cross AUD×GEO | 6 cross-subagent | §4.2 |
| Cross AUD×COM | 6 cross-subagent | §4.4 |
| Cross AUD×LIN | 5 cross-subagent | §4.5 |
| Cross AUD×HW | 3 cross-subagent | §4.6 |
| Cross PER×PHY/GEO | 4 cross-subagent | §4.7 |
| Cross PER×COM | 3 cross-subagent | §4.8 |
| Cross HW×COM/LIN | 5 cross-subagent | §4.9–4.10 |
| Cross PS×… (filter cascades) | 3 cross-subagent | §4.11 |
| **Total multiplicative+combinatorial edges** | **~108** | excludes additive and substitutive |

This is the directed-graph dataset Pass 2 reasons over.

---

## 2. Platform Domains (Hubs)

Hubs = nodes with the highest outbound multiplicative edge count. Hubs amplify everything they connect to — investment in a hub compounds across consumers. Two hub classes emerge: **system-substrate hubs** (Level-2 architectural) and **audio-driver hubs** (Level-0/1 ControlBus fields whose value is set by the number of consumer effects).

### 2.1 System-substrate hubs (Level-2)

```
[INF-01] LayerStack — outbound multiplicative edges: 12 — amplifies:
     COM-03 ConvictionMixer (canonical use case)
     COM-04 SkyScoreComposer
     COM-05 AudioDrivenCrossfader
     COM-07 TwoStepBeatQuantised
     COM-10 DensityGatedLayerStack
     COM-11 EchoComposer
     COM-12 AntiModeNegativeSpace
     COM-14 CloudMaskPerlin
     COM-15 DecoratorPoolEventOverlay (additive but amplified by stack depth)
     COM-16 StainGlassOrthogonalHSV
     GEO-03 ParallaxDepthStackCompositor
     PER-11 MultiScaleMemory (when presented as 2 layers in mixer)
     Justification: brainstorm:141 explicit "~150 LOC unlocks 12 Composition categories"; SA-3 fragment confirms every COM- domain except 01/02 has INF-01 dependency.

[INF-02] FramebufferLPF — outbound multiplicative edges: 8 — amplifies:
     PER-09 ChromaticPhosphor (per-channel τ on framebuffer)
     PER-11 MultiScaleMemory (slow-branch is a degenerate INF-02)
     PER-13 EchoSpatialDelay (subsumed)
     LIN-03 LogWarpedPhosphorTrail (variant)
     LIN-11 LpfDragCentreOriginCrossFade (specialisation)
     COM-X every composer (uniform softness knob)
     T-04 ambient-state triad
     T-06 three-stack diffusion triad
     Justification: brainstorm:40 names this K1's most under-served lineage gap; SA-3 cites as anchor of Theme 1.

[INF-03] PSRAMFrameRing — outbound multiplicative edges: 7 — amplifies:
     COM-11 EchoComposer (canonical use case)
     GEO-13 InterStripPhaseDelayBuffer (DUAL-STRIP-EXCLUSIVE)
     PER-13 EchoSpatialDelay (subsumed)
     PER-14 PredictiveTrailOpticalFlow (uses ring's prev-frame slot)
     LIN-06 CentreOriginRadialTimeScope (when scaled to multi-second history)
     T-09 PSRAMFrameRing × LayerStack × DualStrip triad substrate
     plus future Time-Warp Replay (V1.1 long-tail)
     Justification: brainstorm Infra #6; cited by 4 of 4 subagents (highest cross-cite count).

[INF-04] TempoPhaseContinuous — outbound multiplicative edges: 11 — amplifies:
     PER-15 BeatLockedRefresh (full doctrine)
     PER-16 TempoPhaseModulatedDecay
     PER-26 TempoLockedHeatEquation
     COM-07 TwoStepBeatQuantisedComposer
     COM-13 ParamCrossWire (tempo-phase modulator)
     COM-15 DecoratorPoolEventOverlay (tempo-quantised triggers)
     LIN-01 PitchClassPendulumBouquet (single-tempo variant)
     LIN-09 BeatParityBloomSpriteInjection (full doctrine)
     LIN-12 TempoPhasePendulumCometFan (with INF-05)
     GEO-02 StandingWaveHarmonicLattice (drives ω_n)
     T-03 Captain triple, T-08 tempo-phase mode swap triad
     Justification: brainstorm:52 names this K1's second under-served lineage gap; closing it unlocks Theme 2 (~6 categories).

[INF-06] EffectRoleFlags — outbound multiplicative edges: 6 — amplifies (mostly as enabler/lint):
     COM-12 AntiModeNegativeSpace (invert-input flag)
     COM-16 StainGlassOrthogonalHSV (renders-colour-only / renders-geometry-only)
     INF-02 mandatory ship (opt-out flag prevents double-trail)
     PER-09/11/13/14/PER-X opt-out hooks
     COM-04 SkyScoreComposer (background-vs-foreground tagging)
     Justification: SA-3 STRONG cross-cutting flag; SA-3 D38 explicit "lint check at boot"; brainstorm:154 Captain Q3 derivative.
     Note: this is an "infrastructure-or-fail" hub — without it several Level-1 capabilities silently mis-render.

[INF-08] sinLUT256 — outbound multiplicative edges: 7 (as precondition) — amplifies:
     PHY-01 SpringMassDamperLattice
     PHY-07 KuramotoPhaseLattice (1024 sin/frame budget)
     PHY-10 WaveEquationLeapfrog
     GEO-01 BilateralWaveInterference
     GEO-02 StandingWaveHarmonicLattice (3.8 ms naïve → 0.2 ms with LUT)
     GEO-04 HolographicTwoSourceFringe
     GEO-07 SpatialFourierSynthesizer
     GEO-12 MultiOriginCollisionField
     Justification: SA-2 explicit per-domain budget analysis; "precondition hub" — without it, half of Pillar A is over-budget.
```

### 2.2 Audio-driver hubs (Level-0/1)

```
[AUD-02] ChromaVector chroma[12] — outbound multiplicative edges: 6 — amplifies:
     PHY-07 KuramotoPhaseLattice (drives ω_i)
     GEO-02 StandingWaveHarmonicLattice (drives A_n)
     LIN-08 AttackOnlyPitchClassVelocityField
     LIN-10 MotionBlurCachedChromagramDots
     COM-13 ParamCrossWire (chroma-as-modulator)
     T-02 cross-strip pitch lattice (HW-02 × HW-03 × AUD-02)

[AUD-04] OnsetFlux — outbound multiplicative edges: 5 — amplifies:
     PHY-05 ParticlePoolNewtonianIntegrator (spawn trigger)
     GEO-12 MultiOriginCollisionField (emit-point driver)
     PER-19 OnsetTriggeredCrossBlend
     PER-14 PredictiveTrailOpticalFlow (motion vector amplifier)
     COM-15 DecoratorPoolEventOverlay (canonical trigger)

[AUD-05] BeatTrackTempo — outbound multiplicative edges: 5 — amplifies:
     PER-15 BeatLockedRefresh (canonical use case)
     PHY-01 SpringMassDamperLattice (centre-impulse injection)
     LIN-09 BeatParityBloomSpriteInjection (fallback path without INF-04)
     COM-07 TwoStepBeatQuantisedComposer (without INF-04, step-square approximation)
     T-03 Captain's silence-beat-multiscale triple

[HW-02] DualStrip — outbound multiplicative edges: 14 — amplifies:
     GEO-01 BilateralWaveInterference (DUAL-STRIP-EXCLUSIVE)
     GEO-04 HolographicTwoSourceFringe
     GEO-06 CircularRingTopology (counter-rotation variant)
     GEO-13 InterStripPhaseDelayBuffer (DUAL-STRIP-EXCLUSIVE)
     PHY-09 ViscousCrossStripBleed (DUAL-STRIP-EXCLUSIVE)
     COM-X most composers gain stereo readout
     LIN-X most cross-lineage fusions naturally mirror-symmetric
     PS-05 ReflectiveTwinContract (TENSION counter-amplifier)
     T-01, T-05, T-07, T-09 four triadic synergies route through HW-02
     Justification: HW-02 is the "moat axis" — single most under-exploited K1 affordance per brainstorm:73 verdict.
```

**Hub interpretation:** The graph has TWO distinct hub clusters:
- **System-substrate cluster** {INF-01, INF-02, INF-03, INF-04, INF-06, INF-08} — six Level-2 software substrates whose investment compounds. These are where engine-LOC investment translates to many-effects unlocked.
- **Hardware-affordance cluster** {HW-01 LGP, HW-02 DualStrip, HW-03 CentreOrigin} — three physical/policy invariants that compound by gating multiple capability classes. These cannot be invested in (they exist), but they MUST be respected.

Audio-driver hubs (AUD-02, AUD-04, AUD-05) are already implemented in K1; their "amplification" comes from being CONSUMED by more effects, not from being built. Investment leverage there is low.

---

## 3. Capability Pillars (Clusters)

Tight synergy loops — domains within a pillar interact heavily; loose connections to other pillars. Pillars are independent enough that they can be developed in parallel.

```
Pillar A — CONTINUUM DYNAMICS (PDE-class spatial-coupling)
   Members: PHY-01 SpringLattice, PHY-03 FluidAdvection, PHY-07 Kuramoto, PHY-10 WaveEquation,
            PER-20 HeatEqDiffusion, PER-21 AnisotropicDiffusion, PER-22 FreqDepSpatialLPF,
            PER-25 VelocityAniso, PHY-09 ViscousCrossStripBleed,
            GEO-01 BilateralWaveInterference, GEO-04 HolographicTwoSource, GEO-09 MassConservationFlow
   Internal synergy: 11 multiplicative pairs documented in SA-2; each PDE couples to others via
                     shared INF-08 (sin LUT) + INF-09 (CFL gate) + INF-10 (Hermite resample) substrates.
   Independent of: most of Pillar B (composition is post-render), Pillar D (cross-lineage focuses on
                   audio-mapping not spatial-evolution).
   Loose connection to: HW-01 (LGP MULTIPLIES this pillar — every PDE looks better on diffused glass).
   Substrate dependencies: INF-08 (mandatory), INF-09 (mandatory for stability), HW-07 (FPU + 2 ms budget).
   Status: ALMOST entirely absent. Single largest greenfield opportunity in the registry.

Pillar B — COMPOSITION & ORCHESTRATION
   Members: COM-01 BlendMode (implemented), COM-03 ConvictionMixer, COM-04 SkyScore,
            COM-05 MoodSwitch, COM-06 VoiceMusicSwitcher, COM-07 TwoStep,
            COM-08 MoodStateMachine, COM-09 StoryArc, COM-10 DensityGated,
            COM-11 EchoComposer, COM-12 AntiMode, COM-13 ParamCrossWire,
            COM-14 CloudMask, COM-15 DecoratorPool, COM-16 StainGlass
   Internal synergy: 17 combinatorial pairs documented in SA-3 (most COM- domains share INF-01).
   Independent of: Pillar A (composition is post-render; PDE outputs become a layer like any other).
   Loose connection to: every other pillar (composition is the universal consumer).
   Substrate dependencies: INF-01 (mandatory), INF-06 (mandatory for several specific composers),
                           INF-13 TransitionEngine (already implemented).
   Status: ZoneComposer + BlendMode implemented; LayerStack + 12 composers absent.

Pillar C — TEMPORAL PERSISTENCE
   Members: PER-01..02 (implemented core), PER-09 ChromaticPhosphor, PER-10 LumiCondDecay,
            PER-11 MultiScaleMemory, PER-12 RecursiveAdaptiveFloor, PER-13 EchoSpatialDelay,
            PER-14 PredictiveTrailOpticalFlow, PER-15 BeatLockedRefresh, PER-16 TempoPhaseModulatedDecay,
            PER-17 SchmittTriggerPersistence, PER-18 AudioGatedConditionalDecay,
            PER-19 OnsetTriggeredCrossBlend, PER-23 FluxConservingTrail, PER-24 ConvolutionalKernel,
            PER-26 TempoLockedHeatEquation
   Internal synergy: 8 multiplicative pairs documented in SA-1.
   Tight coupling to: Pillar A via PER-20/21/22/25 (which are spatial-temporal hybrids; could equally
                      sit in Pillar A — overlap intentional).
   Loose connection to: Pillar D (cross-lineage decay variants land here).
   Substrate dependencies: INF-02 (anchor), INF-03 (for echo/optical-flow variants), INF-07 helpers.
   Status: PER-01..08 implemented; PER-09..26 mostly absent (most novel persistence ideas concentrate here).

Pillar D — CROSS-LINEAGE FUSION
   Members: LIN-01..12 (12 SB×ES emergent fusions)
   Internal synergy: low pairwise (each fusion is a discrete capability; fusions don't usually compose
                     pairwise within this pillar).
   HIGH dependency on: Pillar B (most LIN- need INF-01 LayerStack), Pillar A audio-driver hubs
                       (AUD-02, AUD-05, INF-04), Pillar G hardware (HW-02 DualStrip amplifies several).
   Bridge role: this pillar is structurally a **bridge cluster** — its members exist precisely to fuse
                primitives from other pillars. LIN-06 and LIN-10 are the K1-native exemplars (capability
                that ONLY K1 could ship).
   Substrate dependencies: INF-04 (TempoPhase) for half of these, INF-12 (ScalarRings) for several,
                           INF-01 (LayerStack) for several.
   Status: 100% absent. The brand-differentiating layer.

Pillar E — AUDIO INTERPRETATION
   Members: AUD-12 SpectralCentroid, AUD-13 SpectralFlatness, AUD-14 PitchHPS,
            AUD-18 InterBandCofiring, AUD-19 EnergyModulationLock, AUD-20 ChromaDerivative,
            AUD-21 VoiceMusicClassifier, AUD-22 FormantTriangulation,
            AUD-24 VAD, AUD-25 MoodClassifier, INF-11 LongWindowStats
   Internal synergy: 6 multiplicative pairs documented in SA-1 (centroid×flatness 2-D timbre,
                     voice×formant gated vowel, etc.).
   Tight coupling to: Pillar B (every interpretation feature finds expression as a composer driver).
   Loose connection to: Pillar A (interpretation drives parameters in PDE solvers), Pillar D (LIN- fusions).
   Substrate dependencies: AudioActor +200..400 LOC depending on which subset ships; mostly leaf primitives.
   Status: 5 of 12 are render-side reuse (zero DSP work — bridge candidates flagged in SA-1);
           remaining 7 require AudioActor extensions.

Pillar F — DUAL-STRIP GEOMETRY (the moat pillar)
   Members: GEO-01 BilateralWaveInterference, GEO-04 HolographicTwoSource,
            GEO-13 InterStripPhaseDelayBuffer, PHY-09 ViscousCrossStripBleed,
            HW-02 DualStrip (gateway), partial GEO-06 (counter-rotation), partial GEO-12 (cross-strip ripple)
   Internal synergy: 6 dual-strip-EXCLUSIVE pairs (single-strip cannot reproduce).
   Tight coupling to: HW-01 LGP (LGP physically fuses the stereo into one panel — without it the
                      effects read as "two parallel patterns" not "interference").
   Loose connection to: Pillar B (interference patterns become composable layers when LayerStack lands).
   Substrate dependencies: HW-02 (intrinsic), HW-01 (intrinsic), INF-08 (for trig math), INF-03 (for
                           inter-strip delay buffer).
   Status: HW-02 + HW-01 implemented passively; algorithm side 100% absent. Highest moat-strength pillar.

Pillar G — BRAND-META (filter pillar — modulates rather than emits)
   Members: HW-03 CentreOrigin, PS-01..PS-10, VG-01..VG-05
   Internal synergy: PS-01 × PS-03 × PS-04 multiplicative (filter cascade), VG-01 × VG-02 orthogonal.
   Coupling: every other pillar's output passes through Pillar G as a filter. Rejects on brand-voice
             grounds OR engineering grounds.
   Bridge role: this pillar is meta — it filters, it doesn't emit capability. But it's structurally critical:
                ignoring Pillar G's filter cascade produces capability that customer-rejects despite
                engineering-passing.
   Status: implemented as policy/filter; PS-09 WakeUp absent (proposed); PS-10 MoodMode TENSION.
```

---

## 4. Bridge Domains

Bridge domains connect otherwise isolated pillars. Strategic leverage: small investments in bridges unlock cross-pillar synergy that wouldn't otherwise exist.

```
[INF-04] TempoPhaseContinuous — bridges Pillar E (audio) ↔ Pillar B (composition) ↔ Pillar C (persistence) ↔ Pillar D (cross-lineage)
     Connections: AUD-05 (within E) → INF-04 → PER-15/16/26 (Pillar C) + COM-07/13/15 (Pillar B)
                  + LIN-01/09/12 (Pillar D) + GEO-02 (Pillar A precondition)
     Effort: ~210 LOC AudioActor + ~10 LOC ControlBus
     Without it: Pillars C, B, D each lose ~6, ~3, ~3 capabilities respectively.
     **Highest cross-pillar leverage in the registry.**

[INF-06] EffectRoleFlags — bridges Pillar B (composition) ↔ Pillar C (persistence) ↔ Pillar D (cross-lineage)
     Connections: COM-12/16, INF-02 mandatory ship, PER-X opt-out, LIN-X opt-out
     Effort: ~50 LOC IEffect interface extension + per-effect tag updates
     Without it: INF-02 cannot ship as mandatory pass without breaking 8+ self-trailing effects;
                 COM-16 silently drops colour; COM-12 ambiguous.
     Critical "infrastructure-or-fail" bridge.

[INF-08] sinLUT256 — bridges Pillar A (continuum dynamics) ↔ Pillar F (dual-strip geometry)
     Connections: PHY-01/07/10 + GEO-01/02/04/07/12
     Effort: ~10 LOC + ~2 KB PSRAM
     Without it: half of Pillar A and most of Pillar F over the 2.0 ms budget.
     Trivial-LOC, high-leverage bridge.

[AUD-02] ChromaVector — bridges Pillar E (audio) ↔ Pillar A (physics) ↔ Pillar D (cross-lineage)
     Connections: PHY-07 (chroma → ω) + GEO-02 (chroma → A_n) + LIN-08/10
     Effort: zero — already implemented and shipped.
     Status: under-consumed; effect-side investment is what amplifies.

[HW-02] DualStrip — bridges Pillar F (geometry) ↔ Pillar G (brand) ↔ everything else
     Connections: 14 outbound multiplicative edges; gateway to Pillar F entirely.
     Effort: zero hardware-side; algorithm side 100% absent.
     Status: hardware present but algorithm-side under-exploited; PS-05 contract clarification needed
             to unblock interference/parallax interactions (open Q from Pass 1 §7.1).
```

**Bridges NOT in the registry that would help:**
- A bridge between Pillar E (audio interpretation) and Pillar G (brand-meta) — i.e. a domain that
  encodes "which audio features are brand-on-message". Currently brand-voice gate operates on visuals
  not audio; an audio-side brand filter would pre-reject AUD- features that produce "smart-lighting"
  semantics. Candidate name: **PS-AudioLanguageGate**.
- A bridge between Pillar D (cross-lineage) and Pillar F (dual-strip geometry) — most LIN- fusions
  inherit centre-mirror but few exploit dual-strip-as-interference. Candidate: **LIN-DualStripFusion**
  (a meta-domain encoding "any LIN- can be re-cast as inter-strip variant"). Currently latent.

---

## 5. Hidden Synergy Layers

Layers become visible only after the prior layer's substrates land. This is the protocol's core deliverable — non-obvious synergy that only emerges from cumulative substrate investment.

### Layer 0 — GIVEN (Context Preamble)

```
Domains involved: INF-01 LayerStack × INF-02 FramebufferLPF × ControlBus reuse (AUD-01..11)
Emergent capability: K1 lifts from "discrete modes" to "fluid composable modes with audio reactivity".
                     - LayerStack converts mode count from N (additive) to N×N×N (combinatorial).
                     - FramebufferLPF transforms every mode's temporal character from hard-pixel to fluid.
                     - ControlBus reuse means audio-side cost is zero for ~5 categories.
Prerequisite layers: none (this is Layer 0 — accepted as GIVEN).
Dual-strip amplification: yes — LayerStack-per-strip and LPF-per-strip are natural extensions.
Moat strength: STRONG — substrate investment that compounds; not directly copyable without the same engineering work.
LOC budget: ~150 (LayerStack) + ~80 (LPF) = ~230 LOC for the platform.
```

### Layer 1 — Audio-Phase Doctrine (post-Layer-0)

```
Domains involved: INF-04 TempoPhaseContinuous × INF-06 EffectRoleFlags
Emergent capability: K1 gains the rhythm dimension. Phase exposure unlocks Theme 2 (BeatLockedRefresh,
                     TempoModulatedDecay, TwoStep, BeatParityBloomInjection, single-tempo Pendulum,
                     TempoLockedHeatEq, ParamCrossWire-tempo-modulator). RoleFlags lets INF-02 ship as
                     mandatory without double-trailing. Combined: every effect in the catalogue gains
                     OPTIONAL phase modulation AND uniform softness.
Prerequisite layers: Layer 0 (LayerStack to compose the new capabilities; LPF to make them fluid).
Why becomes visible only post-Layer-0: BeatLockedRefresh on a no-LayerStack mode is just "this one effect
                     pulses on the beat" — visually thin. Inside LayerStack with LPF, the same primitive
                     produces "tempo-quantised foreground over slow harmonic background" (T-03 Captain's
                     "strongest novel signature"). Without Layer 0, the synergy doesn't exist.
Dual-strip amplification: yes — phase can lead/lag between strips for stereo-tempo readouts.
Moat strength: STRONG — compositional+algorithmic; replicable but expensive. ESV11 32 kHz reliable
                     beat-track is a hardware-tier moat that competitors must replicate.
LOC budget: ~210 (TempoPhase) + ~50 (RoleFlags) = ~260 LOC.
```

### Layer 2 — Memory Dimension (post-Layer-1)

```
Domains involved: INF-03 PSRAMFrameRing × INF-12 PSRAMScalarRing × INF-07 PersistenceHelpersLibrary
Emergent capability: K1 gains the memory dimension — past frames re-enter the present. Unlocks Echo
                     Composer (COM-11), Time-Warp Replay, Optical-Flow Trail (PER-14 with full ring),
                     LIN-06 CentreOriginRadialTimeScope, LIN-10 MotionBlurCachedChromagramDots,
                     LIN-07 PrismReflectionsIndependentHueDrift, GEO-13 InterStripPhaseDelay (with HW-02).
Prerequisite layers: Layer 1 — beat-locked echoes and tempo-quantised time-warp need INF-04 to be
                     musically anchored; without it, echoes are wall-clock and feel arbitrary.
Why becomes visible only post-Layer-1: an echo at wall-clock time is uninteresting; an echo at
                     ½-beat or 1-beat is musical. The MEMORY dimension only becomes useful when
                     paired with the rhythm dimension.
Dual-strip amplification: yes — INF-03 + HW-02 = GEO-13 inter-strip phase delay (DUAL-STRIP-EXCLUSIVE).
Moat strength: STRONG — combines hardware (16 MB PSRAM K1-unique) with algorithm. SB/ES have ~520 KB
                     total RAM; cannot reproduce the 1.15 MB ring buffer. Hardware-tier moat.
LOC budget: ~120 (PSRAMFrameRing) + ~40 (ScalarRings) + ~250 (PersistenceHelpers) = ~410 LOC.
                     Note: PersistenceHelpers can land partly in Layer 1 (the dt-correct array LPF
                     and emaArrayDt are useful pre-PSRAM-ring).
```

### Layer 3 — Physical Dimension (post-Layer-2) — **the kinematic→dynamical headline**

```
Domains involved: INF-08 sinLUT × INF-09 CFLSubstepGate × INF-10 CubicHermiteResample
                  + selected Pillar A members (PHY-01, PHY-03, PHY-07, PHY-10, PER-20, PER-21)
                  + selected Pillar F members (GEO-01, GEO-04 — DUAL-STRIP-EXCLUSIVE subset)
Emergent capability: K1 lifts from "image transforms with smoothing" to "state evolves under physical
                     laws driven by audio". Pixels become continuum. Spring lattices ring on beats;
                     fluid fronts advect with centroid; standing waves resonate at chroma frequencies;
                     bilateral wave interference physically computes on dual-strip + LGP. **The
                     "liquid centre-origin chiaroscuro on diffused glass" thesis (brainstorm:161)
                     gets its full physical depth here.**
Prerequisite layers: Layer 2 (memory) — many PDE solvers benefit from prev-frame ring buffer for
                     velocity estimation (PER-14) and for stable substep storage. Layer 1 (phase)
                     drives PDE parameters tempo-locked. Layer 0 LayerStack composes PDE outputs.
Why becomes visible only post-Layer-2: a one-shot spring lattice without memory is a frame-by-frame
                     impulse-response — it doesn't ring. With INF-03 storing prev-frame momentum,
                     the lattice has dynamical state. Same for fluid advection (needs prev-frame
                     velocity) and Kuramoto (needs prev-frame phase array).
Dual-strip amplification: ABSOLUTE — GEO-01 BilateralWaveInterference and PHY-09 ViscousCrossStripBleed
                     are geometrically impossible on single-strip devices. T-01 (DualStrip × LGP ×
                     BilateralInterference) is the strongest moat triadic in the registry.
Moat strength: ABSOLUTE on the dual-strip subset; STRONG on the per-strip continuum subset (LGP makes
                     the pixel-to-fluid metaphor optically work in a way naked-strip competitors cannot).
                     This is the layer that earns the "Liquid Light" brand language.
LOC budget: ~10 (sinLUT) + ~80 (CFL gate) + ~120 (Hermite) + ~700 (selected PDE solvers) = ~910 LOC.
                     Discrete: each PDE family costs ~100–200 LOC; ship a subset.
```

### Layer 4 — Session Dimension (post-Layer-3)

```
Domains involved: INF-11 LongWindowAudioStats × COM-08 MoodStateMachine × COM-09 StoryArc
                  × PS-09 WakeUpChoreography × PS-10 MoodDrivenAutonomousModeSelection (gated)
Emergent capability: K1 gains minutes-scale awareness. The visual evolves at song-arc, set-list,
                     and session-arc rates — not just frame-rate. Wake-up choreography opens; mood
                     classifier nudges palette and mode density across minutes; story-arc sequencer
                     supports authored shows.
Prerequisite layers: Layer 3 — without the physical-dimension PDE engines, mood-driven mode shifts
                     produce only crossfades-between-discrete-modes (which is what SB/ES already do).
                     With PDE engines + memory + phase, mood can drive PDE parameters smoothly — the
                     lattice stiffens for "build" mood, softens for "release" — making session-level
                     evolution PERCEPTUALLY DIFFERENT, not just structurally.
Why becomes visible only post-Layer-3: minute-scale evolution on a kinematic engine is just a slow
                     mode-cycler (boring and easy). Minute-scale evolution on a continuum engine is
                     a continuously-evolving physical system whose properties drift musically.
Dual-strip amplification: yes — left strip can run "current" mood while right strip runs "target"
                     during transitions, producing visual cross-fades that read as physical states.
Moat strength: MEDIUM — replicable on platforms with equivalent compute; the curated mood→mode
                     mapping is the moat (depends on quality of curation, not hardware).
TENSION: PS-10 risks PS-01 brand-voice failure ("smart-lighting" framing). Captain Q from Pass 1 §7.5.
LOC budget: ~150 (LongWindowStats) + ~200 (MoodFSM + curated mapping) + ~150 (StoryArc parser) + ~100
                     (WakeUp) = ~600 LOC.
```

### Layer 5 — Audio Interpretation (post-Layer-4)

```
Domains involved: AUD-12 SpectralCentroid + AUD-13 SpectralFlatness + AUD-14 PitchHPS
                  + AUD-18 InterBandCofiring + AUD-21 VoiceMusicClassifier
                  + (optional) AUD-22 Formants + AUD-19 ModulationLock
Emergent capability: K1 stops being "audio-reactive" and becomes "audio-interpreting". The device
                     KNOWS what kind of music is playing (voice vs music; tonal vs noisy; bright vs
                     dark; simple groove vs complex polyphony) and adjusts its physical/compositional
                     state accordingly. **The headline reframe from brainstorm:93: "audio reactivity →
                     audio interpretation."**
Prerequisite layers: Layer 4 — the value of interpretation features compounds when long-window stats
                     are tracking multi-feature TRAJECTORIES, not just instantaneous values. A "voice
                     vs music" flag without minute-scale tracking switches modes too fast (jitter);
                     with INF-11 windows, the switch becomes considered.
Why becomes visible only post-Layer-4: instantaneous voice/music classification produces twitchy
                     mode-switching on borderline content (pop songs with sung+rapped sections, talk
                     radio with music beds). Smoothed at minute-scale (Layer 4), the same primitive
                     is rock-solid. The interpretation primitives DON'T WORK without long-context
                     smoothing.
Dual-strip amplification: yes — voice-mode can render on one strip with reactive on the other,
                     creating "halo around the singer" aesthetic when used with HW-01 LGP.
Moat strength: MEDIUM — research-grade DSP that competitors can match with equivalent ML investment.
                     The K1 advantage is that interpretation lands ON TOP of Layers 1–4, not standalone.
LOC budget: ~12 (centroid) + ~20 (flatness) + ~40 (HPS) + ~80 (cofiring) + ~10 (voice heuristic) = ~160 LOC
                     for the core five; ~200 LOC additional for AUD-22 formants and AUD-19 mod-lock.
```

**Total cumulative LOC across Layers 0–5: ~2,570 LOC.** Brainstorm Infra roadmap (catalogue:139) estimated ~880 LOC for V1.0 launch (Layers 0–1 plus selected Layer-3 leaf primitives). The full vision is ~3× that figure spread across V1.0 / V1.1 / V1.2+.

---

## 6. Moat Analysis Per Layer

Pass 4 will adversarially challenge these. For now:

| Layer | Moat strength | Moat type | Replicability by competitor |
|---|---|---|---|
| Layer 0 (GIVEN) | STRONG | Substrate | Replicable in 230 LOC of equivalent engineering — but every consumer effect must be re-architected to use it; the porting cost is the real moat. |
| Layer 1 (Phase Doctrine) | STRONG | Substrate + audio-DSP | ESV11 32 kHz beat-tracking with calibrated constants is a research investment competitors can replicate, but they would re-discover the calibration. |
| Layer 2 (Memory Dimension) | STRONG | Hardware (PSRAM) + algorithm | 16 MB PSRAM is K1-unique vs SB/ES (520 KB). Competitors at K1's price point that match RAM lose other budget headroom. |
| Layer 3 (Physical Dimension) | **ABSOLUTE on dual-strip subset; STRONG on per-strip subset** | Hardware-mechanical | T-01 (Dual×LGP×Interference) is **geometrically impossible** on single-strip OR no-LGP hardware. Single-strip competitors cannot reproduce inter-strip phase, viscous bleed, or two-source interference — physically. |
| Layer 4 (Session Dimension) | MEDIUM | Curation quality | Replicable on any equivalent platform; moat is curated mood→mode mapping quality, not hardware. |
| Layer 5 (Audio Interpretation) | MEDIUM | Research-grade DSP | Competitors can match with equivalent ML investment. K1 advantage is composition with Layers 1–4 underneath. |

**Headline:** Every ABSOLUTE moat in the registry routes through ≥1 of {HW-01 LGP, HW-02 DualStrip, HW-03 CentreOrigin}. The hardware triad is the only inalienable moat axis. Software-only moats are STRONG-at-best (porting cost) but never absolute. **K1's competitive position rests on Layer 3 + Pillar F.**

---

## 7. External Domain Candidates

Domains from adjacent fields not in the K1 vocabulary that would, if introduced, restructure the topology by serving as new hub or bridge nodes.

```
[E-01] ReactionDiffusion (Gray-Scott / FitzHugh-Nagumo PDE)
       Source field: procedural art / generative chemistry
       Would serve as: HUB in Pillar A — composes multiplicatively with PER-20/21 (heat eq becomes
                       activator-inhibitor system), GEO-01 (interference patterns spawn from RD instabilities),
                       PHY-09 (cross-strip RD coupling produces Turing-pattern stereo).
       Connects to: AUD-04 onset (drives RD parameters), HW-01 LGP (RD spots/stripes look exquisite on
                    diffused glass — this is what the procedural-art community has known for decades).
       Unlocks: a class of self-organising spatial patterns that are PERCEPTUALLY ALIVE in a way no
                heat-eq / wave-eq / Kuramoto produces. RD systems exhibit endogenous structure formation.
       LOC estimate: ~180 (two-component update + parameter tuning + audio-driver mapping).
       Why orchestrator surfaces this: NONE of the 8 SSAs proposed RD. Synthesis bias — the SSAs were
                briefed on canonical SB/ES taxonomy, which has no RD precedent. This is a clear blind spot.

[E-02] PhaseConjugateHolography (LGP-as-conjugate-mirror modeling)
       Source field: optics / holographic imaging
       Would serve as: BRIDGE between Pillar F (dual-strip geometry) and HW-01 LGP physical model.
       Connects to: GEO-04 (hologram model becomes calibrated to actual LGP refractive index instead of
                    abstract "two-source"), HW-01 (turns the LGP from passive smoother into active optical
                    element with measurable phase response).
       Unlocks: physically-accurate inter-strip interference patterns calibrated to the specific glass
                used in K1 hardware. **Could turn T-01 from "looks like interference" to "is interference."**
       LOC estimate: ~50 (parameter calibration table) + measurement campaign cost.
       Why orchestrator surfaces this: SSA-Geometry §4 proposed two-source fringe abstractly; SSA-K1Specific
                §4 proposed cross-strip interference operationally; neither connected the two via the
                physical optics of the LGP medium. Engineering-grade upgrade to the moat.

[E-03] DistanceFieldTopology / MarchingSquares
       Source field: procedural generation / signed distance fields
       Would serve as: BRIDGE between Pillar B (composition) and Pillar F (geometry). Currently GEO-08
                       Voronoi is a leaf node with no consumer.
       Connects to: GEO-08 (Voronoi seeds → distance field → continuous boundaries, not hard partition),
                    COM-14 CloudMaskPerlin (mask becomes distance-field instead of Perlin),
                    HW-01 LGP (DF gradients pre-fit LGP smoothing).
       Unlocks: continuous-boundary composition (vs hard-cell Voronoi) with audio-driven seed motion.
       LOC estimate: ~80.

[E-04] BeatEntropyShannon (entropy of beat-event distribution over windowed time)
       Source field: information theory / music information retrieval
       Would serve as: BRIDGE between AUD-05 BeatTrack and AUD-25 MoodClassifier — currently no continuous
                       scalar measures "rhythmic variability" between them.
       Connects to: AUD-05 (input), AUD-18 InterBandCofiring (cousin metric), AUD-25 (consumer).
       Unlocks: a single scalar that distinguishes "metronomic four-on-floor" from "syncopated jazz" from
                "free-form ambient" — could drive PER-11 fast/slow mix, INF-04 phase confidence weighting,
                Layer 4 mood classifier feature vector.
       LOC estimate: ~30 (windowed Shannon over beat-tick history).

[E-05] PerceptualJND (Just-Noticeable-Difference for brightness contrast at 60 fps under LGP)
       Source field: psychophysics / perceptual psychology
       Would serve as: BRIDGE between INF-02 LPF (algorithm) and PS-03 WHAT-IS-THAT (brand) — currently
                       no domain encodes empirical "what is the smallest perceivable change at K1's
                       refresh rate through K1's diffuser".
       Connects to: PS-03 (filter), INF-02 (cutoff selection), PS-06 RestraintLock (defines "subtle").
       Unlocks: empirical floor for "visible motion" — defends against under-motion (boring) and
                over-motion (seizure) on principled grounds.
       LOC estimate: zero (it's a calibration constant + 1-line guard); requires measurement campaign.
       Why orchestrator surfaces this: Pass 1 coverage flag — none of the 8 SSAs included perceptual
                psychology. SA-4 brand-voice is a proxy but not empirical.

[E-06] KalmanBeatPhasePrediction (state-space lookahead over beat phase + spectral centroid)
       Source field: control theory / Bayesian filtering
       Would serve as: HUB-amplifier — turns AUD-17 lookahead-predictive into a calibrated predictor
                       with confidence intervals; turns PER-14 optical-flow into a state-space tracker.
       Connects to: AUD-17 (specific lookahead implementation), PER-14 (motion-vector estimation),
                    INF-04 (phase prediction), INF-05 (per-bin phase tracking with proper uncertainty).
       Unlocks: principled predictive UX — visuals lead the audio in a way that doesn't overshoot when
                tempo wavers. Especially valuable for Layer 5 audio-interpretation features that need
                stable smoothing under noisy input.
       LOC estimate: ~150 (Kalman state + measurement model + tuning).
       Why orchestrator surfaces this: SSA-AudioDriver §7 proposed lookahead as fixed-offset prediction;
                Kalman version is the principled upgrade with built-in uncertainty quantification.
                Synthesis discarded the uncertainty-quantification angle.
```

---

## 8. Graph Artifact

The full directed graph is large; this section captures the structure formally enough that Pass 3 can topologically sort it. Edges listed in canonical order: source → target, weight, evidence.

**Node set (119 nodes):** as enumerated in Pass 1 §2 (INF-01..14, AUD-01..25, PER-01..26, PHY-01..14, GEO-01..16, COM-01..16, LIN-01..12, HW-01..09, PS-01..10, VG-01..05).

**Edge set (multiplicative + combinatorial only, ~108 edges):**

```
TIER-1 EDGES (substrate-to-consumer, indispensable precedence)
─────────────────────────────────────────────────────────────
INF-01 → COM-03, COM-04, COM-05, COM-07, COM-10, COM-11, COM-12, COM-14, COM-16, GEO-03   weight: combinatorial   evidence: SSA-Composition §1; brainstorm:141
INF-02 → PER-09, PER-11, PER-13, LIN-03, LIN-11   weight: multiplicative   evidence: brainstorm:40; SSA-Persistence header
INF-02 → all COM-X (uniform softness knob)   weight: multiplicative
INF-03 → COM-11, GEO-13, PER-13, PER-14, LIN-06   weight: combinatorial (GEO-13: substrate-mandatory)   evidence: SSA-Geometry §13; brainstorm:146
INF-04 → PER-15, PER-16, PER-26, COM-07, COM-13, COM-15, LIN-01, LIN-09, LIN-12, GEO-02   weight: multiplicative (PER-16, COM-07: combinatorial — strict prerequisite)   evidence: SSA-Persistence B2; SSA-Composition §5; SSA-CrossLineage §1+9+12
INF-06 → COM-12, COM-16, INF-02 (mandatory ship), PER-X opt-out hooks   weight: combinatorial   evidence: SSA-Composition §10+§14; SSA-CrossLineage §3
INF-08 → PHY-01, PHY-07, PHY-10, GEO-01, GEO-02, GEO-04, GEO-07, GEO-12   weight: combinatorial (precondition)   evidence: SSA-Physics §6; SSA-Geometry §1+§2+§4+§7+§12 budget analyses
INF-09 → PHY-01, PHY-03, PHY-10, PER-20, PER-21, GEO-09   weight: combinatorial (CFL stability)   evidence: SSA-Physics §3+§11; SSA-Persistence C1+C2
INF-10 → PHY-02, PHY-07, PHY-08   weight: additive (resample-out)
INF-11 → AUD-25, COM-08, PS-10   weight: multiplicative
INF-12 → LIN-06, LIN-07, LIN-10, AUD-16, PER-X (small history rings)   weight: multiplicative

TIER-2 EDGES (cross-pillar bridges)
────────────────────────────────────
AUD-02 → PHY-07, GEO-02, LIN-08, LIN-10, COM-13   weight: multiplicative
AUD-04 → PHY-05, GEO-12, PER-19, PER-14, COM-15   weight: multiplicative
AUD-05 → PER-15, PHY-01, LIN-09, COM-07 (fallback)   weight: multiplicative
AUD-12 → PHY-01, GEO-01, COM-05   weight: multiplicative
AUD-12 ↔ AUD-13   weight: multiplicative (2-D timbre, mutual amplification)
AUD-14 → PHY-06, AUD-20 (gating), COM-X (palette confidence)   weight: multiplicative
AUD-18 → COM-03 (mix-coefficient driver)   weight: multiplicative
AUD-21 → COM-06, PER-18 (silence-gate semantics)   weight: combinatorial
AUD-23 → COM-03 (founding use case)   weight: multiplicative
AUD-25 → COM-08, PS-10   weight: combinatorial

TIER-3 EDGES (within-pillar synergies — most documented in subagent fragments §3)
────────────────────────────────────────────────────────────────────────────────
PER-11 ↔ PER-15   weight: multiplicative (Captain triple)   evidence: SSA-Persistence l.479
PER-11 → PER-18, PER-14 → PER-20→PER-25, PHY-X internal   (already in SA-1/SA-2 fragments)
PHY-01 ↔ PHY-09 (cross-strip viscoelastic)   weight: multiplicative (DUAL-STRIP-EXCLUSIVE)
PHY-03 → PHY-10 (advection+wave)   weight: combinatorial
GEO-01 ↔ GEO-04 (interference↔hologram limit)   weight: combinatorial
GEO-01 → GEO-13 (interference + temporal echo)   weight: multiplicative (DUAL-STRIP-EXCLUSIVE)
COM-03 ↔ COM-04, COM-03 ↔ COM-10 (mixer family)
COM-15 ↔ COM-16 (decorator+stain-glass via INF-06)
LIN-01 ↔ LIN-12 (both pendulum-class via INF-05)
LIN-09 ↔ LIN-10 (both wholly-new emergent K1-native exemplars)

TIER-4 EDGES (hardware/brand modulation)
─────────────────────────────────────────
HW-01 → all PDE-class (PHY-01, PHY-03, PHY-10, PER-20, PER-21, GEO-01, GEO-04)   weight: multiplicative (LGP optical smoothing)
HW-02 → GEO-01, GEO-04, GEO-13, PHY-09, COM-X-stereo, LIN-X-mirror   weight: multiplicative (DUAL-STRIP gateway; 14 outbound)
HW-03 → all centre-symmetric (LIN-06, LIN-10, GEO-15, PER-X-radial, PHY-X-radial)   weight: multiplicative (policy-anchor)
HW-04 → COM-11, GEO-13 (subsumes INF-03)
HW-05 → COM-X (encoder-driven param injection)   weight: multiplicative when wired
PS-01 ↔ VG-01 (filter cascade — meta-pillar)
PS-05 ↔ HW-02 (TENSION — Captain Q)
VG-02 → all engineering-rejected (zero in current registry — no engineering kills)

CONFLICTS / SUBSTITUTIVES (NOT edges — explicit anti-edges)
───────────────────────────────────────────────────────────
GEO-06 ⊥ HW-03 (CircularRingTopology has no centre by topology)
GEO-10 ⊥ HW-03 (AsymmetricDriftOrigin departs centre — gating proposed)
PER-15 ⊥ PER-16 (alternative phrasings of same idea)
PER-20 ⊥ PER-22 (different approximations of same spatial diffusion)
GEO-09 ⊥ PHY-03 (mass-conservation-strict vs source-included advection)
```

Pass 3 reads this graph and computes the topological sort + minimum-spanning-cover for the kill order.

---

## 9. Open Questions Carrying Forward

1. **PS-05 ReflectiveTwin contract** — does interference fusion satisfy the "one panel" reading? Pre-condition for activating Pillar F.
2. **INF-04 single vs INF-05 bank** — single recommended; bank is V1.1+.
3. **INF-02 mandatory vs opt-in** — depends on INF-06 landing first to avoid double-trail.
4. **Layer 4–5 ordering** — Layer 5 (interpretation) could land partly inside Layer 4 (session) if the smoothing window is long enough; or it could be deferred entirely. Pass 3 must decide.
5. **External domains E-01..E-06** — which (if any) should the kill order incorporate? Pass 3 should evaluate each against marginal cost/benefit at the relevant layer.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:claude-opus-4-7 (orchestrator) | Created — Pass 2 of K1 visual-pipeline synergy-topology protocol. Constructed directed multiplicative-interaction graph from Pass 1 registry; identified 8 platform-domain hubs (INF-01..04, INF-06, INF-08, AUD-02/04/05, HW-02); partitioned 119 domains into 7 capability pillars (A continuum-dynamics; B composition; C persistence; D cross-lineage-fusion; E audio-interpretation; F dual-strip-geometry — moat pillar; G brand-meta — filter pillar); identified 5 bridge domains (INF-04, INF-06, INF-08, AUD-02, HW-02); extracted 6 hidden synergy layers (Layer 0 GIVEN, Layers 1–5 each visible only after prior); proposed 6 external-field domain candidates (Reaction-Diffusion, Phase-Conjugate Holography, Distance-Field Topology, BeatEntropyShannon, PerceptualJND, KalmanBeatPhasePrediction); graded moat strength per layer (Layers 0–2 STRONG, Layer 3 ABSOLUTE on dual-strip subset, Layer 4–5 MEDIUM). Headline: every ABSOLUTE moat routes through ≥1 of {HW-01 LGP, HW-02 DualStrip, HW-03 CentreOrigin}. Total cumulative LOC across Layers 0–5 ≈ 2,570. |
