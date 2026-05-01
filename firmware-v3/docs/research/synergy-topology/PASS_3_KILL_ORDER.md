---
abstract: "Pass 3 of the K1 visual-pipeline synergy-topology protocol. Reads PASS_1_DOMAIN_REGISTRY.md + PASS_2_SYNERGY_TOPOLOGY.md as inputs. Derives the optimal implementation sequence: 6 phases totalling ~3,200 LOC engine + ~1,000 LOC effects, taking K1 from current state (Phase 0) to full audio-interpretation tier (Phase 6). Surface metric is the 4-tuple (modes-renderable × layer-stack-depth × independent-parameter-axes × composition-primitives) plus headline integer. Compares greedy ordering (LayerStack first) against strategic ordering (EffectRoleFlags first), identifies one critical divergence (INF-06 must precede INF-02-as-mandatory), and recommends the strategic order. Phases: 1 Foundation (substrate; ~290 LOC; surface 6,360 = 3×); 2 Tempo Doctrine (~230 LOC; 7,632); 3 V1.0 Founder's Edition Heroes (~1,000 LOC effects; 7,920); 4 Memory + Continuum (~700 LOC; 11,040); 5 Cross-Lineage Long Tail (~400 LOC; 12,960); 6 Session + Interpretation (~600 LOC; 16,500). Phase gates name what K1 becomes after each phase. Final 'left on table' audit lists 12 domains explicitly excluded with conditions for re-inclusion."
---

# Pass 3 — Strategic Sequencing & Kill Order

**Protocol:** K1 Visual Pipeline Synergy Topology — Pass 3 of 4. Inputs: `PASS_1_DOMAIN_REGISTRY.md` (domain IDs) + `PASS_2_SYNERGY_TOPOLOGY.md` (graph + hidden layers). This pass executes sequentially in main context.

**Surface metric (consistent across all phases):** 4-tuple `(modes-renderable × layer-stack-depth × independent-parameter-axes-per-mode × composition-primitives-in-LayerStack-alphabet)` with headline integer = product. The 4 dimensions are independent — a phase that adds modes without adding parameter axes shows up as a multiplier on dim-1 only.

---

## 1. Topological Sort — what MUST land first

From Pass 2 graph artifact (§8 Tier-1 substrate edges), the following hard-precedence chains exist:

```
INF-06 (RoleFlags)            → INF-02 mandatory ship (else double-trail risk on self-trailing effects)
INF-08 (sinLUT)               → PHY-01/07/10, GEO-01/02/04/07/12 (else 2.0 ms budget violated)
INF-09 (CFLSubstepGate)       → PHY-01/03/10, PER-20/21, GEO-09 (else PDE solvers go unstable)
INF-10 (Hermite resample)     → PHY-02/07/08 (else N-node solvers produce step-functions)
INF-01 (LayerStack)           → COM-03..16 + GEO-03 (canonical use-case dependency)
INF-02 (FramebufferLPF)       → PER-09/11/13 + LIN-03/11 (variants of INF-02)
INF-03 (PSRAMFrameRing)       → COM-11, GEO-13, PER-14, LIN-06 (substrate-mandatory)
INF-04 (TempoPhase)           → PER-15/16/26, COM-07, LIN-01/09/12 (literal phase consumer)
INF-12 (ScalarRings)          → LIN-06, LIN-07, LIN-10 (small history rings)
AUD-23 (TempoConfidence)      → COM-03 (founding spectronome semantics)
AUD-25 (MoodClassifier)       → COM-08 (FSM consumer)
AUD-21/24 (VoiceMusic/VAD)    → COM-06 (switcher consumer)
HW-02 + HW-01 (DualStrip + LGP) → GEO-01/04/13, PHY-09 (hardware affordances — already present)
HW-03 (CentreOrigin policy)   → all centre-symmetric effects (already enforced)
PS-05 (ReflectiveTwin contract) → enforcement of dual-strip-as-single-panel reading (Captain decision)
```

**Conflicts (anti-edges) — domains that exclude each other:**
- `GEO-06 ⊥ HW-03` (CircularRing has no centre by topology)
- `GEO-10 ⊥ HW-03` (AsymmetricDriftOrigin departs centre — needs explicit gating)
- `PER-20 ⊥ PER-22` (different approximations of same diffusion)
- `GEO-09 ⊥ PHY-03` (mass-conservation-strict vs source-included advection)

These conflicts are NOT edges to resolve in the kill order — they're flags for Pass 4 review. The kill order assumes one of each pair is selected.

---

## 2. Unlock-Surface Baseline (Phase 0 = current K1)

```
Step 0 — current K1 state (no changes):
  modes-renderable:                  ~106 (existing catalogue)
  layer-stack-depth:                 1   (no overlap composition; ZoneComposer is partition-only)
  independent-parameter-axes/mode:   ~5  (palette, brightness, speed, mood, audio-band — typical)
  composition-primitives:            4   (BlendMode: OVERWRITE/ADD/ALPHA/MULTIPLY) — under ZoneComposer only
  HEADLINE:                          106 × 1 × 5 × 4 = 2,120
```

This is the baseline. Each phase below shows the new 4-tuple after the phase lands.

---

## 3. The Kill Order (Strategic Recommendation)

### Phase 1 — Foundation: "K1 becomes compositional and fluid"

**Goal:** Substrate-only phase. After this, every existing effect renders the same as before, but the engine is ready to compose. Highest leverage-per-LOC of any phase in the registry.

```
Move 1.1: INF-06 EffectRoleFlags                   effort: L (~50 LOC)
          unlocks edges (precondition):              INF-02 mandatory ship; COM-12/16; PER-X opt-out
          Rationale: Strategic placement BEFORE INF-01/02. Without role flags, INF-02 cannot ship as
                     mandatory, and every Phase-3+ effect would need retrofit opt-out hooks.
          Dual-strip awareness: contract-level; sets policy that effects may declare strip-affinity.

Move 1.2: INF-01 LayerStack                         effort: M (~150 LOC)
          unlocks edges:                            COM-03/04/05/07/10/11/12/14/16, GEO-03, PER-11×COM-03
          Cumulative outbound multiplicative edges: 12 → all gated until Phase 2/3 effect-side work
          Dual-strip awareness: per-strip layering possible (an 8th composition primitive).

Move 1.3: INF-02 FramebufferLPF (mandatory pass)     effort: L (~80 LOC)
          unlocks edges:                            PER-09/11/13, LIN-03/11, COM-X uniform softness
          Cumulative outbound multiplicative edges: 8 (some shared with INF-01)
          Dual-strip awareness: independent per-strip cutoff allowed (centre vs edge τ gradient possible).

Move 1.4: INF-08 sinLUT256 + INF-09 CFLSubstepGate   effort: L (~10 + ~80 = ~90 LOC)
          unlocks edges (preconditions for Phase 4): PHY-01/07/10, GEO-01/02/04/07/12, PER-20/21
          Rationale: Cheap, ship now, avoid blocking Phase 4. INF-08 is 10 LOC; INF-09 80 LOC.
          Dual-strip awareness: neutral.

Cumulative surface after Phase 1:
  modes-renderable:                  ~106 (no new modes yet)
  layer-stack-depth:                 3   (LayerStack lands; per-strip × 3 layers reachable)
  independent-parameter-axes/mode:   5   (no audio extensions yet; same per-mode)
  composition-primitives:            4   (BlendModes; LayerStack uses same operator vocabulary)
  HEADLINE:                          106 × 3 × 5 × 4 = 6,360 (3.0× over baseline)
  Qualitative capability tier:       "Compositional and fluid" — every existing effect can layer with every
                                     other; uniform softness knob across catalogue; PDE substrate ready.
LOC: ~370.
```

**Why this phase before Phase 2:** Phase 2's tempo doctrine targets effects that compose IN LayerStack. Without INF-01, BeatLockedRefresh applied to a single mode is a thin gimmick; inside LayerStack with INF-02, it produces T-03 Captain's "strongest novel signature" surface.

### Phase 2 — Tempo Doctrine: "K1 becomes rhythmic"

**Goal:** Surface tempo-phase to effects so the entire catalogue gains optional rhythm modulation.

```
Move 2.1: INF-04 TempoPhaseContinuous (single-tempo)  effort: M (~210 LOC AudioActor + ~10 LOC ControlBus)
          unlocks edges:                            PER-15/16/26, COM-07/13/15, LIN-01/09/12, GEO-02
          Captain decision: Q1.7.2 — recommend single-tempo (matches ESV11 reliability); bank version V1.1+.

Move 2.2: AUD-23 TempoConfidence scalar               effort: L (~30 LOC AudioActor; derivable from auto-corr strength)
          unlocks edges:                            COM-03 ConvictionMixer (founding ES spectronome semantics)
          Note: enables COM-03 to ship with first-class confidence-darkening.

Move 2.3: AUD-21 VoiceMusicClassifier (heuristic)     effort: L (~10 LOC render-side, no AudioActor extension)
          unlocks edges:                            COM-06 (degraded-but-shippable variant), PER-18 silence-gate
          Defer AUD-24 full VAD to V1.2+; heuristic suffices for V1.0.

Move 2.4: PER-18 AudioGatedConditionalDecay           effort: L (~40 LOC; uses PER-01 + AUD-21 + RMS)
          unlocks edges:                            T-04 ambient state (Liquid Stillness) — V1.0 F3 dependency

Cumulative surface after Phase 2:
  modes-renderable:                  ~106 (still — no new effects, just substrate)
  layer-stack-depth:                 3
  independent-parameter-axes/mode:   6   (+1 = phase axis available to every effect)
  composition-primitives:            4
  HEADLINE:                          106 × 3 × 6 × 4 = 7,632 (1.20× over Phase 1)
  Qualitative capability tier:       "Rhythmic" — every effect can be tempo-locked or tempo-modulated;
                                     audio-gated decay enables silence-aware ambient state; voice/music
                                     branching available.
LOC: ~290 (+ Phase 1 = 660 cumulative).
```

### Phase 3 — V1.0 Founder's Edition Heroes: "K1 ships"

**Goal:** Ship the 6 V1.0 must-ship effects (brainstorm catalogue:101–108) on top of the Phase 1+2 substrate. This is where end-customer-visible value lands.

```
Move 3.1: F1 Liquid Bloom (refined SbK1BloomEffect + INF-02 mandatory + INF-06 opt-in)
          effort: L (~50 LOC tuning + opt-in declaration)
          Rationale: existing effect; refinement only.
          Captain hero: signature visual.

Move 3.2: F5 Reflective Twin contract enforcement     effort: L (~100 LOC + per-effect audit pass)
          unlocks: structural quality across catalogue — every effect must honour sub-frame phase coherence
          Captain decision: Q1.7.1 — does interference fusion satisfy "one panel" reading? Recommend YES
                            (LGP physically fuses fringes), defer dual-strip-asymmetric patterns to F4 case.

Move 3.3: F6 First-Light Ignition (PS-09)              effort: L (~100 LOC, single one-shot effect)
          Cinematic 4–6 s wake-up choreography. Asymmetric payoff for tiny effort.

Move 3.4: F2 Centre-Phase Pendulum (LIN-01 single-tempo variant)
          effort: M (~200 LOC; depends on INF-04 from Phase 2)
          Captain hero: proof K1 understands music's pulse, not just amplitude.

Move 3.5: F3 Liquid Stillness ambient curation         effort: M (~100 LOC + curation labour)
          unlocks: T-04 triadic; ruthless curation of 8–12 from 106 ambient programmes
          Captain decision: Q1.7.6 — curation arbitration process undefined; recommend Captain selects
                            with input from brand voice + WHAT-IS-THAT filter.

Move 3.6: F4 Cross-Strip Wave Interference (GEO-01)    effort: M (~250 LOC + INF-08 sinLUT used)
          unlocks: T-01 ABSOLUTE moat triad — DUAL-STRIP-EXCLUSIVE × LGP-EXCLUSIVE
          Demo's "what is that" hero alongside F1.

Cumulative surface after Phase 3:
  modes-renderable:                  ~110 (+4 explicit V1.0 heroes; F5 contract is structural)
  layer-stack-depth:                 3
  independent-parameter-axes/mode:   6
  composition-primitives:            4 (LayerStack still using BlendModes; no new operators added in Phase 3)
  HEADLINE:                          110 × 3 × 6 × 4 = 7,920 (1.04× over Phase 2; modest because surface
                                     gain is qualitative — V1.0 ships — not headline-multiplicative)
  Qualitative capability tier:       "K1 ships V1.0 Founder's Edition." The catalogue includes Liquid
                                     Bloom + Centre Pendulum + Liquid Stillness + Cross-Strip Interference
                                     + Reflective Twin contract + First-Light Ignition. Customer-facing.
LOC: ~800 effects + ~100 contract enforcement = ~900 (+ 660 substrate = ~1,560 cumulative).
```

**Critical observation:** Phase 3's headline integer barely moves (+4%). Surface gain is mostly QUALITATIVE — the catalogue ships V1.0. The 4-tuple metric undervalues "shippability" by design; Captain should weight V1.0 readiness higher than the integer suggests.

### Phase 4 — Memory + Continuum: "K1 becomes physical"

**Goal:** Lift K1 from kinematic to dynamical. Pillar A continuum dynamics + Pillar C memory primitives + selected Pillar F dual-strip-exclusive effects.

```
Move 4.1: INF-03 PSRAMFrameRing                       effort: M (~120 LOC)
          unlocks edges:                            COM-11 EchoComposer, GEO-13 InterStripPhaseDelay,
                                                    PER-14 OpticalFlow (full ring), LIN-06 (multi-second variant)

Move 4.2: INF-12 PSRAMScalarRings (small-buffer registry) effort: L (~40 LOC + ~1.5 KB PSRAM)
          unlocks edges:                            LIN-06 RadialTimeScope (centre history),
                                                    LIN-07 PrismHueDrift, LIN-10 fx_dots[12] cache,
                                                    AUD-16 OnsetHistory ring.

Move 4.3: INF-07 PersistenceHelpersLibrary             effort: M (~250 LOC across 6 helpers)
          unlocks edges:                            ALL Pillar C absent domains (PER-09..PER-26 except some)
          Note: emaArrayDt, dtDecay3, crossBlendArray are nearly already there — split this into
                "extract existing" (~80 LOC) + "new helpers" (~170 LOC).

Move 4.4: INF-10 CubicHermiteResample                 effort: L (~120 LOC)
          unlocks edges:                            PHY-02 Verlet, PHY-07 Kuramoto, PHY-08 PendulumChain

Move 4.5: PHY-01 SpringMassDamperLattice (selected one PDE; 1 of {spring, fluid, wave, Kuramoto})
          effort: H (~250 LOC)
          unlocks edges:                            T-06 three-stack diffusion (with INF-02 + HW-01)
          Captain note: Pass 4 should adversarially compare {spring, fluid-advection, wave-eq, Kuramoto}
                        to pick the optimal first PDE. Heat equation (PER-20) is also a candidate — cheaper,
                        less ambitious. Spring lattice is most novel.

Move 4.6: PER-20 HeatEquationDiffusion1D              effort: L (~80 LOC)
          unlocks edges:                            T-06 + PER-25 velocity-aniso + PER-21 anisotropic-Perona-Malik

Move 4.7: PER-11 MultiScaleMemoryComposite             effort: L (~80 LOC)
          unlocks edges:                            T-03 Captain's "strongest novel signature" triple

Move 4.8: GEO-13 InterStripPhaseDelayBuffer (DUAL-STRIP-EXCLUSIVE) effort: M (~150 LOC)
          unlocks edges:                            T-09 PSRAM × LayerStack × DualStrip triad realised
          Pass 1 §7.1 Captain Q1: this requires PS-05 ReflectiveTwin clarification.

Move 4.9: PER-14 PredictiveTrailOpticalFlow             effort: M (~100 LOC + HW-08 SIMD recommended)
          unlocks edges:                            PER-25 velocity-aniso (PER-14 × PER-20 hybrid)

Cumulative surface after Phase 4:
  modes-renderable:                  ~115 (+5 PDE-class effects + 2 dual-strip-exclusive)
  layer-stack-depth:                 3
  independent-parameter-axes/mode:   8   (+2 = memory dimension + spatial-coupling axis)
  composition-primitives:            4
  HEADLINE:                          115 × 3 × 8 × 4 = 11,040 (1.39× over Phase 3)
  Qualitative capability tier:       "Physical." K1 evolves under physical laws driven by audio. Pixels
                                     are continuum. Inter-strip phase delay produces depth illusions
                                     impossible on competing single-strip hardware. The brand-language
                                     "Liquid Light" gains its full physical depth.
LOC: ~1,190 (+ Phase 1+2+3 = ~2,750 cumulative).
```

**Phase 4 is the defining phase for K1's strategic position.** Layer 3 in Pass 2's hidden-layer stack. Every ABSOLUTE moat in the registry materialises here.

### Phase 5 — Cross-Lineage Long Tail: "K1 becomes brand-distinguished"

**Goal:** K1-native exemplars + cross-lineage fusions. The "neither lineage shipped this" catalogue items that establish K1 as evolved-beyond-port.

```
Move 5.1: LIN-06 CentreOriginRadialTimeScope            effort: L (~60 LOC; uses INF-12 + AUD-04 history)
          unlocks: K1-native exemplar #1; brand-defensibility headline.

Move 5.2: LIN-10 MotionBlurCachedChromagramDots         effort: L (~80 LOC; uses AUD-02 + INF-12)
          unlocks: K1-native exemplar #2; fills two K1 gaps simultaneously.

Move 5.3: LIN-04 RhythmLockedCubicPerlinRibbon          effort: L (~120 LOC; uses INF-04 + bands[])
          Re-purposes kill-listed kaleidoscope cubic-Perlin (rehabilitation per SA-4 viability artifact).

Move 5.4: LIN-08 AttackOnlyPitchClassVelocityField      effort: L (~80 LOC; uses AUD-02 + 12 followers)
          Zero audio extension; ~0.5 ms render.

Move 5.5: LIN-09 BeatParityBloomSpriteInjection          effort: L (~50 LOC; modifies SbK1Bloom)
          Uses INF-04 narrow-window injection.

Move 5.6: V1.1 #7 Pitch Constellations                   effort: M (~150 LOC; chromagram + chroma-derivative + INF-12 fx_dots cache)
          Content-marketing magnet per ProductStrategy F4.

Move 5.7: COM-15 DecoratorPoolEventOverlay               effort: M (~150 LOC; uses AUD-04 onset + INF-04 tempo-quantised triggers)
          unlocks: 4 micro-effects (ring-burst, edge-flash, centre-pop, chroma-splash).

Cumulative surface after Phase 5:
  modes-renderable:                  ~125 (+10 LIN- and accessory)
  layer-stack-depth:                 3
  independent-parameter-axes/mode:   8
  composition-primitives:            5   (+1 = decorator-pool overlay primitive)
  HEADLINE:                          125 × 3 × 8 × 5 = 15,000 (1.36× over Phase 4)
  Qualitative capability tier:       "Brand-distinguished." K1 ships effects that demonstrably could not
                                     be ported from SB or ES — they are emergent fusions only natural on
                                     a platform that takes both lineages as input AND has the K1 hardware
                                     triad. Marketing signal: "neither lineage shipped this."
LOC: ~690 (+ Phase 1+2+3+4 = ~3,440 cumulative).
```

### Phase 6 — Session + Interpretation: "K1 becomes interpretive"

**Goal:** Minute-scale awareness + audio-interpretation features. The "audio reactivity → audio interpretation" reframe.

```
Move 6.1: INF-11 LongWindowAudioStats                  effort: M (~150 LOC + ~24 KB PSRAM)
          unlocks: AUD-25, COM-08, PS-10 substrates.

Move 6.2: AUD-12 SpectralCentroid                       effort: L (~12 LOC AudioActor)
Move 6.3: AUD-13 SpectralFlatness                       effort: L (~20 LOC AudioActor)
          Combined with AUD-12, unlocks 2-D timbre coordinate (multiplicative pair).

Move 6.4: AUD-14 PitchHPSConfidence                     effort: M (~40 LOC AudioActor)
          unlocks: PHY-06 CoulombField (chroma-as-charge gated).

Move 6.5: AUD-18 InterBandCofiringMatrix                effort: M (~80 LOC AudioActor + 256 B)
          unlocks: COM-03 mix-coefficient as polyphony classifier.

Move 6.6: AUD-25 MoodClassifier                         effort: M (~120 LOC; depends on Move 6.1)
          unlocks: COM-08 MoodStateMachineComposer.

Move 6.7: COM-08 MoodStateMachineComposer               effort: M (~200 LOC FSM + curated mood→mode map)
          Captain decision: Q1.7.5 — risk PS-10 brand-voice failure; needs intentional-feel curation.

Move 6.8: PS-10 MoodDrivenAutonomousModeSelection       effort: L (~50 LOC consumer of COM-08)
          GATED on Captain ratification; otherwise hold for V1.2+.

Cumulative surface after Phase 6:
  modes-renderable:                  ~125 (no new modes; modes BECOME interpretive)
  layer-stack-depth:                 3
  independent-parameter-axes/mode:  11   (+3 = centroid, flatness, mood — every mode gains 3 driver axes)
  composition-primitives:            5
  HEADLINE:                          125 × 3 × 11 × 5 = 20,625 (1.38× over Phase 5)
  Qualitative capability tier:       "Interpretive." K1 stops being audio-reactive and becomes
                                     audio-interpreting. Voice vs music distinguished; tonal vs noisy
                                     mapped to palette; polyphony complexity drives composition; minute-scale
                                     mood evolution available.
LOC: ~670 (+ Phase 1..5 = ~4,110 cumulative).
```

---

## 4. Greedy vs Strategic Comparison

```
Greedy ordering (max immediate surface gain at each step):
  1. INF-01 LayerStack          (12 outbound, biggest single jump → surface 6,360)
  2. INF-04 TempoPhase          (11 outbound)
  3. INF-02 FramebufferLPF      (8 outbound — but ships as opt-in only because INF-06 not landed)
  4. INF-03 PSRAMFrameRing      (7 outbound)
  5. INF-08 sinLUT              (7 outbound, precondition)
  6. INF-06 EffectRoleFlags     (6 outbound)
  7. INF-09 CFLSubstepGate
  8. ...

Strategic ordering (recommended; max long-term surface):
  1. INF-06 EffectRoleFlags     (6 outbound — but enables INF-02 mandatory, saving 30+ effects from retrofit)
  2. INF-01 LayerStack          (12 outbound)
  3. INF-02 FramebufferLPF      (8 outbound — ships MANDATORY because INF-06 in place)
  4. INF-08 sinLUT + INF-09 CFL (cheap, ship now to avoid blocking Phase 4)
  5. INF-04 TempoPhase + AUD-23 + AUD-21 + PER-18 (Phase 2 bundle)
  6. V1.0 effects (Phase 3)
  7. INF-03 + INF-12 + INF-07 + PDE primitives (Phase 4)
  8. ...

Divergence points:
  • INF-06 RoleFlags: greedy defers (low immediate surface); strategic prioritises (precondition for INF-02
    mandatory ship). Cost of greedy: every Phase-3+ self-trailing effect must add opt-out hooks retroactively
    (~5–10 LOC × ~30 effects = 150–300 LOC retrofit + risk of double-trail bugs slipping into V1.0).
    Cost of strategic: ~50 LOC up-front before any visible unlock.
    NET: strategic wins by ~100–250 LOC and avoids latent bugs.

  • INF-08/INF-09 sinLUT/CFL: greedy treats as Phase 4 dependency (don't pay LOC cost until Phase 4 needs
    them). Strategic ships in Phase 1 (cheap, unblocks Phase 4 from blocking on substrate). Cost of greedy:
    Phase 4 must include substrate work in Phase-4 LOC budget, raising the bar for Phase 4 to start.
    Cost of strategic: ~90 LOC of Phase-1 LOC that won't be exercised until Phase 4.
    NET: marginal; strategic recommended on planning ergonomics.

Recommendation: STRATEGIC ORDERING. The single critical divergence is INF-06 → INF-02-mandatory. The
                rest is a planning preference.
```

---

## 5. Effort-to-Unlock Ratios

Stack-ranked highest-leverage moves (≥ 5 outbound multiplicative edges or critical-path):

| Move | LOC | Outbound edges | Leverage class | Critical-path? |
|---|---|---|---|---|
| INF-08 sinLUT | 10 | 7 | EXTREME | Yes (Phase 4) |
| INF-06 EffectRoleFlags | 50 | 6 | HIGH | Yes (Phase 1 → Phase 2/3) |
| INF-09 CFLSubstepGate | 80 | 6 | HIGH | Yes (Phase 4) |
| INF-02 FramebufferLPF | 80 | 8 | HIGH | Yes (Phase 1) |
| INF-12 PSRAMScalarRings | 40 | 4+ | HIGH | Yes (Phase 4 → Phase 5) |
| INF-04 TempoPhase | 210 | 11 | HIGH | Yes (Phase 2 → Phase 3 F2) |
| INF-01 LayerStack | 150 | 12 | HIGH | Yes (Phase 1 → all Pillar B) |
| INF-03 PSRAMFrameRing | 120 | 7 | HIGH | Yes (Phase 4) |
| INF-07 PersistenceHelpers | 250 | many (substrate library) | HIGH | Phase 4 |
| AUD-23 TempoConfidence | 30 | 1 (COM-03) but founding | MEDIUM | Phase 2 |
| AUD-12 SpectralCentroid | 12 | 3 (PHY-01, GEO-01, COM-05) | HIGH | Phase 6 (or earlier) |
| LIN-06 RadialTimeScope | 60 | 0 (terminal capability) | MEDIUM (brand value) | Phase 5 |
| LIN-10 MotionBlurChromagram | 80 | 0 (terminal) | MEDIUM (brand value) | Phase 5 |
| F1 Liquid Bloom refinement | 50 | 0 (terminal) | EXTREME (V1.0 hero) | Phase 3 |
| F4 Cross-Strip Interference | 250 | T-01 ABSOLUTE moat materialised | EXTREME (moat) | Phase 3 |

**Three "trivial-LOC critical-path" moves** to highlight: INF-08 sinLUT (10 LOC, unlocks 7 PDE/GEO domains), INF-06 EffectRoleFlags (50 LOC, unblocks INF-02 mandatory ship), AUD-12 SpectralCentroid (12 LOC, enables 2-D timbre when paired with AUD-13). All three should ship as early as possible in their respective phases.

---

## 6. Phase Gates Summary

```
Phase 0:  K1 today                       — surface  2,120  (baseline)
Phase 1:  Compositional and fluid         — surface  6,360  (3.0×)   ~370 LOC substrate
Phase 2:  Rhythmic                        — surface  7,632  (3.6×)   ~290 LOC substrate (660 cum.)
Phase 3:  V1.0 Founder's Edition ships    — surface  7,920  (3.7×)   ~900 LOC effects (~1,560 cum.)
Phase 4:  Physical (continuum + memory)   — surface 11,040  (5.2×)   ~1,190 LOC (~2,750 cum.)
Phase 5:  Brand-distinguished             — surface 15,000  (7.1×)   ~690 LOC (~3,440 cum.)
Phase 6:  Interpretive                    — surface 20,625  (9.7×)   ~670 LOC (~4,110 cum.)
```

Phase 3 = V1.0 launch. Phases 4–6 = V1.1 / V1.2 / V2.0 trajectory. Surface metric saturates around Phase 6 — beyond this, returns diminish (Pass 2 §5 noted Layer 5+ moats are MEDIUM, not STRONG/ABSOLUTE).

---

## 7. "What Are We Leaving on the Table" Audit

Domains explicitly NOT in the Phase-1..6 sequence above. Each annotated with reason and the variable that would change to bring it in.

```
[INF-05] TempoBank per-bin (tempi[N].phase)
   Excluded: speculative — Captain Q1.7.2 recommended single-tempo first.
   Conditions for re-inclusion: V1.1+ if user-research / customer-feedback confirms demand for ES-class
                                 metronome-bouquet visuals (which ProductStrategy filter currently rejects
                                 as "fragmented" and brand-voice-incompatible).

[AUD-19] EnergyModulationSpectrumLock
   Excluded: AMBITIOUS — 120 LOC AudioActor + 50 µs/hop. SSA-AudioDriver flagged as biggest §9 risk.
   Conditions for re-inclusion: V1.2+ research; or if demonstrable customer demand for tremolo-locked
                                 visuals (dub-reggae / vibrato vocal scenes).

[AUD-22] FormantTriangulation
   Excluded: divergent SA-1 singleton; only useful in voice mode and FFT peak-pick is unreliable without LPC.
   Conditions for re-inclusion: only if AUD-21 voice mode lands AND Captain prioritises vowel-space colour mapping.

[AUD-24] Full VAD (vs heuristic AUD-21)
   Excluded: research-grade DSP; heuristic AUD-21 suffices for V1.0.
   Conditions for re-inclusion: V1.2+ if heuristic produces visible misclassifications in production.

[PHY-11] BoidSwarmAttractor
   Excluded: divergent SSA-Physics §12 singleton; speculative.
   Conditions for re-inclusion: V1.2+ research track; or if K1's hardware revision adds RAM enabling
                                 24-agent simulation comfortably.

[GEO-08] VoronoiCellularPartition
   Excluded: divergent SSA-Geometry §8; no consumer in registry.
   Conditions for re-inclusion: if external domain E-03 DistanceFieldTopology is brought in (DF turns
                                 hard partitions into continuous-boundary surfaces — re-evaluates GEO-08).

[GEO-11] RecursiveZoomViewport
   Excluded: divergent niche.
   Conditions for re-inclusion: V1.2+ aesthetic experiment; not currently brand-aligned.

[GEO-14] CounterStreamingInterleavedLayers
   Excluded: divergent niche.
   Conditions for re-inclusion: if external domain "stereo correlation" audio-driver is added (could
                                 drive even/odd LED stream velocities by L/R audio in-phase vs out-of-phase).

[COM-09] StoryArcSequencer
   Excluded: V1.2+ — authoring tooling-heavy; limited V1.0 utility without curated arc library.
   Conditions for re-inclusion: V1.2+ when authored arcs accumulate from real music captures, OR if
                                 brand strategy pivots toward live-show authoring market.

[E-01] ReactionDiffusion (external candidate)
   Excluded from default sequence: not in any SSA proposal; orchestrator-flagged as synthesis blind spot.
   Conditions for re-inclusion: V1.2+ research track; one of the strongest external bridges per Pass 2 §7.
                                 Highest-impact "external" addition the orchestrator surfaced.

[E-02] PhaseConjugateHolography (external candidate)
   Excluded from default sequence: requires LGP optical measurement campaign (calibration table).
   Conditions for re-inclusion: V1.1 if Captain commissions the optical measurement; turns T-01 from
                                 "looks like interference" into "is interference" — engineering-grade moat
                                 upgrade.

[E-06] KalmanBeatPhasePrediction (external candidate)
   Excluded from default sequence: 150 LOC + tuning campaign.
   Conditions for re-inclusion: V1.2+ if AUD-17 lookahead overshooting becomes a customer complaint; or
                                 if mood-classifier (Move 6.6) needs principled uncertainty quantification.

[GEO-06 / GEO-10] Centre-origin departures (CircularRing, AsymmetricDrift)
   Excluded: violate HW-03 invariant; Captain Q1.7.4 unresolved.
   Conditions for re-inclusion: only with Captain ratification of "centre-origin as default with
                                 audio-driven exception zones" interpretation. Not recommended — Pillar G
                                 brand-meta favours strict invariant enforcement.
```

**Cumulative left-on-table cost:** ~13 domains excluded with ~1,500 LOC if all included; preserves V1.0 / V1.1 schedule. None of the excluded domains routes through an ABSOLUTE moat triadic — i.e. no kill-decision risks losing a hardware-tier moat.

---

## 8. Summary Table

| Phase | Name | LOC | Surface | Capability Tier | V-tier |
|---|---|---|---|---|---|
| 0 | Current K1 | — | 2,120 | Baseline | — |
| 1 | Compositional + fluid | ~370 | 6,360 | Substrate ready | V1.0 prereq |
| 2 | Rhythmic | ~290 | 7,632 | Phase doctrine | V1.0 prereq |
| 3 | V1.0 Heroes | ~900 | 7,920 | Customer-shippable | V1.0 |
| 4 | Physical | ~1,190 | 11,040 | Dynamical motion | V1.1 |
| 5 | Brand-distinguished | ~690 | 15,000 | K1-native exemplars | V1.1 |
| 6 | Interpretive | ~670 | 20,625 | Audio interpretation | V1.2+ |

Total: ~4,110 LOC across 6 phases; ~3,200 LOC engine substrate + ~1,000 LOC effects. Brainstorm catalogue's ~880-LOC V1.0 estimate (Infra items 1–5) maps to Phases 1+2 substrate; this Pass 3 estimate is ~660 LOC for Phases 1+2, ~25% under the brainstorm estimate (likely because PersistenceHelpers and PSRAMFrameRing pushed to Phase 4).

---

## 9. Open Questions Carrying Forward to Pass 4

Inherited from Pass 1 §7 + Pass 2 §9, plus Pass 3-specific:

1. **Greedy vs Strategic ordering ratification** — Pass 3 recommends strategic. Pass 4 should adversarially challenge: is the INF-06 → INF-02 precedence ACTUALLY load-bearing, or does the orchestrator overstate the retrofit cost of greedy?
2. **Phase 4 PDE-of-choice** — Pass 4 should evaluate {spring, fluid, wave, Kuramoto, heat-eq} for which lands first. Pass 3 hedged with PHY-01 + PER-20 both in Move 4.5/4.6.
3. **PS-10 inclusion in Phase 6** — currently GATED on Captain. Pass 4 should stress-test the brand-voice failure scenario.
4. **External domain E-01 RD inclusion** — none of the 8 SSAs proposed it. Pass 4 must decide whether the synthesis blind spot is worth a phase slot.
5. **Surface metric validity** — the 4-tuple under-weights V1.0 shippability (Phase 3 only +4%). Pass 4 should challenge whether the metric correctly captures Captain priorities.
6. **All 7 Captain decisions from Pass 1 §7** carry forward.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:claude-opus-4-7 (orchestrator) | Created — Pass 3 of K1 visual-pipeline synergy-topology protocol. Topologically sorted the directed graph from Pass 2; computed unlock-surface 4-tuple per phase; produced 6-phase strategic kill order totalling ~4,110 LOC; compared greedy vs strategic with one critical divergence (INF-06 → INF-02 mandatory); enumerated 12 left-on-table exclusions with re-inclusion conditions; named each phase's qualitative capability tier. Phase 3 = V1.0 ship; Phases 4–6 = V1.1/V1.2/V2.0 trajectory. Surface gain: 9.7× over baseline by end of Phase 6 (saturation). |
