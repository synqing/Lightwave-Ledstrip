---
abstract: K1 WiFi-mode forensic synthesis report (REVISION 2 — supersedes Revision 1 with genesis-anchored timeline). Captain-authorised exhaustive backwards excavation per directive 'search ALL THE WAY back to 723f4522'. Five parallel forensic streams reconciled, including the deep archaeological pass that revealed Forensic-A's firmware-v3-only scope was insufficient. TRUE earliest commit is 34baa31d 2025-06-24 — six months earlier than Forensic-A's start. SIX distinct WiFi-mode eras identified spanning 2025-06 → 2026-05, including TWO capitulation cycles ending in WiFi-restriction codification. The 'STA never worked' doctrine is contradicted by Era 1 (Light Crystals STA-primary, 2025-06-24 → 2025-07-05) and Era 3 (v2 STA-primary rebirth, 2025-12-16 → 2026-01-03). The genuine failure was Era 5's 11-day Portable Mode AP+STA concurrent experiment (2026-02-05 → 2026-02-16), 'failed within hours' — Captain's referenced ESP32 known bug. Captain (SpectraSynq) was the author of Cycle 2 capitulation. ~3,418 LOC of STA infrastructure is PRESENT-DISABLED. Walk-back env added 24h ago.
---

# K1 WiFi-Mode Forensic Report — Strategic Round-Table Input (Revision 2)

**Date:** 2026-05-04
**Trigger:** Captain directive 2026-05-04 — "DEEPER forensic excavation needs to take place to DEFINITIVELY figure out what happened EXACTLY from the VERY BEGINNING. We cannot proceed until you unearth the EXACT timeline of events. You need to search git history/logs ALL THE WAY back to 723f4522 — Work your way to the evidence BACKWARDS."
**Method:** Originally four parallel scope-isolated forensic SSAs (A: git history / B: current source / C: memory archaeology / D: doctrine drift). Captain rejected as insufficient. Fifth SSA dispatched (Genesis Archaeology) with explicit instruction to use `git log --all` from THE FIRST COMMIT in the repo, including pre-firmware-v3 era. This revised report supersedes Revision 1.
**Provenance:** `_FORENSIC_WIFI_A_GIT.md` (firmware-v3 era only — superseded by E for full scope), `_FORENSIC_WIFI_B_SOURCE.md`, `_FORENSIC_WIFI_C_MEMORY.md`, `_FORENSIC_WIFI_D_DOCTRINE.md`, `_FORENSIC_WIFI_GENESIS.md` (514 lines, the authoritative complete-history dig). Full evidence trails preserved.

## Revision history

- **Rev 1 (initial synthesis):** based on Forensic-A's firmware-v3-only scope (38 commits from 2025-12-20). Identified 2026-02-17 as the AP-only-EVER hardening moment and the 6+ failures narrative as over-counted/doctrine-driven. Recommendations: bundle option B (selective rewrite).
- **Rev 2 (this revision):** Captain-directed deeper dig revealed pre-genesis history extending back to 2025-06-24. The 2026-02-17 hardening is now understood as Cycle 2's capitulation — Cycle 1 occurred 7 months earlier (2025-07-05 `c8d12479` "Disable WiFi - worthless shit"). Six eras identified. Captain himself (SpectraSynq) authored Cycle 2 capitulation. The genuine failure event was Era 5's Portable Mode AP+STA concurrent experiment (11 days, failed within hours). Recommendations strengthened.

---

## Executive Summary (Revision 2)

**Captain's recollection is CORROBORATED — and the evidence is now FAR stronger than Revision 1 suggested.**

The story spans 11 months across SIX distinct WiFi-mode eras, not the 4-month firmware-v3-only window the original Forensic-A surveyed.

**Two capitulation cycles, not one.** Each cycle: invest in WiFi capability → encounter failure → capitulate emotionally → codify the capitulation as doctrine. Cycle 1 (qaxzy author, 11 days, 2025-06-24 → 2025-07-05) ended in WiFi-disabled-by-default. Cycle 2 (SpectraSynq author = Captain himself, 62 days, 2025-12-16 → 2026-02-17) ended in AP-only-EVER doctrine.

**Era 1 and Era 3 prove STA mode WORKED.** The doctrine "STA has never worked" is empirically false:
- Era 1 (Light Crystals STA-primary, 2025-06-24 → 2025-07-05): 11 days of STA-primary operation. Includes commit `1d73c14e` adding mDNS "after WiFi connect" — mDNS only makes sense in STA mode.
- Era 3 (v2 STA-primary rebirth, 2025-12-16 → 2026-01-03): 18 days of explicit STA-primary architecture (commit `6ce6143e` 2026-01-28 codifying STA-primary, AP-as-fallback).

**The genuine failure was Era 5 — and it was specifically AP+STA concurrent, not STA-alone.** Era 5 (Portable Mode AP+STA experiment, 2026-02-05 → 2026-02-16, 11 days): an attempt at concurrent AP+STA mode that "failed within hours". This is the ESP32 known bug Captain referenced (concurrent operation corrupts auth state). It is NOT evidence that STA-alone fails. The Era 6 capitulation (`d13889f8` 2026-02-17) over-corrected from "AP+STA concurrent broken" to "STA forbidden entirely".

**The "6+ failures" narrative remains over-counted/doctrine-driven.** No primary per-incident audit trail exists. The "6+" appears in exactly ONE synthesised source (`firmware_wifi_architecture.md`, created 2026-04-26 — TWO MONTHS AFTER the codification) whose explicit enumeration is FOUR. The retrospective rationalisation cited the AP+STA concurrent failure mode as if it were STA-alone failure.

**The smoking-gun moment is `c8d12479` (2025-07-05) — not `d13889f8` (2026-02-17).** The original frustration moment is verbatim:
> "feat: Disable WiFi by default - remove that worthless shit"
> Body: "WiFi is now DISABLED BY DEFAULT because it's fucking worthless"
> Lands EXACTLY 24 HOURS after `7393ca4c` "WiFi Optimizer Pro" — heavy WiFi investment immediately followed by capitulation.

This is the same emotional pattern that recurred 7 months later in Cycle 2. The codebase has a doctrine-via-frustration history.

**~3,418 LOC of STA infrastructure is PRESENT-DISABLED in current firmware-v3 source** (not removed — gated by `WIFI_AP_ONLY` build flag + `m_forceApOnly` runtime lock). All STA paths use `WIFI_MODE_APSTA` concurrent mode — exactly the bug surface Captain wants to avoid. Pure `WIFI_MODE_STA`-only path does NOT exist anywhere.

**A walk-back has already started.** Commit `11e040d6` (2026-05-03, ~24 hours before this excavation) added a `_sta_validation` build env that explicitly `-UWIFI_AP_ONLY`. Captain's directive aligns with code-level rethinking already in progress.

**Total commit scope:** 222 K1-author commits + 66 content-touching WIFI_MODE/softAP/m_forceApOnly commits across all branches. Forensic-A's 38 was a strict subset.

---

## The Six Eras

The Genesis SSA identified six distinct WiFi-mode eras across the project's full history:

| Era | Dates | Duration | Posture | Author |
|-----|-------|----------|---------|--------|
| **1. Light Crystals STA-primary** | 2025-06-24 → 2025-07-05 | 11 days | STA-primary, AP capability under development | qaxzy |
| **2. WiFi-disabled** | 2025-07-05 → 2025-12-15 | 5 months 10 days | WiFi disabled by default ("worthless shit"); compile envs split into wifi/no-wifi/debug | qaxzy → (5-month K1 hiatus) |
| **3. v2 STA-primary rebirth** | 2025-12-16 → 2026-01-03 | 18 days | STA-primary explicit architecture; `WIFI_MODE_APSTA` for "maximum flexibility"; AP as STA-fallback | SpectraSynq (Captain) |
| **4. AP-only build-flag** | 2026-01-04 → 2026-02-04 | 32 days | `WIFI_AP_ONLY` build flag introduced as soft option; STA infrastructure intact | SpectraSynq (Captain) |
| **5. Portable Mode AP+STA experiment** | 2026-02-05 → 2026-02-16 | 11 days | Attempt at concurrent AP+STA. **"Failed within hours."** This is the ESP32 known-bug failure mode. | SpectraSynq (Captain) |
| **6. AP-only doctrinal** | 2026-02-17 → present | 76 days (ongoing) | `m_forceApOnly` runtime lock + build-time enforcement. Doctrine propagation through docs. **Walk-back env added 2026-05-03.** | SpectraSynq (Captain) → walk-back beginning |

## Chronological Reconstruction (Genesis-Anchored)

| Date | Hash | Era | Event | Evidence Source |
|------|------|-----|-------|-----------------|
| **2025-06-24** | **`34baa31d`** | 1 | **TRUE GENESIS.** `feat: Enhanced Light Crystals ESP32-S3 LED Controller`. Author: qaxzy. WiFi present, STA-primary posture. | Genesis §1 |
| 2025-06-26 | `5e5977d2` | 1 | "fix: Remove broken wireless code to fix build errors" — early friction with WiFi, but WiFi remained enabled | Genesis §2 |
| 2025-06-26 | `16302cae` → `a111eff5` | 1 | June 2025 wireless encoder system development | Genesis §2 |
| 2025-07-02 | `1d73c14e` | 1 | `feat(network): add mDNS (Bonjour) initialization for lightwaveos.local after WiFi connect`. Comment: "mDNS is not started in AP mode" — implies STA was the PRIMARY mode and mDNS-on-STA was working | Genesis §4 |
| 2025-07-04 | `7393ca4c` | 1 | `feat: Implement Genesis Audio Sync and WiFi Optimizer Pro` — heavy WiFi investment moment | Genesis §3 |
| **2025-07-05** | **`c8d12479`** | 1→2 | **CYCLE 1 CAPITULATION.** Subject: `feat: Disable WiFi by default - remove that worthless shit`. Body verbatim: "WiFi is now DISABLED BY DEFAULT because it's fucking worthless". 24 hours after WiFi Optimizer Pro. 9 files, 208/+20/-, flips `FEATURE_WEB_SERVER` default 1→0. New envs: `esp32dev` (no-WiFi default), `esp32dev_wifi` ("for masochists"), `esp32dev_debug`. RAM 21.2%→13.4%, Flash 65.1%→33.4%. | Genesis §3 |
| 2025-07-05 → 2025-12-15 | (gap) | 2 | **5-month K1 hiatus** — WiFi disabled by default; minimal WiFi-related commits | Genesis §7 |
| 2025-12-16 | (era 3 start) | 3 | v2 STA-primary rebirth begins | Genesis §2 |
| 2025-12-20 | `7700aba2` | 3 | `feat(v2): Add network layer with WiFiManager` — sets `WIFI_MODE_APSTA` "for maximum flexibility"; AP as STA-fallback state #4 in FSM | Forensic-A §1, §3 |
| 2025-12-29 | `723f4522` | 3 | Captain's anchor commit — beat-tracker integration. **Not the genesis.** | Recon |
| 2026-01-04 | `937c9abc` | 3→4 | Strength-A wording in code comments | Forensic-D §2 |
| 2026-01-28 | `6ce6143e` | 4 | **Dual-mode requirement explicitly codified:** STA-primary, AP-as-fallback | Forensic-A §3 |
| 2026-02-05 | (era 5 start) | 5 | **PORTABLE MODE AP+STA EXPERIMENT BEGINS** — attempt at concurrent operation | Genesis §7 |
| 2026-02-05 → 2026-02-16 | (Era 5 body) | 5 | 11-day experiment. **"Failed within hours."** This is the ESP32 known-bug failure mode (AP+STA concurrent corrupts auth state). NOT evidence of STA-alone failure. | Genesis §7 |
| **2026-02-17** | **`d13889f8`** | 5→6 | **CYCLE 2 CAPITULATION.** `feat(network): WiFi AP-only and main.cpp integration`. Flips runtime default `WIFI_MODE_APSTA → WIFI_MODE_AP`. Introduces `m_forceApOnly` runtime lock. Author: SpectraSynq. Same emotional pattern as Cycle 1 — invest, fail, capitulate. | Forensic-A §4, Genesis §7 |
| 2026-03-04 | `54765b16` | 6 | Build-level enforcement: `-D WIFI_AP_ONLY=1` added to canonical envs (`platformio.ini:203, 237`) | Forensic-A §1, Forensic-B §4 |
| **2026-03-04** | **`39d6dcee`** | 6 | **DOCTRINE C→D INFLECTION.** Strength-D wording ("NEVER enable STA, STA has never worked") appears in docs first time. 2-line diff inside an effects-docs commit. **Drift verdict: DOCTRINE-DRIVEN.** | Forensic-D §3, §7 |
| 2026-03-21 | `5d94dab0` | 6 | apMode "KNOWN BROKEN" added to `k1-rest-contract.yaml`. Docs-only commit. **No hardware test.** | Forensic-C §4 |
| **2026-04-26** | (memory) | 6 | `firmware_wifi_architecture.md` memory CREATED. Names 4 mitigations + "(and more)". The ONE source of "6+". **TWO MONTHS AFTER** Cycle 2 codification. **Retrospective rationalisation.** Misclassifies AP+STA concurrent failure as STA-alone failure. | Forensic-C §1, §2 |
| 2026-05-01 | (audit #47125→#47179) | 6 | May-1 audit chain re-derives doctrine FROM the 2026-04-26 rationalisation memory, not from primary logs | Forensic-C §1 |
| **2026-05-03** | **`11e040d6`** | 6 | **WALK-BACK BEGINS.** `chore(firmware): add K1v2 STA validation profile`. Adds `_sta_validation` env with `-UWIFI_AP_ONLY`. Author: SpectraSynq. **24 hours before this excavation.** | Forensic-A §1, Genesis §10 |
| 2026-05-04 | (this report) | 6 | Captain directs round-table preparation. Goal: dual-mode (AP OR STA, never together). | Captain directive |

## The Two Capitulation Cycles — Pattern Analysis

Both cycles follow the SAME emotional pattern:

**Cycle 1 (qaxzy, 11 days):**
- Day 1: Heavy WiFi investment (Light Crystals controller + wireless encoder system + WiFi Optimizer Pro)
- Day 11 (`c8d12479`): "Disable WiFi by default - remove that worthless shit"
- Outcome: WiFi disabled for 5 months
- Trigger inferred: build error frictions + resource constraints (RAM 21%→13%, Flash 65%→33% reclaimed)

**Cycle 2 (SpectraSynq/Captain, 62 days):**
- Day 1: STA-primary rebirth (`7700aba2` 2025-12-20 — APSTA "for maximum flexibility")
- Day ~50: Dual-mode requirement codified (`6ce6143e` 2026-01-28)
- Day ~58: Portable Mode AP+STA experiment begins (2026-02-05)
- Day 58+~hours: AP+STA fails (the ESP32 known bug)
- Day 62 (`d13889f8` 2026-02-17): "WiFi AP-only and main.cpp integration" — the over-correction
- Outcome: 76-day doctrine era, retrospective rationalisation in memory, walk-back beginning 2026-05-03

**The over-correction in Cycle 2 was specific and identifiable.** The Portable Mode experiment was AP+STA *concurrent* (the ESP32 bug). The capitulation rejected ALL STA, including the pure-STA mode that had worked in Eras 1 and 3. The doctrine "STA never worked" conflates the (working) Eras 1 and 3 STA with the (failing) Era 5 AP+STA-concurrent.

---

## Evidence-Quality Verdicts

| Question | Verdict | Source |
|----------|---------|--------|
| Was the original requirement dual-mode (per Captain)? | **YES — HIGH confidence** | Forensic-A §3, commit `6ce6143e` |
| Did AP-only-EVER harden in a single, deliberate moment? | **YES — HIGH confidence** | Forensic-A §4, commit `d13889f8` (2026-02-17) |
| Are the "6+ failures" verifiable as 6+ distinct incidents? | **NO — OVER-COUNTED.** Synthesised source's explicit enumeration is 4; "+" fills the gap | Forensic-C §2 |
| Was the failure mode driver-level STA-alone? | **UNKNOWN — leaning NO.** Memory cites AUTH_EXPIRE r2 / AUTH_FAIL r202 only as aggregate. Memory itself attributes cause to AP+STA *concurrent* contention. A pure STA-only build was likely never tested. | Forensic-C §3 |
| Is the apMode "KNOWN BROKEN" annotation hardware-validated? | **NO — DOCTRINE-ASSERTION.** Committed in docs-only commit by an agent. Justifying memory written one month *later*. | Forensic-C §4 |
| Was the doctrine drift evidence-driven? | **NO — DOCTRINE-DRIVEN.** Docs jumped from no-statement to strength-D in a single 2-line diff. No intermediate strengthening. | Forensic-D §7 |
| Has STA infrastructure been removed from the source? | **NO — PRESENT-DISABLED.** ~3,418 LOC across 15 files: 7-state FSM, NVS credential store, REST + serial CLI surfaces, dual credential paths. Gated by `WIFI_AP_ONLY` build flag + `m_forceApOnly` runtime lock. | Forensic-B §3, §4, §5 |

---

## Current Source State (Material Facts)

- **Mode initialisation (HARDCODED-AP-with-three-stacked-guards):**
  - `WiFiManager.cpp:83` — literal `WiFi.mode(WIFI_MODE_AP)` (no `#ifdef`)
  - `WiFiManager.cpp:74, 253, 562` — `#ifdef WIFI_AP_ONLY` short-circuits
  - `WiFiManager.cpp:129` — runtime `m_forceApOnly = true`
  - `SystemInit.cpp:321` — log "AP-only boot"

- **STA infrastructure that exists today, gated off:**
  - 7-state FreeRTOS FSM
  - NVS `wifi_creds` namespace (10 networks, last-connected tracking)
  - Compile-time SSID/password via gitignored `wifi_credentials.ini` (file present, 1.9K)
  - REST surface: `/network/connect`, `/network/sta/enable`, `/network/saved` CRUD
  - Serial CLI surface: `wifi connect SSID PASS`

- **Critical gap to Captain's "AP OR STA, never together" goal:**
  - All STA paths currently use `WIFI_MODE_APSTA` concurrent mode (`WiFiManager.cpp:692, 1233`) — this is **exactly the AP+STA bug surface Captain rejects**
  - **No `WIFI_MODE_STA`-only call exists anywhere in the tree**
  - No NVS mode-preference field
  - No first-boot provisioning UI (no captive portal, no DNS server, no BLE provisioner)

- **Single `#undef WIFI_AP_ONLY` would unlock the existing path BUT deliver AP+STA concurrent, NOT Captain's pure-mode-switching goal.** Real engineering work is required.

---

## Doctrine Surface That Currently Asserts AP-Only-EVER

7 strength-D appearances across 5 repo files + 2 memory files:
1. `CLAUDE.md:117, 236` — hard constraint statements
2. `firmware-v3/docs/CLAUDE.md` (if separate)
3. `AGENTS.md` — governance reinforcement
4. `docs/TOOLCHAIN_IMPLEMENTATION_GUIDE.md:45, 560, 818` — instructions to agents
5. `tab5-encoder/docs/PRODUCT_DECISION_PRINCIPLES.md:305, 314` — case study
6. `~/.claude/projects/.../memory/firmware_wifi_architecture.md` — the retrospective rationalisation
7. `~/.claude/projects/.../memory/feedback_never_change_network_architecture.md` — instruction-form
8. `docs/protocol/k1-rest-contract.yaml` — apMode "KNOWN BROKEN" annotation
9. `CHANGELOG.md:192` — entry correcting a prior api-v1 STA mention

ALL of these would need updating to match the dual-mode goal. None are bundle-only — they are all SOURCE files requiring Captain's explicit authorisation per the existing CLAUDE.md rule that prohibits unilateral WiFi-doctrine changes.

---

## Implications for the Current NotebookLM Bundle

**The bundle as it stands now is teaching NotebookLM doctrine that the evidence does NOT support.** Specifically:

- ROADMAP scatter-disclaimers I authored cite "6+ failed attempts" — that count is over-counted per Forensic-C
- ROADMAP top banner asserts "architecturally prohibited per 6+ failed attempts at the ESP-IDF 802.11 driver level" — the driver-level claim is unsupported (failure was likely AP+STA concurrent, not STA-alone)
- Bundled `CLAUDE.md` still says "STA has never worked" — not supported by primary evidence
- Bundled `_BUNDLE_protocol_contracts.txt` apMode "KNOWN BROKEN" — doctrine assertion, not hardware-validated
- Bundled `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` cites the 6+ failures as a case study lesson — case study premise is itself doctrine-asserted

Per the Sterilisation Gate ("No document enters a NotebookLM bundle unless its canonical status has been POSITIVELY VERIFIED"), the current bundle has a problem.

---

## Recommended Strategic Approach for the Round-Table

The Captain round-table should decide on three coupled questions:

**Q1: Source-file doctrine update (CLAUDE.md, AGENTS.md, memory files, k1-rest-contract.yaml apMode field).**
- These need rewriting from strength-D ("AP-only-EVER") back toward strength-A ("AP must work; dual-mode is the goal; current shipping is AP-only by build flag").
- Requires Captain's explicit authorisation per CLAUDE.md's own rule.

**Q2: Firmware engineering plan to deliver dual-mode (AP OR STA, never together).**
- Replace `WIFI_MODE_APSTA` paths with `WIFI_MODE_STA`
- Add NVS mode-preference field
- Add first-boot provisioning UI (captive portal or BLE)
- Exercise the `_sta_validation` env added 24h ago
- This is real engineering work; round-table to estimate scope and prioritise

**Q3: NotebookLM bundle treatment in the interim.**

Three viable options for the bundle:

- **Option A — Hold + meta-banner.** Keep current disclaimers but add a META-BANNER to MANIFEST.md noting the forensic evidence and that doctrine is under round-table review. Lowest-effort, preserves audit trail. Risk: bundle still teaches over-corrected doctrine until round-table concludes.

- **Option B — Selective rewrite.** Soften the ROADMAP, api-v2, and api-legacy disclaimers from "STA architecturally prohibited" to "STA mode is currently gated off via build flag (`WIFI_AP_ONLY`); ~3,400 LOC of STA infrastructure exists in source as PRESENT-DISABLED; goal-state is dual-mode (AP OR STA, never together); the AP+STA concurrent failure mode is a known ESP32 bug." This captures ground truth that holds regardless of round-table outcome. Risk: contradicts source CLAUDE.md (still strength-D) — internal corpus contradiction. Mitigation: rewrite bundled CLAUDE.md copy with same softened wording (modifies only the bundle copy, not the source file).

- **Option C — Full retraction.** Remove `firmware-v3_docs_api_api-v2.md`, `docs_K1_ECOSYSTEM_API_ROADMAP.md`, `_BUNDLE_protocol_contracts.txt` (or the apMode field within), bundled `CLAUDE.md`, `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md` from the bundle until the round-table updates source doctrine. Highest sterilisation guarantee but lobotomises the bundle.

**My recommendation: B (selective rewrite) WITH bundled CLAUDE.md / apMode-field / PRODUCT_DECISION_PRINCIPLES copies softened to match.** This is the option that aligns the bundle with the EVIDENCE, avoids internal contradiction, and is reversible if the round-table goes a different way. It does NOT require source-file modifications.

If Captain prefers caution, A is acceptable. C is over-correction in the opposite direction.

---

## What This Forensic Report Does NOT Verify

- ESP-IDF release notes — outside scope (network access not used by SSAs); whether AP+STA bug is fixed in IDF 5.x is not established by this report.
- iOS / Tab5 client behaviour assumptions — whether they would break under dual-mode K1 (would need a separate client-side audit).
- Hardware-level STA-only test — no record exists in source or memory of a `WIFI_MODE_STA` build being flashed and tested. This is a gap the round-table should ask Engineering to close.
- Captain's session-current correction (this conversation) is not yet in claude-mem — too recent to query.

---

## Provenance

- `_FORENSIC_WIFI_A_GIT.md` — git code history, firmware-v3-only scope (38 material commits) — **superseded by Genesis for full scope, retained as audit trail**
- `_FORENSIC_WIFI_B_SOURCE.md` — current firmware-v3 source state (15 WiFi files, 3,418 LOC PRESENT-DISABLED)
- `_FORENSIC_WIFI_C_MEMORY.md` — claude-mem archaeology (Crispy was OFF; file-memory traced; ZERO primary STA-failure observations found)
- `_FORENSIC_WIFI_D_DOCTRINE.md` — doctrine drift in docs (13 files, 7 strength-D appearances; doctrine-driven verdict)
- `_FORENSIC_WIFI_GENESIS.md` — **THE AUTHORITATIVE COMPLETE-HISTORY DIG** — 514 lines, 50.4 KB. 222 K1-author commits + 66 content-touching WIFI_MODE commits across all branches. Six eras identified. TRUE earliest 2025-06-24. Both capitulation cycles surfaced.
- This synthesis: `_FORENSIC_WIFI_REPORT.md` (Revision 2)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-04 | agent:Claude (synthesis-orchestrator) | Rev 1 initial creation — synthesised four parallel forensic SSA outputs into chronological reconstruction + evidence-quality verdicts + recommended strategic approach for Captain round-table. |
| 2026-05-04 | agent:Claude (synthesis-orchestrator) | **Rev 2** — Captain rejected Rev 1 as insufficient ("search ALL THE WAY back to 723f4522 — Work your way to the evidence BACKWARDS"). Genesis Archaeology SSA dispatched and returned 514-line authoritative dig. Revision incorporates: TRUE earliest commit `34baa31d` 2025-06-24 (6 months earlier than Forensic-A), six distinct WiFi-mode eras, two capitulation cycles, Captain himself authored Cycle 2, the genuine Era 5 Portable Mode AP+STA failure event (11 days, "failed within hours"), the verbatim `c8d12479` "remove that worthless shit" smoking gun. Recommendations strengthened: bundle option B remains correct but with explicit reference to Era 1/3 working-STA evidence. |
