# Design Decision Process: How We Arrived at the Correct Parameter Layout

This document captures the exact cognitive and semantic process that was applied — step by step — to arrive at the correct Tab5 UI parameter allocation. This process was not intuitive to the AI. It was forced through repeated correction by the product owner until the correct reasoning discipline was internalised.

---

## The Process (12 Steps)

### Step 1: Surface the Real Problem (not the symptom)

**What happened:** The user said the UI felt "outgrown." The initial response was to treat this as a visual design problem — fonts, colours, card sizes. Three research agents were deployed on aesthetics, controller references, and brand identity.

**What was wrong:** The problem was not how things looked. The problem was what things were shown, where, and why. Information architecture, not visual design.

**The correction:** "This is fundamentally a menu system design issue." The user identified that the actual problem was navigation and parameter organisation — how 50+ params map to 16 encoders and a touchscreen. No amount of font changes or colour palettes fixes a structural layout problem.

**Principle: Diagnose the actual disease, not the visible symptom.** A UI that "feels cluttered" might be a visual hierarchy problem, but it might also be an information architecture problem. The fix for each is completely different. Applying visual polish to a structural problem wastes everything.

---

### Step 2: Research the Right Domain

**What happened:** After the correction, a UX researcher was deployed to study how professional controllers (Elektron, Push, grandMA3, Resolume, Eos) handle deep parameter sets on limited physical inputs.

**What was learned:** Every professional controller uses a two-level navigation model — persistent controls that never change, plus contextual controls that change with mode/page/focus. This is not a design opinion. It is a universal pattern across every successful hardware controller.

**Principle: Research must target the actual problem domain.** Researching "dark mode colour palettes" was irrelevant. Researching "how do 16 encoders manage 50+ params" was the right question. The quality of your research is determined by whether you asked the right question, not by how thoroughly you researched the wrong one.

---

### Step 3: Propose Structure Before Visuals

**What happened:** The menu system research produced a concrete navigation architecture: Unit A (encoders 0-7) = persistent global params, Unit B (encoders 8-15) = contextual via sidebar tabs. Four tabs: FX PARAMS, EDGE/CLR, ZONES, PRESETS.

**What was right:** The structure was proposed as an information architecture document — not a mockup, not a visual design. The navigation tree, the encoder mapping, the switching mechanism, the maximum depth to any parameter.

**What was wrong:** It was presented as ASCII art, which the user could not evaluate.

**The correction:** "ASCII is not going to cut it AT ALL. HTML renders only." The user cannot evaluate a layout they cannot see. A structural proposal must be rendered visually to be reviewed.

**Principle: Structure must be proposed before visuals, but structure must be SHOWN visually to be evaluated.** The correct sequence is: (1) define the architecture in words, (2) render it as a visual mockup, (3) get feedback on the visual. Never skip step 2 and expect the user to evaluate ASCII art or text descriptions.

---

### Step 4: Preserve What Works

**What happened:** The first visual mockups threw away the existing header and footer design and replaced them with minimal thin treatments. The user immediately rejected this.

**The correction:** "The header and footer widgets from the current design have a significantly superior visual layout/hierarchy compared to the proposed layout which looks weak and disjointed."

**What was learned:** The existing header and footer — with their bold white 2px borders and elevated surface — already solved the framing problem. They create distinct "windows" that bookend the dashboard. The redesign should change the CONTENT area, not destroy the frame.

**Principle: Audit what already works before redesigning. Preserve it.** Not everything in a "bad" design is bad. The current design's header/footer framing was excellent. Its card treatment was the problem. Throwing away the good with the bad is lazy redesign. Surgically identify what works (keep), what's neutral (evaluate), and what fails (fix).

---

### Step 5: Create Visual Hierarchy Through Differentiation, Not Uniformity

**What happened:** The user identified that the current design's problem was not the white borders themselves, but that the SAME white borders appeared on both the frame (header/footer) AND the content (param cards). This destroyed visual hierarchy — the frame blended into the content.

**The correction:** "By removing the 2px white border around the param cards and just implementing the yellow border that highlights whichever param is currently active gives the dashboard landscape 'levels' of separation."

**What was learned:** Visual hierarchy is created by CONTRAST between elements, not by making everything look premium. The solution:
- Frame (header/footer): bold white 2px borders — stands out
- Content (param cards): subtle 1px dark borders — recedes
- Active element: yellow accent border — draws the eye

Three distinct visual levels. Each serves a different cognitive function: frame = orientation, content = information, active = focus.

**Principle: Hierarchy requires at least 3 visual levels with clear differentiation between them.** If everything uses the same border/surface/font treatment, nothing has priority. The user's eye has nowhere to land. Each level must be visually distinct enough that the hierarchy is obvious at arm's length.

---

### Step 6: Explore Placement Exhaustively Before Committing

**What happened:** The tab selector needed a position. Three variants were generated: under header, above footer, left sidebar. The user chose left sidebar but then asked for a sub-variant: sidebar outside the frame vs inside the frame.

**What was learned:** The "obvious" placement (horizontal tabs under the header) was not the best. The sidebar was superior because it doesn't consume horizontal space from the 8-column param grid. But the sidebar's relationship to the frame (inside vs outside) was a separate design decision that required its own A/B comparison.

**Principle: Placement decisions have sub-decisions. Test each level.** Don't stop at "sidebar is better than tabs." Ask: "sidebar outside frame or inside frame?" Each sub-decision changes the visual relationship between elements. Generate the variants and let the eye decide — don't reason about it abstractly.

---

### Step 7: Explore Visual Treatment Exhaustively

**What happened:** With sidebar-inside-frame chosen, six visual treatments were generated for how the sidebar and content area create hierarchy: darker/recessed, raised panel, frame material, tab bleeds, gradient edge, backlit slots.

**Why six:** The user explicitly said "Don't stop at 2-3, generate 5-6 if possible. The more designs we can definitively rule out or roll with, the better."

**What was learned:** You cannot predict which visual treatment will "feel right" from a text description. "Backlit slots" sounds like a specific choice, but you need to see it rendered against the actual layout with actual data to evaluate it. Generating more options costs minutes. Making the wrong choice costs a full redesign cycle.

**Principle: Generate more options than you think you need for visual/aesthetic decisions.** The cost of generating an extra mockup is 5 minutes. The cost of committing to the wrong aesthetic and discovering it later is hours of rework. When in doubt, show more options. Kill fast, commit slow.

---

### Step 8: Use Colour Semantically, Not Decoratively

**What happened:** The user spotted that mockup 06 (zones tab) used distinct colours per zone (cyan, green, orange, purple) and said "The colour contrast is the correct decision." This led to each sidebar tab getting its own colour identity.

**What was learned:** Colour on this interface is not decoration. It is INFORMATION. When you switch from the FX tab (gold) to the ZONES tab (purple), the colour shift across the entire bottom zone — bars, borders, active highlights — immediately communicates "you are in a different context." The user doesn't need to read the tab label. The colour tells them.

**Principle: Every visual property must carry information.** If a colour doesn't tell the user something (which mode, which state, which priority), it's decoration. Decoration competes with information for visual attention. On a 1280x720 controller screen with 16 encoders, every pixel must work.

---

### Step 9: Test Colour Harmony Systematically

**What happened:** With colour-per-tab decided, the anchor colour was fixed (brand gold #FFC700). Five colour wheel harmonies were generated: analogous, complementary, split-complementary, triadic, tetradic.

**Why systematic:** The user did not say "use blue and purple." The user said "test them all." Colour harmony is a solved problem in colour theory — the wheel gives you mathematically balanced options. Testing all five eliminates subjective guessing and lets the eye pick the winner from a complete option set.

**The choice:** Split-complementary, pushed to neon intensity. The hues (azure, purple, teal) against the dark background create maximum legibility while maintaining harmony with the gold anchor.

**Principle: When a decision has a known theoretical framework, use it exhaustively rather than guessing.** Colour harmony, typography scales, grid systems — these are not opinions. They are mathematical relationships. Generate the full set, evaluate visually, pick. Don't guess "blue might look nice."

---

### Step 10: Audit for Duplicates RUTHLESSLY

**What happened:** The EDGE/CLR tab was proposed with: EM Mode, EM Spread, EM Strength, Spatial, Temporal, Gamma, CC Mode. The user caught that Gamma was already in the mode buttons. It was removed.

Then the user caught that EM Mode, Spatial, and Temporal were ALSO already in the mode buttons. Three more duplicates. The tab went from 7 params to 2.

**What was wrong:** The parameter allocation was done by CATEGORY ("EdgeMixer params go in the EdgeMixer tab") rather than by CHECKING WHAT ALREADY EXISTS. The mode buttons already showed Mode, Spatial, and Temporal. Putting them in the tab too meant the user would see "Triadic" in the EDGEMIXER button AND "Triadic" in the EM Mode card — simultaneously, on the same screen.

**The correction:** "If you took the fucking time to give a fucking shit about what you're doing — it will become very obvious what they are."

**Principle: Every parameter must have exactly ONE home. Before placing any param, check every other location on every screen.** Duplicates don't just waste space — they create cognitive confusion. The user sees the same value in two places and wonders: which one is the source of truth? Which one do I interact with? Are they synced? A duplicate is never harmless. It is always a design failure.

**Implementation: build a complete parameter inventory FIRST.** List every controllable param in the system. For each, write down where it currently appears. If it appears in more than one place, one of those places is wrong. Resolve before designing anything.

---

### Step 11: Let Constraints Kill Options

**What happened:** With only 2 orphan params (Spread, Strength), five options were proposed for where to put them. The user killed Option E (encoder-responsive mode buttons) with: "Anything that requires the user to remember the existence of any modes is a failure."

Then the physical constraint killed encoder assignment: "This would totally fuck up encoder assignment because the encoders are physically stacked ENC-A (top) ENC-B (bottom)." The physical layout creates a natural visual-spatial mapping: top encoders = top screen area, bottom encoders = bottom screen area. Assigning bottom-zone encoders to top-zone params violates this spatial mapping.

**What was learned:** Constraints are not limitations to work around. They are FILTERS that eliminate wrong options. The physical encoder layout is a hard constraint. The "no hidden modes" usability requirement is a hard constraint. Each constraint eliminates options until only the correct solution remains.

**Principle: State all constraints FIRST, then evaluate options against them.** Don't generate 5 options and then check constraints. You'll waste time on options that were never viable. Instead: list constraints, then generate only options that satisfy all of them. If an option violates a constraint, it's dead — don't try to salvage it.

**The constraints that killed options:**
- "No hidden modes" → killed Option E (encoder-responsive buttons)
- "Physical encoder stacking" → killed encoder assignment for top-zone params
- "User must see it to know it exists" → killed paging/page-buried solutions

---

### Step 12: Ask Whether Fine Control Adds Value

**What happened:** The final question for Spread (0-60) and Strength (0-255): does the user need fine-grained encoder control, or are fixed increments via tap sufficient?

**The analysis:** Spread has 61 possible values but only 5-6 meaningful positions. The user cannot visually distinguish 30° from 37° of hue rotation on an LED strip. Strength is 0-255 but practically used at Off/Light/Medium/Full. Both params have low benefit from continuous control and high benefit from instant, predictable tap-to-cycle.

**The decision:** Both become tap buttons. The 6-button persistent row is all tap-to-cycle, all the same interaction pattern. No encoder assignment confusion. No hidden modes. No duplicates anywhere.

**Principle: The interaction method must match the parameter's useful resolution.** A parameter with 256 values but only 4 useful positions does not need an encoder. It needs a toggle. A parameter with meaningful gradations across its full range (like Brightness 0-255) justifies an encoder. Match the control precision to the parameter's useful precision. Over-precise controls for low-resolution params create friction without value.

---

## Summary: The 12 Principles

1. **Diagnose the disease, not the symptom.** A cluttered UI might be visual hierarchy OR information architecture. The fix is different.
2. **Research the actual problem domain.** "How do 16 encoders manage 50+ params" not "what colours look good on dark backgrounds."
3. **Propose structure before visuals, but render structure visually.** Architecture in words, evaluation in pixels.
4. **Audit what works before redesigning.** Preserve the good. Fix the bad. Don't nuke everything.
5. **Hierarchy requires 3+ visual levels with clear differentiation.** Frame / content / active — each visually distinct.
6. **Placement decisions have sub-decisions. Test each level.** Don't stop at "sidebar." Ask "inside or outside the frame?"
7. **Generate more visual options than you think you need.** Cost of extra mockup: minutes. Cost of wrong commitment: hours.
8. **Every visual property must carry information.** If colour doesn't mean something, it's competing with things that do.
9. **Use theoretical frameworks exhaustively.** Colour wheel, not gut feel. Grid systems, not eyeballing.
10. **Every parameter gets exactly ONE home. Audit ruthlessly.** Build a complete inventory. Check for duplicates before designing.
11. **Let constraints kill options.** Physical layout, usability rules, and interaction patterns are filters. Use them early.
12. **Match control precision to parameter useful resolution.** Encoder for continuous, tap for discrete. Don't over-engineer low-resolution params.

---

## What This Process Produced

**Starting state:** 116 data points on one screen, flat visual hierarchy, 4 proposed tabs with duplicate params scattered across them.

**Ending state:**
- 8 persistent gauge cards (Unit A, encoder-controlled)
- 6 persistent tap-to-cycle mode buttons (GAMMA, COLOUR, EDGEMIXER, SPATIAL, EM SPREAD, EM STRENGTH)
- 3 sidebar tabs (FX PARAMS, ZONES, PRESETS) with contextual Unit B params
- Zero duplicate params anywhere in the system
- Every param has exactly one home justified by its interaction requirements
- Physical encoder layout respected (top = top, bottom = bottom)
- Neon split-complementary colour identity per tab
- Backlit slot sidebar with inside-frame positioning
- Header/footer preserved from current design

Every element earned its place. Nothing is there by default or by category assumption. Each param was individually evaluated for: where it lives, how it's controlled, and whether it duplicates anything else.

---

*Documented: 2026-03-22*
*This process was not followed naturally. It was imposed through iterative correction. The document exists so it can be followed deliberately in future design work.*
