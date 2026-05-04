# SSA5 Partial Manifest — tab5-encoder

Scope: `tab5-encoder/docs/` (all `.md` enumerated) + ONE source bundle.
Generated: 2026-05-04.

## Docs INCLUDED (14)

| Source | Output filename | Reason |
|---|---|---|
| `tab5-encoder/docs/reference/codebase-map.md` | `tab5-encoder_docs_reference_codebase-map.md` | MANDATORY (architecture map) |
| `tab5-encoder/docs/reference/fsm-reference.md` | `tab5-encoder_docs_reference_fsm-reference.md` | MANDATORY (9 FSMs) |
| `tab5-encoder/docs/reference/lvgl-component-reference.md` | `tab5-encoder_docs_reference_lvgl-component-reference.md` | MANDATORY (LVGL widget tree, anti-patterns) |
| `tab5-encoder/docs/STIMULUS_CONTROL_CONTRACT.md` | `tab5-encoder_docs_STIMULUS_CONTROL_CONTRACT.md` | Cross-system stimulus injection contract — shipped behaviour |
| `tab5-encoder/docs/IMPLEMENTATION_SPEC.md` | `tab5-encoder_docs_IMPLEMENTATION_SPEC.md` | LOCKED decisions for shipped 3-tab UI redesign |
| `tab5-encoder/docs/DESIGN_BRIEF.md` | `tab5-encoder_docs_DESIGN_BRIEF.md` | Problem statement + final layout for shipped redesign |
| `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` | `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` | Ratified design principles (frontmatter abstract; reaffirms K1 AP-only) |
| `tab5-encoder/docs/ARCHITECTURE_DECISION_PRINCIPLES.md` | `tab5-encoder_docs_ARCHITECTURE_DECISION_PRINCIPLES.md` | Ratified architecture principles (frontmatter abstract; matches main.cpp decomposition) |
| `tab5-encoder/docs/ZONE_COMPOSER_V2_SPEC.md` | `tab5-encoder_docs_ZONE_COMPOSER_V2_SPEC.md` | Final V2 layout spec — V2 changes committed 2026-04-03 (memory #42240) |
| `tab5-encoder/docs/EFFECT_ORDER_REFERENCE.md` | `tab5-encoder_docs_EFFECT_ORDER_REFERENCE.md` | Canonical reference for shipped effect-cycling pipeline (162 effects) |
| `tab5-encoder/docs/ROW2_EFFECT_PARAMETER_SPEC.md` | `tab5-encoder_docs_ROW2_EFFECT_PARAMETER_SPEC.md` | Live encoder/UI contract for `effects.parameters.*` WS commands |
| `tab5-encoder/docs/CONTROLSURFACE_RESEARCH.md` | `tab5-encoder_docs_CONTROLSURFACE_RESEARCH.md` | Research-backed semantic analysis of shipped FX PARAMS surface (frontmatter abstract; explicit grounding) |
| `tab5-encoder/docs/MENU_SYSTEM_RESEARCH.md` | `tab5-encoder_docs_MENU_SYSTEM_RESEARCH.md` | Navigation architecture research that informed shipped 3-tab nav (frontmatter abstract) |
| `tab5-encoder/docs/UI_DESIGN_RESEARCH.md` | `tab5-encoder_docs_UI_DESIGN_RESEARCH.md` | Competitive UI research that informed shipped redesign (frontmatter abstract) |

## Docs EXCLUDED

| Source | Reason |
|---|---|
| `tab5-encoder/docs/AGENT_DESIGN_INSTRUCTIONS.md` | Meta about agent process — instruction excludes |
| `tab5-encoder/docs/DECISION_FRAMEWORK_ANALYSIS.md` | Deliberation, not shipped behaviour — instruction excludes |
| `tab5-encoder/docs/DESIGN_DECISION_PROCESS.md` | Process narrative; principles already captured in PRODUCT/ARCHITECTURE_DECISION_PRINCIPLES (which inherit from this) |
| `tab5-encoder/docs/IMPLEMENTATION_SPEC_REVIEW.md` | Review artefact — instruction excludes |
| `tab5-encoder/docs/MANDATE_ENFORCEMENT_PROPOSAL.md` | Proposal — instruction excludes |
| `tab5-encoder/docs/MANDATE_REFINEMENT.md` | Deliberation — instruction excludes |
| `tab5-encoder/docs/PARAM_ALLOCATION_AUDIT.md` | Audit — instruction excludes |
| `tab5-encoder/docs/TAB5_MEMORY_AUDIT_2026-04-03.md` | Dated audit — instruction excludes |
| `tab5-encoder/docs/UI_AUDIT_REPORT.md` | Audit — instruction excludes |
| `tab5-encoder/docs/forensic-audit-2026-04-18.md` | Forensic audit — instruction excludes |
| `tab5-encoder/docs/wave2-ota-handover-2026-04-18.md` | Handover artefact — instruction excludes |

## Bundle: `_BUNDLE_tab5_architecture.txt`

Files packed (8): all verified to exist.
1. `tab5-encoder/src/network/WebSocketClient.h` — WS client public interface
2. `tab5-encoder/src/network/WsMessageRouter.h` — Inbound message router (consumer of K1 WS protocol)
3. `tab5-encoder/src/input/DualEncoderService.h` — 16-encoder unified service (Unit A + Unit B)
4. `tab5-encoder/src/parameters/ParameterMap.h` — Parameter definition table
5. `tab5-encoder/src/parameters/ParameterMap.cpp` — Concrete parameter table data
6. `tab5-encoder/src/parameters/ParameterHandler.h` — Sync controller (debounce, publish, apply)
7. `tab5-encoder/src/ui/ControlSurfaceUI.h` — FX PARAMS encoder-binding surface
8. `tab5-encoder/src/ui/ControlSurfaceUI.cpp` — FX PARAMS implementation

Files flagged (deliberately omitted from bundle):
- `tab5-encoder/src/main.cpp` (3,299 LOC, 143 KB) — too large; structure already captured in codebase-map + ARCHITECTURE_DECISION_PRINCIPLES (which discusses main.cpp decomposition).
- `tab5-encoder/src/network/WebSocketClient.cpp` (1,150 LOC, 39 KB) — header included; impl too large.
- `tab5-encoder/src/ui/DisplayUI.cpp` (121 KB) — too large for budget; LVGL screen root behaviour summarised in lvgl-component-reference and DESIGN_BRIEF.
- `tab5-encoder/src/ui/ZoneComposerUI.cpp` (67 KB, 1,627 LOC) — covered semantically by ZONE_COMPOSER_V2_SPEC doc.

Bundle size: 132,347 bytes.

## STA Scan Results

Scanned all 14 included docs and the source bundle for `STA mode | wifi.*sta | tab5_sta` and related K1↔STA references. Per SSA5 instructions, tab5 STA references are EXPECTED (tab5 connects to K1's AP via STA mode), so the gate is whether any included doc proposes K1 should run STA.

| Source | STA hits | Verdict |
|---|---|---|
| `tab5-encoder/src/*` (bundle) | 0 | clean — no STA references in bundled source |
| `ARCHITECTURE_DECISION_PRINCIPLES.md` | 1 (line 333-354) | Discusses extracting `g_wifiManager` global on tab5 — no K1-STA proposal. EXPECTED. |
| `PRODUCT_DECISION_PRINCIPLES.md` | 1 (line 305, 314) | Explicitly REINFORCES "K1 is AP-only", uses 6+ failed K1-STA attempts as anti-pattern example. NOT a K1-STA proposal — opposite. |
| `reference/fsm-reference.md` | 1 (line 20) | `WiFiConnectionStatus` FSM — tab5 STA-side state machine. EXPECTED. |
| `CONTROLSURFACE_RESEARCH.md`, `MENU_SYSTEM_RESEARCH.md`, `UI_DESIGN_RESEARCH.md` | 0 K1-STA proposals | "K1" appears only as broadcast source / connection target. EXPECTED. |

**No K1-STA proposals found.** All STA mentions are either tab5's own STA mode (correct architecture) or explicit reaffirmations that K1 must remain AP-only.

## Confidence + Unresolved Flags

- **Confidence:** HIGH for mandatory + LOCKED-spec inclusions (refs, IMPLEMENTATION_SPEC, ZONE_COMPOSER_V2_SPEC, ROW2_EFFECT_PARAMETER_SPEC, EFFECT_ORDER_REFERENCE, STIMULUS_CONTROL_CONTRACT, DESIGN_BRIEF). Memory observation #42240 confirms ZONE_COMPOSER_V2 was committed.
- **Confidence:** MEDIUM for the three RESEARCH docs (CONTROLSURFACE/MENU/UI_DESIGN). Each carries a `---abstract:` frontmatter, the IMPLEMENTATION_SPEC and ZONE_COMPOSER_V2_SPEC explicitly reference their conclusions, and the shipped 3-tab + V2 layout matches their recommendations. They were judged as "ratified into current design" rather than orphaned research.
- **Confidence:** HIGH that the two PRINCIPLES docs (PRODUCT/ARCHITECTURE) are post-deliberation distillations rather than process narratives — both have curated abstracts and translate the deliberation captured in the EXCLUDED `DESIGN_DECISION_PROCESS.md`.
- **Unresolved flag:** `IMPLEMENTATION_SPEC.md` and the LOCKED 3-tab redesign — could not verify on hardware in this SSA pass that the LOCKED layout is currently live (the spec asserts it is, no contradicting evidence found, no audit/handover docs included that would dispute it).
- **Unresolved flag:** Bundle excludes `main.cpp` and `DisplayUI.cpp` (143 KB and 121 KB) on token-budget grounds. NotebookLM will see structure (codebase-map, FSM ref) but not the literal entrypoint code. If the user wants those, run a follow-up SSA with chunking.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA5 | Created — tab5-encoder docs curation + architecture bundle |
