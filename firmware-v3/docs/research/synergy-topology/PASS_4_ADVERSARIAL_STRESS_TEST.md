---
abstract: "Pass 4 of the K1 visual-pipeline synergy-topology protocol. Adversarial stress test of Passes 1–3. Audits 11 load-bearing assumptions; identifies 6 missing domains (perceptual-JND, thermal-power-budget, asymmetric-audio-failure-modes, PSRAM-cache-latency, OTA-preset-compatibility, render-budget-per-layer scaling); exploits 5 unresolved SSA tensions (ProductStrategy vs Physics fragmentation; Physics render-budget vs K1Specific µs estimates; AudioDriver classifier-flicker vs ProductStrategy brand-voice; CrossLineage vs Persistence overlap on framebuffer LPF; Composition role-flag scaffolding vs incremental retrofit cost); challenges the top-3 Pass-2-weighted synergy edges (T-01 demoted on physical-interference claim; T-02 GIVEN partially demoted to additive-under-uniform-cutoff; T-03 sustained but unbalanced); proposes 1 alternative kill order (V1.0-greedy ~25% LOC saved at cost of Phase-4 retrofit); identifies 4 black swans (ESP32-P4, user-authoring tooling, hardware-revision-N-strip, live-show market); and challenges the GIVEN by partially demoting INF-02 × INF-01 multiplicativity to conditional-on-per-layer-cutoff. Headline: the registry is structurally sound but the moat claim depends on 2 unverified empirical questions (LGP coherence-loss + ESV11 reliability under adversarial input) that Captain should resolve via measurement before V1.0 ships."
---

# Pass 4 — Adversarial Stress Test

**Protocol:** K1 Visual Pipeline Synergy Topology — Pass 4 of 4. Inputs: Pass 1 + Pass 2 + Pass 3. **No new claims supported by adversarial evidence — only refinements, contradictions, and risk surfaces. Captain reads this against Passes 1–3 to identify the questions that must be resolved before kill-order execution.**

The protocol explicitly states: "Strong, defensible disagreement with consensus is more valuable than weak agreement." This pass leans into disagreement.

---

## 1. Assumption Registry

```
[A-01] All "multiplicative" edges in the cross-subagent matrix are genuinely multiplicative.
       Evidence: each edge cites SSA-source mechanism; mathematical relation argued in §4 of Pass 1.
       Falsification condition: stress-test reveals A×B is decomposable into A+B without information loss.
       Collapse impact: if 30% of multiplicative edges are actually additive, hub identification (Pass 2 §2)
                        revalues — INF-01 outbound count drops from 12 to ~7, INF-02 from 8 to ~5.
                        Strategic ordering still holds but leverage ratios change.
       Verdict: PARTIAL CHALLENGE — see §4 (T-02 GIVEN demotion candidate).

[A-02] Hub centrality is correctly captured by outbound multiplicative edge count.
       Evidence: standard graph-theoretic centrality.
       Falsification condition: edge weights are NOT uniform — a "trivial" multiplicative edge
                                (e.g. INF-01 → COM-15 DecoratorPool, where the layer-stack contribution
                                is a thin gain rather than transformative) counts the same as a
                                "transformative" edge (e.g. INF-01 → COM-03 ConvictionMixer, the canonical
                                use case). Conflating these inflates INF-01's hub claim.
       Collapse impact: INF-04 TempoPhase may actually be the highest-leverage hub (every edge it
                        creates is transformative — phase exposure either is or isn't), demoting INF-01
                        to second place.
       Verdict: CHALLENGE SUSTAINED — Pass 5 (if executed) should weight edges by transformativeness,
                not just count.

[A-03] HW-03 Centre-Origin is an inviolable policy invariant.
       Evidence: CLAUDE.md hard constraint; brand-voice locked.
       Falsification condition: Captain ratifies GEO-10 (AsymmetricDriftOrigin) ≤±20 LED gating; HW-03
                                becomes "default with audio-driven exception zones" not "inviolable".
       Collapse impact: HW-03's hub centrality drops; T-07 brand triad (HW-03 × HW-02 × GEO-15) loses
                        its "every effect honours" character; new domain space opens for asymmetric
                        audio-position-driven effects.
       Verdict: OPEN — Captain Q1.7.4 unresolved. Pass 4 recommends Captain decide explicitly before
                Phase 1 ships, so Phase 4's PDE work knows whether to enforce centre-origin in solver
                boundary conditions or allow drift.

[A-04] ESV11 32 kHz beat tracking is reliable across all musical content.
       Evidence: project memory `firmware_build_envs.md` notes ESV11 _32khz envs are the calibrated
                 path where beat tracking actually works; Captain has flashed and validated.
       Falsification condition: ESV11 degrades on (a) very slow tempos (<60 BPM ambient), (b) rubato
                                or live-band performance with tempo drift, (c) syncopated genres without
                                strong on-beat downbeats, (d) sub-bass drone or sine-wave-only test signals.
       Collapse impact: Phase 2 tempo doctrine partially fails. INF-04 TempoPhase produces noisy phase
                        estimates → Phase-2-derived effects (PER-15 BeatLocked, COM-07 TwoStep, LIN-09
                        BeatParityBloomInjection) would visibly stutter on the failing content.
                        Phase 3 F2 Centre-Phase Pendulum is at moderate risk.
       Verdict: PARTIAL CHALLENGE — Captain should commission a "beat-tracking adversarial test set"
                BEFORE Phase 2 ships. Test signals: ambient (Brian Eno), rubato (live recording),
                heavily syncopated (Aphex Twin), sub-bass-only (Thrillseekers acappella). Pass-rate
                threshold should gate INF-04's reliability claim.

[A-05] HW-01 LGP physically smooths LED quantisation aesthetically.
       Evidence: SSA-K1Specific §1+§7 cited "LGP smears the diffusion into a continuous gradient";
                 brainstorm:73 "two physically separated emitters illuminating one optical medium."
       Falsification condition: LGP diffuser opacity is high enough to DESTROY wave coherence — i.e.
                                what the orchestrator calls "physical interference" is actually
                                visually washed out into uniform brightness (no fringe visible). Or
                                viewing distance / ambient lighting destroys the effect.
       Collapse impact: T-01 (DualStrip × LGP × BilateralWaveInterference) claimed ABSOLUTE moat
                        becomes mathematical-pattern-on-diffused-panel only, not physical interference.
                        Moat strength drops from ABSOLUTE to STRONG. The "Liquid Light" brand language
                        weakens from physically-true to metaphor-only.
       Verdict: SIGNIFICANT CHALLENGE — see §4.1 mirage detection. **Captain must measure this empirically
                before V1.0 marketing copy claims "physical interference."**

[A-06] 16 MB PSRAM is K1-unique vs competitors at this price point.
       Evidence: SSA-K1Specific implicit; brainstorm Theme 4 dual-strip-as-K1-only.
       Falsification condition: WLED hardware variants, Quasar Lights, AVRGZAUDIO, or new entrants
                                ship with comparable PSRAM. Memory market commodity.
       Collapse impact: Layer 2 (Memory Dimension) moat strength drops from STRONG to MEDIUM.
                        Specifically GEO-13 InterStripPhaseDelay loses some "K1-only" claim if
                        a dual-strip competitor matches PSRAM.
       Verdict: NEUTRAL — at K1's $369 price point, PSRAM is currently a differentiator. Likely to
                commodify in 12–24 months. Plan accordingly.

[A-07] Strategic ordering's INF-06 → INF-02 mandatory precedence is load-bearing.
       Evidence: Pass 3 §4 asserted retrofit cost ~150–300 LOC if greedy.
       Falsification condition: greedy retrofit is actually cheaper because most existing self-trailing
                                effects DON'T need opt-out (they overwrite rather than self-feedback);
                                the "30 effects to retrofit" estimate is wrong; actual count is ~5–8.
       Collapse impact: greedy ordering becomes equally good. The "critical divergence" Pass 3 named
                        is a non-issue.
       Verdict: PARTIAL CHALLENGE — orchestrator FABRICATED the "30 effects to retrofit" estimate.
                The number is plausible (effect catalogue is ~106 modes; many sprite-class) but unverified.
                Recommend Captain run `grep -r "EMA\|sprite\|self-feedback" firmware-v3/src/effects/`
                before committing to the strategic order.

[A-08] Surface metric (4-tuple product) correctly captures Captain priorities.
       Evidence: protocol-prescribed metric.
       Falsification condition: V1.0 shippability is the dominant Captain priority; the 4-tuple under-weighs
                                it (Phase 3 only 1.04× over Phase 2 despite shipping V1.0). Surface as
                                modelled is dimensionless; Captain's actual utility function is binary
                                (V1.0 ships YES/NO) until V1.0 ships, then becomes the 4-tuple metric.
       Collapse impact: Pass 3's recommended ordering is NOT optimal under V1.0-binary utility; Pass 4
                        proposes alternative greedy-V1.0 sequence (§5 below).
       Verdict: SIGNIFICANT CHALLENGE.

[A-09] All 119 domains in the registry are independent enough to merit separate IDs.
       Evidence: SSA fragments + orchestrator dedup.
       Falsification condition: GEO-09 MassConservationFlow ⊥ PHY-03 FluidAdvection conflict (§Pass 1)
                                suggests they are alternative implementations of the same domain; should
                                collapse to one. PER-15 BeatLockedRefresh ⊥ PER-16 TempoPhaseModulatedDecay
                                similar.
       Collapse impact: registry shrinks by ~5–10 entries; some Pass 2 hub edge counts decrease by 1–2.
                        Marginal.
       Verdict: WEAK CHALLENGE — orchestrator's choice to keep alternatives as separate IDs preserves
                Pass 4 ability to recommend one over the other. Don't collapse.

[A-10] Brand-voice rejections are reversible; engineering rejections are not.
       Evidence: SA-4 viability artifact; ProductStrategy KILL-LIST analysis.
       Falsification condition: brand voice is more deeply embedded than positioning copy — e.g. the
                                physical product (LGP geometry, dual-strip orientation) is itself a brand
                                expression; loosening brand voice doesn't actually reverse the rejection
                                because the hardware itself is brand-locked.
       Collapse impact: 4 KILL-LIST entries are LESS reversible than orchestrator claimed. Pass 4
                        challenge of brand-voice kills (§4.3) becomes weaker.
       Verdict: PARTIAL CHALLENGE — physical product IS brand-expressive; pure positioning copy is more
                reversible than physical. Pass 4 should distinguish "rejections reversible by copy
                changes" from "rejections requiring hardware changes". Currently lumped.

[A-11] HW-02 DualStrip is "the single most under-exploited K1 affordance" (brainstorm:73 verdict).
       Evidence: brainstorm catalogue convergent theme + 4 SSAs.
       Falsification condition: PSRAM is actually more under-exploited (currently ~0% used by effects;
                                vs dual-strip which is ~50% used as mirror+zone partition).
       Collapse impact: bridge analysis shifts — INF-03 PSRAMFrameRing becomes the highest-leverage
                        bridge instead of HW-02; Phase 4 (memory dimension) might warrant earlier landing
                        than Phase 4.
       Verdict: ALTERNATIVE INTERPRETATION — both are under-exploited. The "more under-exploited"
                ranking depends on whether you weight by hardware-tier moat (favours HW-02 — physically
                impossible vs single-strip) or by capability-coverage (favours INF-03 — powers more
                effects per K1-unique unit).
```

---

## 2. Missing Domains

Domains the registry SHOULD include but doesn't. Each one is a concrete addition Pass 4 recommends Pass 5 (if executed) integrate.

```
[M-01] PerceptualJustNoticeableDifference (JND) under K1's specific hardware
       Why it matters: Pass 1 §6 coverage flag noted absence. Without an empirical JND floor,
                       INF-02 LPF cutoff is chosen by intuition (0.5–14.5 Hz softness range from ES);
                       PER-01 EMA τ values inherited from SB; brightness contrast thresholds undefined.
                       Result: K1 ships effects that flicker imperceptibly (wasted CPU) or under-animate
                       (boring) without principled selection.
       Where it connects: bridges PS-03 WhatIsThatTest (brand) to INF-02 (algorithm) to PS-06 RestraintLock.
       Cost: ~1 LOC (a constant in code) + measurement campaign cost (~1 day with calibrated photometer
             and 8–12 viewers under controlled lighting).
       Pass 4 recommendation: Captain commission JND measurement before Phase 1 ships; bake the floor
                              into INF-02 as the minimum permissible cutoff and PER-X as the minimum
                              permissible τ.

[M-02] ThermalAndPowerBudget (manufacturing/hardware variance)
       Why it matters: 320 LEDs at full white = ~9.6 A current draw; thermal throttling kicks in at
                       sustained loads. PER-23 FluxConservingTrail (excluded in Pass 3) is actually
                       a SAFETY domain for this reason. Phase 4+ continuum-dynamics effects can sustain
                       high brightness across full strip — this risks PSU sag, thermal throttle, or
                       dimming in production hardware lots that have variance vs the dev unit.
       Where it connects: gates PER-23 inclusion; gates Phase 4 PDE solver brightness ceilings;
                          modulates INF-02 LPF via brightness-conserving cutoff.
       Cost: ~80 LOC (per-frame brightness budget enforcement + dimming fallback) + per-effect cap
             review.
       Pass 4 recommendation: VG-06 BrightnessThermalGate should be added as a viability gate alongside
                              VG-01/VG-02. Unflagged in current registry — risk of V1.0 RMA for "K1 dims
                              after 5 minutes".

[M-03] AsymmetricAudioFailureModes
       Why it matters: Pass 1 §6 coverage flag. Sub-bass drone, sine-wave test signals, mono drum
                       loop, clipping audio — what does K1 do? Currently undefined. Risk of locking up,
                       producing harsh visuals, or failing Phase 2 tempo doctrine on adversarial input.
       Where it connects: gates AUD-04 onset detection (mono signal = no spectral flux), AUD-05
                          beat tracking (drone has no beat to track), AUD-21 voice classifier.
       Cost: per-feature failure-mode handling; ~20 LOC each.
       Pass 4 recommendation: PER-FEATURE-DEFAULT semantic spec — when input is degenerate, what does
                              the feature output? Test set covering 8 adversarial input classes.

[M-04] PSRAMCacheLatencyAndBandwidth
       Why it matters: 16 MB PSRAM is FAR slower than internal SRAM (~80 ns vs ~10 ns access; ~80 MB/s
                       vs ~400 MB/s bandwidth). Phase 4 INF-03 1.15 MB ring + per-frame 320×3B reads
                       at 120 Hz = ~115 KB/s — fine. But COM-11 EchoComposer multi-tap reads = up to
                       3 × 320 × 3B × 120 Hz = ~345 KB/s, still fine. PER-14 PredictiveTrailOpticalFlow
                       per-pixel velocity estimate from prev-frame = full ~115 KB/s frame read every
                       frame. Bandwidth fine; latency may spike if PSRAM is contended by other tasks.
       Where it connects: gates Phase 4 INF-03 / PER-14 / COM-11 budget; Phase-4 effects must measure
                          actual PSRAM latency under WiFi-active + audio-actor-active contention.
       Cost: ~50 LOC of profiling + per-effect cache-friendly access pattern review.
       Pass 4 recommendation: profile PSRAM under ESV11 + WiFi-AP + RendererActor before committing
                              Phase 4 LOC budget.

[M-05] OTAPresetCompatibility
       Why it matters: K1 ships V1.0; future V1.1 / V1.2 add new modes per Phases 4–6. Existing customer
                       presets must NOT break when modes are added/renumbered. Currently the registry
                       treats mode count as a fluid 4-tuple dimension; in production it's a stable
                       contract that must be versioned.
       Where it connects: gates Phase 4+5+6 mode introductions; modulates COM-09 StoryArc (if authored
                          arcs accumulate, they reference mode IDs that must remain stable).
       Cost: ~40 LOC (preset migration adapter + mode-ID stability spec).
       Pass 4 recommendation: VG-07 PresetStabilityGate added; mode IDs versioned not renumbered.

[M-06] PerLayerRenderBudget (the metric pathology)
       Why it matters: Pass 3 surface metric multiplies layer-stack-depth as a free dimension. In
                       reality, each layer of LayerStack composition costs render time. At 3 layers,
                       Phase 4 PDE per-layer cost (~100 µs each) sums to ~300 µs alone before
                       compositing. Adding INF-02 LPF (mandatory) + per-layer LPFs adds more. The
                       2.0 ms ceiling is hit faster than the 4-tuple suggests.
       Where it connects: VG-02 Engineering Gate should be applied per-layer not per-effect; gating
                          Phase 4+ multi-layer compositions on per-layer budget.
       Cost: ~30 LOC per-layer profiling instrumentation.
       Pass 4 recommendation: Pass 3 surface metric is OPTIMISTIC; actual realisation depends on
                              maintaining 120 FPS across N composed layers. Phase 4 must measure
                              per-layer cost and possibly reduce LayerStack depth in production to
                              2 (not 3) on PDE-heavy modes.
```

---

## 3. SSA Tension Map

Cross-referenced tensions between SSAs that synthesis papered over:

```
[T-01] SSA-ProductStrategy vs SSA-Physics (FRAGMENTATION REJECTION)
       ProductStrategy concern: brainstorm:130 KILL-LIST item #1 — "Tempo-bank-as-N-pendulum-dots
                                literal port reads as fragmented, not unified." PS-06 RestraintLock
                                rejects multi-element visuals.
       Opposing SSA claim: SSA-Physics §6 Kuramoto Phase Lattice (32 coupled oscillators); SSA-Physics
                           §11 Boid Swarm (24 agents); SSA-Physics §7 Pendulum Chain (16 links).
                           ALL inherently multi-element.
       Resolution: ProductStrategy filter likely rejects all three on brand-voice grounds even if
                   engineering-sound. SSA-Physics enthusiasm masks brand-voice incompatibility.
       Impact on topology: PHY-07 Kuramoto, PHY-08 Pendulum, PHY-11 Boid — all currently in registry —
                           may be Pillar G filtered out. Pass 3 Phase 4 Move 4.5 has PHY-01 Spring
                           Lattice (320 coupled — but reads as continuum not fragments) and PER-20
                           Heat Equation (continuum) as alternatives. Verdict: choose continuum-class
                           PDE over discrete-element-class. PHY-07/08/11 demoted to V1.2+ research,
                           NOT V1.1 trajectory.

[T-02] SSA-Physics vs SSA-K1Specific (RENDER-BUDGET CLAIMS)
       SSA-K1Specific concern: per-category render-time estimates capped at ~25–50 µs; "all 12
                               categories profile under 50 µs of the 2 ms budget."
       Opposing SSA claim: SSA-Physics §1 Spring Lattice 320 coupled oscillators × dt-correct
                           integration ≈ ~250 µs naïvely; ~100 µs with SIMD and substep cap.
       Resolution: K1Specific ESTIMATES were overly optimistic on PDE-class effects; SSA-Physics
                   estimates are realistic. Phase 4 PDE budget is ~100–200 µs/effect, not ~25–50 µs.
       Impact on topology: 3 layers × ~150 µs = ~450 µs JUST for PDE composition before LPF or output
                           pipeline. Phase 4's 11,040 surface assumes 3-layer depth viable on PDE
                           modes — possibly NOT VIABLE; recommend Phase 4 LayerStack-depth on
                           PDE-heavy modes is 2, with surface revising downward.
       Recommendation: Captain measure on hardware before Phase 4 commits.

[T-03] SSA-AudioDriver vs SSA-ProductStrategy (CLASSIFIER FLICKER)
       AudioDriver claim: AUD-21 VoiceMusicClassifier and AUD-25 MoodClassifier are deliverable as
                          continuous-confidence scalars driving smooth transitions.
       ProductStrategy concern: PS-10 MoodDrivenAutonomousModeSelection risks "smart-lighting"
                                framing if transitions feel arbitrary (KILL-LIST item #2 spirit).
       Resolution: classifiers PRODUCE noisy signals on borderline content; smooth-transition
                   architecture handles this if smoothing is Layer-4 long-window aware. Without
                   long-window smoothing, Phase 6 features manifest as flicker → brand-voice failure.
       Impact on topology: Phase 6 Move 6.6 COM-08 MoodFSM MUST land WITH Phase 6 Move 6.1 INF-11
                           LongWindowStats — never separately. The current ordering bundles them
                           correctly, but the dependency must be hard-enforced. Captain Q1.7.5
                           decision needed: either (a) wait for INF-11 + COM-08 together, OR (b)
                           skip PS-10 entirely for V1.0/V1.1.
       Recommendation: bundle COM-08 with INF-11 mandatory; or kill PS-10 from V1.x trajectory.

[T-04] SSA-CrossLineage vs SSA-Persistence (FRAMEBUFFER LPF OVERLAP)
       CrossLineage claim: LIN-03 LogWarpedPhosphorTrail and LIN-11 LpfDragCentreOriginCrossFade as
                           specialised variants of INF-02 (FramebufferLPF).
       Persistence claim: 14 categories of decay primitives, including PER-09 ChromaticPhosphor
                          (per-channel τ on framebuffer); claimed novel.
       Resolution: PER-09 + LIN-03 + LIN-11 + INF-02 are all SAME mechanism (dt-correct framebuffer
                   EMA) with different parameter shapes. Synthesis treated as 4 distinct domains;
                   they're 1 mechanism × 4 parameter regimes. Inflates apparent capability count.
       Impact on topology: Pass 3 Move 4.6 is implicitly already-shipped if INF-02 lands per-channel
                           per-LED τ exposure. PER-09 IS just INF-02 with per-channel parameters.
       Recommendation: collapse PER-09 + LIN-03 + LIN-11 into "INF-02 parameter modes". Adjust
                       Phase 4 LOC down by ~80–120 LOC (no separate PER-09 / LIN-03 implementations
                       needed if INF-02 properly parametrised).

[T-05] SSA-Composition vs (everything else) (ROLE-FLAG SCAFFOLDING)
       Composition claim: INF-06 EffectRoleFlags critical infra; without it COM-X composers fail.
       Everything else: didn't propose role flags as a primitive.
       Resolution: SA-3 was UNIQUE in identifying INF-06 as cross-cutting — it's a Phase 1 substrate
                   that ALL other subagents implicitly assumed but none proposed. Healthy convergence
                   in retrospect; risky as singleton.
       Impact on topology: INF-06 is correctly elevated to Phase 1 Move 1.1. SA-3's lone-voice
                           proposal is the single highest-leverage scope identification of Pass 1.
       Recommendation: ratify SA-3's INF-06 elevation; document the singleton-saved-the-day case as
                       evidence the "watch divergent SSAs" instruction in the protocol works.
```

---

## 4. Synergy Mirage Detection — Top 3 Edges

The protocol mandates: challenge top 3 by Pass-2 graph weight or claimed unlock impact. The orchestrator does not get to choose — Pass 2 already ranked. The top-3 are: T-01 (DualStrip × LGP × Interference, ABSOLUTE moat), T-02 (GIVEN), T-03 (Captain's "strongest novel signature").

### 4.1 Challenge — T-01 DualStrip × LGP × BilateralWaveInterference

```
Claim: ABSOLUTE moat — geometrically impossible on competing single-strip OR no-LGP hardware.
Counter-argument: the "interference" is a mathematical model rendered as brightness pattern; whether
                  the LGP medium produces visible fringes depends on LGP optical properties (diffuser
                  opacity, refractive index, viewing distance, ambient light). Without empirical
                  measurement (Pass 4 §1 A-05), the claim is ASPIRATIONAL not VERIFIED.
                  Specifically: a high-opacity diffuser averages out fine fringe patterns into uniform
                  brightness. The K1 LGP may be in this regime — turning T-01 from "physical interference"
                  into "two-strip pattern on diffused panel that looks somewhat like interference."
                  The competitive moat shrinks: a competitor with single-strip + custom LGP could
                  approximate the visual without reproducing the physics.
Verdict: DEMOTED — the EDGE class is sustained (still combinatorial — three-way), but moat strength
         drops from ABSOLUTE to STRONG conditional on LGP measurement. ABSOLUTE only after Captain
         verifies fringe visibility through K1's specific LGP at customer viewing distance.
Required Captain action: measure interference fringe visibility on K1 LGP empirically before V1.0
                         marketing copy claims "physical interference."
Mitigation: include external domain E-02 (PhaseConjugateHolography) to model LGP as conjugate medium
            with measurable phase response — would give engineering-grade calibration.
```

### 4.2 Challenge — T-02 GIVEN (LayerStack × FramebufferLPF × ControlBus reuse)

```
Claim: substrate-level multiplicative chain GIVEN per Context Preamble.
Counter-argument 1 (additivity): LayerStack and FramebufferLPF operate on orthogonal axes (composition
                                  vs persistence). Each produces independent visual character. Stacking
                                  them ADDS both characters but doesn't AMPLIFY each. Surface modelling
                                  treats the combination as multiplicative; mathematically it's the
                                  Cartesian-product of two independent dimensions — i.e. additive in
                                  information content, not multiplicative.
Counter-argument 2 (asymmetric ordering): the GIVEN doesn't specify ordering. Pass 3 implicitly orders
                                          INF-01 → INF-06 → INF-02. But if INF-02 lands FIRST (as opt-in),
                                          existing self-trailing effects gain uniform softness without
                                          waiting for LayerStack — partial value delivered earlier.
                                          Then INF-01 lands and the multiplicative kick happens.
                                          Strict ordering is a planning constraint, not a structural one.
Counter-argument 3 (ControlBus reuse hidden cost): the GIVEN treats ControlBus reuse as zero-cost
                                                    ("audio-side cost is zero"). But render-side
                                                    consumption requires effect refactoring. The 5
                                                    "free" categories cited still need per-effect
                                                    implementation work. Hidden cost ~30–50 LOC × 5 = 150–250 LOC.
Verdict: PARTIAL DEMOTION — multiplicativity is conditional on per-layer-cutoff exposure (INF-02 lands
         WITH per-layer τ knob; under uniform global cutoff, the chain is additive, not multiplicative).
         ControlBus reuse is NOT zero-cost.
Refined GIVEN claim: INF-01 × INF-02-with-per-layer-τ × ControlBus-reuse-with-effect-side-refactor
                     = STRONG multiplicative substrate. The original "≫ additive" claim holds only
                     with these qualifiers.
Recommendation: Pass 5 (if executed) restate the GIVEN with these qualifiers; Pass 3 Move 1.3 INF-02
                must include per-layer τ exposure spec from day one.
```

### 4.3 Challenge — T-03 BeatTrack × MultiScale × BeatLockedRefresh (Captain's "strongest signature")

```
Claim: triadic — removing any one ELIMINATES the emergent capability.
Counter-argument: 3-arity test reveals asymmetry.
                  REMOVE MultiScale (PER-11): leaves BeatTrack + BeatLockedRefresh = "frame freezes
                  between beats, snaps on tick" — recognisable, valid effect. Loses fast/slow
                  separation but doesn't ELIMINATE.
                  REMOVE BeatLockedRefresh (PER-15): leaves BeatTrack + MultiScale = "fast/slow
                  branches that pulse on beat via tempo-modulated mix coefficient." Different but
                  valid — a tempo-aware multi-scale effect. Doesn't ELIMINATE.
                  REMOVE BeatTrack (AUD-05): leaves MultiScale + BeatLockedRefresh = degenerate
                  (BeatLockedRefresh has no tick to refresh ON). DOES eliminate.
Verdict: SUSTAINED but UNBALANCED — only AUD-05 BeatTrack is truly load-bearing. The triad is
         actually a "BeatTrack + (MultiScale OR BeatLockedRefresh)" plus "(both for full doctrine)."
         Captain's "strongest novel signature" framing slightly oversells — removing PER-11 OR PER-15
         degrades the effect but doesn't eliminate it.
Refined claim: the FULL signature requires all three; the CORE signature requires AUD-05 + (PER-11
                XOR PER-15). Pass 3 Move 4.7 (PER-11 ship) is sufficient for partial signature; full
                signature requires Move 4.7 AND a Phase 4 add of PER-15.
Recommendation: re-examine whether PER-15 BeatLockedRefresh deserves Phase-4 inclusion or whether
                Phase-4's tempo-locked-heat (PER-26) covers the ground. PER-26 IS the tempo-modulated
                continuous variant of PER-15 binary-tick.
```

---

## 5. Counter-Sequencing — Alternative Kill Order

The protocol mandates an alternative ordering optimised for a DIFFERENT objective. Pass 3 optimised for "deepest long-term platform capability." Pass 4 proposes alternative for "fastest path to V1.0 ship."

```
ALTERNATIVE OBJECTIVE: V1.0 ships in minimum LOC and minimum calendar time.
                       Trade longer-term substrate hygiene for shorter critical path.

Phase 1' (V1.0 substrate minimal):                     ~270 LOC
   Move 1.1': INF-01 LayerStack (~150 LOC) — same
   Move 1.2': INF-04 TempoPhase single-tempo (~210 LOC reduced to ~150 by skipping bank infra)
   Move 1.3': INF-08 sinLUT (~10 LOC) + INF-09 CFLSubstepGate (~80 LOC) reduced — only sinLUT for V1.0
              (CFL gate not needed until Phase 4 PDE work)
   Move 1.4': INF-02 FramebufferLPF as OPT-IN ONLY (no INF-06 RoleFlags yet) (~60 LOC reduced)
   SKIPPED: INF-06 RoleFlags (deferred to V1.1)

Phase 2' (V1.0 effects):                               ~700 LOC
   Move 2.1': F1 Liquid Bloom refinement
   Move 2.2': F5 Reflective Twin contract enforcement (lighter — no per-effect audit; just policy doc)
   Move 2.3': F6 First-Light Ignition
   Move 2.4': F2 Centre-Phase Pendulum
   Move 2.5': F3 Liquid Stillness curation (reduced — pick existing 8 ambient programmes, no new code)
   Move 2.6': F4 Cross-Strip Wave Interference (full implementation)

V1.0 SHIPS at ~970 LOC vs Pass 3's strategic ~1,560 LOC. SAVES ~38% LOC.

V1.1 retrofit cost (recovered later):
   - INF-06 RoleFlags retrofit: ~50 LOC
   - Per-effect opt-out hooks for self-trailing modes: ~5 LOC × ~15 affected modes = ~75 LOC
   - INF-09 CFLSubstepGate when Phase 4 starts: ~80 LOC
   - INF-02 mandatory promotion: ~30 LOC migration
   Total V1.1 retrofit: ~235 LOC.

NET V1.0 + V1.1 retrofit: 970 + 235 = 1,205 LOC vs Pass 3's strategic 1,560 LOC at V1.0.
SAVES ~22% LOC AT V1.0; SAVES ~23% LOC AT V1.1.
COST: bug surface during V1.1 retrofit (INF-02 promotion can cause double-trail regressions); planning
       tax (V1.1 has substrate work that V1.0 should have done).

Trade-offs vs Pass 3 strategic:
   PROS: V1.0 ships ~22% faster; substrate work deferred to V1.1 when calendar pressure is lower;
         INF-02 stays opt-in, allowing per-effect tuning during V1.0 customer feedback period.
   CONS: V1.1 has retrofit work that delays new V1.1 capability; bug surface during INF-02 promotion;
         INF-09 CFL gate landing in Phase 4 means Phase 4 starts later; team morale tax (substrate
         work feels like "rework").

Recommendation: depends on Captain's calendar binding. If V1.0 hard-deadline (e.g. Q3 2026 launch
                window committed), GREEDY-V1.0 ordering. If schedule flexibility, STRATEGIC ordering.
                The orchestrator notes Pass 3 implicitly assumed flexibility; Captain should explicitly
                ratify which constraint dominates.

Alternative objective NOT analysed (out of scope but flagged):
   - "Fastest path to brand-defensible visual" — would skip Phase 1 substrate, ship F4 Cross-Strip
     Interference as standalone effect (like SbK1Bloom is today). LOC ~300; ships in weeks not months.
     Cost: no compositional surface; future effects don't compose. Demo-only path.
```

---

## 6. Black Swans

Capabilities or developments NOT in current research that, if they emerge, would fundamentally restructure the synergy topology.

```
[BS-01] ESP32-P4 production-ready with 4× compute headroom and DSP accelerator
        Restructuring impact: Phase 6 audio-interpretation features (centroid, flatness, formants,
                              modulation-spectrum lock) become trivial-cost; mood classifier could be
                              ML-class instead of heuristic. Phase 4 PDE budget triples — multi-PDE
                              composition becomes viable; PHY-07 Kuramoto, PHY-11 Boid revisitable.
                              INF-11 LongWindowStats budget grows from 24 KB to 1+ MB.
        Probability: HIGH (12–18 months) — ESP32-P4 dev kits already shipping; production K1 V3
                     hardware revision plausible 2027.
        Captain action: monitor ESP32-P4 ESP-IDF support; design V1.0 firmware with ESP-IDF 5+
                        forward-compatibility in mind.

[BS-02] User-content-creation interface (preset sharing, effect authoring web tool)
        Restructuring impact: COM-09 StoryArcSequencer becomes V1.1 priority, not V1.2+. User-authored
                              transitions and arcs accumulate; INF-13 TransitionEngine grows from 12
                              types to ~30 user-contributed. Marketing pivot: K1 becomes a platform,
                              not just a product. PS-08 CustomerGeneratedContentCapture moves from
                              V1.1 supportive to V1.0 critical.
        Probability: MEDIUM (6–18 months) — depends on Captain product strategy decision.
        Captain action: explicit decision on platform vs product positioning before V1.0 ships.

[BS-03] Hardware revision N>2 strips (4-strip arc, 6-strip ring, full 360° array)
        Restructuring impact: Pillar F (Dual-Strip Geometry) generalises to N-strip topology.
                              T-01 BilateralInterference becomes N-source interference (more lobes,
                              more nodes). GEO-03 ParallaxDepthStack becomes N-depth-layer parallax.
                              The K1 V1 dual-strip "moat" gains depth as competitors don't follow
                              into N-strip space.
        Probability: LOW for V1 hardware (locked); HIGH for K1 V2 / K2 hardware revisions.
        Captain action: Phase 4 GEO-X primitives should be designed N-strip-extensible if V2 hardware
                        is on roadmap.

[BS-04] Live-show / VJ-market entry
        Restructuring impact: composition + transition layer becomes professional-grade priority;
                              MIDI/OSC/DMX integration becomes pillar; INF-13 TransitionEngine grows
                              into a full transition library. PS-04 Founder's-Edition demo-carry filter
                              gives way to "live-performance demo-carry filter" with different criteria
                              (cue-able, repeatable, fail-soft on signal loss).
        Probability: LOW–MEDIUM (depends on K1's market traction; current positioning is consumer
                     ambient, not pro VJ).
        Captain action: monitor early-customer use cases; if pro-VJ adoption emerges, pivot Phase 6
                        from interpretation toward authoring.
```

---

## 7. Challenge the GIVEN

Per protocol §Pass 4 instruction 7: "Challenge the GIVEN. The infrastructure layer declared as GIVEN... is not exempt from adversarial scrutiny."

The GIVEN: T-02 = INF-01 LayerStack × INF-02 FramebufferLPF × ControlBus reuse, multiplicative chain.

```
Component-by-component scrutiny:

INF-01 LayerStack — GIVEN holds: ZoneComposer is partition-only; LayerStack overlap-permitted is a
                     genuine new capability. Multiplicative claim: 12 outbound consumers; without it,
                     12 effect categories cannot exist. Verdict: GIVEN VERIFIED.

INF-02 FramebufferLPF — GIVEN partially holds (see §4.2 challenge). Multiplicativity conditional on
                         per-layer τ exposure. Under uniform global cutoff, ADDITIVE not multiplicative.
                         Verdict: GIVEN VERIFIED ONLY UNDER PER-LAYER-CUTOFF QUALIFIER.

ControlBus reuse — GIVEN partially demoted. The "5 categories at zero audio-side cost" claim ignores
                    render-side consumer cost (~30–50 LOC × 5 = 150–250 LOC of effect refactoring).
                    Multiplicativity claim still holds — consuming 5 fields with N effects = 5×N
                    interaction surface — but the "free" framing is misleading.
                    Verdict: GIVEN VERIFIED with hidden-cost qualifier.

Ordering claim — Context Preamble does not specify ordering; Pass 3 inferred LayerStack → LPF.
                  Pass 4 alternative INF-02-as-opt-in-first-then-LayerStack also valid (greedy-V1.0
                  ordering). Verdict: ORDERING UNDERSPECIFIED IN GIVEN; Pass 3's ordering is one of
                  multiple valid orderings.

Top-line "≫ additive" claim — only holds with all qualifiers. Naïvely multiplicative is overclaimed.
                                Verdict: REFINE the GIVEN to "STRONG multiplicative under per-layer-cutoff
                                qualifier and effect-side ControlBus consumer work."
```

The GIVEN survives but is refined. Pass 5 (if executed) should restate it with qualifiers.

---

## 8. Summary of Risks and Captain Decisions

Single consolidated list for Captain action:

```
EMPIRICAL MEASUREMENTS REQUIRED BEFORE V1.0:
1. LGP fringe-visibility measurement (resolves A-05; gates T-01 ABSOLUTE moat claim).
2. ESV11 beat-tracking adversarial test set (resolves A-04; gates Phase 2 doctrine).
3. PSRAM cache-latency under WiFi-AP + AudioActor contention (resolves M-04; gates Phase 4 LOC budget).
4. Per-layer render-time profiling at 3-layer LayerStack depth (resolves M-06; gates Phase 4 surface claim).
5. JND empirical floor (resolves M-01; bakes into INF-02 cutoff bounds and PER-X τ bounds).

CAPTAIN DECISIONS REQUIRED BEFORE PHASE 1 SHIPS:
6. Strategic vs Greedy-V1.0 ordering ratification (Pass 3 §4 + Pass 4 §5). Drives ~22% LOC budget.
7. PS-05 ReflectiveTwin contract interpretation (Pass 1 §7.1; gates Pillar F dual-strip-asymmetric edges).
8. INF-04 single-tempo vs INF-05 bank (Pass 1 §7.2; recommend single-tempo).
9. INF-02 mandatory vs opt-in (Pass 1 §7.3; depends on INF-06 landing first per strategic ordering).
10. HW-03 strict invariant vs default-with-exceptions (Pass 1 §7.4; gates GEO-06 / GEO-10 inclusion).
11. PS-10 mood-driven mode selection — kill from V1.x or accept as research track (Pass 1 §7.5).
12. F3 Liquid Stillness curation arbitration process (Pass 1 §7.6).

PASS 4-INTRODUCED ADDITIONS:
13. VG-06 BrightnessThermalGate (M-02) — should be added; ~80 LOC; gates Phase 4+ multi-effect brightness ceilings.
14. VG-07 PresetStabilityGate (M-05) — should be added; ~40 LOC; gates V1.x mode-ID renumbering.
15. PER-09 + LIN-03 + LIN-11 collapse to "INF-02 parameter modes" (T-04 SSA tension); saves ~80–120 LOC.
16. PHY-07 Kuramoto / PHY-08 Pendulum / PHY-11 Boid demote to V1.2+ research per T-01 SSA tension (fragmentation).
17. COM-08 MoodFSM bundled mandatory with INF-11 LongWindowStats per T-03 SSA tension.

PROBABILISTIC RISKS:
18. ESP32-P4 transition (BS-01) — likely 12–18 months; design V1.0 with ESP-IDF 5+ forward-compatibility.
19. Hardware revision N-strip (BS-03) — Phase 4 GEO-X primitives should be N-strip-extensible.
```

---

## 9. Final Verdict

The synergy topology surfaced across Passes 1–3 is structurally sound. The 119-domain registry, 6 platform-domain hubs, 6 hidden synergy layers, and 6-phase strategic kill order survive adversarial review with refinements but no fundamental restructuring.

**Two unverified empirical questions** (LGP fringe coherence; ESV11 robustness) materially gate the moat claims. Captain MUST commission these measurements before V1.0 marketing copy commits to "physical interference" or "tempo-aware" as customer-facing language.

**Three SSA tensions** (Physics-fragmentation, AudioDriver-classifier-flicker, Persistence-LPF-overlap) require explicit resolution before Phase 4 LOC commitment.

**One critical ordering decision** (Strategic vs Greedy-V1.0) depends on Captain's calendar binding — either is defensible given different objectives.

**The K1 visual-pipeline strategic position rests on Layer 3 (Phase 4 in the kill order).** Layers 0–2 are substrate hygiene that any competent embedded LED platform could replicate. Layer 4–5 are MEDIUM moats. The hardware triad {LGP, DualStrip, CentreOrigin} expressed through PDE-class continuum dynamics + dual-strip-exclusive geometry IS the moat. If Captain ships V1.0 without committing to Phase 4, the moat is asserted but not delivered.

The orchestrator's headline recommendation: **strategic ordering, with the 5 empirical measurements and 7 Captain decisions resolved before Phase 1 substrate work begins.** The cost of one Captain meeting is negligible. The cost of an unverified moat claim in V1.0 marketing is brand-voice failure on first customer review.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:claude-opus-4-7 (orchestrator) | Created — Pass 4 of K1 visual-pipeline synergy-topology protocol. Adversarial stress test of Passes 1–3. Audited 11 load-bearing assumptions; identified 6 missing domains (PerceptualJND, ThermalPowerBudget, AsymmetricAudioFailureModes, PSRAMCacheLatency, OTAPresetCompatibility, PerLayerRenderBudget); exploited 5 unresolved SSA tensions; challenged top-3 Pass-2-weighted edges (T-01 demoted ABSOLUTE→STRONG conditional on LGP measurement; T-02 GIVEN partially demoted to additive-under-uniform-cutoff; T-03 sustained but unbalanced — only AUD-05 truly load-bearing); proposed alternative greedy-V1.0 ordering saving ~22% LOC at V1.0 with retrofit cost at V1.1; identified 4 black swans (ESP32-P4, user-authoring, N-strip hardware, live-show market); challenged the GIVEN with refined per-layer-cutoff qualifier. Final verdict: structure sound, 2 empirical measurements + 7 Captain decisions required before Phase 1 ships; 5 unverified-claim risks flagged. |
