# K1 Launch Decision Register

**Governed by:** [GOV-research-pipeline.md](GOV-research-pipeline.md) §9
**Last updated:** 2026-03-06

---

## Active Decisions

### D1: Colour Palette
- **Status:** PENDING — awaiting Captain review
- **Options:** Void / Mineral / Oxide
- **Blocks:** Visual identity, landing page, Blender render library, packaging
- **Research:** colour-palette-research.md
- **Opened:** 2026-03-05
- **Parallel:** Yes — does not block manifesto sprint

### D2: Recognition Device(s)
- **Status:** PENDING — awaiting Captain review
- **Recommendation:** Primary = Edge Line, Secondary = Centre-Out
- **Blocks:** Photography guide, Blender render priority, graphic motif, favicon, social avatar
- **Research:** recognition-device-research.md
- **Opened:** 2026-03-05
- **Parallel:** Yes — does not block manifesto sprint

### D3: Manifesto Enemy
- **Status:** PENDING — **HARD GATE** on manifesto Days 2-5
- **Top candidates:**
  1. **Volume as Meaning** — safest, most specific, directly provable. Content leads with Phase Lock.
  2. **Spectacle Without Substance** — most intuitive, broadest resonance. Content leads with Negative Space.
  3. **Passive Consumption** — most ambitious, asks most of audience. Content leads with interpretation invitations.
- **Blocks:** Manifesto core, content debut sequence, positioning depth, audience filtering
- **Research:** day1-excavation.md, Part 2
- **Opened:** 2026-03-06

### D4: Manifesto Audience
- **Status:** DECIDED — dual-purpose, Nike 1970s model
- **Decision:** Internal alignment first, eventually adapted for public (about page, brand book)
- **Decided by:** Captain, 2026-03-06
- **Rationale:** "Like Nike's manifesto — internal first, but written well enough to publish when the time comes."

### D5: Pricing
- **Status:** EXPLORING — hypothesis stage
- **Range:** $449–$549 (up from original $249)
- **Hypothesis:** $449 gives ~$110/unit BoM headroom at 45% margin. $549 commands attention if first-run experience delivers.
- **Validation needed:** Consumer sentiment research, competitive precedent at $450+ for indie creative hardware, BoM analysis at new price points
- **Opened:** 2026-03-06

### D6: First-Run Choreography
- **Status:** CONCEPT — architecture decisions pending
- **Concept:** QR → welcome page → K1 demo mode → choreographed performance synced to audio → gradual handover to user control
- **Three acts:** Watch This → Now Try This → It's Yours
- **Technical:** ShowDirector actor + REST API. Feasible with existing firmware architecture.
- **Note:** Delivery-time feature, not sprint-now. Goes through War Room research pipeline.
- **Opened:** 2026-03-06

---

## Decision Log (Completed)

| # | Decision | Choice | Date | Decided By |
|---|----------|--------|------|------------|
| D4 | Manifesto Audience | Dual-purpose (Nike model) | 2026-03-06 | Captain |

---

## Governance Notes

- Decisions D1 and D2 can proceed in parallel — they inform visual identity but don't block manifesto text.
- Decision D3 is the **hard gate**. Manifesto Days 2-5 cannot proceed without it.
- Decision D5 requires pipeline validation vectors before it can move from EXPLORING to PENDING.
- Decision D6 is parked for now — it's a delivery-time feature, not a sprint dependency.
