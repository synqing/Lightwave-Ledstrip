# K1 Visual Pipeline — Synergy Topology Extraction Protocol (Codex)

---

## Operational Wrapper — Read Before Executing

### What This Document Is

This is a 4-pass strategic analysis protocol. It is NOT a task list. It is a structured analytical framework that must be executed sequentially — each pass consumes the output of the previous pass. Attempting to shortcut, merge passes, or jump to conclusions will produce a flat priority list instead of the synergy topology this protocol is designed to surface. That is a failure mode.

### Project Context (Essential Background)

**K1-Lightwave** is an ESP32-S3 audio-reactive LED controller driving 320 WS2812 LEDs (2×160 strips) edge-injected into an acrylic Light Guide Plate. All effects originate from LED indices 79/80 at the physical centre and propagate outward. The device captures audio via I2S microphone, extracts beat/tempo/harmonic features in real time, and maps them to visual effects at 120 FPS.

The K1's visual pipeline was architecturally derived from two upstream open-source projects:
- **Sensory Bridge (SB)** — 5 firmware versions: 3.0.0, 3.1.0, 3.2.0, 4.0.0, 4.1.1
- **Emotiscope (ES)** — 4 firmware versions: 1.0, 1.1, 1.2, 2.0

A forensic audit of all 9 upstream versions has revealed critical reconstruction fidelity failures — cases where K1's pipeline was built on incorrect assumptions about what SB/ES actually does. This protocol maps the full capability space and its synergy topology to inform strategic development sequencing.

### Execution Model

**You are a single agent executing all 4 passes sequentially.** Do not attempt to spawn sub-agents or parallelise. Process each pass completely, write its output to disk, then proceed to the next pass.

This is a RESEARCH task. No source code will be modified. All outputs are analytical documents.

**Context management:** This protocol requires processing 10 source documents across 4 analytical passes. That is a significant cognitive load. To maintain quality:

1. **Read all 10 source documents before starting Pass 1.** Build a mental model of the full system before decomposing it.
2. **Write each pass to disk before starting the next.** This forces consolidation and creates a recoverable checkpoint.
3. **After each pass output is written to disk, treat earlier source reads as evictable.** Re-read specific sources on demand for later passes; do not rely on retained memory of files read more than one pass ago. If you find yourself uncertain about a source claim, re-read the source rather than reconstruct from memory. Fabricating from fading context is the single most likely failure mode of this protocol.
4. **If you notice your analysis becoming shallow or repetitive,** state this explicitly in the output rather than papering over it. Honest coverage gaps are more valuable than confident shallow coverage.
5. **Prioritise depth over breadth.** If you cannot cover all domains at equal depth across all 4 passes, it is better to deeply analyse 70% of domains than to superficially cover 100%.

### Dual-LLM Awareness

This same protocol is being run independently on Claude Code Max (Opus). You do not need to account for this in your analysis — just produce the best output you can. The outputs will be compared afterward by Captain. Where the two runs converge, confidence is high. Where they diverge, the divergence is signal — it identifies edges and domains where the underlying reasoning is model-dependent rather than evidence-dependent.

**This means your independent perspective is valuable precisely because it is independent.** Do not try to guess what Claude would say. Produce your own analysis from the source material. Strong, defensible disagreement with consensus is more valuable than weak agreement. Where you would bet against the field, say so. Where you'd hedge, hedge — but flag the hedge explicitly.

### Source Documents — File Locations

All source documents are in the project workspace. You have filesystem access.

**Primary sources:**
- `firmware-v3/docs/research/SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md`
- `firmware-v3/docs/research/SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md`

**Secondary sources (per-axis research agent raw outputs):**
- `.claude/recovered_ssa_outputs/SSA-Physics-a35c14b375a166bcc.md`
- `.claude/recovered_ssa_outputs/SSA-AudioDriver-a797472436462f7f9.md`
- `.claude/recovered_ssa_outputs/SSA-Geometry-a92a95c6330daeedb.md`
- `.claude/recovered_ssa_outputs/SSA-Persistence-a5795be9e307b5798.md`
- `.claude/recovered_ssa_outputs/SSA-Composition-a2dd422ad30c85b59.md`
- `.claude/recovered_ssa_outputs/SSA-CrossLineage-ac1431e4e287e4dfc.md`
- `.claude/recovered_ssa_outputs/SSA-K1Specific-a12ec6509e9f8e474.md`
- `.claude/recovered_ssa_outputs/SSA-ProductStrategy-ac38d0188df830e8e.md`

**Verification step:** Before beginning Pass 1, confirm that all 10 source documents are readable. List each filename and its line count. If any source is missing or unreadable, STOP and report to Captain. Do not proceed with partial sources.

### Output Destination

All outputs land in `firmware-v3/docs/research/synergy-topology/codex/`:

- `PASS_1_DOMAIN_REGISTRY.md`
- `PASS_2_SYNERGY_TOPOLOGY.md`
- `PASS_3_KILL_ORDER.md`
- `PASS_4_ADVERSARIAL_STRESS_TEST.md`
- `EXECUTIVE_SUMMARY.md` (generated after Pass 4 — the Captain-facing artifact and the most-read output. Must contain these sections in order:
  1. **Headline thesis** — 1 sentence capturing the single most important strategic insight
  2. **Top-3 hubs identified** — with confidence rating [high/medium/low] for each
  3. **Recommended kill order** — the Pass 3 winner with phase names and one-line descriptions
  4. **Top-3 surprises vs the GIVEN** — findings that challenge or extend the initial infrastructure layer assumption
  5. **Open questions Captain must resolve** — decisions this analysis cannot make, only surface
  6. **Predicted divergence points vs the other LLM run** — flag where this run took a position that may diverge from the parallel run, with reasoning for why the position is held anyway)

Each pass output must be written to disk before the next pass begins. This ensures recoverability — if a pass fails or produces poor output, it can be re-run without losing prior work.

### Guardrails

1. **No premature optimisation.** Do not skip to implementation recommendations. This protocol is about mapping the possibility space, not selecting from it. Selection is Captain's prerogative.

2. **No confidence inflation.** If an interaction class (additive/multiplicative/combinatorial) is uncertain, say so. A confidently wrong classification is worse than an honestly uncertain one — it corrupts every downstream pass.

3. **No synthesis without citation.** Every claim about a domain's behaviour, an interaction's class, or a synergy edge's existence must cite a specific source document, mode name, parameter, formula, or source line. "Based on the research" is not a citation. "SB 4.1.1 bloom share = 1/6.0 (SSA-CrossLineage, §4.1.1 Parameter Drifts)" is.

4. **No scope creep into implementation.** This protocol produces a strategic map and sequencing recommendation. It does NOT produce implementation plans, code, architecture docs, or PRs. Those are separate tasks that follow after Captain reviews and approves the topology.

5. **Preserve the graph structure.** At every stage, resist the pull toward linear narrative. The output formats are structured for a reason — they prevent graph-structured thinking from collapsing into prose. If you find yourself writing paragraphs where the format specifies structured entries, you are drifting. Correct course.

6. **Flag your own blind spots.** If you notice that your analysis is disproportionately deep on some domains and shallow on others, call it out explicitly. Uneven coverage is a signal that should be surfaced, not hidden.

7. **Respect the dual-strip forcing function.** Every synergy edge, every hub, every kill order phase must be evaluated against the dual-strip centre-origin LGP geometry. This is not optional and not an afterthought — it is the single most differentiating physical characteristic of the K1 and the foundation of its competitive moat. If you complete a section without mentioning dual-strip implications, go back and add them.

### Completion Criteria

The protocol is complete when:
- All 4 pass output files exist in the output destination
- The Executive Summary has been generated
- Every pass output conforms to its specified output format
- No pass contains unresolved "[TODO]" or "[TBD]" placeholders
- Captain has been notified that outputs are ready for review

---

## Context Preamble

You are a strategic systems architect analysing the K1-Lightwave visual pipeline. A forensic audit of all 9 upstream firmware versions (SB 3.0.0, 3.1.0, 3.2.0, 4.0.0, 4.1.1; ES 1.0, 1.1, 1.2, 2.0) has produced a multi-layered research corpus. You have access to both the synthesised conclusions AND the raw per-axis reasoning that produced them.

### Source Hierarchy

**Primary sources — read in full before proceeding. Do not summarise. Internalise the architecture.**

1. `SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md` — per-mode motion-mechanic enumeration with full source-line citations across all 9 releases
2. `SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md` — ~97 candidate motion-mechanic categories distilled into 6 convergent themes

**Secondary sources — per-axis SSA raw outputs. Consult for depth, divergent ideas, intermediate reasoning, and proposals that synthesis may have discarded.**

The brainstorm catalogue was produced by 8 parallel SSAs, each operating along an orthogonal generative axis. Their full outputs — including intermediate reasoning, dead ends, caveats, and ideas that did NOT converge into the final 6 themes — are preserved here:

3. `SSA-Physics-a35c14b375a166bcc.md` — animation physics, spring/wave/PDE dynamics, inertial systems
4. `SSA-AudioDriver-a797472436462f7f9.md` — unexplored audio feature drivers, ControlBus extensions, signal reinterpretation
5. `SSA-Geometry-a92a95c6330daeedb.md` — geometric/topological extrapolation, spatial mapping, coordinate transforms
6. `SSA-Persistence-a5795be9e307b5798.md` — temporal persistence, trail/decay mechanics, framebuffer manipulation
7. `SSA-Composition-a2dd422ad30c85b59.md` — layer composition, blend modes, overlap rendering, mode interaction
8. `SSA-CrossLineage-ac1431e4e287e4dfc.md` — SB×ES cross-lineage fusion, uninherited capabilities, lineage gap analysis
9. `SSA-K1Specific-a12ec6509e9f8e474.md` — K1-specific affordance mining, dual-strip exploitation, centre-origin mechanics
10. `SSA-ProductStrategy-ac38d0188df830e8e.md` — product-strategy filter, viability assessment, market defensibility, Founder's Edition priorities

**Why the secondary sources matter:** Synthesis merges 8 agents into 6 convergent themes. By definition, it discards divergent ideas — proposals that only ONE SSA raised, dead ends that might not be dead, caveats that got smoothed over, and inter-axis tensions that got resolved by majority rather than by rigour. These discarded signals are prime candidates for the hidden synergy layers this protocol is designed to surface. Treat singleton proposals with heightened attention, not reduced attention.

### Hard Constraints (K1 Physical Reality)

- Centre-origin topology: indices 79/80, dual 2×160 LED strips on edge-lit light guide plates
- 320 WS2812B LEDs total
- ESP32-S3 processing, 120 FPS target, 2.0ms render ceiling
- No heap allocation in render path
- All smoothing must be dt-correct
- Sub-8ms audio-to-visual latency

### Known Infrastructure Layer (Already Identified)

The following synergy surface has been identified as the most apparent layer. Treat this as GIVEN — your job begins beyond it:

- **Framebuffer LPF** (system-level persistence primitive) → transforms every mode's temporal character from hard-pixel to fluid/liquid
- **LayerStack** (~150 LOC composition engine) → converts mode count from additive to combinatorial
- **ControlBus audio driver reuse** (5 categories from already-computed fields) → zero audio-side cost, maximum render-side expression when LayerStack exists

These three form a multiplicative chain: LPF × LayerStack × ControlBus ≫ LPF + LayerStack + ControlBus.

---

## PASS 1 — Domain Decomposition & Interaction Mapping

### Objective

Decompose the entire K1 visual pipeline capability space into discrete domains, then map every pairwise and higher-order interaction between them.

### Instructions

1. From the taxonomy and brainstorm catalogue, extract every discrete capability domain. A domain is an independently modifiable subsystem or primitive. Examples include but are not limited to: temporal persistence, spatial mapping, audio feature extraction, colour interpretation, geometric transformation, composition/layering, beat/tempo synchronisation, dual-strip exploitation, parameter modulation, mode transition logic, session/phrase-level temporal awareness, long-context state evolution (how visual behaviour changes over minutes/songs/sessions, not just frames).

   **Abstraction-level tagging.** Each domain must be tagged with its abstraction level: Level-0 (engine primitive — a leaf helper consumable by ≥1 effect), Level-1 (capability — an emergent behaviour composed of ≥1 primitive), or Level-2 (architectural pattern — a system-level structure that organises capabilities). When building the interaction matrix, edges that cross abstraction levels are not prohibited but must be flagged for extra scrutiny — a "multiplicative" cross-level edge may be a category error masquerading as synergy.

   **Cross-reference with SSA raw outputs.** After building the initial domain list from the synthesised documents, scan each of the 8 SSA outputs for capability domains that were proposed but did not survive into the synthesis. Add these as candidate domains with a `[divergent]` flag. A domain that only one SSA proposed is not automatically low-value — it may represent a perspective that no other axis had visibility into.

2. For each domain, document:
   - What it does in isolation (standalone value)
   - What it currently looks like in K1 (implemented, partially implemented, or absent)
   - Its upstream lineage (which SB/ES versions implemented it and how)

3. Build an **interaction matrix**. For every pair of domains (A, B), evaluate:
   - Does A×B produce a capability that neither A nor B produces alone? If yes, describe it.
   - Is the interaction **additive** (A+B), **multiplicative** (A×B > A+B), or **combinatorial** (A×B opens an entirely new possibility space)?
   - Does one domain need to exist before the other becomes valuable? (Prerequisite sequencing)

4. Extend beyond pairwise. Identify **triadic and higher-order synergies** — combinations of 3+ domains where the emergent capability only appears when ALL are present. The known infrastructure layer (LPF × LayerStack × ControlBus) is an example of a triadic synergy. Find others. Cap at top 10 triadic synergies. Each must explain why it is qualitatively different from any of its pairwise sub-synergies — if removing one member merely reduces the effect rather than eliminating the emergent capability, it is not a true triadic synergy. Stop at order-3 unless an order-4 synergy is qualitatively distinct from all of its order-3 sub-synergies.

### Output Format — Pass 1

```
DOMAIN REGISTRY
───────────────
[D01] Domain Name
     Standalone: ...
     K1 Status: [implemented | partial | absent]
     Lineage: ...

INTERACTION MATRIX
──────────────────
[D01 × D03] → Interaction class: [additive | multiplicative | combinatorial]
     Emergent capability: ...
     Prerequisite: D01 must land before D03 becomes valuable
     Dual-strip amplification: [yes — why | no | neutral]

HIGHER-ORDER SYNERGIES
──────────────────────
[D01 × D03 × D07] → ...
     Emergent capability that requires ALL three: ...
     Why it collapses without any single member: ...
```

---

## PASS 2 — Synergy Topology & Hidden Layer Extraction

### Objective

Take the interaction matrix from Pass 1 and perform topological analysis to surface non-obvious synergy layers — combinations and dependency chains that are invisible when domains are evaluated independently.

### Instructions

1. **Graph construction.** Model the interaction matrix as a directed graph:
   - Nodes = domains
   - Edges = multiplicative or combinatorial interactions (ignore purely additive interactions)
   - Edge weight = estimated multiplier class
   - Edge direction = prerequisite sequencing (A must precede B)

2. **Hub identification.** Which domains have the highest number of multiplicative outbound edges? These are **platform domains** — infrastructure that amplifies everything connected to it. The known infrastructure layer contains three hubs (LPF, LayerStack, ControlBus). Are there others?

3. **Cluster detection.** Are there clusters of domains that form tight synergy loops among themselves but are loosely connected to other clusters? These represent **independent capability pillars** that can be developed in parallel.

4. **Bridge domains.** Are there domains that connect otherwise isolated clusters? These are **strategic leverage points** — small investments that unlock cross-cluster synergy.

5. **Hidden layers.** With the graph in front of you, ask:
   - What combinations become possible only AFTER 2+ infrastructure moves land?
   - What capabilities exist in the upstream SB/ES lineage that K1 hasn't inherited, AND that would serve as a hub or bridge domain if introduced?
   - Are there domains OUTSIDE the current 6 convergent themes — from adjacent fields (physics simulation, procedural art, signal processing, perceptual psychology, music theory) — that would create new hub or bridge nodes if introduced?
   - What about the dual-strip centre-origin LGP geometry specifically? Pressure-test every synergy cluster: does the dual-strip architecture amplify, enable, or have no effect? Flag any combination where dual-strip creates a capability that is **geometrically impossible** on single-strip devices.

   **SSA divergence mining.** Consult the individual SSA outputs — not just the synthesis. Pay particular attention to ideas that appeared in only ONE SSA's output and were not carried into the convergent themes. These singleton proposals are candidates for non-obvious bridge domains or latent synergy nodes that synthesis may have prematurely discarded. For each singleton found, evaluate: does it connect two otherwise isolated clusters in the graph? If yes, it's a bridge domain and its absence from synthesis was a loss, not a correct edit. Also examine the intermediate reasoning and dead ends in the SSA appendices — an idea an SSA explored and abandoned may have been abandoned for axis-local reasons that don't apply at the system level.

6. **Moat analysis.** For each synergy layer identified, evaluate: Is this defensible? Can a competitor with a different physical form factor reproduce this combination? If not, why not — is it geometry, is it the specific audio pipeline, is it the composition architecture, or is it the interaction of all three?

### Output Format — Pass 2

```
SYNERGY TOPOLOGY
────────────────
Platform Domains (hubs):
  [D__] — outbound multiplicative edges: N — amplifies: [list]

Capability Pillars (clusters):
  Pillar A: [D__, D__, D__] — internal synergy: ... — independent of Pillar B
  Pillar B: ...

Bridge Domains:
  [D__] connects Pillar A ↔ Pillar B — unlocks: ...

HIDDEN LAYERS
─────────────
Layer 0 (GIVEN): LPF × LayerStack × ControlBus
Layer 1: ... — becomes visible only after Layer 0 lands
Layer 2: ... — becomes visible only after Layer 1 lands
Layer N: ...

Each layer:
  Domains involved: ...
  Emergent capability: ...
  Prerequisite layers: ...
  Dual-strip amplification: [yes/no + reasoning]
  Moat strength: [none | weak | strong | absolute] + reasoning

EXTERNAL DOMAIN CANDIDATES
──────────────────────────
Domains from adjacent fields not currently in the K1 vocabulary:
  [E01] Domain — source field — would serve as [hub | bridge | leaf]
       Connects to: ...
       Unlocks: ...

GRAPH ARTIFACT
──────────────
Nodes: [D01, D02, ..., D__]
Edges (A → B = "A must precede B; A multiplicatively amplifies B"):
  D01 → D03  weight: [multiplicative | combinatorial]  evidence: [source citation]
  D01 → D07  weight: [multiplicative | combinatorial]  evidence: [source citation]
  ...
(Omit purely additive edges. This artifact must be parseable — Pass 3's topological sort consumes it directly.)
```

---

## PASS 3 — Strategic Sequencing & Kill Order

### Objective

Take the synergy topology from Pass 2 and derive the optimal implementation sequence — the minimum number of moves that unlocks the maximum compounding synergy surface at each step.

### Instructions

1. **Dependency resolution.** From the directed graph, compute the topological sort. What MUST land first due to hard prerequisites?

2. **Unlock surface calculation.** At each step in the sequence, calculate the cumulative synergy surface using a consistent, auditable metric. Surface is measured as the 4-tuple: **(modes-renderable) × (layer-stack depth supported) × (independent parameter axes per mode) × (composition primitives in the LayerStack alphabet)**. Express each step's surface as that 4-tuple plus a single headline integer (the product). If a step changes only one dimension (e.g. introduces a new blend mode without adding modes), make this explicit.
   - Step 0 (current K1): standalone modes, no composition, no persistence primitive → express as 4-tuple
   - Step 1 (first move lands): what new synergy edges become active? What's the new 4-tuple?
   - Step N: what does the full surface look like?

3. **Greedy vs. strategic ordering.** Compare two orderings:
   - **Greedy**: at each step, pick the single move that maximises immediate surface gain
   - **Strategic**: accept lower immediate gain on early moves if they unlock dramatically larger gains on subsequent moves
   - If these differ, explain why and recommend which to follow

4. **Effort-to-unlock ratio.** For each move, estimate relative implementation effort (low / medium / high) against the synergy surface it unlocks. Flag any moves that are low effort but sit on critical graph paths (high leverage).

5. **Phase gates.** Group the sequence into natural phases — points where K1 reaches a qualitatively new capability tier. Name each phase by what it makes possible that wasn't possible before.

6. **The "what are we leaving on the table" audit.** After the final phase, look at any domains or synergy edges that were NOT included in the sequence. For each, explain: is it excluded because it's low value, high effort, or because it's speculative? Could any excluded domain become high-value if a future unknown variable changes (new audio features, new hardware revision, user feedback)?

### Output Format — Pass 3

```
KILL ORDER
──────────
Phase 1: [Name] — "K1 becomes ___"
  Move 1.1: [domain/primitive] — effort: [L/M/H] — unlocks: [edges]
  Move 1.2: ...
  Cumulative surface after Phase 1: ...
  Qualitative capability tier: ...

Phase 2: [Name] — "K1 becomes ___"
  ...

Phase N: ...

GREEDY vs STRATEGIC COMPARISON
──────────────────────────────
Greedy order: ...
Strategic order: ...
Divergence points: ...
Recommendation: ...

LEFT ON THE TABLE
─────────────────
[D__] — reason excluded: ... — conditions under which it becomes critical: ...
```

---

## PASS 4 — Adversarial Stress Test

### Objective

Attempt to break, invalidate, or find blind spots in the synergy topology and kill order produced by Passes 1–3.

### Instructions

1. **Assumption audit.** List every assumption embedded in the topology. For each, ask:
   - What evidence supports this?
   - What would make this assumption false?
   - If it's false, which synergy edges collapse?

2. **Missing domain challenge.** Is there a domain that should exist in the registry but doesn't? Think from these angles:
   - User experience / perceptual psychology (how humans actually perceive light and sound together)
   - Failure modes (what happens when audio input is silent, clipping, mono-frequency, or adversarial?)
   - Temporal scale (are we only thinking at frame-level? What about phrase-level, song-level, session-level evolution?)
   - Manufacturing / hardware variance (do LED brightness curves, diffuser opacity, or thermal throttling create domains we're ignoring?)

3. **SSA tension exploitation.** Cross-reference the ProductStrategy SSA's viability assessments against the Physics, Geometry, and AudioDriver SSAs' capability claims. Where ProductStrategy flagged concerns, cost barriers, or diminishing returns that other SSAs ignored, those tensions are your strongest adversarial material. Similarly, compare the K1Specific SSA's hardware constraint awareness against the more unconstrained proposals from Physics and Composition — which proposals silently violate the 2.0ms render ceiling or no-heap constraint when you actually think through implementation? Conversely, where ProductStrategy's rejection is brand-voice-driven (locked positioning, banned language) rather than engineering-driven, flag the brand voice as the constraint and re-evaluate the underlying capability on engineering merit alone.

4. **Synergy mirage detection.** Are any of the claimed multiplicative interactions actually additive when you think harder? Challenge the top 3 synergy edges by Pass 2 graph weight (or by claimed unlock impact in Pass 3). You do not get to choose which 3 to challenge — Pass 2's own ranking selects them. Apply rigorous reasoning to each.

5. **Counter-sequencing.** Propose an alternative kill order that optimises for a DIFFERENT strategic objective (e.g., fastest path to visually differentiated Founder's Edition, vs. deepest long-term platform capability). Does the optimal sequence change when the objective changes?

6. **Black swan domains.** Identify 2–3 capabilities or developments that are NOT in the current research but would, if they emerged, fundamentally restructure the synergy topology. These could be: new ESP32-P4 capabilities, novel audio analysis techniques, user-generated content/programming interfaces, or physical product revisions.

7. **Challenge the GIVEN.** The infrastructure layer declared as GIVEN in the Context Preamble (LPF × LayerStack × ControlBus) was identified through rapid strategic assessment, not rigorous derivation from the source material. It is not exempt from adversarial scrutiny. If you can build a rigorous case that any of the three is additive rather than multiplicative — or that the ordering is wrong — present it. The adversarial pass is the only place where the GIVEN gets stress-tested. Do not skip this.

### Output Format — Pass 4

```
ASSUMPTION REGISTRY
───────────────────
[A01] Assumption: ...
      Evidence: ...
      Falsification condition: ...
      Collapse impact: [which edges/layers fail]

MISSING DOMAINS
───────────────
[M01] ... — why it matters — where it connects

SSA TENSION MAP
───────────────
[T01] SSA-ProductStrategy vs SSA-[axis]: ...
      ProductStrategy concern: ...
      Opposing SSA claim: ...
      Resolution: [ProductStrategy correct | Opposing SSA correct | unresolved — needs empirical test]
      Impact on topology if ProductStrategy is correct: ...

SYNERGY MIRAGE CHALLENGES
─────────────────────────
[Challenge 1] Edge [D__ × D__] claimed as multiplicative
     Counter-argument: ...
     Verdict: [sustained | demoted to additive | collapsed]

ALTERNATIVE KILL ORDERS
───────────────────────
Objective: [Fastest FE visual differentiation]
  Sequence: ...
  Trade-offs vs strategic order: ...

Objective: [Deepest platform capability]
  Sequence: ...
  Trade-offs: ...

BLACK SWANS
───────────
[BS01] ... — restructuring impact: ...
```

---

## Execution Notes

- Each pass builds on the output of the previous pass. Do not skip or compress passes.
- Cite specific modes, parameters, formulas, and source lines from the taxonomy and brainstorm documents. Do not make abstract claims without grounding them in the source material.
- When the synthesised documents lack depth on a specific domain, go to the corresponding SSA output. Physics questions → SSA-Physics. Geometry questions → SSA-Geometry. Do not treat the synthesis as sufficient when the raw axis output exists.
- Singleton proposals (ideas raised by only one SSA) deserve MORE scrutiny, not less. The synthesis process optimises for convergence, which systematically undervalues orthogonal insights. Your job is to correct for that bias.
- When uncertain, flag uncertainty explicitly rather than papering over it with confident language.
- The dual-strip centre-origin LGP geometry is K1's single most differentiated physical characteristic. It should be a forcing function applied to every evaluation, not an afterthought.
- "Additive" means value stacks linearly. 1+1=2. No interaction effects. "Multiplicative" means inputs amplify each other. 2×3=6, not 5 — improving one input increases the value of the other. "Combinatorial" means each new domain multiplies the number of possible interactions across all existing domains — growth is explosive. Be rigorous about which class each interaction actually belongs to. The key distinction: multiplicative is about mutual amplification between two inputs, combinatorial is about the explosion of possible pairings as the set grows. They are related but mechanistically different.