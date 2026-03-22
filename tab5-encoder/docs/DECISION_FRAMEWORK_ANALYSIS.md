---
abstract: "Maps each of the 12 design decision principles from DESIGN_DECISION_PROCESS.md to established decision theory, cognitive science, and engineering design frameworks. Identifies what each framework adds beyond what we captured, where principles apply outside UI design, and 6 missing principles the frameworks reveal. Read when extending the decision process to new domains or when evaluating whether the process is complete."
---

# Decision Framework Analysis: Theoretical Foundations of the 12-Step Process

This document maps each of the 12 principles from `DESIGN_DECISION_PROCESS.md` to established frameworks in decision theory, cognitive science, and engineering design. The goal is threefold: (1) understand the deeper structure beneath each principle, (2) identify what the frameworks reveal that we missed, and (3) determine where these principles apply outside UI design.

---

## Framework Inventory

Before mapping individual principles, these are the primary frameworks referenced throughout the analysis. Each is drawn from a distinct discipline.

| Framework | Discipline | Core Idea |
|-----------|-----------|-----------|
| **Constraint Satisfaction Problems (CSP)** | Computer science / AI | A solution must satisfy all constraints simultaneously. Techniques: constraint propagation, arc consistency, backtracking. |
| **Recognition-Primed Decision Making (RPD)** | Cognitive science (Klein, 1998) | Experts do not compare options. They recognise a pattern, mentally simulate the first viable option, and execute or adjust. |
| **OODA Loop** | Military strategy (Boyd) | Observe, Orient, Decide, Act — repeated faster than the adversary. Orientation (mental models) is the decisive phase. |
| **First Principles Reasoning** | Philosophy (Aristotle), engineering (Musk) | Decompose to fundamental truths. Reason upward from axioms rather than by analogy to existing solutions. |
| **Elimination by Aspects (EBA)** | Behavioural decision theory (Tversky, 1972) | Eliminate options sequentially by applying the most important attribute threshold first, then the next, etc. |
| **Satisficing vs Optimising** | Bounded rationality (Simon, 1956) | Satisficing: accept the first option that meets all thresholds. Optimising: evaluate all options against a utility function. |
| **Minimum Description Length (MDL)** | Information theory (Rissanen, 1978) | The best model of data is the one that compresses it most — shortest combined length of model + data encoded by model. |
| **Dual Process Theory** | Cognitive psychology (Kahneman, 2011) | System 1: fast, intuitive, pattern-matching. System 2: slow, deliberate, analytical. Errors arise when System 1 handles System 2 problems. |
| **Design Structure Matrix (DSM)** | Systems engineering (Steward, 1981) | Map dependencies between design decisions. Identify coupled decisions that must be resolved together vs independent ones that can be parallelised. |
| **Poka-yoke** | Manufacturing (Shingo, 1986) | Design the system so that errors are impossible, not merely unlikely. Mistake-proofing through physical/logical constraints. |
| **Affordance Theory** | Ecological psychology (Gibson, 1979) | An object's properties suggest how it should be used. A knob affords turning. A button affords pressing. Mismatch = user error. |

---

## Principle-by-Principle Mapping

### Principle 1: Diagnose the disease, not the symptom

**Framework: Root Cause Analysis (RCA) + Dual Process Theory**

This is textbook RCA — the discipline formalised in manufacturing (Ishikawa fishbone diagrams), aviation (NTSB investigation protocol), and medicine (differential diagnosis). The principle's specific contribution is identifying a *category error*: treating an information architecture problem as a visual design problem. In Kahneman's terms, the initial response was System 1 — fast pattern-matching ("UI looks cluttered" maps to "fix the visuals") — when the problem required System 2 analysis of structural organisation.

**What the framework adds:** RCA formalises the *5 Whys* technique: ask "why?" recursively until you reach a causal root, not a correlated symptom. The principle captures the first Why ("why does it look cluttered?") but does not prescribe the recursive depth. A more general version would be: *Keep asking why until you reach a cause that, if fixed, prevents the symptom from recurring.* Fixing visual hierarchy does not prevent future clutter if the underlying parameter organisation is wrong. Fixing the information architecture does.

**Where this applies outside UI design:**
- **Software architecture:** A slow API endpoint might look like a database query problem (add an index), but the root cause might be an N+1 query pattern in the ORM layer. Indexing treats the symptom.
- **Product strategy:** Declining user retention might look like a feature gap ("competitors have X"), but the root cause might be onboarding friction. Building feature X does not fix onboarding.
- **Hardware design:** Thermal throttling might look like a heatsink problem, but the root cause might be a power regulation inefficiency upstream. A bigger heatsink treats the symptom.

---

### Principle 2: Research the actual problem domain

**Framework: Problem Framing (Schon, 1983) + Analogical Reasoning**

Donald Schon's concept of *problem framing* argues that how you define the problem determines what solutions are visible. Researching "dark mode colour palettes" framed the problem as aesthetic. Researching "how do 16 encoders manage 50+ params" framed it as information architecture. The frame determines the solution space.

This principle also uses *analogical reasoning* correctly — studying how Elektron, Push, grandMA3, Resolume, and Eos solve the same structural problem. The critical insight is that the analogy must be *structurally isomorphic*: these controllers share the same constraint (limited physical inputs, deep parameter sets) rather than merely being "similar products."

**What the framework adds:** Schon warns about *frame lock* — once you adopt a frame, you cannot see solutions outside it. The principle says "research the right domain," but the deeper version is: *Research at least two distinct problem framings before committing to one.* The team could have framed this as (a) an information architecture problem, (b) a parameter reduction problem (fewer params = no clutter), or (c) a modality problem (move some params to voice/gesture). Only by considering multiple frames do you confirm you have the right one.

**Where this applies outside UI design:**
- **API design:** "How do we version this API?" might be the wrong question. "How do we avoid breaking changes?" is a different problem frame that might lead to additive-only schemas rather than versioning.
- **Team structure:** "How do we hire faster?" vs "How do we reduce the need to hire?" vs "How do we retain better?" — three frames, three completely different solution spaces.
- **Business decisions:** "How do we compete on price?" vs "How do we make price irrelevant?" (Kim & Mauborgne's Blue Ocean framing).

---

### Principle 3: Propose structure before visuals, but render structure visually

**Framework: Levels of Abstraction + Externalisation Theory (Kirsh, 2010)**

This principle operates at two levels. The first — "structure before visuals" — is the standard engineering practice of separating architecture from implementation, or in software terms, defining the interface before the implementation. You do not write CSS before you have a component tree.

The second — "render structure visually to evaluate it" — maps to David Kirsh's *externalisation theory*: cognition is not purely internal. Humans reason better when they can manipulate external representations. An information architecture described in words is an internal representation. A rendered mockup is an external one. The external representation allows spatial reasoning, pattern detection, and gestalt evaluation that text cannot support.

**What the framework adds:** Kirsh identifies that the *medium of externalisation matters*. ASCII art and an HTML mockup are both external representations, but they have different *epistemic affordances*. ASCII lacks proportion, colour, and spatial accuracy — all of which are load-bearing for UI evaluation. The generalised principle: *The fidelity of your external representation must match the fidelity of the decision you are making.* For structural decisions (which panels exist, what goes where), wireframes suffice. For visual hierarchy decisions (border weight, colour), high-fidelity renders are required.

**Where this applies outside UI design:**
- **Software architecture:** Architecture Decision Records (ADRs) are the textual structure. But complex system interactions need sequence diagrams or state machine diagrams to evaluate — the visual externalisation reveals timing issues that text descriptions hide.
- **Hardware design:** Schematics (structure) come before PCB layout (spatial). But the schematic must be rendered as a block diagram for system-level review — reading a flat netlist is like reading ASCII art.
- **Product strategy:** A product roadmap described in bullets is structure. A timeline visualisation is the rendered version. Stakeholders evaluate the timeline, not the bullets.

---

### Principle 4: Audit what works before redesigning

**Framework: Recognition-Primed Decision Making (Klein, 1998)**

This is where RPD maps most directly. Gary Klein studied how fireground commanders, ICU nurses, and military officers make decisions under pressure. They do not generate and compare options. They *recognise* a situation as similar to one they have experienced, mentally simulate the first viable response, and execute. If the simulation reveals a problem, they adapt — but they never discard the entire prior approach to start from scratch.

The principle's insight — "not everything in a bad design is bad" — is the RPD insight applied to design: the existing system embeds solutions to problems you may not even remember having. The header/footer framing had already solved the "where does the dashboard begin and end" problem. Discarding it means re-encountering and re-solving that problem.

**What the framework adds:** RPD research shows that experts and novices differ not in their ability to generate options, but in their ability to *recognise which elements of the current situation are working*. The generalised principle is: *Before generating new solutions, enumerate what the current solution has already solved.* This is subtly different from "preserve what works" — it requires actively cataloguing the solved sub-problems, not just a vague "keep the good stuff."

Klein also identifies *anomaly detection* as a key expert skill: noticing when something that should be working is not. This maps to the complement of Principle 4: while auditing what works, also audit what *should* work but does not.

**Where this applies outside UI design:**
- **Software architecture:** Before rewriting a module, list every edge case the current implementation handles. Rewrites that do not carry forward edge-case handling regress. This is the "second-system effect" (Brooks, 1975).
- **API design:** Before redesigning an API, audit which clients depend on which behaviours. The existing API's "quirks" may be relied upon.
- **Team structure:** Before restructuring a team, identify which informal communication channels currently work. Reorgs that destroy working informal channels create coordination debt.

---

### Principle 5: Hierarchy requires 3+ visual levels with clear differentiation

**Framework: Gestalt Principles (Wertheimer, 1923) + Miller's Law (7 plus or minus 2)**

The three-level hierarchy (frame / content / active) maps to the Gestalt principle of *figure-ground segregation* extended to three layers. Classical figure-ground is binary: one element is figure (attended), the rest is ground (background). This principle adds a third level — the *active* element — which is figure against the content ground, which is itself figure against the frame ground. This creates a depth stack that guides attention without requiring the user to read labels.

Miller's Law (1956) provides the upper bound: working memory holds 7 plus or minus 2 chunks. Three visual levels is well within this limit. If you had 7 levels of visual hierarchy, the user could not reliably distinguish them.

**What the framework adds:** Gestalt psychology identifies specific principles that make hierarchy *work*: similarity (same border weight = same level), proximity (elements near each other are grouped), common region (shared container = shared group), and continuity. The principle states that 3+ levels are needed but does not prescribe *how* to differentiate them. Gestalt provides the toolkit: border weight, surface colour, proximity, enclosure, and alignment.

The deeper version: *Hierarchy is not just visual levels — it is a mapping from visual properties to cognitive categories. Each visual level must correspond to exactly one cognitive role (orientation, information, focus). When two cognitive roles share a visual treatment, the hierarchy collapses.*

**Where this applies outside UI design:**
- **Code architecture:** A well-structured codebase has at least three levels of organisation: packages/modules (frame), classes/files (content), functions/methods (active focus). When all levels look the same (flat file dumps), navigation becomes impossible.
- **Documentation:** Effective docs have: navigation frame (sidebar/TOC), content (the text), and active focus (the search result highlight, the current section indicator). Without all three, users get lost.
- **Organisational structure:** Company → Team → Individual contributor. Three levels with distinct communication patterns. When the levels blur (everyone reports to everyone), decision-making collapses.

---

### Principle 6: Placement decisions have sub-decisions — test each level

**Framework: Design Structure Matrix (DSM) + Hierarchical Decomposition**

This principle identifies that design decisions are not atomic — they decompose into sub-decisions with their own option spaces. "Where does the tab selector go?" decomposes into "horizontal or vertical?" then "inside or outside the frame?" These are *coupled decisions*: the answer to the second depends on the answer to the first.

In DSM terms, these form a *sequential dependency chain*: sidebar vs tabs must be resolved before inside-vs-outside-frame can be evaluated. The DSM technique would map all placement sub-decisions and identify which can be resolved in parallel (independent) vs which must be resolved in sequence (dependent).

**What the framework adds:** DSM reveals that some sub-decisions are *coupled* — they affect each other bidirectionally. For example, sidebar placement affects the available width for the param grid, which affects whether 8 columns fit, which might affect whether a sidebar is viable at all. This circularity means you cannot resolve them strictly sequentially — you need to evaluate the coupled set together (what DSM calls an "iterative block").

The generalised principle: *Before evaluating options, decompose the decision into its sub-decisions and map their dependencies. Resolve independent sub-decisions in parallel. Resolve dependent chains sequentially. Resolve coupled sets iteratively with prototype-and-test.*

**Where this applies outside UI design:**
- **Software architecture:** "Which database should we use?" decomposes into: relational vs document, hosted vs self-managed, read-heavy vs write-heavy optimisation. Each sub-decision constrains the next.
- **Hardware design:** "Which microcontroller?" decomposes into: core count, clock speed, peripheral set, package size, supply chain availability. These are partially coupled — a larger package enables more peripherals but constrains board layout.
- **Product strategy:** "Which market segment?" decomposes into: B2B vs B2C, geographic focus, price point, distribution channel. Each sub-decision opens and closes options for the others.

---

### Principle 7: Generate more options than you think you need

**Framework: Divergent Thinking (Guilford, 1967) + Set-Based Concurrent Engineering (Toyota)**

This principle maps directly to the *diverge-then-converge* model formalised by Guilford and adopted by design thinking (IDEO, Stanford d.school). The divergent phase generates breadth; the convergent phase applies constraints to narrow. Premature convergence — stopping at 2-3 options — is the primary failure mode.

Toyota's Set-Based Concurrent Engineering (SBCE) operationalises this at industrial scale: instead of choosing one design early and iterating on it ("point-based"), Toyota carries multiple designs forward simultaneously, gradually eliminating inferior ones as test data arrives. The principle's "kill fast, commit slow" is the SBCE motto in different words.

**What the framework adds:** SBCE research (Ward et al., 1995) quantifies the counterintuitive economics: carrying 5 options to prototype costs 3x more than carrying 1, but the final design quality is higher and total project time is often *shorter* because you avoid late-stage rework from premature commitment.

Guilford's research adds the concept of *fluency* (quantity of ideas), *flexibility* (variety of categories), and *originality* (uncommonness). The principle demands fluency ("generate more") but the framework adds that you should also demand flexibility — the 6 sidebar treatments should span different *categories* of solution (material, light, geometry, colour), not just 6 variations within one category.

**Where this applies outside UI design:**
- **Software architecture:** When choosing between architectural patterns (event-driven, request-response, CQRS, actor model), evaluate at least 3-4 rather than comparing only the "obvious" two. The K1 firmware's actor model was likely not the first architecture considered.
- **Hiring:** Interview more candidates than you think you need. The marginal cost of one more interview is hours. The cost of hiring the wrong person is months.
- **Business strategy:** Generate multiple go-to-market strategies before committing. A/B test positioning, not just features.

---

### Principle 8: Every visual property must carry information

**Framework: Tufte's Data-Ink Ratio + Shannon's Information Theory**

Edward Tufte's *data-ink ratio* (1983) states: maximise the share of ink (or pixels) that represents data. Every non-data element (decoration, chart junk, redundant borders) reduces the ratio and competes for attention. This principle extends Tufte from data visualisation to interface design: if a colour does not encode state, mode, or priority, it is chart junk.

Shannon's information theory provides the formal underpinning: information is *surprise*. A visual property that is the same everywhere carries zero bits of information. Colour that changes with tab context carries log2(n) bits where n is the number of tabs. The gold/azure/purple/teal colour scheme carries 2 bits of context information — enough to identify which of 4 modes the user is in without reading text.

**What the framework adds:** Shannon also defines *channel capacity* — the maximum rate at which a channel can carry information. A 1280x720 display with 16 encoders has finite visual bandwidth. Every decorative element consumes bandwidth that could carry information. Tufte and Shannon together say: *audit every visual property for its information content. Properties carrying zero bits should be eliminated or repurposed.*

The deeper version: *Every visual property is either signal or noise. There is no neutral decoration. What you intend as decoration, the user's perceptual system processes as signal — and when it carries no meaning, it creates confusion, not beauty.*

**Where this applies outside UI design:**
- **API design:** Every field in an API response should carry information the client needs. Fields that are always null, always the same value, or never consumed by any client are the API equivalent of decorative borders.
- **Log output:** Every log line should carry diagnostic information. Verbose logging with repetitive "entering function X" messages is decoration — it consumes attention bandwidth without aiding diagnosis.
- **Code style:** Every syntactic choice (naming convention, indentation, blank lines) should carry structural information. A blank line between method groups signals "these are different concerns." Inconsistent blank lines are noise.

---

### Principle 9: Use theoretical frameworks exhaustively

**Framework: Normative Decision Theory + Design Space Exploration**

This principle makes an epistemological claim: when a solved theoretical framework exists (colour harmony, typographic scales, grid systems), use it to generate the complete option set rather than sampling heuristically. This is the normative approach to decision-making — derive options from theory rather than from intuition.

Design Space Exploration (DSE), used in hardware/software co-design, formalises this: define the parameter space (hue, saturation, lightness, harmony type), enumerate the valid configurations, and evaluate systematically. The colour wheel harmony test (analogous, complementary, split-complementary, triadic, tetradic) is exactly a DSE across the harmony-type parameter.

**What the framework adds:** DSE research shows that *coverage* matters more than *depth* in early exploration. Testing all 5 harmony types at low fidelity is more valuable than deeply refining 1. This aligns with SBCE — breadth first, depth later.

The framework also reveals a risk: *framework completeness is not problem completeness.* The colour wheel gives you all harmonious combinations, but it does not account for display technology constraints (OLED vs LCD gamut), ambient lighting conditions, or colour-blind accessibility. The theoretical framework covers one dimension of the decision; other dimensions require empirical testing. The generalised principle: *Use theoretical frameworks to generate the complete option set within their domain, but test empirically for dimensions the framework does not cover.*

**Where this applies outside UI design:**
- **Software architecture:** Design patterns (GoF, POSA) are theoretical frameworks for structural problems. When facing a structural problem, enumerate which patterns apply rather than inventing from scratch. But patterns do not account for your specific performance constraints — that requires measurement.
- **DSP/audio:** Windowing functions for FFT (Hann, Hamming, Blackman, Kaiser) are a solved theoretical space. Enumerate and benchmark rather than guessing "Hann is probably fine."
- **Testing:** Boundary value analysis, equivalence partitioning, and pairwise testing are theoretical frameworks for test case generation. Use them to generate the complete test space rather than writing tests by intuition.

---

### Principle 10: Every parameter gets exactly ONE home

**Framework: Minimum Description Length (MDL) + Single Source of Truth (DRY) + Normalisation Theory (Codd)**

This principle is the information-theoretic DRY principle. Edgar Codd's database normalisation theory (1970) formalises exactly why: redundancy creates *update anomalies*. If a value exists in two places, updating one without the other creates inconsistency. In UI terms: if "EM Mode: Triadic" appears in both the mode button and the tab card, changing it via one control may not visually update the other, creating a state confusion that is indistinguishable from a bug.

MDL provides the mathematical justification: a system with n parameters shown in n locations has description length proportional to n. A system with n parameters shown in 2n locations has doubled description length without adding information. The additional complexity is pure overhead — it increases the probability of inconsistency without increasing the system's expressive power.

**What the framework adds:** Codd's normalisation theory goes deeper than "no duplicates." It identifies *functional dependencies*: if knowing parameter A determines parameter B, then B should be stored (and displayed) only in the context of A, not independently. For instance, if EM Mode determines the available Spread values, then Spread's display should be contextual to EM Mode — not shown independently in a location where the user cannot see which mode constrains it.

The deeper principle: *Every parameter has a natural home determined by its functional dependencies. A parameter that depends on a mode should live in that mode's context. A parameter that is globally independent should live in the persistent (always-visible) area. Misplacing a parameter relative to its dependencies forces the user to mentally reconstruct the dependency — a cognitive tax that should be zero.*

**Where this applies outside UI design:**
- **Software architecture:** Every piece of state should have exactly one authoritative source. This is the foundation of CQRS, Redux, and every state management pattern. Duplicated state is duplicated bugs.
- **API design:** Every piece of information should be available from exactly one endpoint. If user profile data is returned by both `/users/{id}` and `/users/{id}/profile`, clients will use both, and inconsistency between them becomes a support burden.
- **Documentation:** Every fact should be documented in exactly one place. Cross-references are fine; duplicated explanations drift apart. This is the rule that `CLAUDE.md` enforces with "one canonical file per topic."
- **Organisational structure:** Every decision domain should have exactly one owner. When two teams both "own" a feature area, decisions stall or contradict.

---

### Principle 11: Let constraints kill options

**Framework: Constraint Satisfaction Problems (CSP) + Elimination by Aspects (EBA)**

This is the most framework-rich principle. It maps to two distinct theoretical traditions.

**CSP (computer science):** The design problem is a CSP where variables are design decisions, domains are option sets, and constraints are hard requirements. The process described — listing constraints, then eliminating options that violate them — is *constraint propagation*: reducing each variable's domain by enforcing constraints.

CSP theory contributes several techniques beyond basic propagation:

- **Arc consistency:** For every option remaining in variable A's domain, there must exist at least one compatible option in variable B's domain. If "sidebar outside frame" is incompatible with all viable colour treatments, it should be eliminated even before colour is decided. This is forward-looking constraint propagation.
- **Constraint ordering:** Apply the *most constraining constraint first* (the "fail-first" heuristic). The constraint that eliminates the most options should be checked first — it maximises pruning efficiency. In the document, "no hidden modes" killed Option E, and "physical encoder stacking" killed encoder assignments. If physical stacking is more restrictive (eliminates more options), it should be checked first.
- **Backtracking:** If you reach a dead end (all options eliminated), the CSP approach is to backtrack to the last decision point and try a different branch. This is not captured in the 12 steps — see "Missing Principles" below.

**EBA (Tversky, 1972):** This is the psychological model of how humans naturally apply constraints. Tversky showed that people eliminate options by selecting the most important attribute, setting a threshold, eliminating all options that fall below it, then repeating with the next attribute. The process described is exactly EBA.

**What EBA theory reveals about constraint ordering:** Tversky's model shows that EBA is *order-dependent* — different orderings of attribute importance can produce different surviving options. This means the order in which constraints are applied matters. The principle says "state all constraints first," but the deeper insight is: *Order constraints by restrictiveness (most eliminating first) and importance (most critical first). If ordering affects the outcome, the constraints are insufficiently independent and the coupled set must be evaluated together.*

**Where this applies outside UI design:**
- **Software architecture:** Technology selection should be constraint-first. List hard requirements (latency ceiling, throughput floor, language ecosystem, team expertise), then eliminate technologies that violate any. Do not evaluate 10 databases on 20 criteria — eliminate by the 3 hardest constraints first.
- **Hiring:** Define non-negotiable requirements first (skills, location, availability), eliminate candidates who fail them, then evaluate remaining candidates on softer criteria. This is literally how structured hiring works (and why unstructured interviews fail — they do not apply constraints in order).
- **Hardware design:** Bill-of-materials selection is a CSP. Power budget, footprint, availability, cost — each constraint eliminates components. Apply in order of restrictiveness.

---

### Principle 12: Match control precision to parameter useful resolution

**Framework: Satisficing (Simon, 1956) + Affordance Theory (Gibson, 1979) + Fitts's Law**

This principle makes a satisficing argument: for parameters where the user cannot distinguish between adjacent values (Spread 30 degrees vs 37 degrees), providing continuous encoder control is *over-engineering*. The useful resolution is ~5 positions. A tap-to-cycle control that satisfices at 5 positions is superior to an encoder that optimises across 61 positions the user cannot perceive.

Gibson's affordance theory explains *why* the tap button is better: a tap button *affords* discrete selection. An encoder *affords* continuous adjustment. When the parameter is discrete, a continuous control's affordance is misleading — it promises precision the parameter does not reward. The mismatch between control affordance and parameter nature creates friction.

Fitts's Law provides quantitative support: the time to acquire a target is proportional to the distance divided by the target width. A tap button on a touchscreen has a large target (the button face) and zero distance (it is always in the same position). An encoder must be turned to a specific position — distance varies, and the "target width" for a specific value on a continuous encoder is small. For low-resolution parameters, the tap button has a lower Fitts's Law cost.

**What the framework adds:** Simon's satisficing theory identifies the *aspiration level* — the threshold at which you stop searching. For Spread, the aspiration level is "one of 5 meaningful positions." For Brightness, the aspiration level is "perceptually smooth adjustment across 256 values." The control method should match the aspiration level, not the parameter's theoretical range.

Affordance theory adds: *Every control method makes an implicit promise about the parameter it controls.* An encoder promises "this parameter rewards fine adjustment." A tap button promises "this parameter has a small set of useful states." Breaking these promises teaches the user that the interface is untrustworthy.

**Where this applies outside UI design:**
- **API design:** A boolean parameter should be a boolean, not an integer that happens to use 0 and 1. An enum with 4 values should be an enum, not a string the client can set to anything. Match the API type to the parameter's useful resolution.
- **Software configuration:** A config parameter with 3 meaningful values ("low", "medium", "high") should not accept an integer 0-100. The precision of the input should match the precision of the system's response.
- **Product strategy:** Pricing tiers (Free / Pro / Enterprise) are satisficing — 3 options that cover the meaningful segments. A continuous pricing slider from $0 to $999 optimises on a dimension customers do not value (exact dollar amount) at the cost of decision complexity.
- **Hardware design:** A status LED that indicates 3 states (off, on, error) needs 3 colours, not a 16-million colour RGB LED. Over-precise hardware for low-resolution information is waste.

---

## OODA Loop Mapping

The 12-step process embeds multiple OODA cycles, not one. Boyd's insight is that the *orientation* phase — where you update your mental model — is the decisive phase. Speed of observation is less important than accuracy of orientation.

| Phase | Steps | What happens |
|-------|-------|-------------|
| **OODA Cycle 1: Problem Framing** | | |
| Observe | Step 1 (surface real problem) | Observe the symptom: "UI feels outgrown" |
| Orient | Step 1 (diagnose disease) | Reframe: this is information architecture, not visual design |
| Decide | Step 2 (research right domain) | Decide to study controller UX, not colour palettes |
| Act | Step 2 (deploy UX researcher) | Execute the research |
| **OODA Cycle 2: Structure** | | |
| Observe | Step 3 (research findings) | Observe the universal two-level navigation pattern |
| Orient | Step 4 (audit what works) | Reframe against existing design: header/footer already work |
| Decide | Step 3 (propose structure) | Decide on Unit A persistent + Unit B contextual |
| Act | Step 3 (render visually) | Produce the mockup |
| **OODA Cycle 3: Visual Treatment** | | |
| Observe | Step 5 (hierarchy problem) | Observe: same borders on frame and content kill hierarchy |
| Orient | Step 5 (differentiation principle) | Reframe: 3 visual levels needed |
| Decide | Steps 6-7 (placement + treatment) | Decide sidebar position, visual treatment |
| Act | Steps 8-9 (colour semantics + harmony) | Execute colour system |
| **OODA Cycle 4: Parameter Allocation** | | |
| Observe | Step 10 (duplicate audit) | Observe: 5 of 7 params already exist elsewhere |
| Orient | Step 11 (constraint application) | Reframe against hard constraints: physical layout, no hidden modes |
| Decide | Step 12 (control precision) | Decide: tap buttons for low-resolution params |
| Act | (implementation) | Build the final layout |

**What OODA reveals:** Each cycle's *Orient* phase is where the critical reframing happens — and in every case, it was forced by user correction, not generated independently. The Observe phase collected the right data, but the Orient phase defaulted to the most obvious frame until corrected. This suggests the process would benefit from a *forced reframing* step: before committing to an orientation, deliberately generate at least one alternative frame. See "Missing Principles" below.

---

## First Principles Analysis

Steps 1, 2, 10, 11, and 12 are all first-principles reasoning — they decompose to fundamentals rather than reasoning by analogy.

| Step | What is decomposed | Fundamental truth revealed |
|------|-------------------|---------------------------|
| 1 | "UI looks cluttered" | Structure determines clutter, not style |
| 2 | "What should we research?" | The question determines the answer space |
| 10 | "Where does this param go?" | Every param has exactly one natural home |
| 11 | "Which option is best?" | Constraints eliminate; you do not need to evaluate what constraints have already killed |
| 12 | "How should this param be controlled?" | Useful resolution determines control method, not theoretical range |

Steps 4-9 are NOT first-principles — they are *empirical*. They generate options and evaluate by visual inspection. This is appropriate: visual aesthetics cannot be derived from first principles. They must be evaluated perceptually. The process correctly uses first-principles reasoning for structural/logical decisions and empirical evaluation for perceptual/aesthetic decisions.

---

## Missing Principles

The frameworks above reveal 6 decision-making dimensions that the 12-step process does not explicitly address. Each is derived from a specific framework and would have changed at least one decision in the process if applied.

### Missing Principle A: Reversibility Assessment

**Source framework:** Jeff Bezos's Type 1 / Type 2 decision framework; options theory in economics.

**The gap:** The 12 steps do not distinguish between reversible and irreversible decisions. Choosing a sidebar colour scheme is highly reversible (change a constant, re-render). Choosing the physical encoder-to-screen mapping is *not* reversible without re-flashing firmware and potentially re-designing hardware.

**What this adds:** For irreversible decisions, invest more in option generation and constraint analysis. For reversible decisions, satisfice earlier — you can always change it. The process spent roughly equal effort on colour harmony (reversible in minutes) and encoder mapping (irreversible without firmware changes). The effort allocation should be weighted toward the irreversible decisions.

**Generalised principle:** *Before investing analysis effort, classify the decision as reversible or irreversible. Invest disproportionately in irreversible decisions. For reversible decisions, commit faster and iterate.*

### Missing Principle B: Sensitivity Analysis

**Source framework:** Decision analysis (Raiffa, 1968); Monte Carlo simulation.

**The gap:** The 12 steps do not ask: "How much does the outcome change if our assumptions are wrong?" The assumption that Spread has only 5 meaningful positions is plausible — but what if a future effect makes Spread values between 30 and 37 degrees perceptually distinct? The decision to use a tap button becomes wrong.

**What this adds:** For each key assumption, ask: "What would change our decision?" If the answer is "almost nothing could change it" (e.g., the constraint that encoders are physically stacked top/bottom), the decision is *robust*. If the answer is "a new effect type could change it" (e.g., Spread resolution), the decision is *sensitive*. Sensitive decisions should either be designed for adaptability or explicitly documented as assumption-dependent.

**Generalised principle:** *For each decision, identify the assumptions it rests on and ask: "If this assumption is wrong, how costly is it to change the decision?" Decisions resting on fragile assumptions should be designed for change.*

### Missing Principle C: Second-Order Effects

**Source framework:** Systems thinking (Meadows, 2008); causal loop diagrams.

**The gap:** The 12 steps evaluate each decision's direct effects but do not trace downstream consequences. For example, making Spread and Strength into tap buttons (Step 12) has a second-order effect: the entire persistent row is now uniform (all tap-to-cycle). This uniformity is a *benefit* — it simplifies the mental model. But it was identified as a side-effect, not as an explicit design goal.

**What this adds:** After each decision, ask: "What does this cause in adjacent subsystems?" Removing duplicate params from the EDGE/CLR tab (Step 10) had the second-order effect of reducing the tab to 2 params, which triggered the question of whether the tab should exist at all. If second-order effects had been traced proactively, this question would have arisen earlier.

**Generalised principle:** *After each decision, trace its effects one level downstream. Ask: "What does this change or enable in adjacent components?" Second-order benefits should be captured; second-order harms should be mitigated before commitment.*

### Missing Principle D: Temporal Robustness (Future-Proofing)

**Source framework:** Real options theory (Myers, 1977); Agile's YAGNI (You Aren't Gonna Need It).

**The gap:** The 12 steps optimise for the current parameter set (50+ params) and current hardware (16 encoders, 1280x720 screen). They do not ask: "Will this design accommodate the next 20 params without a second redesign?"

**What this adds:** This is the tension between real options theory ("keep options open for the future") and YAGNI ("don't build for hypothetical requirements"). The resolution: *Design the structure to be extensible. Design the content to be current.* The 4-tab sidebar structure should accommodate a 5th tab without redesign (extensible structure). But the content of each tab should be exactly what is needed now (current content, no placeholders for hypothetical params).

**Generalised principle:** *Ask: "Does this decision close any doors we might want open in 6 months?" If a minor structural change now preserves optionality, take it. If preserving optionality requires significant complexity, document the constraint and accept it.*

### Missing Principle E: Forced Reframing

**Source framework:** de Bono's Lateral Thinking (1967); Red Team/Blue Team methodology.

**The gap:** The OODA analysis revealed that every reframing in the 12-step process was forced by user correction — the agent did not independently generate alternative frames. This is a systematic weakness. Without the user's corrections, the process would have converged on visual design solutions to a structural problem.

**What this adds:** At each Orient phase, deliberately generate at least one alternative frame before committing. "The UI feels cluttered" could be: (a) visual hierarchy problem, (b) information architecture problem, (c) parameter count problem (too many params exposed), (d) interaction model problem (wrong input modalities). Generating all four frames takes minutes. Committing to the wrong frame costs the entire investigation.

**Generalised principle:** *Before committing to a problem diagnosis, generate at least two alternative framings. If you cannot generate alternatives, you do not understand the problem well enough to solve it.*

### Missing Principle F: Decision Documentation with Rationale

**Source framework:** Architecture Decision Records (ADRs); Design Rationale systems (Lee, 1991).

**The gap:** The 12-step process document is itself the evidence that this principle was missing from the *original* process. The document was written after the fact to capture reasoning that was not recorded during the process. If decisions had been documented as they were made — with rationale, alternatives considered, and constraints applied — the process would have been self-correcting earlier.

**What this adds:** Each decision point should produce a record: what was decided, what alternatives were considered, what constraints eliminated them, and what assumptions the decision rests on. This is not overhead — it is the mechanism by which future designers avoid re-discovering the same dead ends.

**Generalised principle:** *Record the decision, the alternatives, and the reason for elimination — not just the outcome. The decision record is as valuable as the decision itself, because it prevents future re-exploration of eliminated paths.*

The existing document (`DESIGN_DECISION_PROCESS.md`) now serves this function retroactively, but the principle should be applied *during* the process, not after.

---

## Synthesis: The 12 Principles Organised by Framework

| Framework | Principles Covered | Contribution |
|-----------|-------------------|-------------|
| Root Cause Analysis / Dual Process | 1 | Distinguish System 1 (symptom) from System 2 (cause) |
| Problem Framing (Schon) | 2 | The frame determines the solution space |
| Externalisation Theory (Kirsh) | 3 | Fidelity of representation must match fidelity of decision |
| RPD (Klein) | 4 | Experts recognise and preserve working elements |
| Gestalt Principles | 5 | Figure-ground segregation extended to 3+ layers |
| DSM / Hierarchical Decomposition | 6 | Decisions decompose; sub-decisions have dependencies |
| SBCE / Divergent Thinking | 7 | Carry multiple options forward; breadth before depth |
| Data-Ink Ratio / Shannon | 8 | Every property is signal or noise; no neutral decoration |
| Design Space Exploration | 9 | Theoretical frameworks define complete option spaces |
| MDL / Normalisation (Codd) | 10 | Redundancy creates anomalies; one home per datum |
| CSP / EBA | 11 | Constraint propagation eliminates; order matters |
| Satisficing / Affordance Theory | 12 | Control precision must match parameter resolution |

---

## Applicability Beyond UI Design

Every principle in the 12-step process has a direct analogue in at least 3 non-UI domains. This is because the underlying frameworks are domain-independent — they describe *how to make decisions under constraints*, not *how to design user interfaces*.

The process is, at its core, a **constrained creative decision methodology**:

1. Frame the problem correctly (Steps 1-2)
2. Generate structural options, preserving what works (Steps 3-4)
3. Refine through visual/empirical evaluation (Steps 5-9)
4. Eliminate through constraint satisfaction (Steps 10-11)
5. Match solution granularity to problem granularity (Step 12)

This sequence applies to any design problem where the solution space is large, constraints are hard, and evaluation requires externalisation. Software architecture, hardware design, API design, product strategy, and organisational design all share these properties.

The 6 missing principles (reversibility, sensitivity, second-order effects, temporal robustness, forced reframing, decision documentation) are the meta-cognitive layer — they do not produce solutions directly but improve the quality of the decisions that do.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:analysis | Created. Maps 12 design decision principles to established decision theory, cognitive science, and engineering frameworks. Identifies 6 missing principles. |
