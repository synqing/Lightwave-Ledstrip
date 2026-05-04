---
abstract: "Operator-facing manifest for the K1v2 LightwaveOS infographic source pack — eight curated files (1,517 LOC) that feed NotebookLM Studio composition for a six-panel architecture infographic series. Phase 1 authoring complete; Captain Gate 1 (2026-05-04) resolved three of the four standing items: Amendment Z expanded the zone strike to ALL zone numbering, Amendment L locked Phase 0.7 LED-show wording verbatim, and Ban A9 was confirmed HARD-BAN. Phase 0.2 Pinterest references are now provided by Captain (5 image files at `/Users/spectrasynq/Downloads/`) — supplements only; the agent-authored style bible remains controlling. Phase 0.6 (BACKLOG F-6 firmware bug `CONTROLBUS_NUM_ZONES = 4`) remains deferred; the broader Amendment Z strike supersedes it for infographic purposes. Read first when returning to this bundle."
---

# MANIFEST — LightwaveOS K1v2 Infographic Source Pack

## Bundle summary

| Field | Value |
|---|---|
| Bundle name | LightwaveOS K1v2 Infographic Source Pack |
| Purpose | Curated source pack for NotebookLM Studio infographic generation |
| Build target | `esp32dev_audio_esv11_k1v2_32khz` (canonical K1 V2 production) |
| File count | 8 source files + this MANIFEST |
| Total source LOC | 1,517 lines |
| Path | `docs/tooling/notebooklm-bundles/lightwave_ledstrip_infographics/sources/` |
| Final image output | `media/k1v2_infographics/` (Phase 5 — not yet populated) |
| Plan of record | `~/.claude/plans/quirky-tinkering-falcon.md` (Rev 3, 2026-05-04) |
| Doctrine | NotebookLM = visual synthesis only; this pack = technical truth |
| Overall state | **Phase 1 complete; Phase 2 blocked pending Captain decisions** |

### Per-file line counts

| File | Lines |
|---|---:|
| `00_PROJECT_OVERVIEW.md` | 74 |
| `01_VERIFIED_FIRMWARE_FACTS.md` | 124 |
| `02_PIPELINE_PARTITIONING.md` | 113 |
| `03_STYLE_BIBLE.md` | 235 |
| `04_LABEL_WHITELIST.md` | 161 |
| `05_FORBIDDEN_CLAIMS.md` | 263 |
| `06_PART_OUTLINES.md` | 375 |
| `07_TIMING_BUDGETS.md` | 172 |
| **Total** | **1,517** |

---

## Phase status

| Stage | State | Date | Note |
|---|---|---|---|
| **Gate 0** — plan approval | ✅ Approved | 2026-05-04 | Rev 3 of `quirky-tinkering-falcon.md` accepted by Captain. |
| **Phase 1** — source-pack authoring | ✅ Complete | 2026-05-04 | All 8 source files authored, abstracts populated, changelog footers present. This MANIFEST documents that completion. |
| **Phase 2** — calibration pair (Part 5 + Part 6) | ⏳ Pending | — | Captain Gate 1 (2026-05-04) resolved Amendment Z (expanded zone strike), Amendment L (LED-show wording), Ban A9 (HARD-BAN), and Phase 0.2 (Pinterest references provided). Phase 0.6 firmware fix remains deferred but is superseded for infographic purposes by the Amendment Z strike. Phase 2 may now proceed once Captain explicitly authorises calibration-pair generation. |
| **Gate 1** — calibration pair inspection | ⏳ Not started | — | Captain reviews Part 5 + Part 6 against the pass/fail matrix in plan §Phase 2. |
| **Phase 3** — Gate 1 decision tree | ⏳ Not started | — | Outcome decides whether Phase 4 proceeds, pivots to "composition-only", or abandons NotebookLM Studio for this series. |
| **Phase 4** — full series (Parts 1–4 × 2 variants) | ⏳ Not started | — | Conditional on Gate 1 = "Both pass". |
| **Phase 5** — compositing & manual overlay | ⏳ Not started | — | Final outputs land in `media/k1v2_infographics/`. |
| **Gate 2** — proceed / pivot / abandon | ⏳ Not started | — | Reality Checker review before any external surface consumes finals. |

---

## Resolved items

The following questions are definitively answered as of 2026-05-04 and require no further deliberation:

1. **Four firmware-fact falsifications corrected** in the Captain-supplied instruction set, sourced via clangd-grounded Explore-agent verification:
   - "Kalman-filtered tempo tracking" → corrected to **Goertzel-bank novelty envelope** (`TempoTracker.cpp:1–17`).
   - "512 FFT bins" → corrected to **256 magnitude bins from a 512-sample window** (`PipelineCore.h:40`).
   - "DMA-backed FastLED.show()" → corrected to **RMT-peripheral-driven** output (CPU returns immediately; wire time runs in parallel).
   - "Centre-origin around LED 79/80" → tightened to **centre seam between LEDs 79 and 80, implemented as `CENTER_POINT = 80`** (`OtaLedFeedback.h:53`).
2. **Three instruction `[VERIFY]` hedges now confirmed:**
   - `ControlBusFrame ≤ 5120 bytes` — `static_assert` at `ControlBus.h:261–262`.
   - **Lock-free SnapshotBuffer** — double-buffered `m_buf[2]` + atomic sequence with release/acquire fences at `SnapshotBuffer.h:27–52`.
   - **Zero-heap render contract** — confirmed in `RendererActor` documentation and effect-class static-buffer comments.
3. **K1v2 hardware topology locked** — 320 LGP LEDs only (dual 160). The K1v1 30-LED status strip was a deactivated alt-build; it does NOT appear on K1v2 and is documented only as historical context.
4. **Style bible authored manually** by the agent (Amendment 3) — NotebookLM does not extract style from images. Reference images, if Captain provides any, are supporting sources only.
5. **Three gates locked** (Amendment 4) — Gate 0 (plan), Gate 1 (calibration pair), Gate 2 (proceed/pivot/abandon). No five-phase alternative survives.
6. **NotebookLM-as-composition-only doctrine locked** (Amendment 5) — final labels, tables, and numbers overlaid manually downstream.
7. **Calibration PAIR Part 5 + Part 6 locked** (Amendment 1) — single-pilot Part 5 alone is rejected; both panel grammars must pass before the full series proceeds.

---

## Standing items / deferred (Captain decisions outstanding)

The following items remain open after Captain Gate 1 (2026-05-04). Phase 2 may now proceed once Captain explicitly authorises calibration-pair generation; the open items below are not blockers in their current state.

| # | Item | State | Recommendation |
|---|---|---|---|
| **0.6** | Firmware bug `CONTROLBUS_NUM_ZONES = 4` (`ControlBus.h:22`) violates the 3-zone hard rule. Tracked as **BACKLOG F-6**. | Deferred — superseded for infographic purposes | The firmware fix remains a separate engineering session (audit of `ZoneAGC m_zones[]`, `m_chroma_zones[]`, the 4-band partitioning in `ControlBus.cpp:61, 96, 104, 111, 372, 439`, and `AudioActor.cpp:316`). For infographic purposes, Captain Gate 1 Amendment Z expands the strike to ALL zone numbering (Zone 0, 1, 2, 3, 4) — broader than the original BACKLOG F-6 audio-AGC-only strike, and unaffected by the firmware fix landing. The infographic series ships zone-numbering-free regardless of F-6 state. |

A confirmation that **Phase 0.3** output destination = `docs/tooling/notebooklm-bundles/lightwave_ledstrip_infographics/` (sources + MANIFEST) is treated as accepted by virtue of this bundle now existing at that path; flag if Captain disagrees.

## Resolved at Captain Gate 1 (2026-05-04)

The following items are definitively closed by Captain ruling at Gate 1 and require no further deliberation:

| # | Item | Captain ruling |
|---|---|---|
| **0.2** | Pinterest reference image source | RESOLVED. Captain has provided 5 reference images at `/Users/spectrasynq/Downloads/`. They are supplementary inputs only — the agent-authored `03_STYLE_BIBLE.md` remains the controlling source for visual grammar. References inform, but do not override, the style bible. |
| **0.7** | K1v2 LED wire-time measurement (Amendment L) | RESOLVED. Panel 6 LED-show row ships with Captain's exact wording verbatim: `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` Topology-neutral; no `Verify (K1v2). Measurement pending.` placeholder, no `4.8 ms invariant`, no parentheticals about strip topology on this row. The `~6.3 ms` figure is paired with K1v1 strip-topology context only in the adjacent `Historical (K1v1)` grey-accent footnote. |
| **A9** | Ban A9 (invented firmware identifiers) | RESOLVED at HARD-BAN. Confirmed scope: `bins512`, `KalmanTempo`, `DMABuffer`, "512 FFT bins", "Kalman-filtered tempo", "DMA-backed FastLED.show()". Any of these in a generated panel triggers regeneration. |
| **Z** | Zone numbering across the infographic series (Amendment Z) | RESOLVED. Captain expanded the strike from audio-AGC zones only (BACKLOG F-6) to ALL zone numbering — Zone 0, Zone 4, AND user/API-facing identifiers Zone 1 / Zone 2 / Zone 3. The previous narrow exception that allowed user-facing zone identifiers inside Panel 5's EffectContext card is now CLOSED. EffectContext is described in topology-neutral terms (LED buffer, delta-time, render-context flags, audio snapshot). |

---

## File inventory

Read each file's frontmatter `abstract:` field first when deciding relevance — every file is L0/L1/L2 disclosable.

| File | Abstract (one-line) | Lines | Role | Depends on | Required by |
|---|---|---:|---|---|---|
| `00_PROJECT_OVERVIEW.md` | What K1v2 is, audience, build scope, NotebookLM-as-composition doctrine, bundle manifest summary. Read first. | 74 | Entry / orientation | — | All downstream consumers (orientation) |
| `01_VERIFIED_FIRMWARE_FACTS.md` | Confirmed/Derived/Budgeted/Verify-tiered firmware facts with `file:line` citations; canonical truth source. | 124 | Single source of technical truth | `firmware-v3/` source files (read-only citations) | `04_LABEL_WHITELIST.md`, `05_FORBIDDEN_CLAIMS.md`, `06_PART_OUTLINES.md`, `07_TIMING_BUDGETS.md` |
| `02_PIPELINE_PARTITIONING.md` | Core 0 / Core 1 boundary, actor-model task layout, what crosses the SnapshotBuffer bridge. ALL zone numbering struck per Captain Gate 1 Amendment Z. | 113 | Architectural diagram source | `01_VERIFIED_FIRMWARE_FACTS.md` | Panel 1 (System Overview), Panel 4 (Cross-Core Publication) |
| `03_STYLE_BIBLE.md` | Five style families A–E (palette, layout grammar, typography, icon style, density, prompt phrases) + continuity tokens. Agent-authored. | 235 | Visual grammar / Studio prompt source | — (independent, manual) | All six panel prompts in `06_PART_OUTLINES.md` |
| `04_LABEL_WHITELIST.md` | Exact British-English allowed labels + public-safe phrasing table; rejection rule for any unlisted term. | 161 | Lexical filter for Studio output | `01_VERIFIED_FIRMWARE_FACTS.md` (truth ↔ label mapping) | `06_PART_OUTLINES.md`, all manual-overlay text |
| `05_FORBIDDEN_CLAIMS.md` | Eight banned technical claims, seven structural bans (microtext, wide tables, etc.), substitute wording per ban. | 263 | Negative space / constraint register | `01_VERIFIED_FIRMWARE_FACTS.md` (justifies each ban) | `06_PART_OUTLINES.md`, Studio prompt construction |
| `06_PART_OUTLINES.md` | Six panels: title, style family, purpose, required content, whitelisted labels, forbidden items, continuity anchors. | 375 | Panel-prompt manifest | `01_VERIFIED_FIRMWARE_FACTS.md`, `02_PIPELINE_PARTITIONING.md`, `03_STYLE_BIBLE.md`, `04_LABEL_WHITELIST.md`, `05_FORBIDDEN_CLAIMS.md`, `07_TIMING_BUDGETS.md` | Phase 2 + Phase 4 Studio generation calls |
| `07_TIMING_BUDGETS.md` | Timing values per tier (Confirmed/Derived/Budgeted/Verify): 32 kHz audio, 8 ms hop, 8.33 ms frame, 2.0 ms render ceiling, K1v2 LED show = Verify. | 172 | Panel 6 truth source | `01_VERIFIED_FIRMWARE_FACTS.md` | Panel 6 (Timing & Safety Budget) |

---

## Pre-Phase-2 validation checklist

Every item must be confirmed before any NotebookLM Studio generation call is made.

- [ ] All 8 source files exist at the documented path (validated 2026-05-04 — present, total 1,517 LOC).
- [ ] Public-safe phrasing table in `04_LABEL_WHITELIST.md` matches the Rev 3 plan §Phase 1 table verbatim (FFT, tempo, centre origin, snapshot bridge, ControlBus size, LED timing, LED topology, K1v1 historical, zones).
- [ ] All 8 forbidden claims from Rev 3 plan §`05_FORBIDDEN_CLAIMS.md` bans appear in `05_FORBIDDEN_CLAIMS.md` (Kalman / DMA / exact ControlBusFrame size / 512 bins / bare LED 80 / any zone numbering [Captain Gate 1 Amendment Z — broader than original audio-AGC-only strike] / LED timing without provenance / 350 LEDs or 30-LED status strip). Ban A9 is now HARD-BAN per Captain Gate 1: `bins512`, `KalmanTempo`, `DMABuffer`, "512 FFT bins", "Kalman-filtered tempo", "DMA-backed FastLED.show()".
- [ ] All 6 panels structured in `06_PART_OUTLINES.md`, with the Captain Gate 1 Amendment Z strike enforced per panel (no panel mentions any zone numbering — Zone 0, 1, 2, 3, or 4 — anywhere). Panel 5 EffectContext card is described in topology-neutral terms (no user-facing 1/2/3 identifiers).
- [ ] Every required timing value in `07_TIMING_BUDGETS.md` cross-checks against the corresponding `01_VERIFIED_FIRMWARE_FACTS.md` row at the same tier.
- [ ] Style families A–E documented in `03_STYLE_BIBLE.md` with explicit prompt phrases attached to each.
- [ ] Continuity tokens (rail y-position, palette mapping per data class, edge-margin discipline) locked in `03_STYLE_BIBLE.md`.
- [ ] British English in all body text across all 8 source files (centre, colour, behaviour, initialise, organise, optimisation).
- [ ] Each file carries a Document Changelog footer with at least one Created entry.
- [ ] BACKLOG F-6 status confirmed (open / in progress / resolved) before generating any Panel 4 variant that touches cross-core audio publication.
- [ ] Captain rulings recorded for Phase 0.2 (RESOLVED — Pinterest references provided), 0.6 (deferred — superseded by Amendment Z for infographic purposes), 0.7 (RESOLVED — Amendment L exact wording), Ban A9 (RESOLVED — HARD-BAN), and Amendment Z (RESOLVED — all zone numbering struck). See Resolved items section above.

---

## Audit trail

Verification artefacts that produced this bundle:

- **Internal Explore-agent verification** (Phase 0 of plan Rev 1): 18 firmware files inspected, 5 clangd queries (`find_definition`, `get_document_symbols`, `get_hover` on `ControlBusFrame`, `SnapshotBuffer`, `TempoTracker`, `RendererActor`), 12 grep passes for algorithm names and constants. **Four falsifications + three confirmations** logged in plan Rev 3 §Part 1.
- **External specialist consultant amendments** (between Rev 1 and Rev 2): 6 amendments. The Captain then issued 2 corrections to my synthesis of Amendment 6 (audio-AGC zone-conflation) and Amendment 7 (LED topology). All incorporated in plan Rev 3 §Part 1.5.
- **Captain Gate 1 amendments (2026-05-04)**: Amendment Z (zone-numbering strike expanded to all zones), Amendment L (LED-show exact wording), Ban A9 (HARD-BAN), and Phase 0.2 (Pinterest references provided). Applied to `02_PIPELINE_PARTITIONING.md`, `06_PART_OUTLINES.md`, and this MANIFEST. See per-file Document Changelog footers.
- **Cross-reference: `BACKLOG.md` § F-6** — `CONTROLBUS_NUM_ZONES = 4` firmware bug. Separate engineering session, not infographic-blocking; superseded for infographic purposes by Amendment Z (broader strike, independent of firmware-fix state).
- **Cross-reference: upstream forensic-corrected firmware notebook** — NotebookLM ID `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` ("Lightwave-Ledstrip — K1 Project Knowledge Base — forensic-corrected 2026-05-04"). 127 sterilised firmware sources. **Not modified by this bundle**; this bundle is a downstream tightly-curated derivative.
- **Plan revisions**: Rev 1 (initial), Rev 2 (consultant amendments), Rev 3 (Captain bug-corrections). Full changelog at the foot of `~/.claude/plans/quirky-tinkering-falcon.md`.

---

## Operator notes

Quick-reference for the next agent or Captain returning to this bundle:

1. **The bundle is READ-ONLY for downstream agents.** Modifications go through a plan revision (`quirky-tinkering-falcon.md`) and a fresh authoring pass — not through ad-hoc edits to the source files.
2. **Read frontmatter abstracts first.** Each file carries an `abstract:` ≤100 tokens. Scan abstracts to decide which file(s) to open in full. Do NOT read all eight files unless the task explicitly requires it.
3. **All zone numbering is STRUCK across all six panels** per Captain Gate 1 Amendment Z (2026-05-04). The strike covers Zone 0, Zone 4 (the BACKLOG F-6 firmware-bug excess), AND user/API-facing identifiers Zone 1 / Zone 2 / Zone 3. The previous narrow exception that allowed user-facing zone identifiers inside Panel 5's EffectContext card is now CLOSED. Do not work around the strike with namespacing tricks. EffectContext is described in topology-neutral terms (LED buffer, delta-time, render-context flags, audio snapshot). The strike is independent of BACKLOG F-6 status — it stands regardless of whether the firmware fix lands.
4. **K1v1 historical context appears only in Panels 1 and 6**, with a grey-accent `Historical (K1v1)` badge — never as a current K1v2 fact. The 30-LED status strip is deactivated; the ~6.3 ms LED-show measurement is K1v1-era and does not transfer.
5. **Centre-origin wording is locked** to "Centre seam between LEDs 79 and 80, implemented as `CENTER_POINT = 80`." Bare "LED 79" or "LED 80" alone as the centre description is forbidden — the seam-between formulation is mandatory.
6. **NotebookLM produces composition only.** PNG outputs are background / diagram / glow-trace illustration. All final labels, numbers, and tables are overlaid manually downstream against `01_VERIFIED_FIRMWARE_FACTS.md` and `07_TIMING_BUDGETS.md`. Never trust microtext from a Studio render.
7. **British English throughout.** `centre`, `colour`, `behaviour`, `initialise`, `organise`, `optimisation`. Any American-English term that survives into a final overlay is a defect; the label whitelist is the enforcement mechanism.
8. **If you need to verify the bundle is intact**: `ls docs/tooling/notebooklm-bundles/lightwave_ledstrip_infographics/sources/` should show exactly 8 files, `wc -l` should show 1,517 lines total. Divergence indicates drift; re-baseline against this MANIFEST or the plan before generating any image.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | Claude (claude-opus-4-7) | Created. Documents Phase 1 completion of the K1v2 infographic source pack: 8 source files (1,517 LOC), three Captain decisions outstanding for Phase 2 (Pinterest references, BACKLOG F-6 firmware bug, K1v2 LED wire-time measurement), Ban A9 also pending. Maps every source file's role, dependencies, and downstream consumers. Encodes the audio-AGC zone strike, K1v2 320-LED-only topology lock, and centre-seam wording lock. |
| 2026-05-04 | Claude (claude-opus-4-7) | Captain Gate 1 amendments applied. Resolved at Gate 1: Amendment Z (zone strike expanded to ALL zone numbering — Zone 0, 1, 2, 3, 4 — Panel 5 user-facing exception CLOSED); Amendment L (Panel 6 LED-show row uses Captain's exact wording verbatim — `Source-documented LED show path: ~6.3 ms; K1v2 topology-specific measurement pending.` — replacing the `Verify (K1v2). Measurement pending.` placeholder); Ban A9 confirmed HARD-BAN (`bins512`, `KalmanTempo`, `DMABuffer`, "512 FFT bins", "Kalman-filtered tempo", "DMA-backed FastLED.show()"); Phase 0.2 (Pinterest references provided by Captain at `/Users/spectrasynq/Downloads/`, supplements only — style bible remains controlling). Standing-items table reduced to BACKLOG F-6 only (deferred firmware bug, superseded for infographic purposes by Amendment Z). New Resolved-at-Captain-Gate-1 section added. Validation checklist, audit trail, and operator notes updated for Amendment Z scope. |
