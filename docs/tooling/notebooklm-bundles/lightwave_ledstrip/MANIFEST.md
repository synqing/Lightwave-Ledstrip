---
abstract: "Sterilised NotebookLM curation manifest for Lightwave-Ledstrip - REVISION 4 (2026-05-13 rebuild). Full bundle refresh capturing 9 days of post-2026-05-04-sync work: 193 commits + 384 src-tree-file delta. Net delta vs Rev 3: 127 -> 163 NotebookLM sources (+36, of which +13 are new architecture/research docs, +12 are the SynqMatrix naming-review tree, +4 are Phase 1B runtime evidence + README, +1 is the canonical MusicAware audit, +1 is the new EFFECT_AUTHORING_STANDARD_V2, plus selected research summaries). Rev 3 forensic WiFi audit-trail files (6) preserved intact. All Rev 3 strength-A/B forensic-corrected WiFi doctrine carried forward via refreshed source copies. All validation gates PASS."
---

# NotebookLM Bundle Manifest -- Lightwave-Ledstrip (Revision 4)

## Revision history
- **Rev 1** (2026-05-04): Initial synthesis from 5 parallel curation SSAs.
- **Rev 2** (2026-05-04): Post Forensic-A-D audit + corrections (api-v2 / ROADMAP / api-legacy disclaimer scatter, p1-09 zone-count, tab5 RESEARCH hardening, PACK disclaimers).
- **Rev 3** (2026-05-04): Captain Approvals #1 + #2 + #3 applied. 9 source-doctrine surfaces corrected from strength-D over-correction wording to evidence-grounded strength-A/B wording.
- **Rev 4** (2026-05-13, this revision): **Full 9-day refresh.** Captain authorised post-sync rebuild after 193 commits + 384 src-file delta. New canonical material captured: MusicAware audit (first-class source), Phase 1B runtime evidence (DRAM relocation + AudioCtx copy reduction + ControlBus handoff Tier 2), SynqMatrix naming-review tree (12 docs documenting in-flight songAware -> synqMatrix rename), VP_RENDER_PATH and VP_VALIDATION_PROTOCOL audits, INFERENCE_TASK design briefs, EFFECT_AUTHORING_STANDARD_V2, GOOD_LIGHT_SHOW_TAXONOMY, p1-09 zone-state follow-up, k1_songaware decision record / feasibility / control-surface schema, and selected c1/c2/f6/k1v2 research summaries. Source bundles rebuilt against current `feature/synergy-topology-resume-2026-05-05` working tree. Source-rename status surfaced in MANIFEST + integration prompt as **SYNQMATRIX MIGRATION PHASE WARNING**.

## Summary
- Output dir: `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`
- Generated: 2026-05-13
- Total files in directory: **166** (158 .md + 5 source-code .txt bundles + 3 ancillary -- MANIFEST.md, CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md, UPLOAD_HELPER.md)
- NotebookLM-relevant sources to upload: **163** (158 .md + 5 .txt) -- within 300-source ceiling
- Net delta vs Rev 3: **+36 sources** (127 -> 163)
- Total payload size: ~4.0 MB
- Curation method: Rev 3 baseline (5 parallel SSAs + 4 forensic SSAs + Genesis Archaeology + Captain Approvals #1/#2/#3) -> 9-day staleness audit (SSA10) -> Rev 4 full rebuild
- Sterilisation doctrine: every file POSITIVELY VERIFIED canonical or carries explicit revision-banner / in-flight disclaimer; **0 strength-D doctrine residue** retained from Rev 3 corrections

## Validation gates
| Gate | Result |
|------|--------|
| 1. No raw .cpp/.h/.swift/.ts/.js/.yaml/.json/.py | PASS -- 0 found |
| 2. No stale STA doctrine ("KNOWN BROKEN", "architecturally prohibited", "6+ failed") | PASS -- 0 occurrences in content files |
| 3. MANIFEST.md complete | PASS (this file) |
| 4. Every output file is .md or .txt | PASS -- 161 .md + 5 .txt = 166 |
| 5. Total file count < 300 | PASS -- 166 |
| 6. WiFi doctrine corrections traceable to evidence | PASS -- every changed surface references `_FORENSIC_WIFI_REPORT.md` |
| 7. SynqMatrix rename in-flight surfaced to consumers | PASS -- integration prompt + manifest carry SYNQMATRIX PHASE WARNING |

## SYNQMATRIX MIGRATION PHASE WARNING

The source tree is mid-flight on the songAware -> synqMatrix rename (branch `feature/synqmatrix-rename-2026-05-13` per review tree). At time of bundle generation:

- `src/core/synqmatrix/` directory exists and ships `SynqMatrix.h` / `SynqMatrix.cpp` with `SynqMatrixMode`, `SynqMatrixProfile`, `SynqMatrixState`, etc.
- `src/core/songaware/` directory **does not exist** in the current working tree.
- `RendererActor` private fields, REST routes (`/api/v1/songAware/*`), WS commands (`songAware.*`), JSON wire fields (`currentSongState` etc.), and serial CLI prose **still carry the legacy songAware identifier** (~200+ touchpoints inventoried in `docs/temporary/projects/synqmatrix-naming-review/`).
- The `MusicAware_Audit_And_Gap_Analysis.md` document refers to the runtime class as `SongAwareDirector` because that is the runtime name in the inspected branch (`feature/synergy-topology-resume-2026-05-05`). The naming review (separate branch) is the authority on which identifiers will change.

**Agent guidance:** treat `songAware` and `synqMatrix` as referring to the same audio-aware director subsystem during this transition window. Wire-level identifiers (WS commands, REST routes, JSON keys) remain `songAware.*` until the contract rev lands. Internal C++ types are migrating to `SynqMatrix*`.

## STA-mode safety scan (current bundle state)

All STA-related references in the bundle remain **evidence-grounded** per Rev 3 corrections. **ZERO occurrences** of "KNOWN BROKEN", "architecturally prohibited", or "6+ failed" remain in content files. The forensic audit-trail files (`_FORENSIC_WIFI_*.md`) preserve the prior over-corrected doctrine wording as historical evidence, not as live guidance.

| File class | Hits | Nature (post-correction) |
|------|------|--------------------------|
| `_BUNDLE_protocol_contracts.txt` | 3 | YAML descriptions for `apMode` / `connect` reference forensic report + dual-mode goal. |
| `CHANGELOG.md` | refreshed | References forensic report + dual-mode goal. |
| `CLAUDE.md` | refreshed | Hard Constraint paragraph carries current-vs-goal-state framing. |
| `BACKLOG.md` | refreshed | F-5 item -- K1 dual-mode WiFi delivery. |
| `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | refreshed | Three strength-D locations rewritten to evidence-grounded dual-mode wording. |
| `docs_K1_ECOSYSTEM_API_ROADMAP.md` | refreshed | Banner + 48 per-section reminders carry forensic-corrected framing. |
| `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` | refreshed | REVISION BANNER explains case-study correction. |
| `firmware-v3_docs_research_k1v2_sram_psram_reclaim_handoff_2026-05-06.md` | 1 | Benign reference to `_sta_validation` env in capacity audit. |

**Verdict:** corpus is sterile and **evidence-grounded**.

## INCLUDE manifest

### Tier 1 -- root authority (7)
CLAUDE.md, AGENTS.md, BACKLOG.md, README.md, CONTRIBUTING.md, CHANGELOG.md, TRADEMARK.md

### Tier 2 -- architecture & reference docs (18)
firmware-v3 reference: codebase-map, fsm-reference, emotiscope-algorithms, audio-pipeline-parameters, k1-vs-wled-audio-comparison, README-audio-research, wled-audio-reactive-analysis, wled-frequency-mapping-visual, wled-parameter-extract
firmware-v3 root: CONSTRAINTS, CONTEXT_GUIDE
lightwave-ios-v2: CLAUDE, DESIGN_SPEC, reference/codebase-map, reference/fsm-reference
tab5-encoder: reference/codebase-map, reference/fsm-reference, reference/lvgl-component-reference

### Tier 3 -- firmware-v3 technical (44)
EFFECT_DEVELOPMENT_STANDARD, EFFECT_FRAMEWORK_STANDARD, EFFECTS_BEHAVIORAL_REFERENCE, CQRS_STATE_ARCHITECTURE, STIMULUS_CONTROL_CONTRACT, gradient-system, OTA_UPDATE_GUIDE, AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION, NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT, p1-09-migration-cookbook
api: api-v1, api-v2, api-legacy
architecture: DEFENSIVE_BOUNDS_CHECKING, WEB_SERVER_MODULAR_ARCHITECTURE
audio-visual: ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE, AUDIO_OUTPUT_SPECIFICATIONS, AUDIO_SYSTEM_ARCHITECTURE, AUDIO_SYSTEM_ARCHITECTURE_VISUAL, audio-visual-contract-surface, audio-visual-semantic-mapping, bins64-adaptive-guidance, COLOR_PALETTE_SYSTEM, IMPLEMENTATION_PATTERNS, MUSICAL_LOGIC_CANONICAL_MODEL, README, TROUBLESHOOTING, VISUAL_PIPELINE_MECHANICS
debugging: DEBUG_SYSTEM, MABUTRACE_GUIDE, TRACE_INSTRUMENTATION_SPEC
effects-catalog: EFFECTS_INVENTORY, MATH_APPENDIX, PATTERN_TAXONOMY
design: ONSET_DETECTOR_SPEC
measurement_protocols: m1_lgp_fringe
migration: WEB_SERVER_REFACTOR_MIGRATION_GUIDE
performance: RMT_SHOW_PATH_2026-02-28, VALIDATION_OVERHEAD
testing: AUDIO_TEST_HARNESS, METRICS_REFERENCE, TEST_SCENARIOS

### Tier 3a -- NEW canonical material (Rev 4 additions, 12)
- **`firmware-v3_docs_MusicAware_Audit_And_Gap_Analysis.md`** (~40 KB) -- comprehensive audit of the audio-aware director subsystem. Maps current ESV11 32kHz production audio surface, SongAwareDirector behaviour, reference-architecture comparison vs Synesthesia and AutoBPM. First-class source in this bundle (was a crossref in the Hybrid Beat Tracker bundle in Rev 3).
- **`firmware-v3_docs_EFFECT_AUTHORING_STANDARD_V2.md`** -- updated effect-authoring standard.
- **`firmware-v3_docs_GOOD_LIGHT_SHOW_TAXONOMY.md`** -- taxonomy for evaluating light-show quality.
- **`firmware-v3_docs_p1-09-zone-state-followup-plan.md`** -- p1-09 zone-state cleanup follow-up.
- **`firmware-v3_docs_audit_VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md`** -- render-path layer audit.
- **`firmware-v3_docs_audit_VP_VALIDATION_PROTOCOL_2026-05-06.md`** -- validation protocol.
- **`firmware-v3_docs_debugging_VP_STACK_INTROSPECTION_COMMAND_SPEC.md`** -- stack introspection spec.
- **`firmware-v3_docs_design_INFERENCE_TASK_DECISION_BRIEF.md`** -- inference-task placement decision.
- **`firmware-v3_docs_design_INFERENCE_TASK_PLACEMENT_MATRIX.md`** -- inference-task placement matrix.
- **`firmware-v3_docs_research_phase1b_controlbus_dram_relocation_README.md`** -- Phase 1B README.
- **`firmware-v3_docs_research_phase1b_audioctx_copy_reduction_report.md`** -- AudioCtx copy reduction trace report.
- **`firmware-v3_docs_research_phase1b_controlbus_dram_report.md`** -- ControlBus DRAM relocation report.
- **`firmware-v3_docs_research_phase1b_controlbus_handoff_tier2_report.md`** -- ControlBus handoff Tier 2 report.

### Tier 3b -- SynqMatrix naming-review tree (Rev 4 addition, 12)
Captain-authorised in-flight rename surface inventory under `docs/temporary/projects/synqmatrix-naming-review/`:
- 00-INDEX (cross-cutting findings)
- 01-audio-contracts, 02-audio-backends-and-actor, 03-audio-pipeline-onset-tempo
- 04-renderer-actor, 05-actors-and-base, 06-synqmatrix-and-plugin-api
- 07-network-server-routes, 08-network-handlers-and-ws-commands
- 09-serial-and-capture, 10A-hal-config, 10B-core-utilities

### Tier 3c -- selected research summary docs (Rev 4 addition, 13)
c1_mic_domain_envelope_audit, c2_feature_effect_dwell_matrix, f6_controlbus_num_zones_audit, lgp_beat_emotiscope_architecture_review, k1v2_sram_psram_reclaim_handoff, k1_songaware_decision_record, k1_songaware_control_surface_schema, k1_songaware_feasibility_2026-05-12_analysis, k1_songaware_director_mode_matrix_2026-05-12_analysis, k1_medium_phase0_decision_record, k1_medium_phase0_captain_review_table, k1_visual_characterisation_database, trinity_inactive_status_note

### Tier 4 -- source code bundles (.txt) (5)
- `_BUNDLE_firmware_contracts.txt` -- ControlBus.h, EffectContext.h, IEffect.h, IEffectRegistry.h, SynqMatrix.h, EffectTypes.h
- `_BUNDLE_firmware_actors.txt` -- Actor.h, ActorSystem.h, AudioActor.h, RendererActor.h, RendererNode.h, ShowDirectorActor.h, NodeOrchestrator.h, SynqMatrix.h
- `_BUNDLE_protocol_contracts.txt` -- k1-ws-contract.yaml, k1-rest-contract.yaml
- `_BUNDLE_ios_architecture.txt` -- RESTClient.swift, WebSocketService.swift, DeviceDiscoveryService.swift, AppViewModel.swift, EffectViewModel.swift, ParametersViewModel.swift
- `_BUNDLE_tab5_architecture.txt` -- ZoneComposerUI.h, DisplayUI.h, PaletteLedDisplay.h, EncoderService.h, DualEncoderService.h, EncoderProcessing.h

### Tier 5 -- repo docs, marketing, governance (24)
docs/: WORKFLOW_ROUTING, K1_ECOSYSTEM_API_ROADMAP, TOOLCHAIN_IMPLEMENTATION_GUIDE, CAPTURE_PIPELINE_REFERENCE, CAPTURE_TEST_SUITES, DEPENDENCY_LICENSES, HUNT-WAVE1-spec-changes, K1_Waveform_Algorithm_Breakdown, MULTIPLIER_STACK, ONSET_CAPTURE_WORKFLOW
docs subdirs: adr/zone-composer-architecture-decisions, cron/k1-launch-research, design/VOICE_CONTROL_EXPLORATION_PLAN, protocol/README, protocol/zones-command-matrix, protocol/zones-serial-json-parity, superpowers/ios-firmware-parity-phase-{1,2,3-scoping}, tooling/claude-mem-usage-optimisation-2026-05-02
marketing: K1-DUAL-STATE-POSITIONING, K1-LANDING-PAGE-BUILD-SPEC, K1-LAUNCH-VIDEO-SPEC, K1-STRATEGY, K1-TAGLINES

### Tier 5a -- Tab5 docs (11)
ARCHITECTURE_DECISION_PRINCIPLES, CONTROLSURFACE_RESEARCH, DESIGN_BRIEF, EFFECT_ORDER_REFERENCE, IMPLEMENTATION_SPEC, MENU_SYSTEM_RESEARCH, PRODUCT_DECISION_PRINCIPLES (REV-BANNER), ROW2_EFFECT_PARAMETER_SPEC, STIMULUS_CONTROL_CONTRACT, UI_DESIGN_RESEARCH, ZONE_COMPOSER_V2_SPEC

### Tier 5b -- instructions / governance (12)
GOV-amendment-001-dual-pipeline, GOV-research-pipeline, naming-policy-v1, repo-governance-v1, work-blocking-protocol-v1, changelog/_fragment-template
changelog entries (2026-04-27): afs-v2-contract-docs, afs-v2-effect-api-lock, afs-v2-phase-1b-instrumentation, mabutrace-capture-tooling, phase-5-effect-exemplars, phase-5-native-harness

### Tier 6 -- forensic audit trail (preserved from Rev 3, 6)
_FORENSIC_WIFI_REPORT.md, _FORENSIC_WIFI_A_GIT.md, _FORENSIC_WIFI_B_SOURCE.md, _FORENSIC_WIFI_C_MEMORY.md, _FORENSIC_WIFI_D_DOCTRINE.md, _FORENSIC_WIFI_GENESIS.md
These remain in the bundle as evidence-trail anchors for the K1 WiFi mode forensic correction. They preserve the prior over-corrected doctrine wording verbatim as historical evidence; agents must not treat them as current guidance.

## EXCLUDE decisions (Rev 4)

### Excluded by exclusion policy
- `_archive/`, `.pio/`, `.git/`, `.github/`, `.claude/`, `.codex/`, `.planning/`, `node_modules/` -- standard exclusions per CURATION_PROMPT
- `docs/tooling/notebooklm-bundles/` -- our own output directory
- All effect implementations (~349 files under `src/effects/`) -- EFFECT_DEVELOPMENT_STANDARD + EFFECTS_INVENTORY carry the patterns
- All test files -- operational
- `harness/` -- feasibility test harnesses
- `k1-composer/`, `lightwave-dashboard/` -- web tools (no architecture docs warrant inclusion)
- `scripts/`, `tools/` -- utility scripts

### Excluded as session-scoped working evidence
The 9-day window produced ~120+ research evidence files under `firmware-v3/docs/research/evidence/k1_songaware_*` and `firmware-v3/docs/research/k1_medium_phase0_*serial*.md`. These are per-effect / per-run capture logs (action_item_map, audio_feature_intake, boot_default_safety, build_upload_result, classifier_state_machine, captain_visual_scores, etc.). They are session-scoped working evidence, not architectural truth, and follow Rev 2 precedent (`docs/findings.md`, `MULTI_WORKTREE_TESTING_GUIDE.md`, `node-composer-research.md` flipped to EXCLUDE). The canonical decision records (`k1_songaware_decision_record_2026-05-12.md`, `k1_medium_phase0_decision_record_2026-05-12.md`, `k1_medium_phase0_captain_review_table_2026-05-12.md`) are included as the authoritative summaries.

### Excluded as instructions/changelog spam window
~70 instruction-changelog entries dated 2026-05-04 through 2026-05-09 reflect dense audit + remediation cycles. The canonical instruction docs (GOV-amendment-001-dual-pipeline, GOV-research-pipeline, naming-policy-v1, repo-governance-v1, work-blocking-protocol-v1) are included; the per-action changelog fragments are excluded as operational audit-trail rather than architectural truth. The pre-existing 2026-04-27 AFS v2 / mabutrace / phase-5 changelog set is retained because those entries document architectural locks that consumers may need to cite.

### Excluded prompt files
- `firmware-v3/docs/prompts/*.md` -- session-scoped agent prompts, not architectural truth.
- `firmware-v3/docs/research/musicaware_audit_work/{findings,progress,task_plan}.md` -- planning-with-files scaffolding for the MusicAware audit itself; the audit's published output (`MusicAware_Audit_And_Gap_Analysis.md`) is the canonical version.

## Confidence assessment

| Dimension | Confidence | Notes |
|---|---|---|
| Architecture coverage | HIGH | All canonical architecture docs included; new VP_RENDER_PATH + VP_VALIDATION_PROTOCOL audits + EFFECT_AUTHORING_STANDARD_V2 land. |
| Audio pipeline coverage | HIGH | MusicAware audit is now first-class; existing AUDIO_SYSTEM_ARCHITECTURE + AUDIO_OUTPUT_SPECIFICATIONS + audio-visual-semantic-mapping carried forward. |
| Source bundle coverage | HIGH | All five .txt bundles regenerated from current working tree; SynqMatrix.h captured. |
| WiFi doctrine sterilisation | HIGH | Rev 3 forensic correction carried forward intact; zero strength-D residue. |
| SynqMatrix rename surfaced | HIGH | 12-doc naming-review tree included; SYNQMATRIX PHASE WARNING in MANIFEST + integration prompt. |
| Research-evidence noise excluded | HIGH | ~120 session evidence files explicitly excluded; only canonical decision records retained. |
| iOS coverage | MEDIUM-HIGH | Reference docs + 6-file Swift bundle. iOS side has not had a major architecture refactor in the 9-day window. |
| Tab5 coverage | MEDIUM-HIGH | All canonical Tab5 docs included; UI source bundle covers ZoneComposer + encoder services. |

## Unresolved flags for Captain review

None blocking. SynqMatrix rename is mid-flight by design (separate branch); the in-flight state is documented in the SYNQMATRIX PHASE WARNING.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:NotebookLM-orchestrator | Created (Rev 1) -- initial 5-SSA synthesis. |
| 2026-05-04 | agent:NotebookLM-orchestrator | Rev 2 -- Forensic-A-D audit + corrections (api-v2, ROADMAP, api-legacy, p1-09, tab5 RESEARCH, PACK disclaimers). |
| 2026-05-04 | agent:NotebookLM-orchestrator | Rev 3 -- Captain Approvals #1/#2/#3 source-doctrine corrections (9 WiFi-doctrine surfaces). |
| 2026-05-13 | agent:NotebookLM-orchestrator | Rev 4 -- full 9-day refresh; +31 sources; MusicAware audit, Phase 1B evidence, SynqMatrix review tree, VP audits, EFFECT_AUTHORING_STANDARD_V2, INFERENCE_TASK design briefs added; SYNQMATRIX PHASE WARNING surfaced. |
