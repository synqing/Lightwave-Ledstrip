---
abstract: "Pass 1 of the Codex synergy-topology extraction: K1 visual-pipeline capability domains, interaction classes, prerequisite order, and higher-order synergies grounded in the SB/ES taxonomy and raw SSA outputs."
---

# PASS 1 - DOMAIN REGISTRY

Verification before analysis: all 10 requested sources were readable.

| Source | Lines |
|---|---:|
| `firmware-v3/docs/research/SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md` | 231 |
| `firmware-v3/docs/research/SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md` | 186 |
| `.claude/recovered_ssa_outputs/SSA-Physics-a35c14b375a166bcc.md` | 238 |
| `.claude/recovered_ssa_outputs/SSA-AudioDriver-a797472436462f7f9.md` | 238 |
| `.claude/recovered_ssa_outputs/SSA-Geometry-a92a95c6330daeedb.md` | 318 |
| `.claude/recovered_ssa_outputs/SSA-Persistence-a5795be9e307b5798.md` | 533 |
| `.claude/recovered_ssa_outputs/SSA-Composition-a2dd422ad30c85b59.md` | 311 |
| `.claude/recovered_ssa_outputs/SSA-CrossLineage-ac1431e4e287e4dfc.md` | 262 |
| `.claude/recovered_ssa_outputs/SSA-K1Specific-a12ec6509e9f8e474.md` | 284 |
| `.claude/recovered_ssa_outputs/SSA-ProductStrategy-ac38d0188df830e8e.md` | 163 |

## DOMAIN REGISTRY

[D01] Centre-Origin Radial Geometry
     Level: Level-2 architectural pattern.
     Standalone: Establishes LED 79/80 as the invariant origin; every visual event reads as outward/inward from the physical centre.
     K1 Status: implemented as policy and inherited in SbK1 effects; K1 made SB 4.x `shift_leds_up + mirror_image_downwards` mandatory across effects.
     Lineage: SB 4.x centre-origin geometry is canonical (`shift_leds_up + mirror_image_downwards`, symmetric writes); K1 inherits centre-origin policy. Citations: taxonomy lines 62-67, 157-183.

[D02] Fractional Sprite Transport / Bloom Kernel
     Level: Level-0 engine primitive.
     Standalone: Moves previous-frame pixels with sub-pixel interpolation and persistence, enabling bloom and beat-tunnel trails.
     K1 Status: implemented in `SbK1BloomEffect` lineage, but tuning and global reuse remain strategic.
     Lineage: SB 4.1.1 bloom `draw_sprite`, centre-pair injection, `prog²` end-fade; ES bloom and beat_tunnel use sprite self-feedback. Citations: taxonomy lines 35, 41, 49, 66-68, 86, 183.

[D03] Global Framebuffer LPF / Softness Pass
     Level: Level-0 engine primitive; becomes Level-2 when placed in RendererActor as a post-pass.
     Standalone: Adds dt-correct, mode-agnostic temporal persistence and transition softness to any rendered output.
     K1 Status: absent at renderer level; currently each effect owns persistence or has none.
     Lineage: ES `apply_image_lpf` supplies universal trail and `lpf_drag`; K1 gap explicitly called out. Citations: taxonomy lines 43, 90, 114, 145-146, 187, 213; brainstorm lines 35-40, 142.

[D04] LayerStack Overlap Composition
     Level: Level-2 architectural pattern.
     Standalone: Allows N overlapping full-strip layers with alpha/blend/mask rather than partition-only zones.
     K1 Status: absent; ZoneComposer is partition-based, while brainstorm proposes a ~150 LOC sibling LayerStack.
     Lineage: ES has only limited spectronome confidence overlay; K1 composition SSA generalises this. Citations: brainstorm lines 75-83, 141; SSA-Composition lines 19, 27-36.

[D05] ControlBus Render-Side Audio Reuse
     Level: Level-1 capability.
     Standalone: Lets existing fields such as `bands`, `chroma`, STM, onset bands, saliency, tempo fields and confidence drive visuals without audio-side work.
     K1 Status: partially implemented; substantial fields are published but underused on render side.
     Lineage: SB/ES used narrow drivers: bin energy, vu, chroma, spectral flux and ES tempo/novelty. K1 publishes richer fields. Citations: taxonomy lines 96-108; SSA-AudioDriver lines 11, 203, 216-219; brainstorm lines 85-93.

[D06] Tempo Phase / Beat-Phase Exposure
     Level: Level-1 capability; per-bin tempo bank would be Level-2 audio architecture.
     Standalone: Exposes continuous musical phase, beat confidence and optionally a tempo-bank phase surface to effects.
     K1 Status: partial/conflicted in sources: taxonomy says no ES-style phase-bank visible to effects; AudioDriver SSA says `es_phase01_at_audio_t` and tempo confidence exist. Treat single-phase exposure as partial, ES-style bank as absent.
     Lineage: ES tempo bank is signature; K1 gap and Captain decision are explicit. Citations: taxonomy lines 42, 103-105, 115, 141-144, 186, 212; brainstorm lines 42-52, 143, 152; SSA-AudioDriver lines 97-98.

[D07] Motion-Blur Dot Cache / Sub-Pixel Dot Helper
     Level: Level-0 engine primitive.
     Standalone: Canonicalises previous-position cache plus sub-pixel draw-line motion blur for dots and small particles.
     K1 Status: absent as shared helper; effects implement dot motion individually.
     Lineage: ES `fx_dots[]` cache; SB `draw_dot`/`draw_line`; K1 gap. Citations: taxonomy lines 40, 51, 64, 147, 189-190; brainstorm line 145; SSA-CrossLineage lines 158-168.

[D08] Persistence Helpers / Spatial Diffusion Toolkit
     Level: Level-0 engine primitive set.
     Standalone: Supplies dt-correct decay, array EMA, spatial LPF, heat equation, velocity-anisotropic blur and cross-blend helpers.
     K1 Status: partial; dt helpers exist in some effects, but no consolidated helper toolkit.
     Lineage: Extends ES global LPF beyond temporal decay into chromatic, spatial and event-gated persistence. Citations: brainstorm lines 54-63, 144; SSA-Persistence lines 19, 285, 402, 428, 479, 487.

[D09] Dual-Strip Phase / Interference / Parallax
     Level: Level-1 capability built from D01 plus strip-pair routing.
     Standalone: Uses the two physical emitters as phase-related light sources instead of treating them as identical mirrors or independent zones.
     K1 Status: under-exploited; sources state K1 mostly treats strips as independent zones or mirrored halves.
     Lineage: absent from SB/ES single-strip hardware; K1-specific SSAs identify phase, parallax, frequency stereo, prediction overlay and strobe lattice. Citations: brainstorm lines 65-73, 107-108, 156, 161; SSA-K1Specific lines 24-45, 78-99, 250-272; SSA-ProductStrategy lines 67-77.

[D10] PSRAM Frame History / Time Delay Ring
     Level: Level-0 engine primitive; enables Level-1 time-warp capabilities.
     Standalone: Stores past frames for echo, delayed strip B, optical-flow, time-warp replay and long-context frame composition.
     K1 Status: absent as shared primitive; PSRAM exists and ZoneComposer uses PSRAM patterns, but no general frame ring.
     Lineage: ES has single-frame LPF and sprite self-feedback, not arbitrary taps; K1 has 16 MB PSRAM. Citations: brainstorm line 146; SSA-K1Specific lines 47-76, 132-152, 206-228; SSA-Composition lines 161-179.

[D11] Continuum / Dynamical Physics Fields
     Level: Level-1 capability.
     Standalone: Evolves visual state under heat, fluid, spring, wave, Kuramoto, particle or mass-flow dynamics rather than copy/decay kinematics.
     K1 Status: absent as standard vocabulary.
     Lineage: SB/ES are kinematic; Physics SSA argues K1 can move into dynamical motion within budget. Citations: brainstorm lines 54-63, 121-124; SSA-Physics lines 31-50, 69-73, 207-216; SSA-K1Specific lines 13-22, 101-129.

[D12] Chromagram / Pitch-Structure Spatialisation
     Level: Level-1 capability.
     Standalone: Maps 12-note chroma, harmonic motion and pitch confidence into spatial nodes, constellations, prediction or chord-change events.
     K1 Status: partial; chroma exists and SbK1 builds chromagram, but K1 lacks canonical array LPF and shared dot cache for pitch constellations.
     Lineage: SB chromagram modes, ES pitch raster, K1 gap for LPF and dots; ProductStrategy elevates Pitch Constellations as V1.1 candidate. Citations: taxonomy lines 101, 181, 191; brainstorm lines 114-117, 145; SSA-AudioDriver lines 134-146; SSA-K1Specific lines 230-248.

[D13] Audio Interpretation Extensions
     Level: Level-1 capability; some additions are Level-0 scalars.
     Standalone: Adds or exploits spectral centroid, flatness, ZCR, HPS pitch confidence, formants, tremolo lock, voice/music probability, inter-band correlation and classifiers.
     K1 Status: partial; five AudioDriver proposals are render-side only, several are trivial ControlBus extensions, two are ambitious.
     Lineage: SB/ES never extracted spectral shape, pitch confidence, modulation-domain structure, inter-band relationships or higher-level classifiers. Citations: SSA-AudioDriver lines 26-32, 38, 50, 72-82, 110-122, 149-161, 203-219; brainstorm lines 85-93, 155.

[D14] Mode-State / Long-Context Mood Evolution
     Level: Level-2 architectural pattern.
     Standalone: Allows behaviour to evolve across 5-120 s windows, selecting mode families, ambient/reactive state and story arcs rather than responding only frame-by-frame.
     K1 Status: partial; show playback and presets exist, but automatic long-context visual evolution is absent.
     Lineage: not in SB/ES; K1Specific proposes long-window mood drift; Composition proposes Mood Arc and Story-Arc; ProductStrategy requires ambient credibility. Citations: SSA-Composition lines 105-133; SSA-K1Specific lines 174-204; brainstorm lines 106, 121-124, 157.

[D15] Product Signature / Restraint Filter
     Level: Level-2 strategic pattern.
     Standalone: Filters the technical possibility space into K1's differentiated identity: liquid centre-origin chiaroscuro, no rainbows, no graphs, no fragmented analyser aesthetics.
     K1 Status: policy/strategy layer, not code.
     Lineage: ProductStrategy rejects literal tempo-bank pendulums, neural tensors, test-scope modes and kaleidoscope. Citations: brainstorm lines 126-133, 159-161; SSA-ProductStrategy lines 108-128, 134-137.

[D16] First-Light / Event-Triggered Choreography
     Level: Level-1 capability.
     Standalone: Uses one-shot, timed centre-origin choreography for boot, first audio after silence and high-salience moments.
     K1 Status: absent as a formal primitive; ProductStrategy ranks it V1.0 because effort is low and demo value is high.
     Lineage: neither SB nor ES has cinematic boot/wake choreography. Citations: brainstorm lines 103-105; SSA-ProductStrategy lines 79-93, 108-110.

[D17] External Control / Tab5 Spatial Turbulence [divergent]
     Level: Level-1 capability.
     Standalone: Treats external encoder deltas as spatial force injections or turbulence fields composited with audio-reactive base modes.
     K1 Status: protocol-dependent and not part of synthesis themes; raised only by K1Specific SSA.
     Lineage: no SB/ES equivalent; K1 AP/WebSocket/Tab5 ecosystem only. Citations: SSA-K1Specific lines 132-152.

[D18] Perceptual Hardware Calibration / Camera-Ready Output [divergent]
     Level: Level-2 architectural pattern.
     Standalone: Governs brightness, perceived smoothness, LGP diffusion, camera mode and visual legibility under hardware/camera constraints.
     K1 Status: partial; camera mode exists in protocol, but synergy corpus treats perception as implicit rather than a first-class domain.
     Lineage: not SB/ES; inferred from product and protocol constraints. Citations: WebSocket contract camera mode read, protocol lines from `k1-ws-contract.yaml` around camera mode; ProductStrategy lines 128-137.

## INTERACTION MATRIX

Coverage rule: pairs not listed below were inspected and classified as additive or neutral under the current evidence. The matrix below records the non-additive and strategically relevant edges; purely additive pairings do not enter Pass 2's graph.

[D01 x D02] -> Interaction class: multiplicative
     Emergent capability: Liquid Bloom becomes a K1-specific centre-pair tide instead of generic sprite scroll.
     Prerequisite: D01 must constrain D02's injection and propagation before bloom is brand-correct.
     Dual-strip amplification: yes - mirrored centre injection lets both strips read as one plate.
     Evidence: taxonomy lines 35, 66-68, 182-183; ProductStrategy lines 19-23.

[D01 x D07] -> Interaction class: multiplicative
     Emergent capability: Dot motion becomes centre-mirrored musical motion rather than arbitrary dot traces.
     Prerequisite: D07 can exist alone, but D01 makes it K1-compliant.
     Dual-strip amplification: yes - paired dots can move as a physical twin.
     Evidence: taxonomy lines 64, 168-170, 189-190; SSA-CrossLineage lines 158-168.

[D01 x D09] -> Interaction class: combinatorial
     Emergent capability: The dual-strip pair becomes a geometry engine for parallax, interference and stereo information while preserving the centre-origin contract.
     Prerequisite: D01 must define phase references before D09 is legible.
     Dual-strip amplification: yes - this is impossible on single-strip devices.
     Evidence: brainstorm lines 65-73; SSA-K1Specific lines 78-99.

[D01 x D11] -> Interaction class: multiplicative
     Emergent capability: Heat, wave and spring fields inherit a visible gravitational centre instead of becoming generic field simulations.
     Prerequisite: D01 first; D11 then uses centre as source, node, antinode or force well.
     Dual-strip amplification: yes - coupled fields can run across strips without losing the centre.
     Evidence: brainstorm lines 54-63; SSA-K1Specific lines 13-22, 101-129.

[D02 x D03] -> Interaction class: multiplicative
     Emergent capability: Bloom gets both per-mode sprite transport and global ambient softness/cross-fade; however double-trail risk is real.
     Prerequisite: D02 can land first; D03 needs per-effect opt-out before global rollout.
     Dual-strip amplification: neutral.
     Evidence: taxonomy lines 43, 86, 90, 187, 213; brainstorm line 154.

[D02 x D06] -> Interaction class: multiplicative
     Emergent capability: Beat-phase-gated sprite injection; bloom breathes in musical phase rather than continuous amplitude.
     Prerequisite: D06 must expose phase or beat-confidence before D02 can gate inject precisely.
     Dual-strip amplification: yes - centre-pair injection on both strips makes phase visible as a plate event.
     Evidence: taxonomy lines 49, 142; brainstorm lines 42-52; SSA-CrossLineage lines 148-156.

[D02 x D10] -> Interaction class: multiplicative
     Emergent capability: Echo bloom, time-warp bloom and delayed-strip bloom become possible.
     Prerequisite: D10 after a base renderer/effect exists.
     Dual-strip amplification: yes - strip B can be a delayed optical twin.
     Evidence: brainstorm line 146; SSA-K1Specific lines 47-76; SSA-Composition lines 161-179.

[D03 x D04] -> Interaction class: combinatorial
     Emergent capability: Each layer can have shared softness and transition memory; layer combinations stop looking like hard overdraw.
     Prerequisite: either can land first, but combined value requires both.
     Dual-strip amplification: neutral to yes when layers encode strip-specific phase.
     Evidence: brainstorm lines 35-40, 75-83, 141-142.

[D03 x D05] -> Interaction class: multiplicative
     Emergent capability: Existing audio fields modulate global softness, silence decay, stillness and reactive trail without per-effect rewrites.
     Prerequisite: D03 first, D05 drives it.
     Dual-strip amplification: neutral.
     Evidence: brainstorm lines 85-93, 142; SSA-Persistence lines 239-251.

[D03 x D08] -> Interaction class: multiplicative
     Emergent capability: Global LPF becomes one member of a broader persistence alphabet: phosphor, heat, velocity blur, beat-gated decay.
     Prerequisite: D08 should shape D03 implementation so it is not a one-off pass.
     Dual-strip amplification: neutral.
     Evidence: SSA-Persistence lines 479-487; brainstorm lines 139-145.

[D03 x D14] -> Interaction class: multiplicative
     Emergent capability: Ambient stillness and long-context state can persist gracefully across silence and mode changes.
     Prerequisite: D03 must exist for Liquid Stillness; D14 selects when to use it.
     Dual-strip amplification: yes if stillness preserves Reflective Twin.
     Evidence: brainstorm lines 35-40, 106, 154, 157; SSA-ProductStrategy lines 43-54.

[D04 x D05] -> Interaction class: combinatorial
     Emergent capability: Existing ControlBus dimensions become independent layer-selectors, masks, gains and roles; mode count grows by layer combinations.
     Prerequisite: D04 must exist before D05 can become composition alphabet rather than per-effect knobs.
     Dual-strip amplification: yes when layers encode different strip roles.
     Evidence: brainstorm lines 75-93, 141; SSA-Composition lines 27-36.

[D04 x D06] -> Interaction class: multiplicative
     Emergent capability: Beat-quantised two-step, phase-aware crossfades and confidence-gated layers.
     Prerequisite: D06 supplies phase/confidence; D04 supplies the layer space.
     Dual-strip amplification: yes for polyrhythm or strip-lead/lag layers.
     Evidence: taxonomy line 143; brainstorm lines 42-52; SSA-Composition lines 89-104.

[D04 x D09] -> Interaction class: combinatorial
     Emergent capability: Dual-strip can be represented as layers, not just hardware outputs: phase parallax, stereo spectrum, prediction overlay.
     Prerequisite: D09 can produce raw dual-strip effects; D04 makes them composable with base modes.
     Dual-strip amplification: yes - core of the edge.
     Evidence: brainstorm lines 65-83; SSA-K1Specific lines 24-45, 230-272.

[D04 x D10] -> Interaction class: combinatorial
     Emergent capability: Echo Composer, time mirror, delayed strips and frame-history overlays.
     Prerequisite: D10 supplies taps; D04 blends them.
     Dual-strip amplification: yes - strip B can be a delayed layer.
     Evidence: brainstorm line 146; SSA-Composition lines 161-179; SSA-K1Specific lines 47-76.

[D04 x D13] -> Interaction class: multiplicative
     Emergent capability: Classifiers and spectral-shape fields decide layer branches, masks and role separation.
     Prerequisite: D04 first for maximum value; D13 can still drive single effects.
     Dual-strip amplification: yes when one strip carries prediction/context and one carries live state.
     Evidence: brainstorm lines 85-93, 155; SSA-AudioDriver lines 149-161.

[D05 x D08] -> Interaction class: multiplicative
     Emergent capability: Existing audio fields drive persistence behaviour: RMS controls decay, onset gates cross-blends, STM drives shimmer, saliency drives memory.
     Prerequisite: D08 helpers first if reuse is desired; otherwise each effect reinvents it.
     Dual-strip amplification: neutral.
     Evidence: SSA-Persistence lines 239-275, 479; SSA-AudioDriver lines 72-82.

[D05 x D11] -> Interaction class: multiplicative
     Emergent capability: Dynamics become audio-forced systems instead of screensavers.
     Prerequisite: D11 simulations need D05 drivers to stay musically connected.
     Dual-strip amplification: yes when two strips are coupled systems.
     Evidence: SSA-Physics lines 31-50, 207-216; brainstorm lines 54-63.

[D05 x D12] -> Interaction class: multiplicative
     Emergent capability: Chroma and harmonic saliency become pitch constellations, harmonic comets and pitch-class velocity fields.
     Prerequisite: D05 supplies fields; D12 supplies mapping.
     Dual-strip amplification: yes if one strip carries current chord and the other carries predicted/echoed chord.
     Evidence: taxonomy lines 101, 181; SSA-AudioDriver lines 134-146; SSA-K1Specific lines 230-248.

[D05 x D13] -> Interaction class: multiplicative
     Emergent capability: Render-side consumption becomes richer without changing the composer: tonality, voice/music, spectral shape, tremolo and polyphony affect visuals.
     Prerequisite: D05 is the existing surface; D13 selectively extends it.
     Dual-strip amplification: optional.
     Evidence: SSA-AudioDriver lines 203-219; brainstorm lines 85-93.

[D06 x D08] -> Interaction class: multiplicative
     Emergent capability: Decay and diffusion can be beat-locked or tempo-phase-modulated, making memory musical.
     Prerequisite: D06 first or synthetic phase from beat timestamps.
     Dual-strip amplification: yes for phase-offset decay between strips.
     Evidence: brainstorm lines 42-52; SSA-Persistence lines 191-200, 428-441.

[D06 x D09] -> Interaction class: combinatorial
     Emergent capability: Cross-strip polyrhythm, strobe lattice and tempo-beating interference.
     Prerequisite: D06 supplies reliable phase; D09 supplies physical dual oscillators.
     Dual-strip amplification: yes - impossible on single-strip devices.
     Evidence: brainstorm lines 65-73; SSA-K1Specific lines 250-272.

[D06 x D15] -> Interaction class: multiplicative, but constrained
     Emergent capability: Tempo phase becomes signature pendulum and proof-of-listening; literal N-pendulum ports are rejected by product filter.
     Prerequisite: D15 must filter D06 usage.
     Dual-strip amplification: yes - two mirrored pendulums on LGP.
     Evidence: brainstorm lines 103-104, 126-133; SSA-ProductStrategy lines 31-40.

[D07 x D12] -> Interaction class: multiplicative
     Emergent capability: Motion-blur cached chromagram dots and Pitch Constellations.
     Prerequisite: D07 helper must land before D12 feels polished.
     Dual-strip amplification: yes - mirrored pitch petals can read as one object.
     Evidence: brainstorm lines 114-117, 145; SSA-CrossLineage lines 158-168.

[D08 x D11] -> Interaction class: multiplicative
     Emergent capability: Dynamical fields become reusable primitives rather than one-off effects; heat/diffusion helpers serve both persistence and physics.
     Prerequisite: D08 before broad D11 adoption.
     Dual-strip amplification: yes if coupled strip fields use common helper semantics.
     Evidence: brainstorm lines 54-63, 144; SSA-Persistence lines 285-346.

[D09 x D10] -> Interaction class: combinatorial
     Emergent capability: Inter-strip phase coherence by delayed frame reads; strip B becomes strip A in the recent past.
     Prerequisite: D10 supplies history; D09 supplies physical meaning.
     Dual-strip amplification: yes - this edge is exclusively K1.
     Evidence: SSA-Geometry lines 13 category in source output; SSA-K1Specific lines 24-45.

[D09 x D12] -> Interaction class: combinatorial
     Emergent capability: One strip can show current chroma while the other shows predicted chroma; pitch becomes dual-axis information.
     Prerequisite: D12 supplies pitch lattice; D09 assigns strip roles.
     Dual-strip amplification: yes - single-strip cannot show current and predicted chroma without conflict.
     Evidence: SSA-K1Specific lines 230-248.

[D10 x D14] -> Interaction class: multiplicative
     Emergent capability: Long-context mood drift, time-warp replay and story arcs use stored history rather than just EMAs.
     Prerequisite: D10 first for frame-history; D14 can start with scalar windows.
     Dual-strip amplification: optional.
     Evidence: brainstorm line 146; SSA-K1Specific lines 47-76, 174-204.

[D11 x D15] -> Interaction class: multiplicative, constrained
     Emergent capability: Physics is valuable when it supports liquid centre-origin chiaroscuro; unconstrained simulations risk developer-wow.
     Prerequisite: D15 filters which D11 proposals enter launch path.
     Dual-strip amplification: yes, especially spring/wave coupling.
     Evidence: SSA-Physics lines 207-216; brainstorm lines 121-133, 159-161.

[D14 x D15] -> Interaction class: multiplicative
     Emergent capability: Liquid Stillness and Silent Spectacle are curated states rather than a random ambient catalogue.
     Prerequisite: D15 sets the selection criteria; D14 enforces temporal behaviour.
     Dual-strip amplification: yes - every state must preserve Reflective Twin.
     Evidence: brainstorm lines 106, 115, 157, 159-161; SSA-ProductStrategy lines 43-54.

[D15 x D16] -> Interaction class: multiplicative
     Emergent capability: First-Light Ignition becomes the product's introduction, not just another effect.
     Prerequisite: D15 defines tone; D16 executes it.
     Dual-strip amplification: yes - centre spark must expand through the whole LGP as one object.
     Evidence: brainstorm lines 103-105; SSA-ProductStrategy lines 79-93.

[D17 x D04] -> Interaction class: multiplicative, divergent
     Emergent capability: Tab5 turbulence becomes an overlay layer over audio-reactive base modes rather than a separate control mode.
     Prerequisite: D04 first for clean integration.
     Dual-strip amplification: yes if knobs map to strip positions or force fields.
     Evidence: SSA-K1Specific lines 132-152.

[D18 x D03] -> Interaction class: multiplicative, divergent
     Emergent capability: Camera-ready softness, brightness caps and global LPF become part of output correctness, not a capture afterthought.
     Prerequisite: D18 requirements should shape D03's parameter ranges.
     Dual-strip amplification: neutral.
     Evidence: WebSocket camera mode in `k1-ws-contract.yaml`; ProductStrategy lines 128-137.

## HIGHER-ORDER SYNERGIES

[D03 x D04 x D05] -> GIVEN infrastructure layer
     Emergent capability that requires ALL three: mode-agnostic softness plus layer composition plus audio-field reuse; current modes become a combinatorial audio-driven layer space.
     Why it collapses without any single member: without D03 layers are hard and brittle; without D04 there is no combinatorial surface; without D05 layers are static visual stacks.
     Dual-strip implication: amplified when layers carry strip-specific phase or role.
     Evidence: brainstorm lines 35-40, 75-93, 141-142.

[D01 x D06 x D09] -> Dual-strip tempo geometry
     Emergent capability that requires ALL three: phase-locked strobe lattice, mirrored pendulum, polyrhythm and interference where tempo phase is physically visible across two emitters while remaining centre-origin.
     Why it collapses without any single member: without D01 it violates K1 grammar; without D06 it is arbitrary animation; without D09 it is reproducible on a single strip.
     Evidence: brainstorm lines 42-73; SSA-K1Specific lines 250-272.

[D04 x D09 x D10] -> Time-delayed Reflective Twin
     Emergent capability that requires ALL three: strip B can be a delayed, composited optical twin of strip A while other layers remain live.
     Why it collapses without any single member: D10 gives history, D09 gives physical meaning, D04 blends without replacing.
     Evidence: brainstorm line 146; SSA-K1Specific lines 24-45; SSA-Composition lines 161-179.

[D05 x D07 x D12] -> Pitch constellation with motion memory
     Emergent capability that requires ALL three: chroma becomes stable spatial music geometry with smooth sub-pixel motion rather than flickering bars.
     Why it collapses without any single member: without D05 no chroma/saliency signal; without D07 no polished motion trail; without D12 no pitch-space mapping.
     Evidence: taxonomy lines 101, 181, 189-191; SSA-CrossLineage lines 158-168; brainstorm lines 114-117.

[D01 x D08 x D11] -> LGP-native physics field
     Emergent capability that requires ALL three: dt-correct heat/wave/spring fields with centre as source and LGP as perceptual smoother.
     Why it collapses without any single member: without D01 fields lose product grammar; without D08 they are one-off unstable code; without D11 they are just blur.
     Evidence: brainstorm lines 54-63, 144; SSA-K1Specific lines 13-22, 101-129.

[D03 x D14 x D15] -> Liquid Stillness
     Emergent capability that requires ALL three: K1 remains premium and intentional during silence.
     Why it collapses without any single member: without D03 stillness lacks liquid softness; without D14 it has no temporal state; without D15 it becomes generic screensaver.
     Evidence: brainstorm lines 35-40, 106, 154, 157, 159-161; SSA-ProductStrategy lines 43-54.

[D02 x D06 x D15] -> Hero bloom proof-of-listening
     Emergent capability that requires ALL three: Liquid Bloom breathes with musical phase and stays within restrained visual language.
     Why it collapses without any single member: without D02 no hero tide; without D06 it is only amplitude-following; without D15 it can become generic reactive lighting.
     Evidence: brainstorm lines 101-108, 126-133; SSA-ProductStrategy lines 19-40.

[D04 x D13 x D14] -> Context-aware mode orchestration
     Emergent capability that requires ALL three: voice/music, tonality and energy context select or blend mode families over long windows.
     Why it collapses without any single member: D13 classifies, D14 remembers, D04 blends.
     Evidence: brainstorm lines 85-93, 155; SSA-AudioDriver lines 149-161; SSA-Composition lines 105-133.

[D01 x D15 x D16] -> First-Light product ritual
     Emergent capability that requires ALL three: boot/first-audio choreography introduces K1's centre-origin identity.
     Why it collapses without any single member: no centre grammar, no brand restraint or no event choreographer each reduce it to a test pattern.
     Evidence: brainstorm lines 103-105; SSA-ProductStrategy lines 79-93, 128-137.

[D04 x D05 x D17] -> External performance compositor [divergent]
     Emergent capability that requires ALL three: Tab5 knob movement becomes a spatial force layer over live audio interpretation.
     Why it collapses without any single member: without D04 it interrupts effects; without D05 it is not audio-reactive; without D17 there is no physical performance input.
     Evidence: SSA-K1Specific lines 132-152; SSA-Composition lines 27-36.

## Blind Spots Noted

Coverage is deepest for persistence, composition, tempo phase and dual-strip geometry because those have explicit synthesis convergence and raw SSA detail. Coverage is shallower for manufacturing variance, camera exposure and user-perception calibration because the supplied corpus only implies these through product strategy and protocol surfaces.

---

**Document Changelog**

| Date | Author | Change |
|---|---|---|
| 2026-04-26 | agent:codex | Created Pass 1 domain registry, non-additive interaction matrix, and higher-order synergy list from the 10-source protocol corpus. |
