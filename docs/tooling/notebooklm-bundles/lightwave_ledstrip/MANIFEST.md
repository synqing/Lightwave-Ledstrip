---
abstract: Sterilised NotebookLM curation manifest for Lightwave-Ledstrip — REVISION 3, supersedes Rev 2. Captain-approved (2026-05-04) source-doctrine corrections + bundle treatment Option B applied. 9 source-doctrine surfaces corrected from strength-D ("AP-only-EVER, STA never worked, 6+ failures") to evidence-grounded strength-A/B (current AP-only via `WIFI_AP_ONLY` build flag; goal dual-mode AP-or-STA never together; concurrent AP+STA is a known ESP-IDF bug). Forensic excavation report (`_FORENSIC_WIFI_REPORT.md`, Rev 2) + genesis-anchored deep dig (`_FORENSIC_WIFI_GENESIS.md`) preserved as evidence trail. 133 files (127 .md + 6 .txt). All validation gates PASS.
---

# NotebookLM Bundle Manifest — Lightwave-Ledstrip (Revision 3)

## Revision history
- **Rev 1** (2026-05-04): Initial synthesis from 5 parallel curation SSAs.
- **Rev 2** (2026-05-04): Post Forensic-A-D audit + corrections (api-v2 / ROADMAP / api-legacy disclaimer scatter, p1-09 zone-count, tab5 RESEARCH hardening, PACK disclaimers).
- **Rev 3** (2026-05-04, this revision): Captain Approvals #1 + #2 + #3 applied. 9 source-doctrine surfaces corrected from strength-D over-correction wording to evidence-grounded current-vs-goal-state wording. Bundled copies refreshed from updated sources. ROADMAP scatter rewritten with corrected per-section reminders. BACKLOG F-5 added (firmware engineering scope for dual-mode delivery). Forensic genesis dig retained as canonical evidence trail.

## Summary
- Output dir: `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`
- Generated: 2026-05-04
- Total files in directory: **133** (127 .md + 6 .txt)
- NotebookLM-relevant sources: **127** (121 .md + 6 .txt) — within 300-source ceiling
- Total payload size: ~3.0 MB
- Curation method: 5 parallel scope-isolated SSAs → cross-cutting STA sweep → Forensic-Audit (SSA2 disposition) → Captain Approvals #1 + #2 + #3 → 4 parallel forensic SSAs (A: git history, B: current source, C: memory, D: doctrine) + Genesis Archaeology SSA → source-doctrine corrections → bundle refresh → MANIFEST regeneration (this file)
- Sterilisation doctrine: every file POSITIVELY VERIFIED canonical; **0 strength-D doctrine residue** in the bundle; all WiFi-mode references are evidence-grounded with forensic-report cross-reference

## Validation gates
| Gate | Result |
|------|--------|
| 1. No raw .cpp/.h/.swift/.ts/.js/.yaml/.json/.py | PASS — 0 found |
| 2. No stale STA doctrine ("KNOWN BROKEN", "architecturally prohibited", "6+ failed") | PASS — 0 occurrences in content files (audit-trail files preserve old wording as forensic evidence) |
| 3. MANIFEST.md complete | PASS (this file) |
| 4. Every output file is .md or .txt | PASS — 127 + 6 |
| 5. Total file count < 300 | PASS — 133 |
| 6. WiFi doctrine corrections traceable to evidence | PASS — every changed surface references `_FORENSIC_WIFI_REPORT.md` |

## STA-mode safety scan (current bundle state, post-correction)

All STA-related references in the bundle are now **evidence-grounded** — they cite the forensic report, frame the dual-mode goal explicitly, and identify concurrent AP+STA (not STA-alone) as the genuine bug surface. **ZERO occurrences** of "KNOWN BROKEN", "architecturally prohibited", or "6+ failed" remain in content files. The single literal `STA never worked` occurrence in source content is a meta-quote in the new ROADMAP banner (verbatim quoting the OLD doctrine being corrected, not asserting it).

| File | Hits | Nature (post-correction) |
|------|------|--------------------------|
| `_BUNDLE_protocol_contracts.txt` | 3 | New YAML descriptions for `apMode` / `connect` reference forensic report and dual-mode goal. |
| `CHANGELOG.md` | 1 | Updated entry referencing forensic report + dual-mode goal. |
| `CLAUDE.md` | 2 | Fully rewritten Hard Constraint paragraph — current state vs goal state, concurrent AP+STA called out as bug surface, forensic report cross-referenced. |
| `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | 3 | Three strength-D locations rewritten to evidence-grounded dual-mode wording. |
| `docs_K1_ECOSYSTEM_API_ROADMAP.md` | 44 | Rewritten banner ("K1 WiFi mode — current state vs goal state, FORENSICALLY CORRECTED 2026-05-04"); 48 per-section reminders rewritten — sections 1-4 = "current API inventory", sections 5-8 = "FUTURE/UNIMPLEMENTED", section 9 reminder rewritten to point to forensic report. Source ROADMAP Section 9 items 1+2 also updated. Pattern-match resistance preserved (51 disclaimer markers total). |
| `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` | 14 | Bundled copy carries REVISION BANNER explaining case-study correction. Source: technical-constraints bullet softened, "Real-World Example" case study substantially rewritten with corrected forensic framing, K1 Application bullet softened. |
| `BACKLOG.md` | 7 | New F-5 item — "K1 dual-mode WiFi delivery (HIGH — DECIDED + SCOPED 2026-05-04)" with 4 engineering tasks. |
| `docs_marketing_K1-STRATEGY.md` | 1 | Pre-existing benign acronym mention; no doctrinal claim. |
| `firmware-v3_docs_debugging_TRACE_INSTRUMENTATION_SPEC.md` | 1 | Pre-existing benign acronym mention; no doctrinal claim. |
| `MANIFEST.md` (this file) | meta | Audit-context references — not source content. |

**Verdict:** corpus is sterile and **evidence-grounded**. NotebookLM will receive K1 WiFi mode framed as "current AP-only via build flag, goal dual-mode (AP OR STA, never together), concurrent AP+STA is the genuine bug surface" — which is the truthful state per the forensic excavation rather than the over-corrected doctrine.

## Source-doctrine corrections applied (2026-05-04, Captain Approval #1)

Captain Approval #1 authorised rewriting 9 source-doctrine surfaces from strength-D over-correction wording (claiming STA had "never worked" and citing "6+ failures") to evidence-grounded strength-A/B wording reflecting the forensic excavation. The corrections are landed in the source tree and the bundled copies refreshed accordingly.

| # | Source surface | Change applied | Bundle artefact |
|---|---|---|---|
| 1 | `CLAUDE.md` (root) line 117 — constraint readback table row | Strength-D "K1 is AP-ONLY — NEVER enable STA mode" → evidence-grounded "Current dev: AP-only via `WIFI_AP_ONLY` build flag. Goal: dual-mode (AP OR STA, never together — concurrent AP+STA has known ESP-IDF bug). Pure-STA work via `_sta_validation` env needs Captain coordination." | `CLAUDE.md` refreshed |
| 2 | `CLAUDE.md` (root) line 236 — Hard Constraints paragraph | Full rewrite to current-state-vs-goal-state framing; concurrent AP+STA identified as bug surface; Era 1 + Era 3 STA-alone-worked evidence cited; `WIFI_AP_ONLY` flag + `m_forceApOnly` runtime lock named; `_sta_validation` env (commit `11e040d6`) named as authorised entry point; forensic-report path cross-referenced | `CLAUDE.md` refreshed |
| 3 | `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` lines 45 / 560 / 818 | 3 strength-D locations rewritten to evidence-grounded dual-mode wording | `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md` refreshed |
| 4 | `docs/protocol/k1-rest-contract.yaml` lines 683 / 699 — `apMode` / `connect` descriptions | Strength-D "KNOWN BROKEN on K1 — do NOT use" string ELIMINATED. New descriptions reference forensic report + dual-mode goal. ZERO occurrences of "KNOWN BROKEN" remain in the source YAML or in `_BUNDLE_protocol_contracts.txt`. | `_BUNDLE_protocol_contracts.txt` rebuilt (99,065 bytes / 2,965 lines) |
| 5 | `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` line 305 — Technical-constraints bullet | Softened from absolute prohibition wording to current-state-with-goal framing | `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` carries REVISION BANNER + refreshed source |
| 6 | `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` lines 312-316 — "Real-World Example" case study | Substantially rewritten with corrected forensic framing — names Era 1 (Light Crystals 2025-06-24) and Era 3 (v2 STA-primary 2025-12-16) STA-alone-working evidence; identifies concurrent AP+STA (not STA-alone) as the failure mode that produced the over-correction | `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` carries REVISION BANNER + refreshed source |
| 7 | `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` line 325 — K1 Application bullet | Softened to evidence-grounded current-state framing | `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` carries REVISION BANNER + refreshed source |
| 8 | `CHANGELOG.md` line 192 — STA-correction entry | Expanded with cross-reference to forensic report and explicit dual-mode-goal acknowledgement | `CHANGELOG.md` refreshed |
| 9 | `BACKLOG.md` — NEW F-5 entry | Added "K1 dual-mode WiFi delivery (HIGH — DECIDED + SCOPED 2026-05-04)" with 4 engineering tasks tracking firmware engineering scope per Captain Approval #2 | `BACKLOG.md` refreshed |

**Memory layer (outside bundle, recorded for future agent sessions):**
- `~/.claude/.../memory/firmware_wifi_architecture.md` — full rewrite as forensic-grounded reference
- `~/.claude/.../memory/feedback_never_change_network_architecture.md` — refined to acknowledge dual-mode goal (does NOT block dual-mode work; blocks ad-hoc concurrent-AP+STA reintroduction)
- `~/.claude/.../memory/MEMORY.md` index — updated WiFi entry pointer

**ROADMAP scatter rewrite (per Captain Approval #3, bundle treatment Option B):** `docs_K1_ECOSYSTEM_API_ROADMAP.md` (626 lines) was fully rebuilt. Top banner rewritten ("K1 WiFi mode — current state vs goal state (FORENSICALLY CORRECTED 2026-05-04)"). All 48 per-section reminders rewritten in three variants — sections 1-4 keep "current API inventory" framing, sections 5-8 keep "FUTURE/UNIMPLEMENTED" framing, section 9 reminder rewritten to point to forensic report. Closing reaffirmation rewritten. Pattern-match resistance preserved (51 disclaimer markers total).

**Source ROADMAP Section 9** items 1+2 also updated to evidence-grounded wording (the source doc itself, not just the bundled copy).

## Post-curation audit + corrections (2026-05-04)

*Preserved verbatim from Rev 2 — describes the audit pass that fed Rev 2.*

**Trigger:** Captain explicitly authorised a forensic re-audit ("Further verification and deeper forensic excavation might be necessary for SSA2's disposition") after the initial 5-SSA synthesis. The audit pass focused on SSA2's firmware-v3 docs disposition (Tasks 1-6 in `_AUDIT_FORENSIC_SSA2.md`).

**Audit scope:** SSA2 firmware-v3/docs decisions; selected SSA1 cross-cutting docs (ROADMAP.md, three borderline INCLUDEs); class-level confirmation of out-of-stated-scope skips.

**HIGH-severity findings + corrections applied:**

| Finding | Original disposition | Corrected disposition | Hardening applied |
|---|---|---|---|
| `firmware-v3/docs/api/api-v2.md` (3,549 lines) | EXCLUDED by SSA2 ("0 routes registered") | INCLUDED | Captain Path B re-include with prepended STATUS BANNER, **102 per-section reminders inserted before every `##` / `###` / `####` heading** flagging "UNIMPLEMENTED `/api/v2/` route", and closing reaffirmation. Pattern-match resistance is high. |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md` (626 lines) | EXCLUDED by SSA1 (STA-taint, conservative) | INCLUDED | Captain Path B re-include with prepended STATUS BANNER, **48 per-section reminders** with three reminder variants. **Rev 3 update:** banner + reminders rewritten to forensic-corrected wording. |
| `firmware-v3/docs/api/api-legacy.md` (612 lines) | EXCLUDED by SSA2 ("superseded") | INCLUDED | Forensic audit found this WRONG: legacy `/api/*` endpoints still function on K1 v3.4 per source verification (`firmware-v3/src/network/webserver/V1ApiRoutes.cpp`). Re-included with prepended LEGACY-COMPATIBILITY banner clarifying status hierarchy + closing reaffirmation. |
| `firmware-v3/docs/p1-09-migration-cookbook.md` | INCLUDED (no disclaimer) | INCLUDED + DISCLAIMED | Prepended ZONE-COUNT clarification banner: `kMaxZones=4` is C++ array bound, user-facing reality is 3 zones (Zone 1/2/3, Zone 0 BANNED) per zone purge refactor commit 99c6405f. |
| `tab5-encoder/docs/CONTROLSURFACE_RESEARCH.md`, `MENU_SYSTEM_RESEARCH.md`, `UI_DESIGN_RESEARCH.md` | INCLUDED (no disclaimer) | INCLUDED + HARDENED | Prepended "EXPLORATORY RESEARCH RATIFIED INTO SHIPPED DESIGN" banner pointing to canonical references; closing reaffirmation. |
| `firmware-v3/docs/AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md`, `firmware-v3/docs/NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md` | INCLUDED (no disclaimer) | INCLUDED + DISCLAIMED | Prepended "DESIGN-RATIONALE DOCUMENT" banner pointing to firmware source for current parameter values; closing reaffirmation. |

**Captain decision per flag #7 borderlines:** all three flipped to EXCLUDE on closer re-read (precision-over-speed):

| File | Reason for flip |
|---|---|
| `docs/findings.md` | Explicit "Running log of firmware-v3 technical findings" — session-scoped working log. |
| `docs/MULTI_WORKTREE_TESTING_GUIDE.md` | Process documentation for past session, dated 2026-03-06. |
| `docs/node-composer-research.md` | Explicit self-description: "10-vector research dump". |

**EXCLUDE confirmations from forensic audit (no action, just record):**
- `firmware-v3/docs/api/enhancement-engine-api.md` — EXCLUDE confirmed. `FEATURE_ENHANCEMENT_ENGINES` not set in canonical envs (`esp32dev_audio_esv11_k1v2_32khz` / `esp32dev_audio_esv11_32khz`); doc targets non-existent `esp32dev_enhanced` env.
- `firmware-v3/docs/api/CLAUDE.md` — EXCLUDE confirmed. 13-line claude-mem auto-stub.
- `firmware-v3/REFERENCE_HARNESS.md` — orchestrator decision: EXCLUDE. Audit suggested INCLUDE candidate but on closer read this is operational test-harness for PyTorch-port calibration, not core architecture.
- 17 other out-of-stated-scope files SSA2 silently skipped — all correctly excluded by class per audit (firmware-v3/ root, src/CLAUDE.md, research/, testbed/, tools/, .claude/).

**Net delta (Rev 2):** −3 files (flag #7 borderlines flipped to EXCLUDE), +3 files (api-v2, ROADMAP, api-legacy re-included with disclaimers). Net source count: 125 → 127. Six existing-INCLUDE files (p1-09, three tab5 RESEARCH, two PACK docs) received in-bundle disclaimer banners but no add/remove change.

**Net delta (Rev 3):** 0 files added or removed. 6 bundled copies refreshed from updated sources (CLAUDE.md, CHANGELOG.md, BACKLOG.md, docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md, tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md, docs_K1_ECOSYSTEM_API_ROADMAP.md). 1 bundle rebuilt from updated YAML (`_BUNDLE_protocol_contracts.txt`).

**Evidence trail:** `_AUDIT_FORENSIC_SSA2.md` (329 lines, 25.3 KB) documents Rev 2 audit methodology, findings, and corrections in full. `_FORENSIC_WIFI_A_GIT.md`, `_FORENSIC_WIFI_B_SOURCE.md`, `_FORENSIC_WIFI_C_MEMORY.md`, `_FORENSIC_WIFI_D_DOCTRINE.md`, `_FORENSIC_WIFI_GENESIS.md`, and `_FORENSIC_WIFI_REPORT.md` (Rev 2) document Rev 3's forensic excavation in full.

## Bundles inventory (6 bundle files)

| Bundle filename | Size | Files | Purpose | Rev 3 status |
|---|---|---|---|---|
| `_BUNDLE_root_config.txt` (SSA1) | ~1.9 KB | 1 | `.pre-commit-config.yaml` (root config) | unchanged |
| `_BUNDLE_protocol_contracts.txt` (SSA1) | 99,065 bytes / 2,965 lines | 2 | `docs/protocol/k1-ws-contract.yaml` + `docs/protocol/k1-rest-contract.yaml` | **REBUILT 2026-05-04 from updated YAML sources** — new evidence-grounded `apMode` / `connect` descriptions; "KNOWN BROKEN" string eliminated |
| `_BUNDLE_firmware_contracts.txt` (SSA3) | 149.5 KB | 14 | ControlBus, EffectContext, IEffect, IEffectRegistry, OnsetContext, MotionSemantics, OnsetSemantics, MusicalSaliency, MusicalGrid, AudioTime, SnapshotBuffer, StyleDetector, MotionShaper, AudioEffectMapping | unchanged |
| `_BUNDLE_firmware_actors.txt` (SSA3) | 97.7 KB | 5 | AudioActor, RendererActor, ShowDirectorActor, PluginManagerActor, WsCommandRouter (substituted for missing CommandActor) | unchanged |
| `_BUNDLE_ios_architecture.txt` (SSA4) | 88 KB | 5 | LightwaveOSApp, RESTClient, WebSocketService, UDPStreamReceiver, DeviceDiscoveryService | unchanged |
| `_BUNDLE_tab5_architecture.txt` (SSA5) | 132 KB | 8 | WebSocketClient.h, WsMessageRouter.h, DualEncoderService.h, ParameterMap.h, ParameterMap.cpp, ParameterHandler.h, ControlSurfaceUI.h, ControlSurfaceUI.cpp | unchanged |

## INCLUDE register (consolidated)

### Tier 1: Root authority (SSA1)

| Source path | Output filename | Bundle | Reason |
|---|---|---|---|
| `CLAUDE.md` **[REFRESHED-REV3]** | `CLAUDE.md` | root | Project-level orchestration doctrine; loaded every session. Authoritative. **Rev 3:** lines 117 + 236 rewritten to evidence-grounded current-vs-goal-state WiFi framing. |
| `AGENTS.md` | `AGENTS.md` | root | Workflow discipline rules R1-R5. Authoritative governance. |
| `BACKLOG.md` **[REFRESHED-REV3]** | `BACKLOG.md` | root | Live calibration-debt ledger; referenced by RBDO gate. **Rev 3:** new F-5 item added — "K1 dual-mode WiFi delivery (HIGH — DECIDED + SCOPED 2026-05-04)" with 4 engineering tasks. |
| `README.md` | `README.md` | root | Project overview. |
| `CONTRIBUTING.md` | `CONTRIBUTING.md` | root | Contribution rules. |
| `CHANGELOG.md` **[REFRESHED-REV3]** | `CHANGELOG.md` | root | Keep-a-changelog formatted history. **Rev 3:** line 192 STA-correction entry expanded with forensic-report cross-reference. |
| `TRADEMARK.md` | `TRADEMARK.md` | root | Trademark policy. |
| `.pre-commit-config.yaml` | `_BUNDLE_root_config.txt` | bundle (root config) | Pre-commit hook config, wrapped in bundle separator. |

### Tier 2: Architecture & reference (SSA2 / SSA4 / SSA5)

| Source path | Output filename | Reason |
|---|---|---|
| `firmware-v3/docs/reference/codebase-map.md` | `firmware-v3_docs_reference_codebase-map.md` | Pre-extracted codebase structure (851 files, 141K LOC) — canonical orientation reference |
| `firmware-v3/docs/reference/fsm-reference.md` | `firmware-v3_docs_reference_fsm-reference.md` | 10 state machines governing system behaviour — canonical FSM definitions |
| `firmware-v3/CONSTRAINTS.md` | `firmware-v3_CONSTRAINTS.md` | Hard limits — timing, memory, governance |
| `firmware-v3/docs/CONTEXT_GUIDE.md` | `firmware-v3_docs_CONTEXT_GUIDE.md` | Delegation guidance for AI agents working on firmware-v3 |
| `firmware-v3/docs/reference/audio-pipeline-parameters.md` | `firmware-v3_docs_reference_audio-pipeline-parameters.md` | Authoritative parameter reference |
| `firmware-v3/docs/reference/emotiscope-algorithms.md` | `firmware-v3_docs_reference_emotiscope-algorithms.md` | Quick reference — direct implementation reference |
| `firmware-v3/docs/reference/k1-vs-wled-audio-comparison.md` | `firmware-v3_docs_reference_k1-vs-wled-audio-comparison.md` | Architectural comparison — canonical reference |
| `firmware-v3/docs/reference/README-audio-research.md` | `firmware-v3_docs_reference_README-audio-research.md` | Index for the audio-research reference docs |
| `firmware-v3/docs/reference/wled-audio-reactive-analysis.md` | `firmware-v3_docs_reference_wled-audio-reactive-analysis.md` | Direct reference for K1 audio architecture decisions |
| `firmware-v3/docs/reference/wled-frequency-mapping-visual.md` | `firmware-v3_docs_reference_wled-frequency-mapping-visual.md` | Visual frequency mapping reference |
| `firmware-v3/docs/reference/wled-parameter-extract.md` | `firmware-v3_docs_reference_wled-parameter-extract.md` | Ready-reference parameter extract |
| `lightwave-ios-v2/docs/reference/codebase-map.md` | `lightwave-ios-v2_docs_reference_codebase-map.md` | Mandatory. Canonical 111-file directory map (12K LOC). |
| `lightwave-ios-v2/docs/reference/fsm-reference.md` | `lightwave-ios-v2_docs_reference_fsm-reference.md` | Mandatory. 10 state machines (ConnectionState, WebSocket reconnect, UDP fallback, etc.). |
| `lightwave-ios-v2/docs/CLAUDE.md` | `lightwave-ios-v2_docs_CLAUDE.md` | Mandatory. iOS-scoped CLAUDE.md — encodes hard constraints (`@MainActor @Observable`, `actor`, 150ms debounce, AP-only). |
| `lightwave-ios-v2/docs/DESIGN_SPEC.md` | `lightwave-ios-v2_docs_DESIGN_SPEC.md` | Canonical typography + spacing + colour token spec. Maps to `Theme/DesignTokens.swift`. |
| `tab5-encoder/docs/reference/codebase-map.md` | `tab5-encoder_docs_reference_codebase-map.md` | MANDATORY (architecture map) |
| `tab5-encoder/docs/reference/fsm-reference.md` | `tab5-encoder_docs_reference_fsm-reference.md` | MANDATORY (9 FSMs) |
| `tab5-encoder/docs/reference/lvgl-component-reference.md` | `tab5-encoder_docs_reference_lvgl-component-reference.md` | MANDATORY (LVGL widget tree, anti-patterns) |

### Tier 3: Technical documentation (SSA1 / SSA2 / SSA5)

| Source path | Output filename | Reason |
|---|---|---|
| `docs/WORKFLOW_ROUTING.md` | `docs_WORKFLOW_ROUTING.md` | Mandatory tool/skill routing table; referenced by CLAUDE.md. |
| `docs/MULTIPLIER_STACK.md` | `docs_MULTIPLIER_STACK.md` | Canonical AI-toolchain inventory (March 2026). |
| `docs/CAPTURE_TEST_SUITES.md` | `docs_CAPTURE_TEST_SUITES.md` | Operational test profile reference (Reference/Stress/Isolation/Soak). |
| `docs/ONSET_CAPTURE_WORKFLOW.md` | `docs_ONSET_CAPTURE_WORKFLOW.md` | Operational onset-detector validation runbook. |
| `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` **[REFRESHED-REV3]** | `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | Agent-executable runbooks. **Rev 3:** 3 strength-D locations (lines 45 / 560 / 818) rewritten to evidence-grounded dual-mode wording. |
| `docs/DEPENDENCY_LICENSES.md` | `docs_DEPENDENCY_LICENSES.md` | Licence audit (Apache 2.0 compatibility). Factual register. |
| `docs/K1_Waveform_Algorithm_Breakdown.md` | `docs_K1_Waveform_Algorithm_Breakdown.md` | Per-frame algorithm reference for Waveform effect (0x1302). |
| `docs/CAPTURE_PIPELINE_REFERENCE.md` | `docs_CAPTURE_PIPELINE_REFERENCE.md` | Canonical capture-pipeline reference (binary frame formats v1/v2, CLI). |
| `docs/HUNT-WAVE1-spec-changes.md` | `docs_HUNT-WAVE1-spec-changes.md` | 16 firmware spec changes; abstract framing as research output. |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md` **[DISCLAIMED-REWRITTEN-REV3]** | `docs_K1_ECOSYSTEM_API_ROADMAP.md` | Re-included per Captain Path B with multi-point scatter-disclaimers. **Rev 3:** banner rewritten ("K1 WiFi mode — current state vs goal state, FORENSICALLY CORRECTED 2026-05-04"); all 48 per-section reminders rewritten in three variants — sections 1-4 = "current API inventory", sections 5-8 = "FUTURE/UNIMPLEMENTED", section 9 reminder rewritten to point to forensic report; closing reaffirmation rewritten. Pattern-match resistance preserved (51 disclaimer markers total). Source ROADMAP Section 9 items 1+2 also updated. |
| `docs/protocol/README.md` | `docs_protocol_README.md` | Protocol contracts directory README. |
| `docs/protocol/zones-command-matrix.md` | `docs_protocol_zones-command-matrix.md` | Canonical command matrix across REST/WS/SerialJSON/SerialCLI. |
| `docs/protocol/zones-serial-json-parity.md` | `docs_protocol_zones-serial-json-parity.md` | SerialJSON parity inventory. |
| `docs/protocol/k1-ws-contract.yaml` | `_BUNDLE_protocol_contracts.txt` | Bundle (protocol) — wrapped with separator. |
| `docs/protocol/k1-rest-contract.yaml` **[REBUILT-REV3]** | `_BUNDLE_protocol_contracts.txt` | Bundle (protocol). **Rev 3:** lines 683 / 699 `apMode` / `connect` descriptions rewritten; "KNOWN BROKEN" string eliminated; bundle rebuilt 2026-05-04. |
| `docs/adr/zone-composer-architecture-decisions.md` | `docs_adr_zone-composer-architecture-decisions.md` | ADR-001 Zone Composer (Captain-approved 2026-05-01). |
| `docs/cron/k1-launch-research.md` | `docs_cron_k1-launch-research.md` | Scheduled-task definition; canonical recipe. |
| `docs/design/VOICE_CONTROL_EXPLORATION_PLAN.md` | `docs_design_VOICE_CONTROL_EXPLORATION_PLAN.md` | Exploration plan; references current voice harness (microWakeWord proven). |
| `docs/tooling/claude-mem-usage-optimisation-2026-05-02.md` | `docs_tooling_claude-mem-usage-optimisation-2026-05-02.md` | Local integration decisions for claude-mem. |
| `docs/superpowers/ios-firmware-parity-phase-1.md` | `docs_superpowers_ios-firmware-parity-phase-1.md` | Phase 1 plan with RBDO state. Active work. |
| `docs/superpowers/ios-firmware-parity-phase-2.md` | `docs_superpowers_ios-firmware-parity-phase-2.md` | Phase 2 plan. Active work. |
| `docs/superpowers/ios-firmware-parity-phase-3-scoping.md` | `docs_superpowers_ios-firmware-parity-phase-3-scoping.md` | Phase 3 scoping doc. Active work. |
| `docs/marketing/K1-DUAL-STATE-POSITIONING.md` | `docs_marketing_K1-DUAL-STATE-POSITIONING.md` | Locked positioning, banned-language register. |
| `docs/marketing/K1-LANDING-PAGE-BUILD-SPEC.md` | `docs_marketing_K1-LANDING-PAGE-BUILD-SPEC.md` | Master build spec for landing page. |
| `docs/marketing/K1-LAUNCH-VIDEO-SPEC.md` | `docs_marketing_K1-LAUNCH-VIDEO-SPEC.md` | Production spec for launch video. |
| `docs/marketing/K1-STRATEGY.md` | `docs_marketing_K1-STRATEGY.md` | 150 marketing tactics; hardware-grounded. |
| `docs/marketing/K1-TAGLINES.md` | `docs_marketing_K1-TAGLINES.md` | 220 taglines from verified hardware metrics. |
| `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` | `firmware-v3_docs_EFFECT_DEVELOPMENT_STANDARD.md` | MANDATORY — Captain-approved effect-authoring rules |
| `firmware-v3/docs/CQRS_STATE_ARCHITECTURE.md` | `firmware-v3_docs_CQRS_STATE_ARCHITECTURE.md` | Canonical state-management/command-dispatch architecture |
| `firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md` | `firmware-v3_docs_audio-visual_audio-visual-semantic-mapping.md` | Audio-visual intelligence architecture v2.0.0 — canonical mapping doctrine |
| `firmware-v3/docs/api/api-v1.md` | `firmware-v3_docs_api_api-v1.md` | LightwaveOS API v1 — current shipping REST + WebSocket protocol |
| `firmware-v3/docs/api/api-v2.md` **[DISCLAIMED]** | `firmware-v3_docs_api_api-v2.md` | Re-included per Captain Path B as unimplemented spec draft. Carries STATUS BANNER + 102 per-section reminders + closing reaffirmation. |
| `firmware-v3/docs/api/api-legacy.md` **[DISCLAIMED]** | `firmware-v3_docs_api_api-legacy.md` | Re-included after audit caught false-negative EXCLUDE: legacy `/api/*` endpoints still function on K1 v3.4 per source verification (`V1ApiRoutes.cpp`). LEGACY-COMPATIBILITY banner clarifies status hierarchy. |
| `firmware-v3/docs/debugging/MABUTRACE_GUIDE.md` | `firmware-v3_docs_debugging_MABUTRACE_GUIDE.md` | Canonical tracing/Perfetto capture workflow |
| `firmware-v3/docs/debugging/TRACE_INSTRUMENTATION_SPEC.md` | `firmware-v3_docs_debugging_TRACE_INSTRUMENTATION_SPEC.md` | Master spec for adding TRACE_* points |
| `firmware-v3/docs/audio-visual/AUDIO_OUTPUT_SPECIFICATIONS.md` | `firmware-v3_docs_audio-visual_AUDIO_OUTPUT_SPECIFICATIONS.md` | Canonical Part 1 — comprehensive audio output specs |
| `firmware-v3/docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md` | `firmware-v3_docs_audio-visual_VISUAL_PIPELINE_MECHANICS.md` | Canonical Part 2 — rendering pipeline + propagation mechanics |
| `firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` | `firmware-v3_docs_audio-visual_IMPLEMENTATION_PATTERNS.md` | Canonical companion — patterns for effect authors |
| `firmware-v3/docs/audio-visual/COLOR_PALETTE_SYSTEM.md` | `firmware-v3_docs_audio-visual_COLOR_PALETTE_SYSTEM.md` | Canonical companion — palette system reference |
| `firmware-v3/docs/audio-visual/TROUBLESHOOTING.md` | `firmware-v3_docs_audio-visual_TROUBLESHOOTING.md` | Canonical companion — audio-visual troubleshooting guide |
| `firmware-v3/docs/audio-visual/AUDIO_SYSTEM_ARCHITECTURE.md` | `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE.md` | LightwaveOS v2 Audio System Architecture v1.0 — Reference |
| `firmware-v3/docs/audio-visual/AUDIO_SYSTEM_ARCHITECTURE_VISUAL.md` | `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE_VISUAL.md` | Visual companion to architecture doc |
| `firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md` | `firmware-v3_docs_audio-visual_MUSICAL_LOGIC_CANONICAL_MODEL.md` | Single source of truth for what the audio pipeline produces today |
| `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md` | `firmware-v3_docs_audio-visual_audio-visual-contract-surface.md` | "Implementation source of truth" v1.2.0 |
| `firmware-v3/docs/audio-visual/bins64-adaptive-guidance.md` | `firmware-v3_docs_audio-visual_bins64-adaptive-guidance.md` | Current effect-author guidance for bins64 vs bins64Adaptive |
| `firmware-v3/docs/audio-visual/README.md` | `firmware-v3_docs_audio-visual_README.md` | Index for audio-visual documentation suite |
| `firmware-v3/docs/audio-visual/ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md` | `firmware-v3_docs_audio-visual_ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md` | Status: Accepted — canonical ADR for the onset surface |
| `firmware-v3/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` | `firmware-v3_docs_architecture_DEFENSIVE_BOUNDS_CHECKING.md` | Canonical validation pattern used throughout the codebase |
| `firmware-v3/docs/architecture/WEB_SERVER_MODULAR_ARCHITECTURE.md` | `firmware-v3_docs_architecture_WEB_SERVER_MODULAR_ARCHITECTURE.md` | Current architecture description |
| `firmware-v3/docs/migration/WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md` | `firmware-v3_docs_migration_WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md` | Migration COMPLETE — guide for adding new routes/commands |
| `firmware-v3/docs/debugging/DEBUG_SYSTEM.md` | `firmware-v3_docs_debugging_DEBUG_SYSTEM.md` | LightwaveOS Debug System v1.0.0 — Status: Active |
| `firmware-v3/docs/effects-catalog/EFFECTS_INVENTORY.md` | `firmware-v3_docs_effects-catalog_EFFECTS_INVENTORY.md` | Canonical effects inventory v1.0.0 |
| `firmware-v3/docs/effects-catalog/MATH_APPENDIX.md` | `firmware-v3_docs_effects-catalog_MATH_APPENDIX.md` | Mathematical functions reference — canonical (2,837 lines ≈ 35-40K words; well under NotebookLM 500K-word/source ceiling) |
| `firmware-v3/docs/effects-catalog/PATTERN_TAXONOMY.md` | `firmware-v3_docs_effects-catalog_PATTERN_TAXONOMY.md` | Rendering pattern taxonomy — canonical |
| `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` | `firmware-v3_docs_EFFECT_FRAMEWORK_STANDARD.md` | Ratified default classifications — 12 LOAD-BEARING properties |
| `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md` | `firmware-v3_docs_EFFECTS_BEHAVIORAL_REFERENCE.md` | Auto-generated reference for all 174 registered effect IDs |
| `firmware-v3/docs/gradient-system.md` | `firmware-v3_docs_gradient-system.md` | K1 gradient rendering system design — canonical |
| `firmware-v3/docs/OTA_UPDATE_GUIDE.md` | `firmware-v3_docs_OTA_UPDATE_GUIDE.md` | Current OTA update guide |
| `firmware-v3/docs/STIMULUS_CONTROL_CONTRACT.md` | `firmware-v3_docs_STIMULUS_CONTROL_CONTRACT.md` | Cross-stack stimulus override contract — canonical |
| `firmware-v3/docs/AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` **[DISCLAIMED]** | `firmware-v3_docs_AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md` | Engineering reference for the LGPExperimentalAudioPack effects. Hardened with prepended "DESIGN-RATIONALE DOCUMENT" banner pointing to firmware source for current parameter values. |
| `firmware-v3/docs/NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md` **[DISCLAIMED]** | `firmware-v3_docs_NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md` | Refactor blueprint for the 132-151 pack. Hardened with prepended "DESIGN-RATIONALE DOCUMENT" banner. |
| `firmware-v3/docs/p1-09-migration-cookbook.md` **[DISCLAIMED]** | `firmware-v3_docs_p1-09-migration-cookbook.md` | Mechanical reference for the per-zone state migration pattern. Hardened with prepended ZONE-COUNT clarification banner. |
| `firmware-v3/docs/design/ONSET_DETECTOR_SPEC.md` | `firmware-v3_docs_design_ONSET_DETECTOR_SPEC.md` | ADR-002 — Accepted spec for onset detection pipeline |
| `firmware-v3/docs/performance/RMT_SHOW_PATH_2026-02-28.md` | `firmware-v3_docs_performance_RMT_SHOW_PATH_2026-02-28.md` | Current RMT4 architecture explanation (FastLED show path) |
| `firmware-v3/docs/performance/VALIDATION_OVERHEAD.md` | `firmware-v3_docs_performance_VALIDATION_OVERHEAD.md` | Current validation performance characteristics |
| `firmware-v3/docs/testing/AUDIO_TEST_HARNESS.md` | `firmware-v3_docs_testing_AUDIO_TEST_HARNESS.md` | Status: Production — audio effect validation test harness |
| `firmware-v3/docs/testing/METRICS_REFERENCE.md` | `firmware-v3_docs_testing_METRICS_REFERENCE.md` | Status: Production — effect validation metrics reference |
| `firmware-v3/docs/testing/TEST_SCENARIOS.md` | `firmware-v3_docs_testing_TEST_SCENARIOS.md` | Status: Production — audio effect validation test scenarios |
| `firmware-v3/docs/measurement_protocols/m1_lgp_fringe.md` | `firmware-v3_docs_measurement_protocols_m1_lgp_fringe.md` | Current measurement protocol for LGP fringe-coherence |
| `tab5-encoder/docs/STIMULUS_CONTROL_CONTRACT.md` | `tab5-encoder_docs_STIMULUS_CONTROL_CONTRACT.md` | Cross-system stimulus injection contract — shipped behaviour |
| `tab5-encoder/docs/IMPLEMENTATION_SPEC.md` | `tab5-encoder_docs_IMPLEMENTATION_SPEC.md` | LOCKED decisions for shipped 3-tab UI redesign |
| `tab5-encoder/docs/DESIGN_BRIEF.md` | `tab5-encoder_docs_DESIGN_BRIEF.md` | Problem statement + final layout for shipped redesign |
| `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md` **[REWRITTEN-REV3]** | `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` | Ratified design principles. **Rev 3:** bundled copy carries REVISION BANNER explaining the case-study correction; source: technical-constraints bullet (line 305) softened, "Real-World Example" case study (lines 312-316) substantially rewritten with corrected forensic framing, K1 Application bullet (line 325) softened. |
| `tab5-encoder/docs/ARCHITECTURE_DECISION_PRINCIPLES.md` | `tab5-encoder_docs_ARCHITECTURE_DECISION_PRINCIPLES.md` | Ratified architecture principles; matches main.cpp decomposition |
| `tab5-encoder/docs/ZONE_COMPOSER_V2_SPEC.md` | `tab5-encoder_docs_ZONE_COMPOSER_V2_SPEC.md` | Final V2 layout spec — V2 changes committed 2026-04-03 |
| `tab5-encoder/docs/EFFECT_ORDER_REFERENCE.md` | `tab5-encoder_docs_EFFECT_ORDER_REFERENCE.md` | Canonical reference for shipped effect-cycling pipeline (162 effects) |
| `tab5-encoder/docs/ROW2_EFFECT_PARAMETER_SPEC.md` | `tab5-encoder_docs_ROW2_EFFECT_PARAMETER_SPEC.md` | Live encoder/UI contract for `effects.parameters.*` WS commands |
| `tab5-encoder/docs/CONTROLSURFACE_RESEARCH.md` **[HARDENED]** | `tab5-encoder_docs_CONTROLSURFACE_RESEARCH.md` | Research-backed semantic analysis of shipped FX PARAMS surface. Prepended "EXPLORATORY RESEARCH RATIFIED INTO SHIPPED DESIGN" banner. |
| `tab5-encoder/docs/MENU_SYSTEM_RESEARCH.md` **[HARDENED]** | `tab5-encoder_docs_MENU_SYSTEM_RESEARCH.md` | Navigation research that informed shipped 3-tab nav. Prepended "EXPLORATORY RESEARCH RATIFIED INTO SHIPPED DESIGN" banner. |
| `tab5-encoder/docs/UI_DESIGN_RESEARCH.md` **[HARDENED]** | `tab5-encoder_docs_UI_DESIGN_RESEARCH.md` | Competitive UI research that informed shipped redesign. Prepended "EXPLORATORY RESEARCH RATIFIED INTO SHIPPED DESIGN" banner. |

### Tier 4: Source code bundles (SSA3 / SSA4 / SSA5)

**Bundle 1 — `_BUNDLE_firmware_contracts.txt` (149.5 KB, 14 files):**

| # | Source path | Size |
|---|---|---|
| 1 | `firmware-v3/src/audio/contracts/ControlBus.h` | ~30.0 KB |
| 2 | `firmware-v3/src/plugins/api/EffectContext.h` | ~48.5 KB |
| 3 | `firmware-v3/src/plugins/api/IEffect.h` | ~10.5 KB |
| 4 | `firmware-v3/src/plugins/api/IEffectRegistry.h` | ~1.5 KB |
| 5 | `firmware-v3/src/plugins/api/OnsetContext.h` | ~0.8 KB |
| 6 | `firmware-v3/src/audio/contracts/MotionSemantics.h` | ~11.5 KB |
| 7 | `firmware-v3/src/audio/contracts/OnsetSemantics.h` | ~1.1 KB |
| 8 | `firmware-v3/src/audio/contracts/MusicalSaliency.h` | ~7.1 KB |
| 9 | `firmware-v3/src/audio/contracts/MusicalGrid.h` | ~5.6 KB |
| 10 | `firmware-v3/src/audio/contracts/AudioTime.h` | ~1.1 KB |
| 11 | `firmware-v3/src/audio/contracts/SnapshotBuffer.h` | ~3.9 KB |
| 12 | `firmware-v3/src/audio/contracts/StyleDetector.h` | ~1.8 KB |
| 13 | `firmware-v3/src/audio/contracts/MotionShaper.h` | ~4.2 KB |
| 14 | `firmware-v3/src/audio/contracts/AudioEffectMapping.h` | ~15.9 KB |

**Bundle 2 — `_BUNDLE_firmware_actors.txt` (97.7 KB, 5 files):**

| # | Source path | Size |
|---|---|---|
| 1 | `firmware-v3/src/audio/AudioActor.h` | ~41.9 KB |
| 2 | `firmware-v3/src/core/actors/RendererActor.h` | ~37.5 KB |
| 3 | `firmware-v3/src/core/actors/ShowDirectorActor.h` | ~5.8 KB |
| 4 | `firmware-v3/src/plugins/PluginManagerActor.h` | ~7.3 KB |
| 5 | `firmware-v3/src/network/webserver/WsCommandRouter.h` | ~2.6 KB |

**Bundle 3 — `_BUNDLE_ios_architecture.txt` (88 KB, 5 files):**

| Path | Role |
|---|---|
| `lightwave-ios-v2/LightwaveOS/App/LightwaveOSApp.swift` | App entry — `@main` root, environment wiring, AppViewModel hand-off (58 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/RESTClient.swift` | Actor-isolated REST client; 150ms debounce; AP-only base URL (1,175 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/WebSocketService.swift` | Actor-isolated WS service; reconnection state machine (849 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/UDPStreamReceiver.swift` | Actor-isolated UDP receiver; high-frequency LED stream ingest (147 lines). |
| `lightwave-ios-v2/LightwaveOS/Network/DeviceDiscoveryService.swift` | K1 discovery via Bonjour/mDNS on local AP network (318 lines). |

**Bundle 4 — `_BUNDLE_tab5_architecture.txt` (132 KB, 8 files):**

| # | Source path | Role |
|---|---|---|
| 1 | `tab5-encoder/src/network/WebSocketClient.h` | WS client public interface |
| 2 | `tab5-encoder/src/network/WsMessageRouter.h` | Inbound message router (consumer of K1 WS protocol) |
| 3 | `tab5-encoder/src/input/DualEncoderService.h` | 16-encoder unified service (Unit A + Unit B) |
| 4 | `tab5-encoder/src/parameters/ParameterMap.h` | Parameter definition table |
| 5 | `tab5-encoder/src/parameters/ParameterMap.cpp` | Concrete parameter table data |
| 6 | `tab5-encoder/src/parameters/ParameterHandler.h` | Sync controller (debounce, publish, apply) |
| 7 | `tab5-encoder/src/ui/ControlSurfaceUI.h` | FX PARAMS encoder-binding surface |
| 8 | `tab5-encoder/src/ui/ControlSurfaceUI.cpp` | FX PARAMS implementation |

### Tier 5: Instructions & governance (SSA1)

| Source path | Output filename | Reason |
|---|---|---|
| `instructions/GOV-amendment-001-dual-pipeline.md` | `instructions_GOV-amendment-001-dual-pipeline.md` | Governance amendment 001. |
| `instructions/GOV-research-pipeline.md` | `instructions_GOV-research-pipeline.md` | Research pipeline governance. |
| `instructions/naming-policy-v1.md` | `instructions_naming-policy-v1.md` | Naming policy v1. |
| `instructions/repo-governance-v1.md` | `instructions_repo-governance-v1.md` | Repo governance v1. |
| `instructions/changelog/_fragment-template.md` | `instructions_changelog__fragment-template.md` | Changelog-fragment YAML template (governance pipeline schema). |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-contract-docs.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-contract-docs.md` | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-effect-api-lock.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-effect-api-lock.md` | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation.md` | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--mabutrace-capture-tooling.md` | `instructions_changelog_2026-04-27--firmware-v3--mabutrace-capture-tooling.md` | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--phase-5-effect-exemplars.md` | `instructions_changelog_2026-04-27--firmware-v3--phase-5-effect-exemplars.md` | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--phase-5-native-harness.md` | `instructions_changelog_2026-04-27--firmware-v3--phase-5-native-harness.md` | Governance-pipeline changelog fragment. |

## EXCLUDE register (consolidated)

*Unchanged from Rev 2.*

### Dated session handovers / incident postmortems

| Source path | Reason |
|---|---|
| `docs/COMMIT_CONTEXT_2026-03-12.md` | Date-stamped commit-series narrative. Stale handover. |
| `docs/CC_AGENT_TOOLCHAIN_COMPLETION.md` | Dated one-shot CC orchestration prompt. Session-style handover. |
| `docs/CC_HANDOFF_REMAINING_TASKS.md` | Explicit handoff document. |
| `docs/findings.md` | Captain decision per flag #7: explicit "Running log of firmware-v3 technical findings" — session-scoped working log. Flipped from INCLUDE to EXCLUDE during forensic audit. |
| `docs/MULTI_WORKTREE_TESTING_GUIDE.md` | Captain decision per flag #7: process documentation for past session, dated 2026-03-06. Flipped from INCLUDE to EXCLUDE during forensic audit. |
| `firmware-v3/docs/SESSION_HANDOVER_20260323.md` | Session handover — dated artifact. |
| `firmware-v3/docs/SESSION_HANDOVER_20260325_ONSET_HARDENING.md` | Session handover — dated artifact. |
| `firmware-v3/docs/audit/move_0_2_centre_origin_audit_2026-04-27.md` | Dated audit. |
| `firmware-v3/docs/audit/phase_5_visual_sign_off_2026-04-28.md` | Dated audit. |
| `firmware-v3/docs/audit/PHASE_E_CATALOGUE_AUDIT_2026-04-30.md` | Dated audit. |
| `firmware-v3/docs/forensic-audit-2026-04-17.md` | Dated forensic audit. |
| `firmware-v3/docs/INCIDENT_LED_STABILITY_POSTMORTEM_2026-03-04.md` | Dated incident postmortem. |
| `firmware-v3/docs/TECHNICAL_DEBT_AUDIT_2026-03-04.md` | Dated technical-debt audit. |
| `firmware-v3/docs/STAGE1_CHERRY_PICK_PLAN.md` | Stage cherry-pick plan. |
| `firmware-v3/docs/design/ONSET_QUARANTINE_20260325.md` | Dated quarantine matrix. |
| `firmware-v3/docs/nvs-partition-grow-captain-decision-2026-04-18.md` | Captain decision doc, exclusion class. |
| `firmware-v3/docs/p1-10-ota-hash-captain-decision-2026-04-18.md` | Captain decision doc, exclusion class. |
| `lightwave-ios-v2/docs/AUDIT_REPORT.md` | Dated audit (3 February 2026). Recommendations contradict canonical CLAUDE.md hard constraints. |
| `tab5-encoder/docs/TAB5_MEMORY_AUDIT_2026-04-03.md` | Dated audit — instruction excludes. |
| `tab5-encoder/docs/forensic-audit-2026-04-18.md` | Forensic audit. |
| `tab5-encoder/docs/wave2-ota-handover-2026-04-18.md` | Handover artefact. |

### Pre-implementation deliberation / Captain decision process

| Source path | Reason |
|---|---|
| `docs/TOOLCHAIN_ORCHESTRATION_PROMPT.md` | Master orchestration prompt — operational artefact superseded by canonical TOOLCHAIN_IMPLEMENTATION_GUIDE.md. |
| `docs/node-composer-research.md` | Captain decision per flag #7: explicit self-description "10-vector research dump". Flipped from INCLUDE to EXCLUDE during forensic audit. |
| `firmware-v3/docs/specs/16kHZ_Nyquist_LUT.md` | Pre-implementation LUT spec note. |
| `firmware-v3/docs/specs/STM-128-BAND-UPGRADE-PROMPT.md` | Codex agent prompt + spec, pre-implementation. |
| `firmware-v3/docs/specs/STM-CODEX-PROMPT.md` | Codex agent prompt. |
| `firmware-v3/docs/specs/STM-DUAL-EDGE-SPEC.md` | DRAFT — pending feasibility benchmark. |
| `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-CODEX-PROMPT.md` | Codex agent prompt. |
| `firmware-v3/docs/specs/STM-SPECTRAL-SHOOTOUT-SPEC.md` | A/B benchmark spec, pre-implementation. |
| `firmware-v3/docs/design/CLOSED_LOOP_QUALITY_SYSTEM.md` | DRAFT — for discussion. |
| `firmware-v3/docs/design/INFERENCE_TASK_DECISION_BRIEF.md` | Programme decision brief — planning. |
| `firmware-v3/docs/design/INFERENCE_TASK_PLACEMENT_MATRIX.md` | Programme placement matrix — planning. |
| `firmware-v3/docs/prompts/audioactor-coefficient-revert-prompt.md` | Agent prompt, not firmware doc. |
| `firmware-v3/docs/prompts/CORRECTION-audioactor-revert-cancelled.md` | Agent prompt correction. |
| `firmware-v3/docs/prompts/device-baseline-enforcement-prompt.md` | Agent prompt. |
| `firmware-v3/docs/prompts/edge-mixer-implementation-prompt.md` | Agent prompt. |
| `firmware-v3/docs/prompts/edgemixer-rgb-matrix-refactor-prompt.md` | Agent prompt. |
| `firmware-v3/docs/prompts/edgemixer-validation-test-plan.md` | Agent prompt. |
| `firmware-v3/docs/prompts/firmware-checklist-install.md` | Agent prompt. |
| `firmware-v3/docs/prompts/gstack-install.md` | Agent prompt. |
| `firmware-v3/docs/prompts/waveform-freeze-bug-fix-prompt.md` | Agent prompt. |
| `firmware-v3/REFERENCE_HARNESS.md` | Operational test-harness for PyTorch-port calibration, not core architecture. Forensic audit upgraded candidate to EXCLUDE on closer read. |
| `tab5-encoder/docs/AGENT_DESIGN_INSTRUCTIONS.md` | Meta about agent process. |
| `tab5-encoder/docs/DECISION_FRAMEWORK_ANALYSIS.md` | Deliberation, not shipped behaviour. |
| `tab5-encoder/docs/DESIGN_DECISION_PROCESS.md` | Process narrative; principles already captured in PRODUCT/ARCHITECTURE_DECISION_PRINCIPLES. |
| `tab5-encoder/docs/IMPLEMENTATION_SPEC_REVIEW.md` | Review artefact. |
| `tab5-encoder/docs/MANDATE_ENFORCEMENT_PROPOSAL.md` | Proposal. |
| `tab5-encoder/docs/MANDATE_REFINEMENT.md` | Deliberation. |
| `tab5-encoder/docs/PARAM_ALLOCATION_AUDIT.md` | Audit. |
| `tab5-encoder/docs/UI_AUDIT_REPORT.md` | Audit. |

### Unshipped speculative research

| Source path | Reason |
|---|---|
| `firmware-v3/docs/research/findings/*` (24 files) | Dated research findings. |
| `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/**/*` (~36 files) | Dated runtime-evidence reports. |
| `firmware-v3/docs/research/synergy-topology/**/*` (~12 files) | Research passes, no ratification. |
| `firmware-v3/docs/research/spazz_redesign_2026-04-30/**/*` (~22 files) | Research substrate not ratified into firmware doctrine. |
| `firmware-v3/docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` | Substrate doc — ratified output is `EFFECT_FRAMEWORK_STANDARD.md` (INCLUDED). |
| `firmware-v3/docs/research/aubio-onset-reference.md` | Research reference, not ratified. |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md` | Dated baseline, research. |
| `firmware-v3/docs/research/AUDIO_LATTICE_CONFIGURATION_INVESTIGATION.md` | Investigation doc, exploratory. |
| `firmware-v3/docs/research/AUDIO_MUSICAL_LOGIC_SOURCE_AUDIT.md` | Source audit, research. |
| `firmware-v3/docs/research/CHORD_ROOT_ORIGIN_TRACE.md` | Research trace. |
| `firmware-v3/docs/research/CONSOLIDATED_ASSESSMENT.md` | Assessment doc, research. |
| `firmware-v3/docs/research/EMBEDDED_TRACING_RESEARCH_2026.md` | Research doc, dated. |
| `firmware-v3/docs/research/emotiscope-silence-detection-research.md` | Research, not ratified. |
| `firmware-v3/docs/research/essentia-onset-analysis.md` | Research analysis. |
| `firmware-v3/docs/research/phase5_audio_source_audit_checkpoint_2026-04-27.md` | Dated checkpoint. |
| `firmware-v3/docs/research/PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md` | Phase research synthesis. |
| `firmware-v3/docs/research/README_SILENCE_DETECTION.md` | Research index. |
| `firmware-v3/docs/research/README-emotiscope-research.md` | Research index. |
| `firmware-v3/docs/research/sb_chroma12_lineage_checkpoint_2026-04-27.md` | Dated lineage checkpoint. |
| `firmware-v3/docs/research/SB_ES_MOTION_BRAINSTORM_CATALOGUE_2026-04-26.md` | Brainstorm catalogue. |
| `firmware-v3/docs/research/SB_ES_MOTION_MECHANICS_TAXONOMY_2026-04-26.md` | Motion taxonomy, research. |
| `firmware-v3/docs/research/SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` | Framework reconstruction, research substrate. |
| `firmware-v3/docs/research/session-notes-2026-04-30/*` | Session notes. |
| `firmware-v3/docs/research/SILENCE_DETECTION_*` (4 files) | Research, not ratified. |
| `firmware-v3/docs/research/silence-detection-comparison.md` | Research comparison. |
| `firmware-v3/docs/SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md` | Research doc — wled-* and emotiscope-algorithms.md cover the canonical ports. |
| `lightwave-ios-v2/docs/EDGEMIXER_INTEGRATION_RESEARCH.md` | Integration research dated 2026-03-25; speculative architectural prose. Has not yet shipped. |

### Files contradicting CLAUDE.md / superseded / DRAFT / proposal-only

| Source path | Reason |
|---|---|
| `firmware-v3/docs/api/enhancement-engine-api.md` | Feature-flagged build (`FEATURE_ENHANCEMENT_ENGINES=1`) — not default shipping behaviour. Audit confirmed: flag not set in canonical envs (`esp32dev_audio_esv11_k1v2_32khz` / `esp32dev_audio_esv11_32khz`); doc targets non-existent `esp32dev_enhanced` env. |
| `firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md` | Foundation contract — "implementation not yet authorised beyond policy/helper design". |
| `firmware-v3/docs/audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md` | Dated 2025-12-29; superseded by IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS canonicals. |
| `firmware-v3/docs/audio-visual/audio-bloom-implementation.md` | Single-effect implementation note, dated; superseded by general docs. |
| `firmware-v3/docs/audio-visual/audio-gate-fix-2025-01.md` | Dated 2025-01-21 fix log — historical. |
| `firmware-v3/docs/audio-visual/GDFT_VERIFICATION_REPORT.md` | Dated 2025-12-29 verification report — historical. |
| `firmware-v3/docs/audio-visual/SALIENCY_ARCHITECTURE_REVIEW.md` | "Awaiting Implementation Decision" — proposal. |
| `firmware-v3/docs/architecture/WEBSERVER_BASELINE_INVENTORY.md` | Pre-refactor baseline — refactor complete per migration guide. |
| `firmware-v3/docs/debugging/SERIAL_DEBUG_CHAOS_MAP.md` | "DOG'S BREAKFAST" current-state diagnostic doc — superseded by DEBUG_SYSTEM. |
| `firmware-v3/docs/debugging/SERIAL_DEBUG_REDESIGN_PROPOSAL.md` | Proposal — not implemented. |
| `firmware-v3/docs/debugging/ARCHITECTURE_REVIEW.md` | Manual review dated 2026-01-22 — superseded by DEBUG_SYSTEM. |
| `firmware-v3/docs/debugging/trace_spec_sections/*.md` (10 files) | Detail files for TRACE_INSTRUMENTATION_SPEC.md (INCLUDED). Master spec already in bundle. |
| `firmware-v3/docs/effects-catalog/GAP_REPORT.md` | Dated gap report — informational, not canonical. |
| `firmware-v3/docs/implementation/WEBSERVER_REFACTOR_IMPLEMENTATION_SUMMARY.md` | Implementation summary — overlaps migration guide. |
| `firmware-v3/docs/p1-09-zone-state-followup-plan.md` | Follow-up plan (in-flight); cookbook is the canonical mechanical reference. |

### Auto-generated stubs / non-content (per SSA2 + audit)

| Source path | Reason |
|---|---|
| `firmware-v3/docs/CLAUDE.md` | claude-mem-context auto-generated index stub only (12 lines). Audit confirmed FLAGGED for Captain — EXCLUDE correct. |
| `firmware-v3/docs/api/CLAUDE.md` | claude-mem-context stub. Audit confirmed: 13-line claude-mem auto-stub. |
| `firmware-v3/docs/effects-catalog/CLAUDE.md` | claude-mem-context stub. |
| `docs/marketing/previews/` | HTML preview assets; HTML cannot enter NotebookLM. |
| `lightwave-ios-v2/LightwaveOS/ViewModels/AppViewModel.swift` (776 lines) | Audit identifies it as monolith — would dilute architectural signal in bundle. fsm-reference.md captures observable surface. |
| `lightwave-ios-v2/LightwaveOS/Views/ContentView.swift` | Out of scope — bundle is architecture-focused (network actors + entry). |
| `lightwave-ios-v2/LightwaveOSTests/ConnectionStateTests.swift` | Test code, not architecture. |
| `tab5-encoder/src/main.cpp` (3,299 LOC, 143 KB) | Too large; structure already captured in codebase-map + ARCHITECTURE_DECISION_PRINCIPLES. |
| `tab5-encoder/src/network/WebSocketClient.cpp` (1,150 LOC, 39 KB) | Header included; impl too large. |
| `tab5-encoder/src/ui/DisplayUI.cpp` (121 KB) | Too large for budget; LVGL screen behaviour summarised in lvgl-component-reference and DESIGN_BRIEF. |
| `tab5-encoder/src/ui/ZoneComposerUI.cpp` (67 KB, 1,627 LOC) | Covered semantically by ZONE_COMPOSER_V2_SPEC doc. |

## Captain flags ledger (UPDATED Rev 3)

### Resolved flags (9)

| # | Flag | Resolution |
|---|---|---|
| 1 | api-v1.md vs api-v2.md (SSA2) | RESOLVED — both INCLUDED. v2 carries multi-point scatter-disclaimers (102 per-section reminders) identifying it as unimplemented spec draft. |
| 2 | firmware-v3/docs/CLAUDE.md (SSA2) | RESOLVED — confirmed claude-mem 13-line auto-stub. EXCLUDE correct. |
| 3 | K1_ECOSYSTEM_API_ROADMAP.md (SSA1) | **FULLY RESOLVED Rev 3 (was: re-included with disclaimers).** Captain Approval #1 + #3 authorised forensic-corrected wording. Banner + 48 reminders + closing reaffirmation rewritten to evidence-grounded current-vs-goal-state framing. Pattern-match resistance preserved. |
| 5 | effects-catalog/MATH_APPENDIX.md (SSA2) | RESOLVED — 2,837 lines ≈ 35-40K words, well under NotebookLM 500K word/source ceiling. No split needed. |
| 7 | Borderline INCLUDEs (SSA1: findings/MULTI_WORKTREE/node-composer) | RESOLVED — all three flipped to EXCLUDE per Captain precision-over-speed directive. |
| 8 | tab5 RESEARCH docs INCLUDE (SSA5) | RESOLVED — hardened with prepended "EXPLORATORY RESEARCH RATIFIED INTO SHIPPED DESIGN" banners + closing reaffirmation. |
| 11 | enhancement-engine-api.md (SSA2) | RESOLVED — audit confirmed EXCLUDE correct. `FEATURE_ENHANCEMENT_ENGINES` not set in canonical envs; doc targets non-existent `esp32dev_enhanced` env. |
| 14 | PACK decomposition/blueprint docs (SSA2) | RESOLVED — hardened with prepended "DESIGN-RATIONALE DOCUMENT" banners pointing to firmware source for current parameter values. |
| **NEW** | **AP-only-EVER doctrine itself** | **RESOLVED Rev 3.** Captain Approval #1 authorised correction of 9 source-doctrine surfaces from strength-D ("AP-only-EVER, STA never worked, 6+ failures") to evidence-grounded strength-A/B (current AP-only via build flag; goal dual-mode AP-or-STA never together; concurrent AP+STA is the genuine bug surface). Captain Approval #2 authorised firmware engineering scope (BACKLOG F-5 — landed). Captain Approval #3 authorised bundle treatment Option B (selective rewrite — landed). |

### Standing flags (9) — defensible SSA calls, no further action recommended

| # | Flag | Status |
|---|---|---|
| 4 | `AppViewModel.swift` omission (SSA4) | Defensible per overseer + Captain. 776-line iOS monolith deliberately omitted to avoid biasing NotebookLM toward overloaded pattern. fsm-reference.md captures observable surface. |
| 6 | Date-stamped governance changelog fragments under `instructions/changelog/` (SSA1) | Kept INCLUDED (governance audit trail). Date-stamped naming touches the reject rule but content is canonical fragment-template structure. |
| 9 | Missing source files (SSA3) | Documented. `RenderContext.h` does not exist (render context lives in `EffectContext.h`); `firmware-v3/src/commands/CommandActor.h` does not exist (no CommandActor class anywhere); `WsCommandRouter.h` substituted as closest equivalent. |
| 10 | Effects-catalog dating (SSA2) | INCLUDED but flagged for future verification. `EFFECTS_INVENTORY.md`, `MATH_APPENDIX.md`, `PATTERN_TAXONOMY.md` dated 2026-02-21 (~2.5 months old). |
| 12 | `SENSORYBRIDGE_AUDIO_PROCESSING_RESEARCH.md` (SSA2) | EXCLUDED, defensible. wled-* and emotiscope-* references cover canonical ports. |
| 13 | `research/spazz_redesign_2026-04-30/` (SSA2) | EXCLUDED, defensible. Research substrate not ratified into firmware doctrine. |
| 15 | `audio-visual/AUDIO_REACTIVE_EFFECTS_ANALYSIS.md` (SSA2) | EXCLUDED, defensible. Duplicates content now in IMPLEMENTATION_PATTERNS + VISUAL_PIPELINE_MECHANICS canonicals. |
| 16 | tab5 `IMPLEMENTATION_SPEC.md` currency (SSA5) | INCLUDED, hardware-currency not verified in this pass. Spec asserts shipped-LOCKED status; no contradicting evidence found. |
| 17 | tab5 `main.cpp` / `DisplayUI.cpp` omission (SSA5) | Bundle excludes on size grounds (143 KB + 121 KB). Structure covered by codebase-map + FSM ref. |

## Per-SSA confidence

- **SSA1 (root + cross-cutting docs + protocol bundle):** HIGH for explicit handover/COMMIT_CONTEXT/CC exclusions and protocol/instructions/governance bundles. MEDIUM for borderline INCLUDE calls — these were subsequently flipped to EXCLUDE during the forensic audit (precision-over-speed correction). Every included file was opened far enough to read its abstract or top-level framing. STA scan run twice (broad sweep + included-file resweep).
- **SSA2 (firmware-v3 docs):** HIGH on initial pass — 8 flags surfaced for Captain. Forensic audit identified TWO HIGH-severity false-negative excludes (api-legacy.md, p1-09 zone-count clarification missing); both corrected. Confidence post-audit: HIGH with documented hardening on PACK and p1-09 docs.
- **SSA3 (firmware-v3 source bundles):** HIGH. All 19 advertised file paths verified by `ls`/`find` before reading; sizes match files actually packed. `RenderContext.h` and `CommandActor.h` non-existence verified by exhaustive `find firmware-v3/src -name "*.h"`. Bundle headers written verbatim, full-content, with exact separator format. STA scan run literally over each packed file path; every hit inspected and classified as false positive.
- **SSA4 (lightwave-ios-v2):** HIGH. All four included docs verified to exist, classified per include/exclude rule, free of STA-mode contamination. All five Swift files verified by `find` before bundling. Bundle assembled by streaming `cat` directly to disk — no Swift content entered agent context. AppViewModel exclusion is deliberate curation call (motivated by AUDIT_REPORT findings).
- **SSA5 (tab5-encoder):** HIGH for mandatory + LOCKED-spec inclusions (refs, IMPLEMENTATION_SPEC, ZONE_COMPOSER_V2_SPEC, ROW2_EFFECT_PARAMETER_SPEC, EFFECT_ORDER_REFERENCE, STIMULUS_CONTROL_CONTRACT, DESIGN_BRIEF). Memory observation #42240 confirms ZONE_COMPOSER_V2 was committed. Three RESEARCH docs (CONTROLSURFACE/MENU/UI_DESIGN) were MEDIUM on initial pass; post-audit hardened with RATIFIED-INTO-DESIGN banners → confidence upgraded.
- **Forensic audit (SSA-Audit, Rev 2):** HIGH — surfaced 2 HIGH-severity false-negative excludes (api-legacy, p1-09 zone-count) plus several confirmations of correct dispositions. Audit also validated SSA2's 17 silent skips as correctly classified by class (out of stated scope). Methodology and findings documented in `_AUDIT_FORENSIC_SSA2.md`.
- **Forensic-Genesis (SSA-Genesis, Rev 3):** EXTREMELY HIGH — 514-line authoritative dig surfaced 6 eras (Pre-genesis & Genesis 2025-06-24 → 2025-07-17, hiatus 2025-07-18 → 2025-12-08, v2 rebirth 2025-12-09 → 2025-12-29, Tab5/dual-network 2026-01-02 → 2026-02-04, Portable Mode AP+STA experiment 2026-02-05 → 2026-02-16, AP-only doctrinal 2026-02-17 → present) + 2 capitulation cycles + Era 5 Portable Mode AP+STA failure (`1d1589c6` → `0b270a48` → `9ef320fc` → `78601d7a` → doctrinal inflection at `d13889f8` 2026-02-17) as the genuine event the over-corrected doctrine was based on. Captain-confirmed via Approvals #1 + #2 + #3 on 2026-05-04.

## Provenance (full audit trail)

- **Rev 1 partial manifests** — `_MANIFEST_PARTIAL_SSA1.md` through `_MANIFEST_PARTIAL_SSA5.md`
- **Rev 2 audit** — `_AUDIT_FORENSIC_SSA2.md` (329 lines, 25.3 KB)
- **Rev 3 forensic excavation** — `_FORENSIC_WIFI_A_GIT.md`, `_FORENSIC_WIFI_B_SOURCE.md`, `_FORENSIC_WIFI_C_MEMORY.md`, `_FORENSIC_WIFI_D_DOCTRINE.md`, `_FORENSIC_WIFI_GENESIS.md` (514-line genesis-anchored deep dig), `_FORENSIC_WIFI_REPORT.md` (Rev 2)
- **Source-side reflection of Rev 3:** `BACKLOG.md` § F-5 entry tracks the firmware engineering scope per Captain Approval #2 (4 engineering tasks for K1 dual-mode WiFi delivery)
- **Curation prompt at:** `docs/tooling/notebooklm-bundles/CURATION_PROMPT.md`

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:Claude (synthesis-SSA) | Initial creation — aggregated 5 partial manifests into consolidated MANIFEST.md (125 sources). |
| 2026-05-04 | agent:Claude (SSA-Synth-2, post-audit) | Captain-directed forensic re-audit + multi-point disclaimer scatter on api-v2/ROADMAP/api-legacy + zone-count disclaimer on p1-09 + RATIFIED hardening on tab5 RESEARCH + DESIGN-RATIONALE hardening on PACK decomposition. Net: −3 files (flag #7 borderlines flipped to EXCLUDE), +3 files (api-v2, ROADMAP, api-legacy re-included with disclaimers). Net source count: 125 → 127. STA mention count rose from 9 to 22 (intended hardening effect from disclaimer scatter). 8 Captain flags resolved; 9 remain as defensible standing items. Evidence trail: `_AUDIT_FORENSIC_SSA2.md`. |
| 2026-05-04 | agent:Claude (SSA-Synth-3, post-Captain-approval) | **Rev 3.** Captain Approvals #1 + #2 + #3 applied. 9 source-doctrine surfaces corrected from strength-D over-correction wording ("AP-only-EVER, STA never worked, 6+ failures") to evidence-grounded strength-A/B wording (current AP-only via `WIFI_AP_ONLY` build flag; goal dual-mode AP-or-STA never together; concurrent AP+STA is the genuine bug surface). 6 bundled copies refreshed from updated sources (CLAUDE.md, CHANGELOG.md, BACKLOG.md, docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md, tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md, docs_K1_ECOSYSTEM_API_ROADMAP.md). 1 bundle rebuilt (`_BUNDLE_protocol_contracts.txt`, 99,065 bytes) — "KNOWN BROKEN" string eliminated. Pattern-match resistance preserved (51 ROADMAP disclaimer markers). Memory layer updated (firmware_wifi_architecture.md rewrite, feedback_never_change_network_architecture.md refinement, MEMORY.md index update). BACKLOG F-5 added (4 engineering tasks for K1 dual-mode WiFi delivery). 9 Captain flags now resolved (was 8) — flag #3 fully resolved (was: re-included with disclaimers); AP-only-EVER doctrine itself resolved as new entry. 8 standing flags unchanged. Evidence trail: `_FORENSIC_WIFI_A_GIT.md` + `_FORENSIC_WIFI_B_SOURCE.md` + `_FORENSIC_WIFI_C_MEMORY.md` + `_FORENSIC_WIFI_D_DOCTRINE.md` + `_FORENSIC_WIFI_GENESIS.md` (514 lines) + `_FORENSIC_WIFI_REPORT.md` (Rev 2). |
