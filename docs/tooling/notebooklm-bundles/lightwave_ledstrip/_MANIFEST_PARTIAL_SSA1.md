# SSA1 Partial Manifest — NotebookLM Curation (Lightwave-Ledstrip)

**Scope owned by SSA1:** root authority docs, `docs/` root .md files, `docs/` subdirs (one level), `instructions/` .md files, protocol contracts bundle, pre-commit bundle.

**Out of scope (other SSAs):** firmware-v3/, lightwave-ios-v2/, tab5-encoder/, harness/, k1-composer/, lightwave-dashboard/, scripts/, tools/, _archive/, .claude/, .github/.

**Convention:** flattened paths use `_` instead of `/`. Root authority docs keep their original names.

---

## INCLUDE

| Source path | Output filename | Bundle membership | Reason |
|---|---|---|---|
| `CLAUDE.md` | `CLAUDE.md` | root | Project-level orchestration doctrine; loaded every session. Authoritative. |
| `AGENTS.md` | `AGENTS.md` | root | Workflow discipline rules R1–R5. Authoritative governance. |
| `BACKLOG.md` | `BACKLOG.md` | root | Live calibration-debt ledger; referenced by RBDO gate. |
| `README.md` | `README.md` | root | Project overview. |
| `CONTRIBUTING.md` | `CONTRIBUTING.md` | root | Contribution rules. |
| `CHANGELOG.md` | `CHANGELOG.md` | root | Keep-a-changelog formatted history. |
| `TRADEMARK.md` | `TRADEMARK.md` | root | Trademark policy. |
| `.pre-commit-config.yaml` | `_BUNDLE_root_config.txt` | bundle (root config) | Pre-commit hook config, wrapped in bundle separator. |
| `docs/WORKFLOW_ROUTING.md` | `docs_WORKFLOW_ROUTING.md` | docs | Mandatory tool/skill routing table; referenced by CLAUDE.md. |
| `docs/MULTIPLIER_STACK.md` | `docs_MULTIPLIER_STACK.md` | docs | Canonical AI-toolchain inventory (March 2026). Abstract-tagged, current. |
| `docs/CAPTURE_TEST_SUITES.md` | `docs_CAPTURE_TEST_SUITES.md` | docs | Operational test profile reference (Reference/Stress/Isolation/Soak). |
| `docs/MULTI_WORKTREE_TESTING_GUIDE.md` | `docs_MULTI_WORKTREE_TESTING_GUIDE.md` | docs | Operational how-to for parallel hardware testing. Referenced by MEMORY.md. |
| `docs/ONSET_CAPTURE_WORKFLOW.md` | `docs_ONSET_CAPTURE_WORKFLOW.md` | docs | Operational onset-detector validation runbook. |
| `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | docs | Agent-executable runbooks for tooling installation; explicitly enforces AP-only. |
| `docs/DEPENDENCY_LICENSES.md` | `docs_DEPENDENCY_LICENSES.md` | docs | Licence audit (Apache 2.0 compatibility). Date-stamped but factual register, not handover. |
| `docs/K1_Waveform_Algorithm_Breakdown.md` | `docs_K1_Waveform_Algorithm_Breakdown.md` | docs | Per-frame algorithm reference for Waveform effect (0x1302). |
| `docs/node-composer-research.md` | `docs_node-composer-research.md` | docs | Self-described "research dump" — read when designing the Node Composer. Abstract-tagged. |
| `docs/CAPTURE_PIPELINE_REFERENCE.md` | `docs_CAPTURE_PIPELINE_REFERENCE.md` | docs | Canonical capture-pipeline reference (binary frame formats v1/v2, CLI). |
| `docs/findings.md` | `docs_findings.md` | docs | Curated findings/decisions register — not a one-shot handover. |
| `docs/HUNT-WAVE1-spec-changes.md` | `docs_HUNT-WAVE1-spec-changes.md` | docs | 16 firmware spec changes with abstract framing as research output. |
| `docs/protocol/README.md` | `docs_protocol_README.md` | docs/protocol | Protocol contracts directory README. |
| `docs/protocol/zones-command-matrix.md` | `docs_protocol_zones-command-matrix.md` | docs/protocol | Canonical command matrix across REST/WS/SerialJSON/SerialCLI. |
| `docs/protocol/zones-serial-json-parity.md` | `docs_protocol_zones-serial-json-parity.md` | docs/protocol | SerialJSON parity inventory. Abstract-tagged. |
| `docs/protocol/k1-ws-contract.yaml` | `_BUNDLE_protocol_contracts.txt` | bundle (protocol) | Wrapped with separator. |
| `docs/protocol/k1-rest-contract.yaml` | `_BUNDLE_protocol_contracts.txt` | bundle (protocol) | Wrapped with separator. |
| `docs/adr/zone-composer-architecture-decisions.md` | `docs_adr_zone-composer-architecture-decisions.md` | docs/adr | ADR-001 Zone Composer (Captain-approved 2026-05-01). |
| `docs/cron/k1-launch-research.md` | `docs_cron_k1-launch-research.md` | docs/cron | Scheduled-task definition; canonical recipe. |
| `docs/design/VOICE_CONTROL_EXPLORATION_PLAN.md` | `docs_design_VOICE_CONTROL_EXPLORATION_PLAN.md` | docs/design | Exploration plan, abstract-tagged, references current voice harness (microWakeWord proven). |
| `docs/tooling/claude-mem-usage-optimisation-2026-05-02.md` | `docs_tooling_claude-mem-usage-optimisation-2026-05-02.md` | docs/tooling | Local integration decisions for claude-mem; abstract-tagged. Date in filename per project naming. |
| `docs/superpowers/ios-firmware-parity-phase-1.md` | `docs_superpowers_ios-firmware-parity-phase-1.md` | docs/superpowers | Phase 1 plan with RBDO state. Active work. |
| `docs/superpowers/ios-firmware-parity-phase-2.md` | `docs_superpowers_ios-firmware-parity-phase-2.md` | docs/superpowers | Phase 2 plan. Active work. |
| `docs/superpowers/ios-firmware-parity-phase-3-scoping.md` | `docs_superpowers_ios-firmware-parity-phase-3-scoping.md` | docs/superpowers | Phase 3 scoping doc. Active work. |
| `docs/marketing/K1-DUAL-STATE-POSITIONING.md` | `docs_marketing_K1-DUAL-STATE-POSITIONING.md` | docs/marketing | Locked positioning, banned-language register. |
| `docs/marketing/K1-LANDING-PAGE-BUILD-SPEC.md` | `docs_marketing_K1-LANDING-PAGE-BUILD-SPEC.md` | docs/marketing | Master build spec for landing page. |
| `docs/marketing/K1-LAUNCH-VIDEO-SPEC.md` | `docs_marketing_K1-LAUNCH-VIDEO-SPEC.md` | docs/marketing | Production spec for launch video. |
| `docs/marketing/K1-STRATEGY.md` | `docs_marketing_K1-STRATEGY.md` | docs/marketing | 150 marketing tactics; hardware-grounded. |
| `docs/marketing/K1-TAGLINES.md` | `docs_marketing_K1-TAGLINES.md` | docs/marketing | 220 taglines from verified hardware metrics. |
| `instructions/GOV-amendment-001-dual-pipeline.md` | `instructions_GOV-amendment-001-dual-pipeline.md` | instructions | Governance amendment 001. |
| `instructions/GOV-research-pipeline.md` | `instructions_GOV-research-pipeline.md` | instructions | Research pipeline governance. |
| `instructions/naming-policy-v1.md` | `instructions_naming-policy-v1.md` | instructions | Naming policy v1. |
| `instructions/repo-governance-v1.md` | `instructions_repo-governance-v1.md` | instructions | Repo governance v1. |
| `instructions/changelog/_fragment-template.md` | `instructions_changelog__fragment-template.md` | instructions | Changelog-fragment YAML template (governance pipeline schema). |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-contract-docs.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-contract-docs.md` | instructions | Governance-pipeline changelog fragment (commit-style YAML, not session handover). |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-effect-api-lock.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-effect-api-lock.md` | instructions | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation.md` | `instructions_changelog_2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation.md` | instructions | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--mabutrace-capture-tooling.md` | `instructions_changelog_2026-04-27--firmware-v3--mabutrace-capture-tooling.md` | instructions | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--phase-5-effect-exemplars.md` | `instructions_changelog_2026-04-27--firmware-v3--phase-5-effect-exemplars.md` | instructions | Governance-pipeline changelog fragment. |
| `instructions/changelog/2026-04-27--firmware-v3--phase-5-native-harness.md` | `instructions_changelog_2026-04-27--firmware-v3--phase-5-native-harness.md` | instructions | Governance-pipeline changelog fragment. |

---

## EXCLUDE

| Source path | Reason |
|---|---|
| `docs/COMMIT_CONTEXT_2026-03-12.md` | Date-stamped commit-series narrative; explicit reject category in scope. Stale handover. |
| `docs/CC_AGENT_TOOLCHAIN_COMPLETION.md` | Dated one-shot CC orchestration prompt; describes a completed install state. Session-style handover. |
| `docs/CC_HANDOFF_REMAINING_TASKS.md` | Explicit handoff document; explicit reject category. |
| `docs/TOOLCHAIN_ORCHESTRATION_PROMPT.md` | Master orchestration *prompt* (paste-into-fresh-session style). Operational artefact superseded by the canonical TOOLCHAIN_IMPLEMENTATION_GUIDE.md (which IS included). Including both pollutes corpus with two near-duplicate authorities. |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md` | **STA-tainted.** Line 426 floats "Leader-as-AP with followers-as-STA" as an integration option. Although qualified as "currently architecturally prohibited", the file proposes STA as a viable future option. Per scope hard-reject doctrine: EXCLUDE. Recommend Captain re-author with the option removed before re-including. |
| `docs/marketing/previews/` | HTML preview assets; HTML cannot enter NotebookLM (skipped silently per scope rules). |

---

## STA scan results

Scanned all candidate `.md` files under `docs/`, `instructions/`, plus root authority docs. Hits:

| File | Line | Snippet | Disposition |
|---|---|---|---|
| `CLAUDE.md` | 117, 236 | "K1 is AP-ONLY — NEVER enable STA mode" | INCLUDE-safe (anti-STA guard) |
| `CHANGELOG.md` | 192 | "STA mode... corrected to AP-only reality" | INCLUDE-safe (correction note) |
| `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md` | 45, 560, 818 | "Never enable STA mode", "K1 is AP-ONLY" | INCLUDE-safe (constraint memory) |
| `docs/CC_HANDOFF_REMAINING_TASKS.md` | 106 | "K1 is AP-ONLY. Never enable STA mode." | EXCLUDED for handoff/dated reasons (STA mention itself is anti-STA). |
| `docs/COMMIT_CONTEXT_2026-03-12.md` | 44 | "preserves AP-only behaviour (no STA mode)" | EXCLUDED for date-stamped reasons (STA mention itself is anti-STA). |
| `docs/K1_ECOSYSTEM_API_ROADMAP.md` | 426 | "Leader-as-AP with followers-as-STA is one option but conflicts with the AP-only constraint" | **EXCLUDED — STA proposed as option even though qualified.** |

No other STA hits. No `wifi_sta`, `WIFI_MODE_STA`, "station mode", or "WiFi client" matches anywhere in scope.

---

## Confidence

**HIGH** for the INCLUDE/EXCLUDE separations of explicit handover/COMMIT_CONTEXT/CC files (rule is unambiguous), and for the protocol/instructions/governance bundles.

**MEDIUM** for the borderline calls listed under Unresolved flags below.

**Reasoning:** Every included file was opened far enough to read its abstract or top-level framing. STA scan was run twice (broad sweep + included-file resweep). British English checked in manifest. Bundles use the exact separator format from scope.

---

## Unresolved flags for Captain

1. **`docs/K1_ECOSYSTEM_API_ROADMAP.md` — EXCLUDED on STA-taint conservative read.** The line is qualified ("currently architecturally prohibited", "conflicts with the AP-only constraint"), but it does propose Leader-as-AP/followers-as-STA as "one option". Per scope rule "If a file proposes STA as viable, EXCLUDE it" — I excluded. **If Captain wants this roadmap in the corpus**, the source file needs an edit (delete or rewrite line 426) and re-curation. Alternative: include as-is and accept the contradiction with CLAUDE.md hard constraint.
2. **`docs/findings.md` — INCLUDED on "running log of findings" framing.** The file describes itself as a findings log spanning beat-tracker iterations, ESV11 tuning, etc. — it has an abstract and is curated, not a session dump. But it does cover historical investigations rather than current state. **If Captain treats findings.md as historical-only and not authoritative**, EXCLUDE.
3. **`docs/MULTI_WORKTREE_TESTING_GUIDE.md` — INCLUDED.** Opens with "On 2026-03-06, we deployed a workflow..." dated incident framing, but it's a how-to guide referenced by MEMORY.md (`multi-worktree-testing.md`). I treated it as operational reference. If the corpus rule is strict on dated incident phrasing, EXCLUDE.
4. **`docs/node-composer-research.md` — INCLUDED.** Self-described "research dump" from 2026-03-23. Abstract says "Read when designing or building the K1 Node Composer", framing it as a reference artefact. If Captain wants only canonical specs (not research dumps) in NotebookLM, EXCLUDE.
5. **Date-stamped governance changelog fragments (`instructions/changelog/2026-04-27--*.md`) — INCLUDED.** They are commit-style YAML records produced by GOV-research-pipeline, not session handovers. The scope explicitly says "instructions/ — copy all `.md` files". If the date-stamped naming triggers the dated-handover rule despite the governance pipeline schema, EXCLUDE.
6. **No `firmware-v3/`, `lightwave-ios-v2/`, `tab5-encoder/` files curated here.** Files under those prefixes were already present in the output dir (other SSAs working in parallel). I did not modify or delete them.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:SSA1 | Created. 51 file copies + 2 wrapped bundles. STA scan clean except K1_ECOSYSTEM_API_ROADMAP.md (excluded). 6 unresolved flags surfaced. |
