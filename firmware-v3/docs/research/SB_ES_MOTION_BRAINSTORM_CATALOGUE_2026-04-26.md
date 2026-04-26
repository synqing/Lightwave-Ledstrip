---
abstract: "Synthesis of an 8-SSA parallel brainstorm extrapolating new motion-mechanic categories beyond the canonical SB/ES taxonomy. ~95 raw category proposals across 7 generative axes (animation-physics, unexplored audio drivers, geometric/topological extrapolation, persistence/decay mutation, composition/layering, SB×ES cross-lineage fusion, K1-specific affordance mining) plus a CEO/founder strategic filter. Identifies 6 convergent themes where multiple SSAs independently proposed similar primitives (high-conviction signal); maps a 6-piece engine-infrastructure roadmap (LayerStack, global framebuffer LPF, tempo-phase exposure, fx_dots[] cache, PersistenceHelpers, PSRAM frame ring) that collectively unlocks most proposals; carries a ranked top-12 priority shortlist filtered through K1's locked product positioning ('Music. Made visible.', liquid centre-origin chiaroscuro). The headline thesis: K1 lifts effects from SB/ES kinematic motion (image transforms with smoothing) to dynamical motion (state evolves under physical laws driven by audio interpretation, on a dual-strip LGP medium that no single-strip competitor can reproduce). Read alongside SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md when authoring EFFECT_FRAMEWORK_STANDARD.md or scoping V1.0 launch effects."
---

# K1 Motion-Mechanic Brainstorm Catalogue

**Track D companion to `SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md` and `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md`. Research preservation, NOT policy.**

This catalogue synthesises an 8-SSA parallel brainstorm. Each SSA was briefed with the canonical taxonomy as the "what's already covered" baseline, K1's hard constraints (centre origin 79/80, dual 2×160, 120 FPS / 2.0 ms ceiling, no heap in render(), dt-correct, 16 MB PSRAM), and a specific generative angle. SSAs invoked `/brainstorming`, `/animation-physics`, `/spectrasynq-audio-pipeline`, `/signal-processing-verification` skills as appropriate plus Context7 / claude-mem MCP. Per-category detail (mechanism, audio integration, persistence parameters, render-time estimates, lineage, risk) is in the cached SSA outputs at `/private/tmp/.../tasks/`; this document is the synthesis layer.

## SSA inventory

| SSA | Angle | Categories returned | Top-pick recommendation |
|---|---|---|---|
| Physics | Animation-physics primitives (springs, fluids, particles, Verlet, Kuramoto, easing curves) | 12 | SpringLatticeBloom, FluidAdvectionWavefront, ViscousCrossStripBleed |
| AudioDriver | Unexplored audio features (spectral centroid, flatness, HPS, ZCR, formants, STM, modulation spectrum, chroma derivative, voice-classifier) | 12 | OnsetHistoryTimeAsXScroll, BeatPhasePredictiveFlash, VoiceVsMusicClassifier |
| Geometry | New topologies (interference, standing-wave, parallax, holographic, fractal, ring, spatial-Fourier, Voronoi, mass-flow, drift origin, recursive zoom, multi-origin, inter-strip phase delay, counter-streaming) | 14 | Inter-Strip Phase-Coupled Coherence (dual-strip-only), Bilateral Wave Interference, Parallax Depth-Stack |
| Persistence | New decay primitives (chromatic phosphor, asymmetric, multi-scale, recursive floor, echo, predictive, beat-locked, tempo-phase τ, Schmitt, conditional, onset cross-blend, heat-eq, anisotropic, frequency-dependent, flux-conserving, convolutional, velocity-aniso, tempo-locked heat) | 14 | Multi-Scale Memory Composite, Beat-Locked Refresh, Velocity-Anisotropic Blur |
| Composition | Layer/composition patterns (Conviction Mixer, Sky/Score, Mood Switch, Voice/Music, Two-Step, Mood Arc, Story-Arc, Density Stack, Time Mirror, Anti-Mode, Param Cross-Wire, Cloud Mask, Decorator, Stain Glass) | 14 | Conviction Mixer, Sky/Score, Param Cross-Wire |
| CrossLineage | SB × ES fusions (12 synergistic pairings classified by lineage dominance) | 12 | Motion-Blur Cached Chromagram Dots, Centre-Origin Radial Time Scope, Beat-Parity Bloom Sprite Injection |
| K1Specific | Hardware-affordance mining (LGP-native, dual-strip phase/parallax/interference, PSRAM frame-history, external-sync, ControlBus richness) | 12 | Cross-Strip Wave Interference, Dual-Strip Phase Parallax, LGP Gradient-Native Heat-Field |
| ProductStrategy | CEO/founder filter against locked brand positioning | 7 families + 4-item kill-list | F1 Liquid Bloom, F2 Centre-Phase Pendulum, F6 First-Light Ignition |

**Total raw category proposals: ~97.** Synthesis below collapses overlap, surfaces convergent signal, and applies the ProductStrategy lens.

## Convergent themes (high-conviction signal)

Six themes were proposed by multiple SSAs independently. Convergence across orthogonal generative angles is the strongest evidence that these primitives deserve K1 launch-track investment.

### Theme 1 — Global framebuffer LPF as universal trail mechanism

| SSA | Proposal |
|---|---|
| Persistence | A3 Multi-Scale Memory Composite (fast + slow EMA) |
| CrossLineage | #3 Log-Warped Phosphor Trail (SB log warp × ES global LPF) |
| CrossLineage | #11 lpf_drag Cross-Fade Preserving Centre Origin |
| K1Specific | implicit in #10 Optical-Flow Trail (frame-history pass) |
| ProductStrategy | F3 Liquid Stillness ambient state (requires this primitive) |

**Verdict:** This is K1's most under-served lineage gap. ES has it as `apply_image_lpf` with knob-driven dt-correct cutoff `0.5 + (1−√softness)·14.5 Hz`; K1 has nothing equivalent at the renderer level. Building it would unlock the ambient state, cross-fade transitions, and a uniform softness knob across the whole catalogue.

### Theme 2 — Beat-locked / tempo-phase-modulated decay & refresh

| SSA | Proposal |
|---|---|
| Persistence | B1 Beat-Locked Refresh, B2 Tempo-Phase Modulated Decay |
| Composition | #5 Two-Step (beat-quantised mode swap), #11 Param Cross-Wire (tempo-phase modulator) |
| CrossLineage | #9 Beat-Parity Bloom Sprite Injection, #2 Centre-Mirrored Beat-Parity Comet Pair |
| K1Specific | #12 Phase-Locked Strobe Lattice |
| ProductStrategy | F2 Centre-Phase Pendulum (the entire family) |

**Verdict:** Tempo-phase exposure to effects is one of two K1 lineage gaps that, when closed, unlocks 6+ proposed categories. Currently K1's audio chain has reliable beat tracking on ESV11 but does not surface a continuous `tempi[].phase` field to effects. Closing this single gap is the highest-leverage AudioActor extension in the brainstorm.

### Theme 3 — Spatial diffusion / wave PDE / spring lattice (continuum dynamics)

| SSA | Proposal |
|---|---|
| Physics | #1 SpringLatticeBloom, #3 FluidAdvectionWavefront, #11 RopeWavePropagation, #6 KuramotoPhaseLattice |
| Persistence | C1 Heat-Equation Diffusion, C2 Anisotropic Diffusion (Perona-Malik), D2 Tempo-Locked Heat Equation |
| Geometry | #1 Bilateral Wave Interference, #2 Standing-Wave Resonance Lattice, #4 Holographic Two-Source, #9 Mass-Conservation Pixel Flow |
| K1Specific | #1 LGP Gradient-Native Heat-Field, #7 1-D Shallow-Water Surface |

**Verdict:** Continuum-physics primitives appear across 4 SSAs. They share three traits: (a) they're CHEAP-to-MODERATE on K1's render budget (320 LEDs × ~5–10 ops + dt-correct integration), (b) they're SB/ES-impossible because both lineages decay luminance temporally without spatial coupling, (c) they look exquisite on LGP (the optical diffusion smooths quantisation that would look glitchy on naked LEDs). The thesis from SSA-Physics — **K1 unlocks dynamical motion (state evolves under physical laws) where SB/ES were limited to kinematic motion (image transforms with smoothing)** — is the cleanest framing.

### Theme 4 — Dual-strip phase / interference / parallax

| SSA | Proposal |
|---|---|
| Physics | #8 ViscousCrossStripBleed |
| Geometry | #1 Bilateral Wave Interference, #4 Holographic Two-Source, #6 Counter-Rotating Phase Wheel, #13 Inter-Strip Phase-Coupled Coherence |
| K1Specific | #2 Dual-Strip Phase Parallax Depth, #4 Cross-Strip Wave Interference, #5 Frequency-Spatial Strip Stereo, #11 Chroma-Spatial Pitch Lattice, #12 Phase-Locked Strobe Lattice |

**Verdict:** The single most under-exploited K1 affordance. K1 currently treats dual-strip as either "two zones rendered independently" (ZoneComposer) or "two halves rendered identically" (mirror). It is rarely treated as **two physically separated emitters illuminating one optical medium capable of interference, phase, parallax, and dual-axis-information patterns**. SB and ES are single-strip — these effects are physically impossible on competing hardware. A category that brand-differentiates K1 at a glance.

### Theme 5 — Layer composition / overlap rendering

| SSA | Proposal |
|---|---|
| Composition | All 14 categories (LayerStack as enabler) |
| ProductStrategy | F1+F2+F3 implicitly stacked; F5 Reflective Twin enforces unification |
| CrossLineage | #5 Halo Composite with Tempo-Confidence Swell |

**Verdict:** K1's ZoneComposer is partition-based (disjoint zones, no overlap permitted by `validateLayout()`). Adding a single ~150-LOC `LayerStack` class (PSRAM N-buffer overlap renderer, sibling to ZoneComposer, reusing the existing 4 BlendMode operators) unlocks 12 of the 14 composition categories. **One infrastructure piece, twelve new mode classes.**

### Theme 6 — Audio interpretation (spectral centroid, formants, voice/music, mood)

| SSA | Proposal |
|---|---|
| AudioDriver | All 12 categories |
| Composition | #4 Voice/Music Switcher, #6 Mood-State-Machine Composer |
| ProductStrategy | F3 Liquid Stillness (silence/music classifier needed for ambient↔reactive transition) |

**Verdict:** K1's ControlBus already ships substantial audio dimensions that the render side never reads (STM temporal modulation, harmonic saliency, motion-semantic jitter/syncopation/pitch_contour_dir, onset band-split, audio confidence). Five of the AudioDriver SSA's twelve categories are pure render-side reuse — zero DSP work required. The remainder requires AudioActor extensions ranging from trivial (spectral centroid scalar — 12 lines of code) to ambitious (formant tracking, modulation spectrum). Headline reframe: **"audio reactivity → audio interpretation"** — moving from "how loud each band is" to "what kind of music is playing".

## Strategic priority shortlist (ranked)

Filtered through the ProductStrategy SSA's CEO/founder lens applied to all 97 raw proposals. Ranking weights: visual signature value, technical feasibility within K1 constraints, differentiation moat vs SB/ES/WLED, founder-language defensibility ("WHAT IS THAT" test), launch-video carry potential.

### V1.0 launch-track (must ship)

| # | Category | Source SSAs | Engine cost | Why it carries |
|---|---|---|---|---|
| 1 | **Liquid Bloom** (refined SbK1BloomEffect) | ProductStrategy F1, CrossLineage #9 | LOW (existing engine + tuning) | The signature mode. The single visual K1 is sold on. LGP makes it look like ink in warm water — physically impossible to reproduce on naked WS2812. |
| 2 | **Centre-Phase Pendulum** | ProductStrategy F2, CrossLineage #1+#12, K1Specific F2 | MEDIUM (tempo phase exposure to effects) | Proof that K1 understands music's pulse, not just amplitude. Closes K1 lineage gap #1. |
| 3 | **First-Light Ignition** ("The Wake-Up") | ProductStrategy F6 | LOW (one-shot effect, ~100 LOC) | Cinematic 4–6s startup choreography. Tiny effort, asymmetric payoff — opens the launch video at second 0. |
| 4 | **Liquid Stillness** ambient state (curated) | ProductStrategy F3, AudioDriver #11, Persistence A3 | MEDIUM (curate 8–12 from existing 106 ambient programmes; needs Theme 1 framebuffer LPF) | "Extraordinary when silent" — the dual-state half of locked positioning. Without it, K1 is a $369 toy. |
| 5 | **Reflective Twin** quality (structural) | ProductStrategy F5, K1Specific Theme 4 | LOW (mostly enforcement + depth-echo helper) | Makes the dual-strip vanish into a single 330mm panel of light. Without it, K1 reads as two strips, not one object. Non-negotiable contract for every effect. |
| 6 | **Cross-Strip Wave Interference** | K1Specific #4, Geometry #1+#4 | MEDIUM (sin LUT + dual phase accumulators) | Demo's "what is that" hero alongside Liquid Bloom. Physically impossible on single-strip — moat is hardware-mechanical. ~32 µs render. |

### V1.1 fast-follow

| # | Category | Source SSAs | Engine cost | Why later |
|---|---|---|---|---|
| 7 | **Pitch Constellations** | ProductStrategy F4, CrossLineage #10, K1Specific #11 | MEDIUM (chromagram + dt-correct array LPF + circle-of-fifths layout) | Content-marketing magnet — music YouTubers will record explainer videos. Ship clean, not rushed. |
| 8 | **Silent Spectacle** ambient drama | ProductStrategy F7, Geometry #2, K1Specific #1 | MEDIUM (Perlin walk + dt-correct global LPF) | Captures customer-generated content for social loop. Needs Theme 1 LPF. |
| 9 | **Onset History Time-as-X Scroll** (centre-origin radial variant) | AudioDriver #6, CrossLineage #6 | LOW (160-sample ring buffer) | Reinterpretation of ES history-axis through K1 centre-origin invariant. Wholly emergent — neither lineage shipped this. ~0.2 ms render. |
| 10 | **Conviction Mixer** layer composer | Composition #1 | MEDIUM (LayerStack class, ~150 LOC) | Direct generalisation of ES spectronome to N≥2 layers + N confidence channels. Unlocks 11 other Composition categories as side effects. |

### V1.2+ research / aspirational

| # | Category | Why later |
|---|---|---|
| 11 | **Spring-mass-damper centre-origin lattice (SpringLatticeBloom)** | Visually novel; needs hardware A/B vs Liquid Bloom to justify catalogue slot. |
| 12 | **Story-Arc Sequencer** | Authoring tooling-heavy; revisit after launch when authored arcs accumulate from real music captures. |

### Kill-list (explicitly do NOT pursue)

Per ProductStrategy SSA filter against locked brand positioning ("restraint beats maximalism", "monochrome wins over rainbow", "Music made visible" not "instrument-grade analyser"):

1. **Tempo-bank-as-N-pendulum-dots literal port** (ES `metronome` direct copy). On 330mm LGP it reads as fragmented, not unified. Use the tempo bank as engine plumbing for F2; do not render N≥8 pendulums directly.
2. **Neural-network tensor visualisation** (ES `neurons`). Developer-wow only. ES disables it in production for the same reason.
3. **History-buffer-as-spatial-axis "scope" modes** (ES `debug`, SB `waveform`). Banned territory: looks like DSP test fixture, contradicts brand voice. The centre-origin radial variant (#9 above) is exempt.
4. **Multi-mirror PRISM kaleidoscope** (SB 3.2 `kaleidoscope`). RGB-channel-rotation halos and three-virtual-camera Perlin spaghetti contradict locked "restraint" voice.

## Engine-infrastructure roadmap

The brainstorm collectively identifies six pieces of engine infrastructure. Stack-ranked by leverage (number of categories unlocked per LOC of new code):

| # | Infrastructure | LOC estimate | Categories unlocked | Priority |
|---|---|---|---|---|
| 1 | **`LayerStack`** (PSRAM N-buffer overlap renderer, sibling to ZoneComposer, reuses 4 BlendMode operators) | ~150 LOC | 12 Composition categories + V1.0 F1+F2+F3 layered ambient/reactive composition | V1.0 |
| 2 | **Global framebuffer LPF / `apply_image_lpf` analogue** (dt-correct, knob-driven cutoff `0.5 + (1−√softness)·14.5 Hz`, RendererActor post-pass with per-effect opt-out) | ~80 LOC | Persistence Theme 1 (6+ categories) + V1.0 F3 ambient state + V1.1 #8 Silent Spectacle | V1.0 |
| 3 | **Tempo phase exposure to effects** (`controlBus.tempoPhase01` continuous 0..1; optionally per-bin `tempi[N].phase` bank — single-tempo first, bank later) | ~200 LOC AudioActor + ~10 LOC ControlBus | V1.0 F2 + 6 Theme 2 categories + 3 CrossLineage fusions + 2 K1Specific categories | V1.0 |
| 4 | **`PersistenceHelpers.h`** (`dtDecay3`, `emaArrayDt`, `crossBlendArray`, `spatialLPF1D`, `heatStep1D`, `velocityAniso1D`) | ~250 LOC | All 14 Persistence categories + reusable across the whole catalogue | V1.0 |
| 5 | **`fx_dots[]` motion-blur cache** (12-slot prev-position cache + `set_dot_position`/`draw_dot` helpers, dt-correct) | ~80 LOC | Motion-Blur Cached Chromagram Dots + sub-pixel velocity ribbon + V1.1 #7 Pitch Constellations | V1.0 |
| 6 | **PSRAM frame ring buffer** (10s × 320 LED × 3B × 120 FPS = ~1.15 MB; circular write at frame head, arbitrary read at delay offset) | ~120 LOC | Time-Warp Replay, Optical-Flow Trail, Echo Composer, Long-Context Mood-Drift | V1.1 |

**Headline:** ~880 LOC of engine work (items 1–5) lands V1.0's must-ship launch shortlist. Item 6 is V1.1 and unlocks the long-tail social-content modes.

## Cross-cutting open questions for Captain

1. **Tempo-phase exposure scope (Theme 2 / Infra #3):** ship single-tempo-phase first (matches K1 ESV11 reliable beat tracking), or scope-up to a per-bin tempo bank (matches ES `tempi[N].phase` doctrine, more ambitious DSP work)? Single-tempo unlocks F2 Pendulum; bank version unlocks the literal-import categories that ProductStrategy filter rejects anyway. **Recommend single-tempo first.**
2. **Centre-origin policy interpretation (Geometry #6 #10):** is centre-origin a structural invariant, or a default with audio-driven exception zones? Two Geometry categories (#6 Circular topology, #10 Asymmetric drift origin) intentionally depart from centre-origin — gated to ≤±20 LED departure for #10 — and need ratification before V1.0.
3. **Framebuffer LPF integration mode (Infra #2):** is the global LPF a mandatory RendererActor pass that ALL effects inherit, or a per-effect opt-in? Mandatory eliminates per-effect persistence boilerplate but risks double-trails on existing self-trailing effects. Per-effect opt-in is safer but loses the "uniform softness knob" UX win.
4. **ControlBus extension list (Theme 6 / SSA-AudioDriver):** ship which of the 7 proposed audio features (`spectralCentroid`, `spectralFlatness`, `pitchHz`/`pitchConfidence`, `formantF1`/`F2`, modulation-spectrum tremolo, voice/music probability, chroma derivative) for V1.0? AudioDriver SSA flags 5 categories as pure-render-side (no extension needed); AudioActor extension cost spans 12 to 120 lines per feature.
5. **Dual-strip exploitation budget (Theme 4 / K1Specific):** how aggressively should V1.0 lean on dual-strip-only effects? F5 Reflective Twin is enforced; #6 Cross-Strip Wave Interference is V1.0; but #2 Phase Parallax Depth, #5 Frequency-Spatial Stereo, #11 Chroma-Spatial Pitch Lattice, #12 Phase-Locked Strobe Lattice are all V1.1+ unless prioritised explicitly.
6. **F3 Liquid Stillness curation arbitration:** who decides which 8–12 of the existing 106 ambient programmes survive? ProductStrategy SSA flagged "ruthless curation > generation" but the arbitration process is undefined. Captain decision.

## Headline thesis (one paragraph)

K1 inherits SB's centre-mirror geometry, chromagram colour grammar, and squaring contrast curve. K1 inherits ES's sprite self-feedback, dt-correct LPF, and tempo-phase doctrine. K1 has hardware affordances neither lineage anticipated: a 330 mm Light Guide Plate that physically diffuses pixels into smooth gradients, a dual 2×160 LED strip topology that supports interference / phase / parallax patterns no single-strip device can produce, 16 MB PSRAM that holds 10+ seconds of frame history, and a 120 FPS dt-correct render budget with comfortable headroom. The brainstorm catalogue says: stop porting SB/ES kinematic primitives one by one. Build six small infrastructure pieces (~880 LOC). Layer five high-conviction themes — universal trail, beat-locked decay, continuum dynamics, dual-strip phase patterns, audio interpretation — through a refined Liquid Bloom hero plus tempo-phase Centre Pendulum plus First-Light Ignition opener plus Liquid Stillness ambient state plus Cross-Strip Interference demo-shock-effect. The rest is V1.1 long tail. The kill-list is firm: no rainbow cycling, no DSP-fixture aesthetics, no 64-pendulum-dot screensavers, no neural-tensor visualisers — they collapse the locked positioning ("Music made visible. Liquid Light.") into yet another RGB strip product. K1's defensible visual identity is **liquid centre-origin chiaroscuro on diffused glass** — the only audio-reactive light that looks like a *liquid* rather than a *graph*.

## Appendix — Source SSA outputs

Per-category detail (mechanism, audio integration, persistence parameters, render-time estimates, lineage notes, viability scoring) is preserved in cached SSA outputs. To reload any specific category for implementation planning:

| SSA agent ID | Output file |
|---|---|
| Physics | `/private/tmp/.../tasks/a35c14b375a166bcc.output` |
| AudioDriver | `/private/tmp/.../tasks/a797472436462f7f9.output` |
| Geometry | `/private/tmp/.../tasks/a92a95c6330daeedb.output` |
| Persistence | `/private/tmp/.../tasks/a5795be9e307b5798.output` |
| Composition | `/private/tmp/.../tasks/a2dd422ad30c85b59.output` |
| CrossLineage | `/private/tmp/.../tasks/ac1431e4e287e4dfc.output` |
| K1Specific | `/private/tmp/.../tasks/a12ec6509e9f8e474.output` |
| ProductStrategy | `/private/tmp/.../tasks/ac38d0188df830e8e.output` |

(Outputs are ephemeral system files. If they have rotated, the SSA prompts can be re-dispatched against the canonical taxonomy doc to regenerate.)

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-26 | agent:opus-4.7-1M (orchestrator + 8 SSAs) | Created — synthesis of 8 parallel forensic-creative SSAs across orthogonal generative angles. ~97 raw category proposals collapsed into 6 convergent themes, a 12-item ranked priority shortlist, a 4-item kill-list, and a 6-piece engine-infrastructure roadmap (~880 LOC) that lands V1.0 launch-track. Headline thesis: K1 lifts effects from SB/ES kinematic motion to dynamical motion on a dual-strip LGP medium that single-strip competitors cannot reproduce. |
