---
abstract: "Mandatory agent instructions for UI/layout/design work. 12 failure-mode corrections, pre-design checklist, duplicate audit protocol, and constraint collection template. Read BEFORE generating any mockup, layout, or parameter allocation."
---

# Agent Design Instructions

These instructions are MANDATORY for any agent performing UI design, layout, parameter allocation, information architecture, or visual treatment work. They encode 12 failure modes that AI agents fall into by default, discovered through iterative correction during a real design exercise. Each instruction exists because the default agent behaviour produced the wrong result and had to be corrected by the human.

**Source:** `tab5-encoder/docs/DESIGN_DECISION_PROCESS.md` — the full narrative of how these principles were discovered.

**Scope:** Any task that involves deciding what appears on screen, where it appears, how it is controlled, or how it looks. This includes but is not limited to: UI mockups, parameter allocation, control mapping, navigation design, visual hierarchy, colour selection, and layout proposals.

---

## The 12 Failure Modes and Corrective Instructions

### Failure Mode 1: Treating Symptoms as Diseases

**Default agent behaviour:** User says "the UI looks cluttered" or "this feels outgrown." Agent immediately begins generating cleaner CSS, adjusting font sizes, tweaking padding, or proposing colour palette changes. The agent treats the stated symptom as the diagnosis and jumps to visual fixes.

**Why this is wrong:** A UI that "feels cluttered" could be any of three fundamentally different problems: (a) visual hierarchy failure — the right content is there but styled poorly, (b) information architecture failure — too much content in the wrong places, or (c) functional failure — the wrong content is shown entirely. The fix for each is completely different. Visual polish applied to an information architecture problem wastes the entire session.

**Corrective instruction:**

> Before proposing any solution to a UI complaint, CLASSIFY the problem into one of three categories:
>
> 1. **Visual** — the right content is present and correctly placed, but styling (fonts, colours, spacing, borders) creates confusion
> 2. **Structural** — the content itself is wrong, misplaced, duplicated, or organised by the wrong principle (information architecture failure)
> 3. **Functional** — the underlying data, features, or interactions are wrong regardless of presentation
>
> State your classification explicitly and justify it with evidence from the current state. If the user's complaint could be structural or visual, investigate the structural hypothesis FIRST — structural problems are more expensive to fix later and more likely to be misdiagnosed as visual.
>
> Do NOT proceed to generating solutions until the classification is confirmed.

**Detection heuristic:** Am I about to change how something LOOKS without first questioning whether it should EXIST or be HERE? If yes, stop. Classify first.

---

### Failure Mode 2: Researching the Wrong Domain

**Default agent behaviour:** Agent receives a UI improvement task and immediately researches visual design trends, colour palettes, typography, or "modern dashboard aesthetics." The research is thorough but irrelevant because it targets the wrong problem domain.

**Why this is wrong:** If the actual problem is "how do 16 physical encoders manage 50+ parameters," then researching dark mode colour palettes is wasted work regardless of quality. The quality of research is determined by whether the right question was asked, not by how thoroughly the wrong question was answered.

**Corrective instruction:**

> After classifying the problem (Failure Mode 1), identify the DOMAIN that the solution lives in. Ask: "What category of existing solved problems does this belong to?"
>
> Examples of correct domain identification:
> - "UI feels cluttered" + structural diagnosis = research how professional controllers (Elektron, Push, grandMA3) handle deep parameter sets on limited physical inputs
> - "UI feels cluttered" + visual diagnosis = research visual hierarchy techniques for information-dense dashboards
> - "Navigation is confusing" = research two-level navigation patterns (persistent + contextual) in hardware controller UIs
>
> The research domain must match the problem classification. If you classified the problem as structural, your research must target information architecture, not visual design.

**Detection heuristic:** Does my research query match the problem classification? If I classified "structural" but I am searching for colour palettes or font pairings, I have drifted into the wrong domain.

---

### Failure Mode 3: Proposing Architecture Without Visual Rendering

**Default agent behaviour:** Agent proposes a layout or navigation structure as text description, ASCII art, bullet-point hierarchy, or JSON structure. Then asks the user: "Does this layout work for you?" The user cannot evaluate what they cannot see.

**Why this is wrong:** Humans evaluate layouts visually. A text description of "8 gauge cards in Unit A, 6 tap buttons below, sidebar with 3 tabs on the left" communicates zero about spacing, proportion, visual weight, readability at arm's length, or whether elements compete for attention. ASCII art is marginally better but still fails to convey visual hierarchy, colour relationships, or actual screen proportions.

**Corrective instruction:**

> The correct sequence for any layout/UI proposal is:
>
> 1. **Define the architecture in words** — what elements exist, how they are grouped, what the navigation model is, what changes contextually vs. stays persistent. This is an intermediate working step.
> 2. **Render it as a viewable artefact** — HTML mockup, screenshot, interactive prototype. This is the deliverable the user evaluates.
> 3. **Get feedback on the visual** — the user responds to what they SEE, not what they read.
>
> Text descriptions and ASCII art are INTERMEDIATE STEPS, not deliverables. Never present them as the thing to approve. If you cannot render a visual, explicitly state that limitation and propose how to bridge the gap (e.g., "I will generate an HTML file you can open in a browser").
>
> When generating HTML mockups:
> - Use realistic dimensions matching the target display (e.g., 1280x720 for Tab5)
> - Include actual parameter names and sample values, not "Param 1," "Param 2"
> - Render at the colour depth and contrast ratio of the actual target device
> - Show multiple states if the design involves mode switching (show each mode as a separate mockup)

**Detection heuristic:** Am I about to ask the user to approve a layout they cannot see rendered? If the artefact I am presenting is text, ASCII, or a bullet list, I have not completed step 2.

---

### Failure Mode 4: Redesigning Everything Instead of Auditing What Works

**Default agent behaviour:** Agent receives a redesign task and generates entirely new designs from scratch. Existing header styles, footer treatments, border patterns, colour schemes, and typographic choices are all discarded and replaced. The agent treats "redesign" as "start from zero."

**Why this is wrong:** Not everything in a "bad" design is bad. In the Tab5 case, the header and footer framing (bold white 2px borders, elevated surfaces) was excellent — it created clear visual windows that bookended the content area. The problem was ONLY in the content card treatment (same borders as the frame, destroying hierarchy). Throwing away the good with the bad is lazy redesign.

**Corrective instruction:**

> Before generating any redesign proposal, perform a KEEP/EVALUATE/FIX audit of the current design:
>
> 1. **KEEP** — elements that are working well. Justify why. These are preserved in all proposals.
> 2. **EVALUATE** — elements that are neutral or unclear. These may or may not change.
> 3. **FIX** — elements that are broken or causing the diagnosed problem. These must change.
>
> Present this audit to the user BEFORE generating mockups. The user can correct misclassifications before work begins.
>
> All generated proposals MUST preserve KEEP elements unchanged. If a proposal discards a KEEP element, it must explicitly justify why.

**Detection heuristic:** Am I about to generate a design that replaces everything? Have I identified which specific elements are broken vs. working? If I cannot list 3+ things the current design does well, I have not audited thoroughly enough.

---

### Failure Mode 5: Creating Flat Visual Hierarchy

**Default agent behaviour:** Agent applies uniform styling across all elements — same border weight, same surface treatment, same font emphasis. The result looks "clean" but has no visual hierarchy. The user's eye has nowhere to land because nothing has priority.

**Why this is wrong:** A controller UI must communicate at least three visual levels simultaneously: (a) frame/orientation — where am I? (b) content/information — what are the values? (c) focus/active — what am I currently controlling? If all three use the same border, surface, and emphasis treatment, the user must READ to navigate instead of SEEING.

**Corrective instruction:**

> Every layout MUST define at least 3 distinct visual levels. For each level, specify:
>
> - **Border treatment** (weight, colour, style)
> - **Surface treatment** (background shade, elevation/shadow)
> - **Typography treatment** (weight, size, colour)
>
> The three levels must be VISUALLY DISTINCT from each other at arm's length. If a user squints at the screen from 60cm away, they should be able to identify which elements are frame, which are content, and which are active — purely from visual weight, not from reading text.
>
> Common three-level hierarchy:
> - **Frame** (header/footer/nav): bold borders, high-contrast surfaces — establishes orientation
> - **Content** (cards/values/lists): subtle borders, receding surfaces — conveys information
> - **Active/Focus** (selected item/current encoder): accent colour, elevated surface — directs attention
>
> Test: describe your hierarchy to yourself. If you say "2px border" for more than one level, you have a hierarchy problem.

**Detection heuristic:** Am I using the same border weight, the same background shade, or the same font emphasis for elements at different hierarchy levels? If elements at different levels look the same, I have created flat hierarchy.

---

### Failure Mode 6: Stopping at the First Placement Decision

**Default agent behaviour:** Agent evaluates placement options (e.g., "tabs under header vs. sidebar vs. above footer"), picks one, and moves on. The sub-decisions within the chosen option are never explored.

**Why this is wrong:** Placement decisions are hierarchical. Choosing "sidebar" is a top-level decision, but "sidebar inside the content frame vs. outside the content frame" is a sub-decision that changes the visual relationship between navigation and content. Each level of the hierarchy needs its own evaluation.

**Corrective instruction:**

> After selecting a placement option, ask: "What sub-decisions exist within this choice?"
>
> For any chosen placement, generate at least 2 sub-variants that explore the spatial relationships:
> - Inside vs. outside the parent container
> - Flush vs. inset from edges
> - Fixed width vs. responsive
> - Separated by gap vs. shared border
>
> Present sub-variants as side-by-side mockups. Do not assume the first rendering of a placement choice is optimal.

**Detection heuristic:** Have I made a placement decision and immediately moved to the next element? If I chose "sidebar" but did not explore sidebar sub-variants, I stopped one level too early.

---

### Failure Mode 7: Generating Too Few Options

**Default agent behaviour:** Agent proposes ONE solution — sometimes two — and asks "does this work?" This creates a binary accept/reject dynamic. If the user rejects it, the agent generates one more. This serial exploration is slow, expensive, and fails to explore the option space.

**Why this is wrong:** For aesthetic, layout, and visual treatment decisions, the cost of generating an extra option is minutes. The cost of committing to the wrong choice is a full redesign cycle (hours). Additionally, users often cannot articulate what they want until they see multiple options — comparison reveals preference.

**Corrective instruction:**

> For any decision involving aesthetics, layout, placement, colour, or visual treatment:
>
> - **Minimum 3 options** for placement/structural decisions
> - **Minimum 5-6 options** for visual treatment/aesthetic decisions
> - **Present all options simultaneously** for comparison, not sequentially
> - **Label each option clearly** (Option A, B, C... with one-line description of the differentiating characteristic)
> - **Include at least one "safe" option and one "bold" option** to bracket the range
>
> Do NOT ask "do you want me to generate alternatives?" Just generate them. The user can always say "Option B is perfect, stop here." But they cannot compare options that do not exist.

**Detection heuristic:** Am I about to present a single option and ask for approval? If yes, generate at least 2 more before presenting. Am I about to ask "should I explore alternatives?" — stop asking and just generate them.

---

### Failure Mode 8: Using Colour Decoratively

**Default agent behaviour:** Agent selects colours based on aesthetic preference ("blue looks professional," "this palette is trending") without assigning semantic meaning to each colour choice. Colours are applied for visual appeal rather than to communicate information.

**Why this is wrong:** On an information-dense controller screen, every visual property consumes the user's attention budget. Decorative colour competes with informational colour for that budget. If blue is used because it looks nice, the user's brain tries to decode what blue MEANS — and gets no answer. That is cognitive waste. If blue means "you are in the Zones context," the colour instantly communicates state without requiring the user to read a label.

**Corrective instruction:**

> Before applying any colour to a UI element, answer: "What information does this colour communicate?"
>
> Valid answers:
> - "This colour indicates which mode/context/tab is active" (state communication)
> - "This colour indicates priority/severity/urgency" (importance signalling)
> - "This colour groups related elements" (association)
> - "This colour indicates interactive vs. static" (affordance)
>
> Invalid answer: "This colour looks good" (decoration). If the only justification is aesthetic, either find informational meaning for the colour or remove it.
>
> Exception: brand colours used consistently for brand identity have implicit informational meaning ("this is our product").

**Detection heuristic:** Can I explain what each colour in my design MEANS without using the words "looks good," "aesthetically pleasing," "modern," or "clean"? If any colour has no semantic justification, it is decoration.

---

### Failure Mode 9: Guessing When Theoretical Frameworks Exist

**Default agent behaviour:** Agent makes colour harmony decisions by intuition ("I think purple and gold work well together"), typography scale decisions by eyeballing ("14px for body, 18px for headers seems right"), or grid decisions by feel. The agent has access to mathematical frameworks but does not use them.

**Why this is wrong:** Colour harmony, typographic scales, and grid systems are solved mathematical problems. The colour wheel provides five named harmony relationships (analogous, complementary, split-complementary, triadic, tetradic) that are guaranteed to produce balanced palettes. Using intuition instead of these frameworks means the agent is solving a problem that was solved decades ago — and solving it worse.

**Corrective instruction:**

> When making decisions in domains with established theoretical frameworks, APPLY THE FRAMEWORK EXHAUSTIVELY:
>
> **Colour harmony:** Given an anchor colour, generate ALL five harmony types (analogous, complementary, split-complementary, triadic, tetradic). Render each as a mockup. Let the user pick from the complete set.
>
> **Typography:** Use a modular scale (1.125, 1.200, 1.250, 1.333, 1.414, 1.618). Do not hand-pick sizes.
>
> **Grid systems:** Calculate grid divisions from the target resolution and minimum touch/readability targets. Do not eyeball column widths.
>
> **Spacing:** Use a consistent spacing scale (4px, 8px, 12px, 16px, 24px, 32px, 48px). Do not use arbitrary values.
>
> When presenting framework-generated options, name the framework: "These are the 5 colour wheel harmonies from the anchor #FFC700" — not "here are some colour options I thought might work."

**Detection heuristic:** Am I about to pick a colour, font size, or spacing value by intuition? Does a mathematical framework exist for this decision? If yes, apply the framework instead of guessing.

---

### Failure Mode 10: Placing Elements by Category Without Auditing for Duplicates

**Default agent behaviour:** Agent groups parameters by conceptual category. "EdgeMixer parameters go in the EdgeMixer tab." "Zone settings go in the Zones tab." "Colour settings go in the Colour tab." This category-based placement sounds logical but ignores what ALREADY EXISTS in other locations. The result: the same parameter appears in two places (e.g., in the mode buttons AND in the tab), creating duplicates.

**Why this is wrong:** Duplicates are never harmless. When the user sees "Triadic" in the EDGEMIXER button AND "Triadic" in an EM Mode card, they face three questions: Which is the source of truth? Which do I interact with? Are they synced? This is cognitive overhead that the UI should have prevented. Worse, duplicates indicate that the designer placed elements by ASSUMPTION ("this belongs here because of its category") rather than by INVENTORY ("this does not exist anywhere else yet").

**Corrective instruction:**

> Before placing ANY element in a layout, execute the Duplicate Audit Protocol (see below). This is not optional. This is not "check quickly." This is a systematic, exhaustive search of every screen, every tab, every mode, every persistent element.
>
> The rule: **every parameter gets exactly ONE home.** If a parameter appears in more than one place, one of those places is WRONG. Resolve the duplication before proceeding with the design.
>
> Category-based grouping is a STARTING HYPOTHESIS, not a final allocation. After initial grouping, run the audit. Expect to find duplicates — the first pass almost always has them.

**Detection heuristic:** Am I placing an element because "it belongs in this category"? Have I checked whether that element already exists somewhere else? If I have not searched ALL other locations for this element, I am placing by assumption, not by audit.

---

### Failure Mode 11: Generating Options That Violate Known Constraints

**Default agent behaviour:** Agent generates 5 options for a design decision. The user evaluates them and rejects 3 because they violate constraints that were already known (physical hardware layout, usability principles, interaction patterns). The time spent generating, rendering, and discussing those 3 options was wasted.

**Why this is wrong:** Constraints are not limitations to "work around" — they are FILTERS that eliminate wrong options before any work is spent on them. If the physical encoder layout creates a top-bottom spatial mapping, any option that assigns bottom encoders to top-screen elements is dead on arrival. Generating it wastes time.

**Corrective instruction:**

> Before generating ANY options, collect and list ALL known constraints using the Constraint Collection Template (see below). Then, for each option you are about to generate, verify it against every constraint BEFORE rendering it.
>
> If an option violates a constraint, do NOT generate it, do NOT present it, do NOT mark it as "with caveats." It is dead. Move on.
>
> Present the constraint list to the user alongside your options: "These options were generated within these constraints: [list]. Options violating [constraint X] were excluded."
>
> If the remaining option space after constraint filtering contains fewer than 3 options, report this to the user: "After applying constraints, only N viable options remain." The user may choose to relax a constraint — that is their decision, not the agent's.

**Detection heuristic:** Am I about to generate an option without checking it against my constraint list? Do I even HAVE a constraint list? If I cannot list at least 3 hard constraints for this design task, I have not collected constraints yet.

---

### Failure Mode 12: Assigning Maximum-Fidelity Controls to Every Parameter

**Default agent behaviour:** Agent assigns the highest-fidelity control type (encoder, slider, fine-grained input) to every parameter. The reasoning is "more precision is always better" or "encoders are the primary input, so everything gets an encoder."

**Why this is wrong:** A parameter with 256 possible values but only 4 useful positions (Off, Light, Medium, Full) does not benefit from an encoder. The user scrolls through 252 useless intermediate values to reach the 4 that matter. The encoder becomes FRICTION, not a feature. Meanwhile, a tap-to-cycle button hits the 4 useful values instantly.

**Corrective instruction:**

> Before assigning a control type to any parameter, answer these questions:
>
> 1. **What is the parameter's total range?** (e.g., 0-255, 0-60, enum of 5 values)
> 2. **How many USEFUL distinct values does the user actually need?** (Not the technical range — the perceptually or functionally meaningful positions)
> 3. **Can the user perceive the difference between adjacent values?** (e.g., can they see the difference between hue spread 30 and 37 on an LED strip?)
>
> Control type assignment:
> - **< 8 useful values:** Tap-to-cycle button. Instant, predictable, no overshoot.
> - **8-15 useful values:** Coarse encoder (large step size) OR tap-to-cycle with a longer list.
> - **16-50 useful values:** Encoder with medium step size.
> - **50+ useful values with meaningful gradations:** Fine encoder. This is the ONLY case where a full-range encoder is justified.
>
> Additionally: if the parameter is set-and-forget (rarely adjusted after initial setup), bias toward a simpler control even if the range would technically justify an encoder. Reserve encoder bandwidth for parameters the user actively performs with.

**Detection heuristic:** Am I assigning an encoder to a parameter with fewer than 16 useful values? If yes, reconsider. Am I assigning the same control type to every parameter? If yes, I have not evaluated individual parameters.

---

## Pre-Design Checklist

An agent MUST answer ALL 10 questions before generating any mockup, layout proposal, or parameter allocation. Answers must be written out, not just thought about. If any answer is "I don't know," that gap must be filled before proceeding.

```
PRE-DESIGN CHECKLIST:

1. PROBLEM CLASSIFICATION:
   Is this a visual problem, structural problem, or functional problem?
   Evidence for classification: ___

2. CURRENT STATE AUDIT:
   What currently works well and must be preserved? (List 3+ KEEP items)
   What is broken and must change? (List FIX items)
   What is neutral? (List EVALUATE items)

3. CONSTRAINT INVENTORY:
   Physical constraints (hardware layout, screen size, input devices): ___
   Interaction constraints (no hidden modes, maximum depth to any param): ___
   Technical constraints (update rates, protocol limits, rendering budget): ___
   Brand/visual constraints (colour anchors, typography, existing identity): ___
   Usability constraints (arm's length readability, no memorisation required): ___

4. PARAMETER INVENTORY:
   Total number of controllable parameters: ___
   Complete list with current location of each: ___
   Any duplicates found: ___ (resolve before proceeding)

5. CONTROL TYPE ANALYSIS:
   For each parameter: total range, useful values, perceptual resolution: ___
   Proposed control type for each with justification: ___

6. TARGET DISPLAY:
   Resolution: ___
   Viewing distance: ___
   Colour depth/gamut: ___
   Minimum readable font size at viewing distance: ___

7. NAVIGATION MODEL:
   Persistent elements (always visible): ___
   Contextual elements (change with mode/page): ___
   Maximum depth to any parameter: ___
   How does the user know what context they are in? ___

8. VISUAL HIERARCHY:
   Level 1 (frame/orientation) treatment: ___
   Level 2 (content/information) treatment: ___
   Level 3 (active/focus) treatment: ___
   Are all three visually distinct at arm's length? ___

9. PRIOR ART:
   What professional products solve a similar problem? ___
   What patterns do they share? ___
   What is the relevant research domain? ___

10. DELIVERABLE FORMAT:
    How will proposals be rendered? (HTML mockup, screenshot, prototype): ___
    How many variants will be generated? (minimum 3 for structural, 5 for visual): ___
    What dimensions/resolution will mockups use? ___
```

---

## Duplicate Audit Protocol

Execute this protocol BEFORE finalising any parameter allocation. This is not optional.

### Step 1: Build the Complete Inventory

List EVERY controllable parameter in the system. For each, record:

```
| Parameter Name | Current Location(s) | Control Type | Controlled By |
|----------------|---------------------|--------------|---------------|
| Brightness     | Header gauge card   | Encoder 0    | Unit A Enc 0  |
| Gamma Mode     | Mode button row     | Tap cycle    | Touch         |
| EM Mode        | Mode button row     | Tap cycle    | Touch         |
| ...            | ...                 | ...          | ...           |
```

### Step 2: Search for Duplicates

For each parameter in the inventory:

1. Search ALL screens/tabs/modes/overlays for the parameter name or its abbreviation
2. Search for any control that modifies the same underlying value, even if labelled differently
3. Search for any display-only instance (a read-only indicator showing the same value as an interactive control counts as a potential duplicate if it creates confusion about which is the source of truth)

### Step 3: Resolve Duplicates

For each duplicate found:

1. **Which location is primary?** — where does the user FIRST encounter this parameter in normal workflow?
2. **Which location has the correct control type?** — does the parameter's useful resolution match the control type at each location?
3. **Is the secondary location justified?** — a display-only readout in a status bar MAY be justified if the primary control is buried 2+ levels deep. But the display must be clearly READ-ONLY (no interaction affordance).
4. **If not justified: remove the secondary instance.** Update the inventory.

### Step 4: Verify Zero Duplicates

After resolving all duplicates, scan the inventory one final time:

- Sort by parameter name alphabetically
- Verify each name appears exactly once
- Verify no two entries control the same underlying value

### Step 5: Document the Allocation

The final inventory IS the parameter allocation document. Every element in the design must trace back to one entry in this inventory. If a mockup shows an element not in the inventory, either the inventory is incomplete or the element should not exist.

---

## Constraint Collection Template

Fill this template BEFORE option generation. Every constraint eliminates at least one class of solutions. A design generated without constraint awareness will fail review.

```
CONSTRAINT COLLECTION — [Design Task Name]

=== PHYSICAL CONSTRAINTS ===
(Hardware, spatial, mechanical — cannot be changed by software)

1. Screen resolution: ___
2. Physical input devices and layout: ___
3. Viewing distance and angle: ___
4. Physical mapping rules (e.g., top encoder = top screen area): ___
5. Number of simultaneous inputs: ___

=== INTERACTION CONSTRAINTS ===
(Usability rules derived from user needs and product principles)

1. Maximum depth to any parameter: ___ levels
2. "Anything that requires the user to remember is a failure" — Y/N: ___
3. No hidden modes (every control visible in its context): Y/N: ___
4. Consistent interaction pattern within a group: Y/N: ___
5. Set-and-forget vs. performance parameters: ___ (list each)

=== TECHNICAL CONSTRAINTS ===
(Protocol limits, rendering budgets, update rates)

1. Frame rate / update rate: ___
2. Communication protocol limits: ___
3. Render time budget: ___
4. Memory constraints: ___

=== VISUAL/BRAND CONSTRAINTS ===
(Established identity elements that must be preserved)

1. Anchor/brand colour: ___
2. Typography (font, minimum size): ___
3. Existing elements designated KEEP (from current-state audit): ___
4. Dark/light mode: ___
5. Colour must be semantic, not decorative: Y/N: ___

=== ELIMINATES ===
(For each constraint above, state what classes of solutions it kills)

Constraint 1 eliminates: ___
Constraint 2 eliminates: ___
...
```

---

## Quick Reference: The 12 Principles as Agent Rules

For embedding in system prompts. Each rule is one sentence.

1. **Classify before solving.** Determine whether a UI problem is visual, structural, or functional before proposing any fix.
2. **Research the diagnosed domain.** Match research queries to the problem classification, not to the surface complaint.
3. **Render to evaluate.** Architecture proposals must be visually rendered; text and ASCII are intermediate steps, not deliverables.
4. **Audit before redesigning.** Perform a KEEP/EVALUATE/FIX audit of the current design and preserve what works.
5. **Differentiate hierarchy levels.** Define 3+ visually distinct levels (frame, content, active) — uniform styling destroys hierarchy.
6. **Explore sub-decisions.** Every placement choice has sub-variants; do not stop at the top-level decision.
7. **Generate more options.** Minimum 3 for structural, 5 for visual — the cost of an extra option is minutes, the cost of the wrong choice is hours.
8. **Colour must mean something.** Every colour must communicate state, priority, grouping, or affordance — not just look good.
9. **Use theoretical frameworks.** Colour wheel, typographic scales, grid systems — apply exhaustively instead of guessing.
10. **Audit for duplicates.** Build a complete parameter inventory and verify zero duplicates before finalising any allocation.
11. **Collect constraints first.** List all hard constraints before generating options; do not present options that violate known constraints.
12. **Match control to resolution.** Evaluate each parameter's useful distinct values; assign tap-to-cycle for <8, encoder for 50+.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:prompt-engineer | Created from DESIGN_DECISION_PROCESS.md. 12 failure modes, pre-design checklist, duplicate audit protocol, constraint collection template. |
