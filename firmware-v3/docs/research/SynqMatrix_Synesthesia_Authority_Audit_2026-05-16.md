---
abstract: "AMBER read-only evidence audit on whether 'Synesthesia / Family B is reference-grade authoritative for SynqMatrix' can stand in the SynqMatrix Director RFC (2026-05-15). Consolidates four parallel SSAs across Claude memory, repo docs, firmware source, and NotebookLM. Finds: Synesthesia RE corpus is real (off-repo); 'Family B' is bundling-layer metadata only; Davies/IBT/MIREX trace to Tier-2 academic compass-artifact, not Synesthesia; no written Captain decision designates Synesthesia/Family B as authoritative. V0 ratification unblocked; Q9.5 'reference-grade' framing must be downgraded. No sign-off authority; no mutations beyond this single file."
---

# SynqMatrix Synesthesia Authority Audit — 2026-05-16

## 0. Executive Verdict

**AMBER.**

Synesthesia is a real, durable, dated (8-9 January 2026) reverse-engineered reference body sitting off-repo at `~/Workspace_Management/Software/Synesthesia/` and is acknowledged in the MusicAware audit as one of two Tier-1 reference bodies. But the chain "Synesthesia → Family B → reference-grade → authoritative for SynqMatrix" only stands at link one. The "Family B" label exists in exactly one place in the entire inventoried corpus — `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64` — as bundling-layer NotebookLM metadata, not as a load-bearing designation; the original Synesthesia RE source documents contain zero "Family A" or "Family B" strings (SSA-4 grep-verified). The decision-maker who adjudicated "Family B over Family A on confidence + octave handling" is unattributed. The strongest sub-claim driving the authority framing — Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms — is explicitly attributed by the MusicAware audit (`firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61-65`) to Tier-2 academic compass-artifact, with Synesthesia Tier-1 stated as "NOT SPECIFIED". The RFC's own RBDO label correctly identifies this as DEGRADED-MODE. V0 sign-off can proceed on live-debounce evidence; the "reference-grade authoritative" framing in Q9.5 must be replaced with the narrower claim the evidence will support.

## 1. Scope and Non-Mutation Guarantee

This audit consolidates four parallel evidence SSAs that inspected:

- **SSA-1** — Claude memory surfaces: `~/.claude/CLAUDE.md`, project-level CLAUDE.md tree, `AGENTS.md`, MEMORY.md plus 45 topic files, settings JSON.
- **SSA-2** — Repository documentation: RFC, MusicAware audit, NOTEBOOK_REGISTRY, k1_songaware_* research suite, Lane D handovers, BACKLOG, CHANGELOG, decision records, marketing.
- **SSA-3** — Firmware source code: `firmware-v3/src/core/synqmatrix/{SynqMatrix.h, SynqMatrix.cpp}`, ESV11 backend and vendor tempo path, BeatTracker, adapter, shim.
- **SSA-4** — NotebookLM corpora: Lightwave-Ledstrip (167 sources), Synesthesia P4 Beat Tracker (85), Hybrid Beat Tracker — Auto BPM RE Port (28), SpectraSynq War Room (93), SpectraSynq Doctrine & Architecture (38).

**Mutations performed by this audit:**

- Firmware source: 0 changes.
- RFC / session handovers / existing research files: 0 changes.
- Claude memory (MEMORY.md, topic files): 0 changes.
- New markdown files created beyond this audit: 0.
- Files written: exactly 1 — this audit, at `firmware-v3/docs/research/SynqMatrix_Synesthesia_Authority_Audit_2026-05-16.md`.

**RBDO label for this audit: GROUNDED.** Every premise is traced to one of the four SSA evidence packages with file:line citations preserved verbatim. Load-bearing citations (NOTEBOOK_REGISTRY:64, MusicAware audit lines 61-65, RFC Q9.2 lines 769-778, RFC Q9.5 lines 798-802, RFC Q4 closure lines 138-145) were independently re-read by the synthesis SSA before quoting.

## 2. Memory Surface Inventory

| # | Surface | Path | Size | Load Status |
|---|---|---|---|---|
| 1 | Global Claude instructions | `~/.claude/CLAUDE.md` | 26 KB | LOADED_AT_START |
| 2 | Global Claude settings | `~/.claude/settings.json` | 4.7 KB | LOADED_AT_START (config only) |
| 3 | Local Claude settings overlay | `~/.claude/settings.local.json` | 765 B | LOADED_AT_START (config only) |
| 4 | Workspace-level CLAUDE.md | `Workspace_Management/Software/CLAUDE.md` | 7.2 KB | LOADED_AT_START |
| 5 | Project CLAUDE.md | `Lightwave-Ledstrip/CLAUDE.md` | ~26 KB | LOADED_AT_START |
| 6 | Project `.claude/CLAUDE.md` | `Lightwave-Ledstrip/.claude/CLAUDE.md` | 523 B | LOADED_AT_START (index only) |
| 7 | Project local override | `Lightwave-Ledstrip/CLAUDE.local.md` | — | NOT_FOUND |
| 8 | Project agent doctrine | `Lightwave-Ledstrip/AGENTS.md` | 10.3 KB | LOADS_ON_DEMAND |
| 9 | Project rules directory | `Lightwave-Ledstrip/.claude/rules/` | — | NOT_FOUND |
| 10 | Project auto-memory index | `~/.claude/projects/-Users-spectrasynq-…/memory/MEMORY.md` | 13.6 KB | LOADED_AT_START |
| 11 | Project auto-memory topic files | (same directory, 45 topic files) | varies | LOADS_ON_DEMAND |

## 3. Search Method

**SSA-1 (Claude memory).** Direct read of each of the eleven surfaces above. Ripgrep over `~/.claude/` and project memory directory for the eleven critical terms: "Synesthesia", "Family A", "Family B", "reverse-engineered", "Davies", "IBT", "MIREX", "reference-grade", "SynqMatrix", "SongAware", "MusicAware". Zero hits across all eleven terms in memory surfaces. Adjacent governance rules (`feedback_no_editorialising.md`, `feedback_research_backed_only.md`) flagged as constraining the handling, not as supporting the claim.

**SSA-2 (Repository documentation).** Repo-wide ripgrep on the same eleven terms across `firmware-v3/docs/`, `firmware-v3/`, repo root, `docs/`, `instructions/`, `BACKLOG.md`, `CHANGELOG.md`. Direct read of: `SynqMatrix_Director_RFC_2026-05-15.md`, `MusicAware_Audit_And_Gap_Analysis.md`, `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md`, all nine `k1_songaware_*_2026-05-12*.md` files, Lane D handovers (`SESSION_HANDOVER_20260514_Lane_D_Evidence.md`, `SESSION_HANDOVER_20260515_SynqMatrix_RFC.md`), `BACKLOG.md`, `CHANGELOG.md`. Branch: `feature/synqmatrix-rename-2026-05-13`; HEAD: `dc46cc9b`.

**SSA-3 (Source code).** Ripgrep over `firmware-v3/src/`, `firmware-v3/include/`, `firmware-v3/test/` on the same terms. Direct read of `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}`, ESV11 backend headers + vendor tempo path, BeatTracker, EsV11Adapter, 32 kHz shim. All hits classified as semantic neighbours (Emotiscope citation, visual-effect family labels, Band-Ratio "authoritative" wording) — none designate Synesthesia / Family B as authoritative for beat-tracking.

**SSA-4 (NotebookLM).** 15 `notebook_query_start` + 15 `notebook_query_status` async cycles (sync `notebook_query` avoided to dodge 60 s socket timeout on whole-corpus retrieval). Five notebooks queried: Lightwave-Ledstrip, Synesthesia P4 Beat Tracker, Hybrid Beat Tracker — Auto BPM RE Port, SpectraSynq War Room, SpectraSynq Doctrine & Architecture. Zero mutations. All 13 cited paths under `~/Workspace_Management/Software/Synesthesia/` verified to exist on disk.

No inaccessible paths. Notebook snapshot last-sync 2026-05-13/14; RFC dated 2026-05-15 — RFC is 1-2 days newer than notebooks, so on-disk artefacts outrank notebooks on any conflict (this rule did not trigger; all material findings are consistent across surfaces).

## 4. Evidence Ledger

| # | Claim | File:Line | Quote (≤200 chars, verbatim) | Source Type | Date | Trust vs Claim |
|---|---|---|---|---|---|---|
| 1 | C1 (RE corpus exists) | `~/Workspace_Management/Software/Synesthesia/Docs/Synesthesia.RE/02_ALGORITHMS/00_Forensic_Analysis_Complete.md` | "Speculation vs. Fact Ratio: 70/30 (mostly reverse-engineered from constraints)." (SSA-4 cited) | EXTERNAL_REFERENCE | 8-9 Jan 2026 | SUPPORTS (corpus exists) but caveats authority |
| 2 | C1 / C8 | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:9` | "[FACT] Reference evidence came from /Users/spectrasynq/…/Synesthesia/Docs, …Tab5.DSP/docs/Auto_BPM_Technical_Analysis.md, and …compass_artifact…" | RESEARCH_DOC | 2026-05 | SUPPORTS C1 weakly; CONTRADICTS C6 (Davies/IBT/MIREX trace to compass-artifact) |
| 3 | C6 (Lock semantics) | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61` | "Synesthesia Tier 1: [FACT] NOT SPECIFIED as explicit lock/demotion… Tier 2 Academic Consensus: Davies & Plumbley lock after 3 consecutive consistent beat-period observations; IBT 5 s induction…kills an agent after 8 consecutive bad predictions" | RESEARCH_DOC | 2026-05 | REFUTES C6 |
| 4 | C6 (Tolerance) | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:62` | "Synesthesia Tier 1: [FACT] Beat-alignment tolerance NOT SPECIFIED… Tier 2 Academic Consensus: ±70 ms fixed…±46.4 ms IBT inner tolerance plus asymmetric outer" | RESEARCH_DOC | 2026-05 | REFUTES C6 |
| 5 | C6 (Re-acquire) | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:65` | "Synesthesia Tier 1: [FACT] NOT SPECIFIED as coast/reacquire" | RESEARCH_DOC | 2026-05 | CONTRADICTS C6 |
| 6 | C5 (Internal conflict) | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:167` | "[FACT] Synesthesia Tier 1 docs conflict internally on confidence and octave handling: the app-facing reverse-engineered docs and synesthesia/ control extraction are not identical." | RESEARCH_DOC | 2026-05 | CONTRADICTS C5 |
| 7 | C2 / C3 (Family-B label provenance) | `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64` | "…plus 4 Tier-2 cross-project context docs (Lightwave-Ledstrip MusicAware audit, Synesthesia Family B beat-tracker, academic compass-artifact survey)." | BUNDLING_METADATA | 2026-05-13 | PARTIAL for C2 (label exists); CONTRADICTS C4/C5 (Tier-2 cross-project, not in-repo authority) |
| 8 | C2 (Family-B absent in original RE) | `~/Workspace_Management/Software/hybrid-beat-tracker/.../crossref_synesthesia_beat_tracker_control_extraction.md` (MANIFEST.md annotation, SSA-4 cited) | "Synesthesia 'Family B' reference. Adjudicated to win on confidence + octave handling over Family A (Synesthesia.RE)." Decision-maker NOT NAMED. | BUNDLING_METADATA | 2026-01-12 | PARTIAL C2; ASSERTED_ONLY C3 |
| 9 | C2 (Source-doc grep) | `~/Workspace_Management/Software/Synesthesia/Docs/synesthesia/beat_tracker_control_extraction.md` + `…/beat_tracker_discretization.md` | "Zero 'Family A' or 'Family B' strings" (SSA-4 grep-verified, 226 + 335 lines) | EXTERNAL_REFERENCE | 2026-01-12 | CONTRADICTS literal Family A/B taxonomy in original RE |
| 10 | C5 / C8 (RFC self-flag) | `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md` frontmatter line 3 (rbdo field) | "DEGRADED-MODE for the Davies/IBT/MIREX framing — Captain's pre-decided context names artefacts not present in inventoried sources; RFC respects authority but flags the citation gap." | GENERATED_ASSERTION | 2026-05-15 | RFC itself does NOT claim VERIFIED; consistent with this audit |
| 11 | C7 (V0 unaffected) | `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md:138-145` (Q4 RBDO block) | "Fallback. Treat the closure as: V0 = live debounce ratified; Davies/IBT/MIREX framing deferred to V1+ when reference sources are inventoried." | GENERATED_ASSERTION | 2026-05-15 | SUPPORTS C7 |
| 12 | C5 (RFC Q9.2 self-flag) | `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md:769-778` | "HYPOTHESIS. Captain's framing likely comes from reverse-engineered Synesthesia documentation ('Family B canonical'). The RFC respects this authority…" | GENERATED_ASSERTION | 2026-05-15 | RFC labels HYPOTHESIS, not VERIFIED |
| 13 | C2 / C3 (RFC Q9.5 self-flag) | `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md:798-802` | "Family A vs Family B terminology is not in any inventoried source." | GENERATED_ASSERTION | 2026-05-15 | RFC openly states the citation gap |
| 14 | C4 (Source-tier silence on Synesthesia) | `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}` (SSA-3 ripgrep) | "Synesthesia/synesthesia: 0 source-code hits; Davies/MIREX/IBT (beat-tracking sense): 0 hits; reference-grade: 0 hits" | DIRECT_SOURCE | 2026-05 (HEAD `dc46cc9b`) | CONTRADICTS C4 at implementation tier |
| 15 | C4 (Actual upstream cited in source) | `firmware-v3/src/audio/backends/esv11/EsV11Backend.h:3` | "Emotiscope v1.1_320 end-to-end audio backend (capture + DSP + tempo)" | DIRECT_SOURCE | 2026-05 | CONTRADICTS Synesthesia-as-canonical-upstream; Emotiscope is the cited upstream |
| 16 | C4 (Vendor tempo path) | `firmware-v3/src/audio/backends/esv11/vendor/tempo.h:1` | "Vendored core of Emotiscope v1.1_320 tempo pipeline." | DIRECT_SOURCE | 2026-05 | CONTRADICTS C4 |
| 17 | C4 ('authoritative' in source is unrelated) | `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:335-336` | "alongside the authoritative BR_ events." | DIRECT_SOURCE | 2026-05 | NEUTRAL ("authoritative" applies to Band-Ratio onset events, not Family B) |
| 18 | C2 ("family" in synqmatrix code is visual) | `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp:40-50, 311, 737` + `SynqMatrix.h:197, 245` | "selectedFamily / targetFamily / policy family strings 'baseline' / 'interference' / 'advanced_optical' / 'mathematical'" | DIRECT_SOURCE | 2026-05 | CONTRADICTS conflation of Director "family" with beat-tracking Family A/B |
| 19 | C4 / C8 (Research suite silent) | `firmware-v3/docs/research/k1_songaware_*_2026-05-12*.md` (all nine files, SSA-2 ripgrep) | "Zero occurrences of Synesthesia, Family A/B, Davies, IBT, MIREX, reverse-engineered, reference-grade." | RESEARCH_DOC | 2026-05-12 | CONTRADICTS authority claim |
| 20 | C4 / C8 (Lane D + handover silence) | `firmware-v3/docs/research/SESSION_HANDOVER_20260514_Lane_D_Evidence.md` + `SESSION_HANDOVER_20260515_SynqMatrix_RFC.md` | "Zero mention of Synesthesia, Family A/B, Davies, IBT, MIREX." (SSA-2 cited) | SESSION_HANDOVER | 2026-05-14/15 | CONTRADICTS C4 |
| 21 | C4 (BACKLOG silence) | `BACKLOG.md` (SSA-2 ripgrep) | "No Synesthesia / Family / Davies / IBT / MIREX / reference-grade mentions." | RESEARCH_DOC | 2026-05 | CONTRADICTS C4 |
| 22 | C5 (Sole "reference-grade" hit is marketing) | `docs/marketing/K1-TAGLINES.md:218` | "Reference-grade listening, now visible" | RESEARCH_DOC | 2026-05 | UNRELATED to the authority claim; only "reference-grade" occurrence in repo |
| 23 | C8 (Claude memory silence) | `~/.claude/projects/-Users-spectrasynq-…/memory/MEMORY.md` + 45 topic files | "Zero hits across all 11 critical terms" (SSA-1 ripgrep) | CLAUDE_MEMORY | 2026-05 | CONTRADICTS any "previously decided" framing of authority |
| 24 | C8 (Editorialising rule) | `~/.claude/projects/.../memory/feedback_no_editorialising.md:9` | "labelled 5 reference effects as 'gold standard' patterns when no such designation existed… 'There is absolutely no value in presenting a scenario to be anything more than it actually is.'" | CLAUDE_INSTRUCTION | 2026-03-23 | Doctrinal constraint AGAINST inflating Synesthesia to "reference-grade" without evidence |
| 25 | C5 (NotebookLM treatment) | NotebookLM Lightwave-Ledstrip notebook (`cafda274-…`) — SSA-4 Q-C | "Treats Synesthesia as 'Reference evidence' / 'Tier 1 reference architecture target' but flags internal Tier-1 conflicts." | NOTEBOOKLM_INDEX | 2026-05-14 | Treats Synesthesia as a reference body, NOT as authoritative for SynqMatrix |
| 26 | C5 (Hybrid Beat Tracker corpus) | NotebookLM Hybrid Beat Tracker notebook (`14f9f49c-…`) — SSA-4 Q-C / Q-D | "Auto BPM Technical Analysis (not Synesthesia) is 'THE RE source-of-truth'. Davies/IBT/MIREX classified as Tier-2 academic compass-artifact." | NOTEBOOKLM_INDEX | 2026-05-13 | CONTRADICTS Synesthesia-as-authoritative; corroborates C6 refutation |
| 27 | C4 (Doctrine notebook excludes Synesthesia) | NotebookLM SpectraSynq Doctrine & Architecture (`0648af9c-…`) — SSA-4 Q-C | "Excludes Synesthesia entirely and names Emotiscope as K1's canonical upstream ancestor." | NOTEBOOKLM_INDEX | 2026-05-14 | CONTRADICTS Synesthesia-as-canonical-upstream framing |
| 28 | C1 / C5 (Self-rated confidence) | NotebookLM Synesthesia P4 Beat Tracker (`8e36af95-…`) — SSA-4 Q-F | "Synesthesia Implementation: 75%; Parameter Values: 70% (Typical ranges, not Synesthesia-specific)." | NOTEBOOKLM_INDEX | 2026-05-13 | RE itself self-disclaims parameter authority |

## 5. Claim Classification Matrix

| Claim | Status | Best Evidence Path | Best Evidence Quote (≤150 chars) | Confidence | RFC Impact |
|---|---|---|---|---|---|
| **C1** — Synesthesia reverse-engineered corpus exists as a real, dated, durable reference body | **VERIFIED** | `~/Workspace_Management/Software/Synesthesia/Docs/Synesthesia.RE/02_ALGORITHMS/00_Forensic_Analysis_Complete.md` (13 cited paths verified on disk) | "5,043 lines across 6 documents, dated 8-9 January 2026, by Gravity Current / Charles Arciniega" (SSA-4 Q-A) | HIGH | Q9.5 may acknowledge corpus exists |
| **C2** — "Family B" is a named taxonomy distinguishing two Synesthesia source families | **PARTIAL** | `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64` + hybrid-beat-tracker MANIFEST.md | "Synesthesia 'Family B' reference. Adjudicated to win on confidence + octave handling over Family A" — bundling metadata only; original RE docs contain zero Family A/B strings | MEDIUM | Q9.5 must clarify: label is bundling-layer, not RE-native |
| **C3** — "Family B over Family A" is an adjudicated authority decision with an attributable decision-maker | **ASSERTED_ONLY** | hybrid-beat-tracker MANIFEST.md (SSA-4 Q-B) | "Decision-maker NOT NAMED in any source." | MEDIUM | Q9.5 cannot inherit unattributed adjudication |
| **C4** — Synesthesia / Family B is authoritative for SynqMatrix at implementation tier | **CONTRADICTED** | `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}` + `firmware-v3/src/audio/backends/esv11/EsV11Backend.h:3` | "Synesthesia: 0 source-code hits" + "Emotiscope v1.1_320 end-to-end audio backend" | HIGH | Q9.5 "authoritative" framing fails source-tier check |
| **C5** — "Reference-grade" is an evidenced designation for Synesthesia in the inventoried corpus | **CONTRADICTED** | `MusicAware_Audit_And_Gap_Analysis.md:167` + RFC Q9.5 | "Synesthesia Tier 1 docs conflict internally on confidence and octave handling" + "Family A vs Family B terminology is not in any inventoried source" | HIGH | Q9.5 "reference-grade authoritative" must be retracted; downgrade to "one of two Tier-1 reference bodies" |
| **C6** — Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms originate from Synesthesia | **CONTRADICTED** (REFUTED) | `MusicAware_Audit_And_Gap_Analysis.md:61-65` | "Synesthesia Tier 1 NOT SPECIFIED… Tier 2 Academic Consensus: Davies & Plumbley… IBT 5 s induction… ±70 ms fixed… ±46.4 ms IBT inner tolerance" | HIGH | Q9.2 framing is a category error: these constants are Tier-2 academic, not Synesthesia |
| **C7** — V0 sign-off is unaffected by the Synesthesia authority question | **SUPPORTED** | RFC Q4 RBDO block (`SynqMatrix_Director_RFC_2026-05-15.md:138-145`) + SSA-3 source audit | "V0 = live debounce ratified; Davies/IBT/MIREX framing deferred to V1+" — SynqMatrix.cpp ships no disputed constants | HIGH | V0 closure path remains valid |
| **C8** — There exists a written Captain decision record designating Synesthesia/Family B as authoritative | **ABSENT** | BACKLOG.md, CHANGELOG.md, decision records, Lane D handovers, k1_songaware_* suite, Claude memory | "Zero mention" across all surfaces (SSA-1 + SSA-2 + SSA-4 all confirm) | HIGH | Q9.5 cannot claim prior Captain ratification; requires fresh Captain confirmation |

## 6. Back-Test Against Current RFC Q9.2 and Q9.5

**RFC Q9.2 (lines 769-778, verbatim):**

> "Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms (citation reconciliation). RFC observation. These constants are named in Captain's onwards brief as the lock state machine for V0, but are not present in: 2026-05-12 k1_songaware_* research suite (SSA1 confirmed exhaustive read). Live firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp} (SSA3 confirmed exhaustive read). ESV11 vendor headers… MusicAware_Audit_And_Gap_Analysis.md (SSA4 confirmed). HYPOTHESIS. Captain's framing likely comes from reverse-engineered Synesthesia documentation ('Family B canonical')."

**Audit verdict on Q9.2.** The HYPOTHESIS label is correctly applied (the RFC does not assert these constants ARE Synesthesia-derived). However, the audit finds the hypothesis itself is REFUTED by `MusicAware_Audit_And_Gap_Analysis.md:61-65`, which explicitly attributes Davies-3 / IBT-8 / IBT ±46.4 ms / MIREX ±70 ms to **Tier-2 academic consensus (compass-artifact)** and states "Synesthesia Tier 1: NOT SPECIFIED" for the corresponding rows. Q9.2 should be rewritten so the hypothesis cites Tier-2 academic compass-artifact as the origin — not Synesthesia. The proposed action (V0 = live-debounce ratified; constants deferred to V1+ tempo-tracker work) remains valid and is corroborated by SSA-3 source audit.

**RFC Q9.5 (lines 798-802, verbatim):**

> "Family A vs Family B distinction (citation gap). RFC observation. Captain's brief states 'Family B of the Synesthesia reverse-engineered docs is authoritative over Family A.' Family A vs Family B terminology is not in any inventoried source. Captain action requested. Either (a) confirm this is shorthand for 'the reverse-engineered Synesthesia source is authoritative over earlier inferred references' (in which case the RFC's existing Family-B-as-authoritative framing is sufficient), or (b) surface the specific Family A document so the RFC can reconcile both."

**Audit verdict on Q9.5.** The "citation gap" framing is correct; the candidate sub-questions (a) and (b) are too narrow. The audit surfaces a third reality the RFC does not present: "Family B" IS a label in the inventoried corpus, but only as **bundling-layer NotebookLM metadata** in `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64` and in the hybrid-beat-tracker MANIFEST.md cross-reference — NOT as a designation in the original Synesthesia RE source documents (which contain zero "Family A" / "Family B" strings, SSA-4 grep-verified). The "RFC's existing Family-B-as-authoritative framing" therefore fails the C4/C5 evidence test. The "reference-grade authoritative" language in the RFC's existing framing must be replaced with a narrower, evidenced phrasing.

## 7. Recommended Sign-Off Language

The block below is offered as a verbatim copy-paste alternative to the current Q9.5 framing. It is decision-grade, preserves uncertainty honestly, and does not overstate the evidence base.

```
Q9.5 — Family A vs Family B (revised under 2026-05-16 evidence audit).

(a) Captain authority is ratified over the scope and direction of SynqMatrix V0.
    The Director's V0 visual policy matrix, debounce constants, and live state
    classifier ship as currently implemented at HEAD dc46cc9b on the rename
    branch.

(b) Synesthesia is acknowledged as a real, dated (8-9 January 2026), durable
    reverse-engineered reference body living at
    ~/Workspace_Management/Software/Synesthesia/. The MusicAware Audit and
    Gap Analysis already treats it as one of two Tier-1 reference bodies
    alongside Auto BPM.

(c) The phrase "reference-grade authoritative for SynqMatrix" is withdrawn
    from this RFC. The 2026-05-16 evidence audit finds: zero written Captain
    decision designating Synesthesia or "Family B" as authoritative; no
    occurrence of "Family A" or "Family B" in the original Synesthesia RE
    source documents; the only "Family B" reference in the inventoried
    corpus is bundling-layer NotebookLM metadata in
    docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64 and the
    hybrid-beat-tracker MANIFEST.md cross-reference.

(d) V0 sign-off (live debounce ratified, 9-state vocabulary kept,
    Transition + Steady observe-only with V1 deprecation gate) stands on
    Lane D evidence and live source — not on Synesthesia evidence. The
    Synesthesia authority debate does not gate V0.

(e) Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms
    semantics are deferred to V1+ work in EsBeatClock and the tempo-tracker
    layer, not in SynqMatrix.cpp. The MusicAware audit attributes these
    specifically to Tier-2 academic compass-artifact, not to Synesthesia, so
    any V1+ design will cite the academic source directly.

(f) Captain confirmation requested on a single point: whether "Family B
    canonical" in the onwards brief was intended as (i) shorthand for
    "the Synesthesia reverse-engineered corpus is one of two Tier-1
    reference bodies" (in which case this revised Q9.5 stands as written),
    or (ii) a literal taxonomy distinguishing two named Synesthesia
    source-document families (in which case the corresponding source
    documents need to be named so they can be inventoried into the audit
    trail under this repo).

Default action if not overridden: ship V0 closure exactly as described in
§ 3.3 of the RFC, fold this revised Q9.5 into the RFC, and open a V1
ticket for tempo-tracker semantics citing compass-artifact directly.
```

## 8. Blockers / Missing Sources

1. **No written Captain decision record designating Synesthesia or "Family B" as authoritative.** BACKLOG.md, CHANGELOG.md, decision records, Lane D + RFC handovers, k1_songaware_* research suite, and the entire Claude memory surface are silent on the authority designation. Only Captain can confirm whether such a record exists outside the inventoried surfaces, or whether the framing was verbal / brief-only.

2. **The "Family B over Family A" adjudication has no attributed decision-maker.** The hybrid-beat-tracker MANIFEST.md cross-reference is the sole inventoried source that uses the labels in an authority context (SSA-4 Q-B), and it does not name who adjudicated the win.

3. **"Family A" and "Family B" are bundling-layer metadata only.** The original Synesthesia RE source documents at `~/Workspace_Management/Software/Synesthesia/Docs/synesthesia/beat_tracker_control_extraction.md` (226 lines) and `…/beat_tracker_discretization.md` (335 lines) contain zero "Family A" or "Family B" strings (SSA-4 grep-verified). The taxonomy lives in NotebookLM/manifest metadata, not in the RE itself.

4. **Davies-3 / IBT-8 / IBT ±46.4 ms / MIREX ±70 ms are attributed by the MusicAware audit to Tier-2 academic compass-artifact, not Synesthesia.** This is the strongest single contradiction in the entire evidence package and refutes the RFC's Q9.2 "HYPOTHESIS" that these constants derive from Synesthesia "Family B canonical".

5. **The Synesthesia RE corpus is off-repo.** It lives at `~/Workspace_Management/Software/Synesthesia/Docs/` (5,043 lines across 6 documents, plus the broader Docs tree indexed in the Synesthesia NotebookLM at 85 sources). If V1+ tempo-tracker work is going to cite Synesthesia (or a successor Auto BPM analysis), the orchestrator may want a formal vendoring or audit-manifest reference under `firmware-v3/docs/research/` so the citation chain is auditable inside this repo without depending on a separate workspace path.

## 9. Final Recommendation

**PROCEED_WITH_DEGRADED_Q9_5.**

V0 sign-off can proceed against Lane D evidence and the live SynqMatrix source — the Synesthesia authority question does not gate V0 because the V0 ship state contains zero Synesthesia / Family-B / Davies / IBT / MIREX dependencies (SSA-3 confirmed exhaustive read of `SynqMatrix.h`/`.cpp`). The RFC's own Q4 RBDO closure (lines 138-145) already correctly frames this as DEGRADED-MODE. The audit recommends folding the Section 7 revised Q9.5 block into the RFC, replacing the "reference-grade authoritative" framing with the narrower, evidenced phrasing offered there. Davies/IBT/MIREX framing is deferred to V1+ tempo-tracker work and will be cited against Tier-2 academic compass-artifact directly when that work begins (per `MusicAware_Audit_And_Gap_Analysis.md:61-65`). Captain confirmation is requested on a single point — whether "Family B canonical" was shorthand or a literal taxonomy — but this confirmation is required for the RFC to fold the revised Q9.5, not for V0 ship.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-16 | agent:general-purpose (synthesis SSA, orchestrated by Captain via Claude Code) | Created. Consolidates four parallel evidence SSA returns (Claude memory, repo docs, source code, NotebookLM) into a single read-only audit on whether "Synesthesia / Family B is reference-grade authoritative for SynqMatrix". Verdict AMBER. Final recommendation PROCEED_WITH_DEGRADED_Q9_5. No firmware source, no RFC sign-off, no Claude memory, no other markdown mutated. |
