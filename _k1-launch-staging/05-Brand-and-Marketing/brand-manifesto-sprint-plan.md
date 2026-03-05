# Brand Manifesto Sprint Plan
**K1 Launch Planning — Brand & Marketing**
Date: 2026-03-05
Status: Draft (v1)

---

## What This Is (and Is Not)

The brand manifesto is a **foundational document**, not marketing copy. It declares what SpectraSynq believes, what it opposes, and why the K1 exists. Every subsequent piece of content — product page, social post, packaging insert, press briefing — derives from this document.

It is NOT: a tagline exercise, a feature list, a brand guidelines document, or a style guide. Those are downstream outputs that the manifesto informs.

---

## Sprint Objective

Produce a complete v1 Brand Manifesto for SpectraSynq in 5 working days. The document must be:

1. **Emotionally resonant** — it makes someone care before they know what K1 does
2. **Operationally useful** — any team member can read it and make a brand-consistent decision
3. **Short** — target 400–600 words. If it needs to be longer than 600, something is wrong.

---

## Required Inputs (Pre-Sprint)

These must exist BEFORE Day 1 begins. The sprint cannot start without them.

| Input | Status | Owner | Notes |
|-------|--------|-------|-------|
| Quiet Confidence voice spec (`quiet-confidence-spec-v0.1.md`) | **MISSING** — not found in staging area | Captain | Core voice reference. Sprint is blocked without this. |
| Colour palette decision (Void / Mineral / Oxide) | Awaiting decision | Captain | Palette research complete. Need selection. |
| Recognition device decision (Dark Room Glow + Thin Edge Profile) | Awaiting decision | Captain | Research complete (v1). Need confirmation. |
| War Room brand/GTM knowledge notes | **INACCESSIBLE** — host filesystem only | Captain | Need relevant notes exported to launch staging. |
| Captain's "why I built this" statement | **NEEDED** — does not exist | Captain | 3–5 sentences, raw and unpolished. The founder's conviction in their own words. Cannot be fabricated by an agent. |

### Pre-Sprint Blockers

**Critical:** The Quiet Confidence spec and Captain's personal "why" statement are hard dependencies. The manifesto's authenticity depends on founder voice, not agent-generated copy. Without these, the sprint produces a template at best.

**Recommended:** Captain exports relevant War Room notes (brand, GTM, positioning) into `_k1-launch-staging/05-Brand-and-Marketing/inputs/` before Day 1.

---

## 5-Day Sprint Plan

### Day 1: Antagonist & Belief System

**Goal:** Define what SpectraSynq opposes and what it believes.

**Deliverable:** `manifesto-draft-day1-antagonist.md`

**Work:**
- Identify the antagonist. [HYPOTHESIS] Candidates: the attention economy's visual noise, LED products that prioritise spectacle over atmosphere, technology that demands attention rather than serving the space.
- Draft 3 antagonist framings (1–2 sentences each). Captain selects one.
- Extract 3–5 core beliefs from the Quiet Confidence spec, product design decisions, and Captain's "why" statement.
- Write beliefs as declarative statements: "We believe [X]." No hedging.

**Decision required from Captain:** Select antagonist framing by end of Day 1.

---

### Day 2: Origin Story & Founding Tension

**Goal:** Articulate why K1 exists and what tension it resolves.

**Deliverable:** `manifesto-draft-day2-origin.md`

**Work:**
- Distil Captain's "why" statement into a founding narrative (3–4 sentences max).
- Define the tension: what was wrong or missing in the world that made K1 necessary?
- Connect antagonist (Day 1) to origin: the founding tension should be a direct response to what the brand opposes.
- Draft the opening section of the manifesto (100–150 words).

**Decision required from Captain:** Approve or redirect the founding tension framing.

---

### Day 3: Promise & Philosophy

**Goal:** Define what SpectraSynq commits to — not product features, but principles.

**Deliverable:** `manifesto-draft-day3-promise.md`

**Work:**
- Translate product design decisions into philosophical commitments:
  - Centre origin (79/80 outward) → "Everything begins from one point"
  - No rainbows → "Restraint is a feature"
  - Audio-reactive → "Technology that listens before it speaks"
  - LGP form factor → "Light as surface, not source"
- Draft the promise section (100–150 words). This is the heart of the manifesto.
- Cross-check against Quiet Confidence spec for voice alignment.

**Decision required from Captain:** Confirm or adjust the philosophical commitments.

---

### Day 4: Full Draft Assembly & Voice Pass

**Goal:** Assemble complete v1 manifesto and refine to Quiet Confidence voice.

**Deliverable:** `manifesto-v1-draft.md`

**Work:**
- Assemble Days 1–3 into a single document: Antagonist → Origin → Promise → Call to belief.
- Apply Quiet Confidence voice constraints:
  - Allowed verbs: study, explore, attempt, capture, observe, test, refine, carve, reveal
  - Banned: "humbled", "big things coming", "revolutionising", "changing the game"
  - No "please approve of me" energy
- Trim to 400–600 words. Every sentence must earn its place.
- Internal read-aloud test: if any sentence feels like it belongs on a Kickstarter page, cut it.

**Decision required from Captain:** Read the full draft. Mark any sentence that feels false.

---

### Day 5: Final Edit, Derivative Extracts, and Lock

**Goal:** Lock v1 manifesto and produce derivative assets.

**Deliverables:**
- `manifesto-v1-final.md` — locked manifesto
- `manifesto-derivatives.md` — extracted assets:
  - Tagline candidate (max 6 words, derived from manifesto)
  - Social bio (max 160 chars)
  - 3 pull-quotes for content use
  - "About" page opening paragraph

**Work:**
- Final edit pass with Captain.
- Extract derivatives.
- Cross-check derivatives against recognition device decisions (Dark Room Glow / Thin Edge Profile language should echo manifesto language).
- Lock v1. Any future changes require a versioned revision, not edits.

**Decision required from Captain:** Final sign-off on v1.

---

## Sprint Cadence

- **Daily standup:** 15 min, async or sync. Captain reviews previous day's deliverable and provides direction.
- **Work blocks:** 2–3 hours of focused drafting per day. This is a writing sprint, not a research sprint.
- **Agent role:** Research support, competitive voice analysis, draft iterations. Agent does NOT write the manifesto autonomously — Captain's voice and conviction must be present in every section.

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Quiet Confidence spec doesn't exist yet | HIGH | CRITICAL | Captain writes even a rough v0.1 before sprint starts. 200 words is enough. |
| Captain unavailable for daily decisions | MEDIUM | HIGH | Pre-record decisions on Day 0: select antagonist from shortlist, provide "why" statement, delegate Day 4-5 edit to a trusted reviewer if needed. |
| Manifesto drifts into feature description | MEDIUM | MEDIUM | Hard rule: zero product specs. If K1 is mentioned by name, it must be in the context of belief, not capability. |
| Agent voice overwrites founder voice | HIGH | CRITICAL | Agent generates options; Captain selects and rewrites in their own words. The manifesto must pass the "would Captain say this out loud?" test. |

---

## Success Criteria

The v1 manifesto is complete when:

1. **Captain can read it aloud without cringing.** If any sentence feels performative, it fails.
2. **A stranger who reads it understands what SpectraSynq stands for without knowing what K1 does.** The manifesto works at the belief level, not the product level.
3. **Three derivative assets (tagline, bio, pull-quotes) can be extracted without additional writing.** If derivatives require new writing, the manifesto is incomplete.
4. **The document is under 600 words.** Brevity is non-negotiable.

---

## Next Steps

**Captain must provide before sprint can begin:**

1. The Quiet Confidence spec (even a rough draft — `quiet-confidence-spec-v0.1.md`)
2. A raw "why I built this" statement (3–5 sentences, stream of consciousness is fine)
3. Export any relevant War Room brand/GTM notes to the launch staging directory
4. Confirm colour palette selection (Void / Mineral / Oxide)
5. Confirm recognition device selection (Dark Room Glow + Thin Edge Profile)

Once these inputs land, the sprint can begin the following Monday.

---

## References

- [How to Write a Brand Manifesto — Inkbot Design](https://inkbotdesign.com/brand-manifesto/)
- [Brand Manifesto Definition — TMDesign](https://medium.com/theymakedesign/brand-manifesto-definition-tips-and-examples-a7e737e09b4d)
- [10 Components of a Brand Manifesto — Sales & Marketing](https://salesandmarketing.com/the-10-components-of-a-successful-brand-manifesto/)
- [Why Every Startup Needs a Brand Manifesto — Bolder Agency](https://www.bolderagency.com/journal/why-every-startup-needs-a-brand-manifesto)
- [The Power of Vision, Mission, Values, and Manifestos — Focus Lab](https://www.focuslab.agency/blog/the-power-of-vision-mission-values-and-manifestos)
