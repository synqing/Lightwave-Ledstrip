---
abstract: "SSA2 partial manifest — firmware-v3 docs curation pass for the LightwaveOS NotebookLM corpus. Documents the 51 firmware-v3 markdown files INCLUDED in the bundle and the 100+ EXCLUDED. Excludes session handovers, dated audits/incidents, captain decision docs, pre-implementation specs, exploratory research, and feature-flagged build docs. Sterilisation doctrine: when in doubt, exclude. STA scan results — no STA-promoting docs included."
---

# SSA2 Partial Manifest — firmware-v3 docs

**Agent:** SSA2
**Scope:** firmware-v3/ documentation only (no source code, no other subprojects)
**Output dir:** `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`
**Path-flatten rule:** `firmware-v3/docs/foo/bar.md` → `firmware-v3_docs_foo_bar.md`
**Files written:** 51 (10 mandatory + 41 candidate INCLUDE)

## INCLUDE table

### Mandatory (per curation prompt)

| Source path | Output filename | Reason for inclusion |
|---|---|---|
| `firmware-v3/docs/reference/codebase-map.md` | `firmware-v3_docs_reference_codebase-map.md` | Pre-extracted codebase structure (851 files, 141K LOC) — canonical orientation reference |
| `firmware-v3/docs/reference/fsm-reference.md` | `firmware-v3_docs_reference_fsm-reference.md` | 10 state machines governing system behaviour — canonical FSM definitions |
| `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` | `firmware-v3_docs_EFFECT_DEVELOPMENT_STANDARD.md` | MANDATORY per CLAUDE.md — Captain-approved effect-authoring rules |
| `firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md` | `firmware-v3_docs_CQRS_STATE_ARCHITECTURE.md` | Canonical state-management/command-dispatch architecture |
| `firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md` | `firmware-v3_docs_audio-visual_audio-visual-semantic-mapping.md` | Audio-visual intelligence architecture v2.0.0 — canonical mapping doctrine |
| `firmware-v3/docs/api/api-v1.md` | `firmware-v3_docs_api_api-v1.md` | LightwaveOS API v1 reference — current shipping REST + WebSocket protocol |
| `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md` | `firmware-v3_docs_debugging_MABUTRACE_GUIDE.md` | Canonical tracing/Perfetto capture workflow |
| `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md` | `firmware-v3_docs_debugging_TRACE_INSTRUMENTATION_SPEC.md` | Master spec for adding TRACE_* points across the system |
| `firmware-v3/CONSTRAINTS.md` | `firmware-v3_CONSTRAINTS.md` | Hard limits — timing, memory, governance |
| `firmware-v3/docs/CONTEXT_GUIDE.md` | `firmware-v3_docs_CONTEXT_GUIDE.md` | Delegation guidance for AI agents working on firmware-v3 |

> NOTE: `firmware-v3/docs/CLAUDE.md` was on the mandatory list but contains ONLY a `<claude-mem-context>` recent-activity stub (12 lines, auto-generated index). EXCLUDED as junk content. Flagging for Captain.

### Candidate (judged INCLUDE)

| Source path | Output filename | Reason for inclusion |
|---|---|---|
| `firmware-v3/docs/audio-visual/AUDIO_OUTPUT_SPECIFICATIONS.md` | `firmware-v3_docs_audio-visual_AUDIO_OUTPUT_SPECIFICATIONS.md` | Canonical Part 1 reference — comprehensive audio output specs |
| `firmware-v3/docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md` | `firmware-v3_docs_audio-visual_VISUAL_PIPELINE_MECHANICS.md` | Canonical Part 2 — rendering pipeline + propagation mechanics |
| `firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` | `firmware-v3_docs_audio-visual_IMPLEMENTATION_PATTERNS.md` | Canonical companion document — patterns for effect authors |
| `firmware-v3/docs/audio-visual/COLOR_PALETTE_SYSTEM.md` | `firmware-v3_docs_audio-visual_COLOR_PALETTE_SYSTEM.md` | Canonical companion — palette system reference |
| `firmware-v3/docs/audio-visual/TROUBLESHOOTING.md` | `firmware-v3_docs_audio-visual_TROUBLESHOOTING.md` | Canonical companion — audio-visual troubleshooting guide |
| `firmware-v3/docs/audio-visual/AUDIO_SYSTEM_ARCHITECTURE.md` | `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE.md` | LightwaveOS v2 Audio System Architecture v1.0 — Reference Documentation |
| `firmware-v3/docs/audio-visual/AUDIO_SYSTEM_ARCHITECTURE_VISUAL.md` | `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE_VISUAL.md` | Visual companion to the architecture doc — quick understanding reference |
| `firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md` | `firmware-v3_docs_audio-visual_MUSICAL_LOGIC_CANONICAL_MODEL.md` | Single source of truth for what the audio pipeline produces today (frontmatter abstract explicitly states this) |
| `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md` | `firmware-v3_docs_audio-visual_audio-visual-contract-surface.md` | "Implementation source of truth" v1.2.0 (frontmatter status) |
| `firmware-v3/docs/audio-visual/bins64-adaptive-guidance.md` | `firmware-v3_docs_audio-visual_bins64-adaptive-guidance.md` | Current effect-author guidance for bins64 vs bins64Adaptive |
| `firmware-v3/docs/audio-visual/README.md` | `firmware-v3_docs_audio-visual_README.md` | Index for the audio-visual documentation suite |
| `firmware-v3/docs/audio-visual/ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md` | `firmware-v3_docs_audio-visual_ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md` | Status: Accepted — canonical ADR for the onset surface |
| `firmware-v3/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` | `firmware-v3_docs_architecture_DEFENSIVE_BOUNDS_CHECKING.md` | Canonical validation pattern used throughout the codebase |
| `firmware-v3/docs/architecture/WEB_SERVER_MODULAR_ARCHITECTURE.md` | `firmware-v3_docs_architecture_WEB_SERVER_MODULAR_ARCHITECTURE.md` | Current architecture description (Phases 1-3 complete, Phase 4 in progress, but doc itself describes shipped architecture) |
| `firmware-v3/docs/migration/WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md` | `firmware-v3_docs_migration_WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md` | Migration COMPLETE per its own status — guide for adding new routes/commands |
| `firmware-v3/docs/debugging/DEBUG_SYSTEM.md` | `firmware-v3_docs_debugging_DEBUG_SYSTEM.md` | LightwaveOS Debug System v1.0.0 — Status: Active |
| `firmware-v3/docs/effects-catalog/EFFECTS_INVENTORY.md` | `firmware-v3_docs_effects-catalog_EFFECTS_INVENTORY.md` | Canonical effects inventory v1.0.0 |
| `firmware-v3/docs/effects-catalog/MATH_APPENDIX.md` | `firmware-v3_docs_effects-catalog_MATH_APPENDIX.md` | Mathematical functions reference — canonical |
| `firmware-v3/docs/effects-catalog/PATTERN_TAXONOMY.md` | `firmware-v3_docs_effects-catalog_PATTERN_TAXONOMY.md` | Rendering pattern taxonomy — canonical |
| `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` | `firmware-v3_docs_EFFECT_FRAMEWORK_STANDARD.md` | Ratified default classifications — 12 LOAD-BEARING properties (frontmatter) |
| `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md` | `firmware-v3_docs_EFFECTS_BEHAVIORAL_REFERENCE.md` | Auto-generated reference for all 174 registered effect IDs — current |
| `firmware-v3/docs/gradient-system.md` | `firmware-v3_docs_gradient-system.md` | K1 gradient rendering system design — canonical (frontmatter abstract) |
| `firmware-v3/docs/OTA_UPDATE_GUIDE.md` | `firmware-v3_docs_OTA_UPDATE_GUIDE.md` | Current OTA update guide |
| `firmware-v3/docs/STIMULUS_CONTROL_CONTRACT.md` | `firmware-v3_docs_STIMULUS_CONTROL_CONTRACT.md` | Cross-stack stimulus override contract — canonical |
| `firmware-v3/docs/AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` | `firmware-v3_docs_AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` | Complete engineering reference for the LGPExperimentalAudioPack effects |
| `firmware-v3/docs/NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md` | `firmware-v3_docs_NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md` | Refactor blueprint for the 132-151 pack — current effect doctrine |
| `firmware-v3/docs/p1-09-migration-cookbook.md` | `firmware-v3_docs_p1-09-migration-cookbook.md` | Mechanical reference for the per-zone state migration pattern |
| `firmware-v3/docs/reference/audio-pipeline-parameters.md` | `firmware-v3_docs_reference_audio-pipeline-parameters.md` | Authoritative parameter reference (frontmatter) |
| `firmware-v3/docs/reference/emotiscope-algorithms.md` | `firmware-v3_docs_reference_emotiscope-algorithms.md` | Quick reference — direct implementation reference |
| `firmware-v3/docs/reference/k1-vs-wled-audio-comparison.md` | `firmware-v3_docs_reference_k1-vs-wled-audio-comparison.md` | Architectural comparison — canonical reference |
| `firmware-v3/docs/reference/README-audio-research.md` | `firmware-v3_docs_reference_README-audio-research.md` | Index for the audio-research reference docs |
| `firmware-v3/docs/reference/wled-audio-reactive-analysis.md` | `firmware-v3_docs_reference_wled-audio-reactive-analysis.md` | Direct reference for K1 audio architecture decisions |
| `firmware-v3/docs/reference/wled-frequency-mapping-visual.md` | `firmware-v3_docs_reference_wled-frequency-mapping-visual.md` | Visual frequency mapping reference |
| `firmware-v3/docs/reference/wled-parameter-extract.md` | `firmware-v3_docs_reference_wled-parameter-extract.md` | Ready-reference parameter extract |
| `firmware-v3/docs/design/ONSET_DETECTOR_SPEC.md` | `firmware-v3_docs_design_ONSET_DETECTOR_SPEC.md` | ADR-002 — Accepted spec for onset detection pipeline |
| `firmware-v3/docs/performance/RMT_SHOW_PATH_2026-02-28.md` | `firmware-v3_docs_performance_RMT_SHOW_PATH_2026-02-28.md` | Current RMT4 architecture explanation (FastLED show path) |
| `firmware-v3/docs/performance/VALIDATION_OVERHEAD.md` | `firmware-v3_docs_performance_VALIDATION_OVERHEAD.md` | Current validation performance characteristics |
| `firmware-v3/docs/testing/AUDIO_TEST_HARNESS.md` | `firmware-v3_docs_testing_AUDIO_TEST_HARNESS.md` | Status: Production — audio effect validation test harness |
| `firmware-v3/docs/testing/METRICS_REFERENCE.md` | `firmware-v3_docs_testing_METRICS_REFERENCE.md` | Status: Production — effect validation metrics reference |
| `firmware-v3/docs/testing/TEST_SCENARIOS.md` | `firmware-v3_docs_testing_TEST_SCENARIOS.md` | Status: Production — audio effect validation test scenarios |
| `firmware-v3/docs/measurement_protocols/m1_lgp_fringe.md` | `firmware-v3_docs_measurement_protocols_m1_lgp_fringe.md` | Current measurement protocol for LGP fringe-coherence (frontmatter abstract) |

## EXCLUDE table

### Session handovers / dated handover artifacts

| Source path | Reason |
|---|---|
| `firmware-v3/docs/SESSION_HANDOVER_20260323.md` | Session handover — dated artifact, exclusion class |
| `firmware-v3/docs/SESSION_HANDOVER_20260325_ONSET_HARDENING.md` | Session handover — dated artifact, exclusion class |

### Dated audits / incidents / postmortems

| Source path | Reason |
|---|---|
| `firmware-v3/docs/audit/move_0_2_centre_origin_audit_2026-04-27.md` | Dated audit, exclusion class |
| `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md` | Dated audit, exclusion class |
| `firmware-v3/docs/audit/PHASE_E_CATALOGUE_AUDIT_2026-04-30.md` | Dated audit, exclusion class |
| `firmware-v3/docs/forensic-audit-2026-04-17.md` | Dated forensic audit |
| `firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md` | Dated incident postmortem |
| `firmware-v3/docs/TECHNICAL_DEBT_AUDIT_2026-03-04.md` | Dated technical-debt audit |
| `firmware-v3/docs/STAGE1_CHERRY_PICK_PLAN.md` | Stage cherry-pick plan, exclusion class |
| `firmware-v3/docs/design/ONSET_QUARANTINE_20260325.md` | Dated quarantine matrix — verification state document, not canonical |

### Captain decision docs

| Source path | Reason |
|---|---|
| `firmware-v3/docs/nvs-partition-grow-captain-decision-2026-04-18.md` | Captain decision doc, exclusion class |
| `firmware-v3/docs/p1-10-ota-hash-captain-decision-2026-04-18.md` | Captain decision doc, exclusion class |

### Agent prompts (not documentation of firmware behaviour)

| Source path | Reason |
|---|---|
| `firmware-v3/docs/prompts/audioactor-coefficient-revert-prompt.md` | Agent prompt, not firmware doc |
| `firmware-v3/docs/prompts/CORRECTION-audioactor-revert-cancelled.md` | Agent prompt correction |
| `firmware-v3/docs/prompts/device-baseline-enforcement-prompt.md` | Agent prompt |
| `firmware-v3/docs/prompts/edge-mixer-implementation-prompt.md` | Agent prompt |
| `firmware-v3/docs/prompts/edgemixer-rgb-matrix-refactor-prompt.md` | Agent prompt |
| `firmware-v3/docs/prompts/edgemixer-validation-test-plan.md` | Agent prompt |
| `firmware-v3/docs/prompts/firmware-checklist-install.md` | Agent prompt |
| `firmware-v3/docs/prompts/gstack-install.md` | Agent prompt |
| `firmware-v3/docs/prompts/waveform-freeze-bug-fix-prompt.md` | Agent prompt |

### Research/exploratory (no ratification)

| Source path | Reason |
|---|---|
| `firmware-v3/docs/research/findings/*` (24 files) | Dated research findings, exclusion class |
| `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/**/*` (~36 files) | Dated runtime-evidence reports, exclusion class |
| `firmware-v3/docs/research/synergy-topology/**/*` (~12 files) | Research passes, no ratification into doctrine |
| `firmware-v3/docs/research/spazz_redesign_2026-04-30/**/*` (~22 files) | Research substrate (canonical_*, SSA*, SYNTHESIS, PORT_PLAN, PIPELINE_REFORM) — NOT ratified into firmware doctrine. Implementation track open. EXCLUDE |
| `firmware-v3/docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` | Substrate doc — ratified output is `EFFECT_FRAMEWORK_STANDARD.md` (INCLUDED) |
| `firmware-v3/docs/research/aubio-onset-reference.md` | Research reference, not ratified |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md` | Dated baseline, research |
| `firmware-v3/docs/research/AUDIO_LATTICE_CONFIGURATION_INVESTIGATION.md` | Investigation doc, exploratory |
| `firmware-v3/docs/research/AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md` | Source audit, research |
| `firmware-v3/docs/research/CHORD_ROOT_ORIGIN_TRACE.md` | Research trace |
| `firmware-v3/docs/research/CONSOLIDATED_ASSESSMENT.md` | Assessment doc, research |
| `firmware-v3/docs/research/EMBEDDED_TRACING_RESEARCH_2026.md` | Research doc, dated |
| `firmware-v3/docs/research/emotiscope-silence-detection-research.md` | Research, not ratified |
| `firmware-v3/docs/research/essentia-onset-analysis.md` | Research analysis |
| `firmware-v3/docs/research/phase5_audio_source_audit_checkpoint_2026-04-27.md` | Dated checkpoint, exclusion class |
| `firmware-v3/docs/research/PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md` | Phase research synthesis |
| `firmware-v3/docs/research/README_SILENCE_DETECTION.md` | Research index |
| `firmware-v3/docs/research/README-emotiscope-research.md` | Research index |
| `firmware-v3/docs/research/sb_chroma12_lineage_checkpoint_2026-04-27.md` | Dated lineage checkpoint |
| `firmware-v3/docs/research/SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md` | Brainstorm catalogue, exploratory |
| `firmware-v3/docs/research/SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md` | Motion taxonomy, research |
| `firmware-v3/docs/research/SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` | Framework reconstruction, research substrate (its outputs landed in `EFFECT_FRAMEWORK_STANDARD.md`) |
| `firmware-v3/docs/research/session-notes-2026-04-30/*` | Session notes, exclusion class |
| `firmware-v3/docs/research/SILENCE_DETECTION_*` (4 files) | Research, not ratified |
| `firmware-v3/docs/research/silence-detection-comparison.md` | Research comparison |
| `firmware-v3/docs/SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md` | Research doc — wled-* and emotiscope-algorithms.md cover the canonical ports. EXCLUDE |

### Pre-implementation specs / drafts

| Source path | Reason |
|---|---|
| `firmware-v3/docs/specs/16kHZ_Nyquist_LUT.md` | Pre-implementation LUT spec note |
| `firmware-v3/docs/specs/STM-128-BAND-UPGRADE-PROMPT.md` | Codex agent prompt + spec, pre-implementation |
| `firmware-v3/docs/specs/STM-CODEX-PROMPT.md` | Codex agent prompt |
| `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` | DRAFT — pending feasibility benchmark |
| `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-CODEX-PROMPT.md` | Codex agent prompt |
| `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-SPEC.md` | A/B benchmark spec, pre-implementation |
| `firmware-v3/docs/design/CLOSED_LOOP_QUALITY_SYSTEM.md` | DRAFT — for discussion |
| `firmware-v3/docs/design/INFERENCE_TASK_DECISION_BRIEF.md` | Programme decision brief — planning, not canonical state |
| `firmware-v3/docs/design/INFERENCE_TASK_PLACEMENT_MATRIX.md` | Programme placement matrix — planning, not canonical state |

### Superseded / DRAFT / proposal-only

| Source path | Reason |
|---|---|
| `firmware-v3/docs/api/api-v2.md` | API v2 not yet shipping — v1 (INCLUDED) is the canonical current API. FLAG: if v2 has shipped, this should INCLUDE instead of v1 |
| `firmware-v3/docs/api/api-legacy.md` | Legacy API — superseded by v1 |
| `firmware-v3/docs/api/enhancement-engine-api.md` | Feature-flagged build (`FEATURE_ENHANCEMENT_ENGINES=1`) — not default shipping behaviour |
| `firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md` | Foundation contract — "implementation not yet authorised beyond policy/helper design" (frontmatter status) |
| `firmware-v3/docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md` | Dated 2025-12-29; consolidated; superseded by IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS canonicals (INCLUDED) |
| `firmware-v3/docs/audio-visual/audio-bloom-implementation.md` | Single-effect implementation note, dated; superseded by general docs |
| `firmware-v3/docs/audio-visual/audio-gate-fix-2025-01.md` | Dated 2025-01-21 fix log — historical |
| `firmware-v3/docs/audio-visual/GDFT_VERIFICATION_REPORT.md` | Dated 2025-12-29 verification report — historical |
| `firmware-v3/docs/audio-visual/SALIENCY_ARCHITECTURE_REVIEW.md` | "Awaiting Implementation Decision" — proposal, not canonical |
| `firmware-v3/docs/architecture/WEBSERVER_BASELINE_INVENTORY.md` | Pre-refactor baseline — refactor complete per migration guide |
| `firmware-v3/docs/debugging/SERIAL_DEBUG_CHAOS_MAP.md` | "DOG'S BREAKFAST" current-state diagnostic doc — superseded by DEBUG_SYSTEM (INCLUDED) |
| `firmware-v3/docs/debugging/SERIAL_DEBUG_REDESIGN_PROPOSAL.md` | Proposal — not implemented as canonical state |
| `firmware-v3/docs/debugging/ARCHITECTURE_REVIEW.md` | Manual review dated 2026-01-22 — superseded by DEBUG_SYSTEM |
| `firmware-v3/docs/debugging/trace_spec_sections/*.md` (10 files) | Detail files for TRACE_INSTRUMENTATION_SPEC.md (INCLUDED). Master spec already in bundle; details would duplicate. Per their README abstract, they are deep reference only |
| `firmware-v3/docs/effects-catalog/GAP_REPORT.md` | Dated gap report — informational, not canonical |
| `firmware-v3/docs/implementation/WEBSERVER_REFACTOR_IMPLEMENTATION_SUMMARY.md` | Implementation summary — overlaps migration guide (INCLUDED is more authoritative) |
| `firmware-v3/docs/p1-09-zone-state-followup-plan.md` | Follow-up plan (in-flight); cookbook (INCLUDED) is the canonical mechanical reference |

### Junk content

| Source path | Reason |
|---|---|
| `firmware-v3/docs/CLAUDE.md` | claude-mem-context auto-generated index stub only (12 lines). Mandatory list included this file, but content is a recent-activity ledger, not documentation. FLAGGED |
| `firmware-v3/docs/api/CLAUDE.md` | claude-mem-context stub |
| `firmware-v3/docs/effects-catalog/CLAUDE.md` | claude-mem-context stub |

## STA scan results

Grep pattern: `wifi.?sta|station.mode|wifi_sta|enable.*sta` (case-insensitive) across all candidate INCLUDE files and mandatory includes.

**INCLUDE-set hits (all SAFE — no STA-promoting content):**

| File | Line | Context | Verdict |
|---|---|---|---|
| `firmware-v3_docs_reference_fsm-reference.md` | 159 | `**Network types:** None, WiFiStation, WiFiAP, Ethernet, EspHosted` | SAFE — documents the ConnectionState enum that exists in the type system; not a recommendation to use STA |
| `firmware-v3_docs_debugging_MABUTRACE_GUIDE.md` | 274 | `\| ws_client_count / wifi_ap_mode / trace_mode_enabled \|` | SAFE — references the AP-only telemetry counter |
| `firmware-v3_docs_architecture_WEB_SERVER_MODULAR_ARCHITECTURE.md` | (incidental) | matched on word "stage" — false positive | SAFE |
| `firmware-v3_docs_design_CLOSED_LOOP_QUALITY_SYSTEM.md` | (false positive — matched "Phase 1") | EXCLUDED anyway (DRAFT) | n/a |

**EXCLUDE-set STA-relevant docs (correctly excluded):**

| File | Excerpt | Why excluded |
|---|---|---|
| `forensic-audit-2026-04-17.md` | "AP-only invariant"; "remove dead WiFi STA paths" | Treats STA paths as DEAD CODE — alignment correct, but file is a dated audit (excluded by class) |
| `TECHNICAL_DEBT_AUDIT_2026-03-04.md` | `WiFiManager.cpp:166 -- WiFi state machine` | Dated audit — excluded by class |

**No file in the INCLUDE set proposes WiFi STA mode as viable.** All STA mentions are either type-system references (`WiFiStation` enum value) or AP-only confirmations.

## Confidence

**Confidence: HIGH** — reasoning:
- All 10 mandatory includes are present (1 flagged as stub-content). Path flattening verified.
- Candidate triage applied frontmatter-status / explicit-superseded / DRAFT signals consistently.
- No STA-promoting docs leaked into the INCLUDE set.
- Sterilisation doctrine respected: when in doubt (DRAFT, "awaiting decision", "research substrate"), EXCLUDE.

Lower-confidence aspects:
- `EFFECTS_BEHAVIORAL_REFERENCE.md`: auto-generated; if effect IDs have changed since generation, fields will be stale. INCLUDED on the basis that it claims to be the current reference for 174 IDs. FLAG.
- `effects-catalog/EFFECTS_INVENTORY.md` / `MATH_APPENDIX.md` / `PATTERN_TAXONOMY.md`: dated 2026-02-21; ~ 2.5 months old. Effect catalog likely shifted since. FLAG.
- `architecture/WEB_SERVER_MODULAR_ARCHITECTURE.md` and `migration/WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md`: both describe the SAME refactor at different status timestamps. The migration guide says "All 141 WS commands migrated. processWsCommand() removed." The architecture doc says "Phase 4 (Tests) In Progress". They are CONSISTENT (architecture migrated, tests still in progress). Both INCLUDED.

## Unresolved flags for Captain

1. **`firmware-v3/docs/CLAUDE.md`** is on the mandatory list but is a 12-line claude-mem auto-generated stub. EXCLUDED by SSA2 as junk; please confirm or override.
2. **API v1 vs v2.** Per curation prompt, v1 INCLUDED. v2 (3,210 lines, version 2.0.0) is in the tree. If v2 is in fact the shipping API (v1 deprecated), the bundle should swap them. Currently v1 INCLUDED, v2 EXCLUDED — please confirm.
3. **`enhancement-engine-api.md`** — feature-flagged build (`FEATURE_ENHANCEMENT_ENGINES=1`); EXCLUDED on the basis it is not default shipping behaviour. If K1 production builds enable this flag, INCLUDE.
4. **`SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md`** — research doc on calibration/noise gating algorithm. EXCLUDED on the basis that the wled-* and emotiscope-* reference docs cover canonical ports. If the SB-specific noise-gating algorithm is in current shipping firmware as a DOCTRINE doc (not just research), INCLUDE.
5. **`research/spazz_redesign_2026-04-30/*`** (22 files) — extensive forensic redesign substrate for ChevronWaves / Snapwave / LGPWaveCollision. EXCLUDED as research substrate not ratified into firmware doctrine. If the redesign has landed and these became the canonical effect-design reference, they should INCLUDE selectively (PIPELINE_REFORM, SYNTHESIS, PORT_PLAN are most ratification-shaped; canonical_* docs are upstream-source extractions). Currently EXCLUDED en bloc.
6. **`AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` + `NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md`** — INCLUDED. These are dense engineering references for specific effect packs. They MAY contain stale parameter values relative to what shipped. Captain should confirm they describe current effect behaviour, not original-design rationale.
7. **`audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md`** — EXCLUDED on the basis it duplicates content now in IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS. If it contains content not present in those, INCLUDE.
8. **`docs/effects-catalog/MATH_APPENDIX.md`** — 2,837 lines. Single largest non-API doc. NotebookLM single-source word limit may be exceeded. FLAG: may need split or replacement with a curated subset.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA2 (claude-opus-4-7-1m) | Created. firmware-v3 docs curation pass: 51 files INCLUDED, ~120 EXCLUDED. STA scan clean. Confidence HIGH with 8 flags for Captain. |
