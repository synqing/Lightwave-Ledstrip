---
abstract: "Five complete rebuilds of the design decision mandate, each through a different lens: Natural Law, Failure Autopsy, Conversation, Decision Tree, and One-Page Synthesis. The final Tear-down 5 is the candidate for adoption into CLAUDE.md or as a skill. Read when evaluating which form the governance mandate should take."
---

# Mandate Refinement: Five Rebuilds

Source material:
- `DESIGN_DECISION_PROCESS.md` -- the 12 original principles
- `MANDATE_ENFORCEMENT_PROPOSAL.md` -- the current enforcement proposal (16 commandments)
- `DECISION_FRAMEWORK_ANALYSIS.md` -- the 6 missing principles from decision theory

Each tear-down below is a complete, standalone rebuild. Not a critique of the previous -- a fresh attempt to express the same truths in a form that flows naturally.

---

## Tear-down 1: The Natural Laws

These are not rules imposed on agents. They are properties of the problem space. Violate them and the work fails -- not because someone enforces the violation, but because the violation is structurally unsound, the way a bridge with unresolved forces collapses regardless of the inspector's opinion.

**The Law of Diagnosis.**
A symptom and its cause require different interventions. Treating clutter with visual polish when the cause is structural disorganisation is like treating a fever with ice baths -- the temperature drops, the infection remains. If you cannot name the disease separately from the symptom, you have not yet diagnosed anything.

**The Law of Singular Existence.**
Every element in a system exists in exactly one place. If it appears in two places, one of those is a lie -- a stale copy, a contradictory source, a confusion the user must resolve that the designer was supposed to resolve for them. Duplication is not redundancy. It is inconsistency waiting to be noticed.

**The Law of Visibility.**
A decision you cannot see is a decision you cannot evaluate. Structure described in words is a hypothesis. Structure rendered visually is evidence. No amount of reasoning about a layout replaces looking at it. The medium of evaluation must match the fidelity of the decision -- wireframes for structure, high-fidelity renders for hierarchy and colour.

**The Law of Conservation.**
Working solutions encode the answers to problems you have forgotten you solved. Discarding a working element to rebuild from scratch re-exposes you to every problem it had already resolved. Audit before you redesign. The question is never "what should I build?" but "what has already been built that I must not destroy?"

**The Law of Constraint Supremacy.**
Constraints do not limit your options. They reveal your options. An option that violates a hard constraint was never viable -- evaluating it is waste. State your constraints first. Let them annihilate the impossible. Whatever survives is your actual design space. If nothing survives, a constraint is wrong or the problem needs reframing.

**The Law of Proportional Control.**
The precision of a control must match the precision of the parameter. An encoder for a four-value toggle is over-engineering. A binary switch for a 256-step gradient is under-engineering. When the control promises more precision than the parameter rewards, the user learns that precision is meaningless here -- and stops trusting precision everywhere.

**The Law of Signal Purity.**
Every visual property is either signal or noise. There is no such thing as neutral decoration. What the designer intends as ornament, the user's perceptual system processes as information -- and when it carries no meaning, it generates confusion, not beauty. If a colour does not encode state, mode, or priority, it is stealing bandwidth from colours that do.

**The Law of Exhaustive Generation.**
The cost of generating one more option is minutes. The cost of committing to the wrong option is hours or days. For any decision where the option space is finite and bounded -- colour harmonies, placement positions, visual treatments -- generate the complete set. Partial exploration is premature convergence. You cannot choose the best option from a set you never generated.

---

These eight laws are not a checklist. They do not have an order. They are simultaneous truths about how design decisions interact with reality. A design that obeys all eight may still be ugly or wrong -- but a design that violates any of them is *structurally* defective, and no amount of craft can rescue it.

---

## Tear-down 2: The Failure Autopsy

Every principle in the original process exists because a specific mistake was made. Forget the principles. Here are the mistakes.

### The Actual Failures

**Failure 1: Category Error.**
The agent treated a structural problem as a visual problem. Three research agents were deployed on aesthetics -- fonts, colour palettes, dark mode references. None of their output was usable because the problem was information architecture: how 50+ parameters map to 16 encoders. Hours of agent time burned on the wrong question.

**Failure 2: Invisible Proposals.**
The agent proposed a navigation architecture as ASCII art. The user could not evaluate it. The feedback was: "ASCII is not going to cut it AT ALL." A structural proposal that cannot be seen cannot be judged. The agent produced output that was technically correct but practically useless.

**Failure 3: Destruction of Working Elements.**
The agent replaced the existing header and footer design with a minimal treatment. The user rejected it immediately -- the bold 2px white borders and elevated surfaces were the one thing the current design got right. The agent threw away the good with the bad because it did not audit what was already working.

**Failure 4: Hierarchy Collapse.**
The same white 2px border appeared on both the frame (header/footer) and the content cards. Everything looked equally important, which means nothing looked important. Three cognitive layers (frame, content, active focus) were rendered as one. The user had to explain what visual hierarchy was.

**Failure 5: Duplicate Parameters.**
The EDGE/CLR tab was proposed with 7 parameters. The user caught that 5 of them already existed in the mode buttons. The agent had placed parameters by category ("EdgeMixer params go in the EdgeMixer tab") without checking what already existed on screen. The user's response was profane and justified.

**Failure 6: Constraint-Violating Options.**
Options were presented that violated known constraints -- physical encoder stacking, the "no hidden modes" usability rule. Time was spent discussing options that were never viable. Every minute evaluating an impossible option is a minute not spent on a possible one.

**Failure 7: Over-Precise Controls.**
An encoder (256 positions) was considered for a parameter with 5 useful values. The encoder affords continuous adjustment; the parameter does not reward it. The mismatch teaches the user that the interface's promises are unreliable.

### The Gates

Each gate below is the minimum intervention that would have prevented the corresponding failure. One sentence. One question. Answer it before proceeding.

| # | Gate | Prevents |
|---|------|----------|
| G1 | **Before proposing any solution, state whether the problem is visual, structural, or functional. Provide evidence.** | Failure 1 |
| G2 | **Every structural proposal must be rendered as a visual mockup. No text-only or ASCII proposals for layout decisions.** | Failure 2 |
| G3 | **Before redesigning, list every element in the current design as KEEP, EVALUATE, or FIX. Do not modify KEEP elements.** | Failure 3 |
| G4 | **The design must have at least 3 visually distinct hierarchy levels. Name them and state how each is differentiated.** | Failure 4 |
| G5 | **Before placing any element, search every existing screen, tab, and control for it. If it already exists somewhere, do not place it again.** | Failure 5 |
| G6 | **List all hard constraints before generating options. Eliminate any option that violates a constraint before presenting it.** | Failure 6 |
| G7 | **For each parameter, count its useful distinct values. If fewer than 8, use a tap control. If 50+, use an encoder. State the count.** | Failure 7 |

Seven gates. Seven failures. Each gate is verifiable -- you can check whether the agent actually did it. Together they cover every mistake that was made during the actual design exercise. If the agent passes all seven, it has avoided every error that empirically occurred.

No theoretical failure modes. No hypothetical risks. Just the things that actually went wrong, and the specific checks that would have caught them.

---

## Tear-down 3: The Conversation

Here is everything you need to know before you touch a design.

Before you do anything, figure out what the actual problem is. Not the symptom -- the cause. "It looks cluttered" is a symptom. The cause might be bad visual hierarchy, or it might be that you have 50 parameters crammed into a space designed for 20. Those are completely different problems with completely different fixes. If you get this wrong, everything downstream is wasted effort. Take the time.

Once you know the real problem, build the inventory. Every parameter, every control, every element -- where does it currently live? Write it down. If something shows up in two places, one of those is wrong, and you need to kill it before you design anything else. Duplicates are not a minor issue. They are the single most common design failure in this project, and they will survive to the final mockup if you do not hunt them at the start.

Now list your constraints. The physical ones (encoders are stacked top-bottom, the screen is 1280x720), the interaction ones (nothing hidden, nothing the user has to remember the existence of), the brand ones (gold anchor colour, existing header/footer frame). Let the constraints murder options. Do not generate five ideas and then check constraints -- you will waste time on things that were never possible. Constraints first, then ideas.

When you do generate ideas, generate more than you think you need. Three for structural decisions, five for visual ones. Show them all. The cost of one extra mockup is five minutes. The cost of picking the wrong option because you never saw the right one is a full redesign cycle. And for the love of clarity, render everything visually -- no ASCII art, no text descriptions of layouts. If the human cannot see it, the human cannot evaluate it. End of story.

Before you finalise, check your work. Does everything have exactly one home? Is there a clear visual hierarchy -- frame, content, active focus -- with each level looking genuinely different? Does every colour mean something, or is some of it just decoration? Does the control type match the parameter -- tap for things with a handful of useful values, encoder for things that reward continuous adjustment?

That is the whole process. Diagnose, inventory, constrain, generate, check. Everything else is detail.

---

## Tear-down 4: The Decision Tree

```
START
  |
  v
What type of change are you making?
  |
  +-- Cosmetic (< 5 LOC, single visual property) -----> PROCEED. No gate.
  |
  +-- Structural (layout, navigation, parameter allocation, information architecture)
  |     |
  |     v
  |   Have you classified the problem as VISUAL, STRUCTURAL, or FUNCTIONAL?
  |     |
  |     +-- No ---------> STOP. Classify with evidence before continuing.
  |     |
  |     +-- Yes
  |           |
  |           v
  |         Have you built the complete element/parameter inventory?
  |           |
  |           +-- No ----> STOP. List every element and its current location.
  |           |
  |           +-- Yes
  |                 |
  |                 v
  |               Does any element appear in more than one location?
  |                 |
  |                 +-- Yes -> STOP. Resolve every duplicate to a single home.
  |                 |
  |                 +-- No
  |                       |
  |                       v
  |                     Have you listed all hard constraints?
  |                       |
  |                       +-- No -> STOP. Collect physical, interaction,
  |                       |         technical, and brand constraints.
  |                       +-- Yes
  |                             |
  |                             v
  |                           Have you audited the current design (KEEP / EVALUATE / FIX)?
  |                             |
  |                             +-- No -> STOP. Audit before redesigning.
  |                             |
  |                             +-- Yes
  |                                   |
  |                                   v
  |                                 Generate options.
  |                                 Structural: minimum 3.
  |                                 Visual: minimum 5.
  |                                 All rendered visually.
  |                                 All constraint-violating options eliminated.
  |                                   |
  |                                   v
  |                                 PROCEED to mockup and evaluation.
  |
  +-- Visual only (colour, font, spacing, border treatment -- no layout change)
        |
        v
      Does the change affect visual hierarchy (number of levels or differentiation)?
        |
        +-- No ---------> Are you adding/changing colour?
        |                    |
        |                    +-- No -> PROCEED. No gate.
        |                    |
        |                    +-- Yes -> Does the colour encode information
        |                               (state, mode, context)?
        |                                 |
        |                                 +-- Yes -> PROCEED.
        |                                 |
        |                                 +-- No --> STOP. Colour must carry
        |                                           meaning. Remove or repurpose.
        |
        +-- Yes --------> STOP. This is now a STRUCTURAL change.
                          Re-enter at Structural branch.
```

The tree has five terminal STOP nodes. Each corresponds to a specific failure from the actual design exercise:

| STOP Node | Failure It Prevents |
|-----------|-------------------|
| Classify first | Category error -- treating structural problems as visual |
| Build inventory | Duplicate parameters placed without checking |
| Resolve duplicates | Elements appearing in two locations simultaneously |
| List constraints | Options presented that violate hard constraints |
| Colour must carry meaning | Decorative colour competing with informational colour |

Two additional quality checks are embedded in the flow nodes:
- Current design audit (prevents destroying working elements)
- Minimum option count (prevents premature convergence)

The tree does not try to capture every nuance. It captures the **decision points where things actually went wrong**, and forces the agent to answer the right question at the right moment.

---

## Tear-down 5: The One Page

### The Briefing

Before you do anything, figure out whether the problem is visual, structural, or functional -- because the fix is completely different for each. Then build the inventory: every parameter, every control, every element, where it currently lives. If anything appears twice, kill the duplicate before you design anything else. List your hard constraints and let them murder impossible options before you waste time generating them. Audit the current design -- find what works and protect it. Then generate your options (at least 3 for structural, 5 for visual), render them all visually, and check: does every element have exactly one home? Does every colour mean something? Does every control match its parameter's actual useful resolution? That is the whole process. Diagnose, inventory, constrain, generate, verify.

### The Laws

These are properties of the design space, not rules. Violate any of them and the work is structurally defective regardless of craft or intention.

**Singular Existence.** Every element exists in exactly one place. A duplicate is not redundancy -- it is an inconsistency the user must resolve that the designer should have resolved.

**Diagnosis Before Treatment.** A symptom and its cause require different interventions. If you cannot name the structural cause separately from the visible symptom, you have not diagnosed anything.

**Visibility.** A decision you cannot see is a decision you cannot evaluate. Every structural proposal must be rendered at the fidelity the decision demands -- wireframes for structure, full renders for hierarchy and colour.

**Conservation.** Working solutions encode answers to forgotten problems. Audit and preserve before redesigning. The question is not "what should I build?" but "what must I not destroy?"

**Constraint Supremacy.** Constraints do not limit options. They reveal them. State constraints first, generate options within the surviving space. If nothing survives, reframe the problem.

**Signal Purity.** Every visual property is signal or noise. What the designer intends as decoration, the user processes as information. If a colour does not encode state, mode, or priority, it is stealing bandwidth.

**Proportional Control.** The precision of the control must match the precision of the parameter. An encoder for a 4-value toggle and a tap button for a 256-step gradient are both broken promises.

### The Decision Gate

```
What are you changing?
  Cosmetic (< 5 LOC)  ---------> Proceed.
  Visual (no layout change) ----> Does the colour carry meaning? Yes -> Proceed. No -> Stop.
  Structural -------------------> Continue below.

Have you classified the problem (visual / structural / functional)?  No -> Stop.
Have you built the inventory and resolved all duplicates?             No -> Stop.
Have you listed all hard constraints?                                 No -> Stop.
Have you audited the current design (KEEP / EVALUATE / FIX)?         No -> Stop.

Generate options (3+ structural, 5+ visual). Render visually.
Eliminate constraint violations. Present survivors.                   -> Proceed.
```

### Why This Exists

This is not theoretical. Every law and every gate exists because a specific mistake was made during a real design exercise:

- Three agents deployed on visual research for a structural problem. (Diagnosis)
- Five of seven proposed parameters already existed elsewhere on screen. (Singular Existence)
- Navigation architecture presented as ASCII art -- user could not evaluate it. (Visibility)
- Working header/footer replaced with a weaker treatment for no reason. (Conservation)
- Options presented that violated physical encoder constraints. (Constraint Supremacy)
- Uniform border treatment collapsed three hierarchy levels to one. (Signal Purity)
- Encoder considered for a parameter with five useful values. (Proportional Control)

The gates are not bureaucracy. They are scar tissue. Each one marks a wound that cost hours of rework. Follow them not because they are mandated, but because the alternative is re-learning the same lessons at the same cost.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:prompt-engineer | Created. Five complete rebuilds of the design decision mandate: Natural Law, Failure Autopsy, Conversation, Decision Tree, and One-Page Synthesis. |
| 2026-03-23 | captain + cowork:opus | Appended K1 Node Composer audit. Backtest of research design against firmware reality. |

---

## K1 Node Composer — Design Audit

### Context

This audit backtests the Node Composer research dump (5-vector investigation, 2026-03-23) against actual firmware primitives, audio pipeline architecture, and the forensic findings from the 5L-AR effect failure analysis. The question: does this design, as proposed, actually solve the problem it claims to solve?

### Verdict

The architecture is sound. The node catalogue has gaps. The audio parity assumption is wrong and must be addressed or the tool's core promise is undermined.

---

### 1. Framework Choice: React Flow — ACCEPTABLE WITH CAVEAT

React Flow is event-driven, not per-frame. The research acknowledges this and proposes a custom `requestAnimationFrame` execution loop. That's correct — the graph UI and the execution engine are separate concerns.

**Caveat:** React Flow renders 50+ nodes as React components, each with a canvas mini-visualisation. React's reconciliation overhead at 60fps is real. The performance budget claims "0.50ms React/DOM overhead" — that is optimistic. React re-renders triggered by state changes (node values updating every frame) will propagate through the virtual DOM. This needs either: (a) aggressive memoisation with `React.memo` on every node component, (b) canvas rendering decoupled from React's render cycle via refs, or (c) acceptance of 30fps for the UI while the execution engine runs at 60fps internally.

Litegraph.js would be mechanically simpler (Canvas2D, native per-frame loop, proven at 100+ nodes) but has worse documentation and ecosystem. The choice is defensible if the React rendering cost is managed.

### 2. Audio Parity: CRITICAL GAP — THE DESIGN'S BIGGEST RISK

The research assumes browser audio analysis (Meyda.js FFT + Essentia.js beat detection) will produce results comparable to the firmware's ESV11 pipeline. **They won't.** The divergence is structural, not parametric.

| Dimension | Firmware (ESV11) | Browser (Meyda/Essentia) | Consequence |
|---|---|---|---|
| Frequency analysis | Goertzel, 64 adaptive-width pitch bins | FFT, uniform bin spacing | Non-uniform vs uniform frequency resolution. Low-frequency accuracy diverges. |
| Chroma | Octave-summing of Goertzel bins (no tuning correction) | Tuning-aware filter banks | Firmware shows octave confusion; browser resolves octaves. Different chroma profiles from same audio. |
| Beat detection | Phase-locked oscillator on novelty Goertzel curve | Autocorrelation / onset peak-picking | Can be ~180° out of phase on weak fundamentals. |
| Smoothing | Asymmetric attack/release (fast up, slow down) + spike removal + zone-wise AGC | Fixed EMA or simple max-follower | Firmware emphasises transients; browser is uniformly smooth. |
| Sample rate | 12.8 kHz, 50 Hz hop rate | 44.1/48 kHz, variable hop rate | Different aliasing profiles, different temporal resolution. |

**What this means:** An effect that looks great in the browser preview may look different on the K1 hardware — not because the rendering math is different, but because the *audio input data* is different. The whole point of this tool is "see what it looks like before you flash." If the audio inputs diverge 15–25% on magnitude and potentially 180° on beat phase, the preview is misleading.

**Mitigation options (pick one):**
- **Option A (best):** Implement a "firmware-faithful" audio analysis mode in the browser that replicates the ESV11 Goertzel pipeline in JavaScript/WASM. Expensive to build but eliminates the parity gap entirely.
- **Option B (pragmatic):** Accept the divergence for the POC. Add Phase 4 (live WebSocket preview to K1) as the "ground truth" mode. The browser preview becomes a fast approximation; the hardware preview becomes the validation step.
- **Option C (calibration):** Run identical audio through both pipelines, measure the divergence per ControlBus field, and apply correction curves in the browser. Fragile but may be sufficient.

**Recommendation:** Option B for POC. Option A for production. The tool is still valuable with approximate browser audio — it shows signal flow, value distributions, and spatial patterns. But the user must know the browser preview is an approximation, not a prediction.

### 3. Type System: GAP — ARRAY_8 and ARRAY_12 MISSING

The type enum defines: SCALAR, ARRAY_160, CRGB_160, ANGLE, BOOL, INT.

The node catalogue defines "All Bands" (ARRAY_8) and "All Chroma" (ARRAY_12). These types don't exist in the type system. How does a Band(i) source node (SCALAR output) connect to a node that expects all 8 bands? How does a chroma-driven colour node receive 12 chroma values?

**Options:**
- Add ARRAY_8 and ARRAY_12 as types. Simple, but proliferates types.
- Remove "All Bands" and "All Chroma" nodes. Force per-bin wiring. Verbose but type-safe.
- Implement a generic ARRAY type with runtime length checking. Flexible but loses static type safety.

**Recommendation:** Add ARRAY_8 and ARRAY_12. Two extra types for a total of 8. The firmware has these as fixed-size arrays; the node editor should mirror that.

### 4. Node Catalogue: MOSTLY COMPLETE — THREE GAPS

Cross-referencing the proposed 30 node types against what the working effects actually use:

**Present and correct:**
- EMA Smooth, Asymmetric Follower, Max Follower, Scale, Power, Clamp — all map to SmoothingEngine.h
- Gaussian, Triangular Wave, Standing Wave, Centre Melt — geometry coverage is good
- HSV→RGB, Palette Lookup, Blend, Scale Brightness, Fade To Black — composition coverage is good
- SET_CENTER_PAIR output with mirror — correct

**Missing:**

**Gap A: TemporalOperator / Envelope system.** The firmware has 8 temporal operators (Anticipation, Attack, Sustain, Decay, Follow-Through, Recoil, Ease-In-Out, Suspension) with 4 preset envelopes (Impact, Standard, Dramatic, Tension). These are used for beat-triggered dynamics — a beat fires, an envelope shapes the response over time. The node catalogue has no envelope/trigger node. Without this, beat-reactive dynamics are limited to simple exponential decay.

**Gap B: SubpixelRenderer.** The firmware has anti-aliased fractional LED positioning (`renderPoint`, `renderLine`). Effects that position elements at non-integer LED positions use this for smooth motion. No node equivalent exists.

**Gap C: Circular Chroma Hue.** ChromaUtils provides `circularChromaHueSmoothed()` — a combined circular weighted mean of 12 chroma bins + circular EMA smoothing. This produces the tonal anchor hue that working effects use for colour. The node catalogue has "Circular EMA" and "Circular Chroma Hue" separately, which is correct for modularity, but the combined function is the most-used pattern and deserves a convenience node.

**Gap D: Per-zone state.** Working effects (EsOctaveRef, EsBloomRef) maintain per-zone state arrays (`chromaSmooth[kMaxZones][12]`, `maxFollower[kMaxZones]`). The node editor's state management (`Map<NodeId, NodeState>`) doesn't model zone-awareness. For POC this is acceptable (single zone), but the full design needs it.

### 5. Architecture Decisions: SOUND

ADR-001 (pull-based synchronous): Correct. Matches firmware's render() pattern. No caching needed for per-frame audio data.

ADR-002 (code generation over interpretation): Correct. Zero graph overhead on ESP32. The precedent (Max/MSP gen~, Unreal Blueprints) is well-established.

ADR-003 (explicit state management): Correct. State in engine-owned Map enables reset, snapshot, scrubbing, and clean C++ export.

ADR-004 (fixed type system with ARRAY_160): Correct in principle, needs ARRAY_8/ARRAY_12 additions per §3 above.

### 6. What's Missing from the Design

Beyond the node catalogue gaps:

**A. Graph validation.** What happens with cycles? Disconnected subgraphs? The topological sort will fail or produce undefined behaviour on cyclic graphs. Need either: cycle detection at connection time (reject the edge) or feedback edges with one-frame delay (like audio feedback loops in Pure Data).

**B. Error propagation.** What if a node produces NaN? Infinity? A negative brightness? Each node needs output clamping or sanitisation, or NaN propagation will corrupt the entire downstream graph in one frame.

**C. Dual-strip architecture.** The K1 has 2×160 LEDs. The type system models CRGB_160 (one half-strip). SET_CENTER_PAIR mirrors to CRGB_320. But some working effects (Holographic) treat the two strips differently — strip1 and strip2 get complementary palette indices. The output node needs a mode: "mirror" (default) or "independent" (two CRGB_160 inputs).

**D. Undo/redo.** Mentioned in passing ("snapshot for undo/redo") but not designed. For a creative tool where you're experimenting with parameter values, undo is not optional.

### 7. Does This Tool Solve the Problem?

Yes — with a qualification.

The problem: agents make perceptual decisions they cannot evaluate. The tool's answer: put human eyes at every node in the signal chain. That's correct. The mini-visualisations (sparklines for scalars, waveforms for arrays, colour strips for CRGB) mean the human sees the value distribution transform at every processing stage. No more guessing what `normBass * wave * beatMod; brightness *= brightness` produces.

The qualification: the tool solves the *design* problem (how do you create an effect that looks good?) but not the *production* problem (how do you generate 30+ effects efficiently?). Each effect designed in the node editor is hand-crafted by a human turning knobs. That's the right approach for quality — but it doesn't scale the way agent-generated effects were supposed to.

That may be the correct trade-off. Twenty hand-crafted effects that work are worth more than a hundred agent-generated effects that don't. But it's worth being explicit: this tool replaces the agent as effect designer. The agent's role moves to building and maintaining the tool itself.

### 8. POC Scoping Assessment

The proposed POC (9 node types, all layers exercised) is correctly scoped with one addition: the POC must include at least one TemporalOperator/envelope node (Gap A above), because beat-triggered dynamics are the single most common pattern in working effects, and the POC must prove that pattern works end-to-end.

Revised POC node count: 10 (add one Envelope node to Layer 2: Processing).

---

### Summary of Findings

| Area | Verdict | Action Required |
|---|---|---|
| Framework (React Flow) | Acceptable | Manage React rendering cost at 60fps |
| Audio parity | **Critical gap** | Option B (accept for POC, add hardware preview in Phase 4) |
| Type system | Gap | Add ARRAY_8, ARRAY_12 |
| Node catalogue | Mostly complete | Add Envelope, SubpixelRenderer, dual-strip output mode |
| Architecture (ADRs) | Sound | No changes |
| Graph validation | Missing | Add cycle detection, NaN sanitisation |
| Undo/redo | Missing | Design before POC |
| Problem-solution fit | Yes, with trade-off | Tool replaces agent as effect designer |
| POC scope | Correct | Add one Envelope node (10 types total) |

---

### Post-Audit Update (2026-03-24)

**Status: POC → Full catalogue in one session.** The agent expanded from 10 to 44 node types (18 sources, 12 processing, 8 geometry, 5 composition, 1 output) across 45 files, 8,416 LOC, 409KB bundle. Zero compilation errors. All ControlBus fields are now exposed as source nodes. All processing primitives from the research spec are implemented.

**Audit findings addressed:**

| Original Finding | Status | Notes |
|---|---|---|
| Audio parity (§2) | **RESOLVED — Option A** | Agent ported ESV11 Goertzel pipeline line-by-line to TypeScript (717 lines). Gaussian windowing, adaptive block sizing, interlacing, noise floor, autoranging — all faithful to C source. This was the audit's critical gap; it no longer exists. |
| Node catalogue gaps (§4) | **Partially resolved** | Asymmetric Follower, Circular EMA, dt-Decay, Schmitt Trigger, Centre Melt, Scroll Buffer, all composition nodes now present. Still missing: TemporalOperator/Envelope, SubpixelRenderer, Palette Lookup node. |
| Type system (§3) | **Open** | ARRAY_8 and ARRAY_12 still not in the type enum. Per-bin wiring works for POC but won't scale to chroma-driven effects. |
| React 60fps (§1) | **Confirmed concern** | Screenshot shows 18 FPS with 6 nodes. With 44 available, complex graphs will hit this harder. Decoupling canvas rendering from React reconciliation is now priority. |
| Graph validation (§6A) | **Open** | No cycle detection observed in engine.ts. |
| Undo/redo (§6D) | **Open** | State snapshot/restore exists in engine but no UI binding. |

**New observation from screenshot:** The Gaussian node's Centre parameter defaults to 0.00 (LED 0), not 79/80. The centre-origin rule is a hard constraint. Either the Gaussian node should default centre to 79, or the output node's mirror stage must handle this. Needs verification.

**Overall assessment:** The tool has moved from "promising design" to "functional prototype with the full node vocabulary." The critical audio parity risk is eliminated. The remaining gaps (Envelope node, type system, FPS, undo) are engineering tasks, not architectural risks. The tool is ready for real effect design attempts.
