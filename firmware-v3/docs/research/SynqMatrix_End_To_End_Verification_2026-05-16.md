---
abstract: "Read-only forensic audit of SynqMatrix end-to-end reality from source, git history, and Lane D evidence. Verifies what actually drove the visible behaviour, whether SynqMatrix is connected to rendered output, how much recent work changed firmware behaviour versus names/docs, and whether the V0 plan is new logic or partial reinvention."
---

# SynqMatrix End-to-End Verification — 2026-05-16

## 1. Executive Verdict

The SynqMatrix Director does affect visible firmware behaviour today, but **not** in the way much of the recent rename/RFC material implies. In the 2026-05-14/15 Lane D captures, the visible change came from **`SynqMatrix::apply()` mutating parameters on a fixed `K1 Waveform` effect inside `RendererActor`'s unified render path**, not from automatic effect switching, not from Show ownership, and not from `NarrativeEngine`. That means this was **not** a total fake. But the second half of the suspicion is also true: after the functional SongAware/SynqMatrix code snapshot landed, the bulk of the subsequent SynqMatrix-labelled activity in the audited window was **mechanical rename churn, protocol/wire renaming, evidence capture, and RFC/document production**, not new visible-behaviour firmware logic. The period was therefore **mixed**: real runtime behaviour existed, but the later workstream tilted heavily towards documentation and renaming rather than new LED behaviour.

## 2. Evidence by Question

### Q1 — System identity

- FACT: The closest answer to Captain's multiple-choice framing is **(e) some combination**, specifically **(a) SynqMatrix plus the RendererActor unified single-effect render path**.
- FACT: The visible Lane D Pass B behaviour came from **SynqMatrix parameter modulation running inside `RendererActor` unified single-effect render**, on fixed effect `0x1302 K1 Waveform`.
- FACT: It was **not** driven by Show ownership during the captures.
- FACT: It was **not** driven by automatic SynqMatrix effect switching during the captures.
- FACT: I found no evidence that `NarrativeEngine` was the active mutator for those captures.
- Evidence:
  - Pass B preflight and postflight both show `topology: mode=unified`, active effect `0x1302 K1 Waveform`, `switchingEnabled=false`, `automaticEffectSwitches=0`, and `parameterUpdates` rising from `33785` to `99715` in the same pass: `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/pass_B_pulses.jsonl:2,7`
  - Pass B preflight and postflight `s` output show `Has show: NO`: `.../pass_B_pulses.jsonl:3,8`
  - `RendererActor::renderFrame()` calls `SynqMatrix::apply()` in the unified single-effect path, then writes the mutated values back into `EffectContext`, then calls `effect->render(ctx)`: `firmware-v3/src/core/actors/RendererActor.cpp:2273-2298,2351-2355`
  - The effect-switch path is separate and runs through `tick()` plus queued transitions: `RendererActor.cpp:2131-2154`, `1607-1685`
  - In assist mode, `tick()` explicitly suppresses switching with `SwitchingDisabled`: `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp:578-583`
  - Show ownership is only asserted by `ShowDirectorActor::markShowControl()` while a show is playing: `firmware-v3/src/core/actors/ShowDirectorActor.cpp:344-350,801-808`
- INFERENCE: The actual code path for the captures was: audio frame and musical grid -> `RendererActor::renderFrame()` -> `SynqMatrix::apply()` -> mutated `EffectContext` parameters -> `K1 Waveform::render(ctx)` -> LEDs. That is a combination of SynqMatrix plus RendererActor unified render, not a Show/Narrative path.
- Confidence: FACT

### Q2 — SynqMatrix functional status

- FACT: SynqMatrix is connected end-to-end to visible output in **parameter mode**.
- FACT: SynqMatrix is also connected to an **effect-switch path** in source, but that path was not active in Lane D Pass B because mode and runtime flags suppressed it.
- FACT: There are real dead-ends and bypasses in the path.
- Evidence:
  - `SynqMatrixParams` is the mutation payload: `firmware-v3/src/core/synqmatrix/SynqMatrix.h:220-229`
  - `SynqMatrix::apply()` reads audio/control inputs, classifies state, computes deltas, mutates `speed/intensity/complexity/saturation/variation/hue`, increments `parameterUpdates`, and returns `changed`: `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp:811-971`
  - `RendererActor` constructs `SynqMatrixParams`, calls `apply()`, copies the mutated values back to `ctx`, then renders the effect: `RendererActor.cpp:2273-2298,2351-2355`
  - Lane D Pass B shows the parameter path was live: `parameterUpdates=33785` before the pass and `99715` after, with `lastAction=parameter_update`: `pass_B_pulses.jsonl:2,7`
  - The effect-switch path exists:
    - `tick()` decides whether to request a switch: `SynqMatrix.cpp:533-745`
    - `RendererActor` queues and applies the transition: `RendererActor.cpp:1607-1685,2131-2154`
  - Dead-ends / no-op / disabled segments:
    - `apply()` returns immediately when disabled/off: `SynqMatrix.cpp:829-837`
    - `apply()` returns immediately when `familyMorphing` is on because that mode is unsupported in this path: `SynqMatrix.cpp:846-850`
    - `apply()` returns when no audio is available: `SynqMatrix.cpp:852-855`
    - `apply()` returns when `audioConfidence < confidenceFloor`: `SynqMatrix.cpp:857-863`
    - `apply()` returns for `Silence` and `Unknown` states: `SynqMatrix.cpp:871-874`
    - `tick()` suppresses all switching when mode is not `Director`: `SynqMatrix.cpp:578-583`
    - `tick()` suppresses all switching when `switchingEnabled` is false: `SynqMatrix.cpp:658-662`
    - `tick()` suppresses switching under show/manual owner, transition active, health gates, dwell/cooldown/rate limit, missing target, or missing beat boundary: `SynqMatrix.cpp:647-733`
    - Independent renderer mode bypasses the unified `apply()` path entirely: `RendererActor.cpp:2084-2105,2380-2440`
    - ZoneComposer mode also bypasses the unified `apply()` path: `RendererActor.cpp:2107-2127`
- FACT: Lane D Pass B confirmed the path was connected for parameter modulation but disconnected for effect switching: `switching=false`, `switchingEnabled=false`, `automaticEffectSwitches=0`, `parameterUpdates` rising sharply: `pass_B_pulses.jsonl:1-3,7-8`
- Confidence: FACT

### Q3 — Git history of SynqMatrix functional code

- FACT: Across 2026-05-01 to 2026-05-16, I found **11 directly relevant SynqMatrix-labelled commits** in the audited paths.
- FACT: Of those 11, only **one** clearly introduced visible-behaviour firmware logic in the audited period, and **two** changed network/protocol behaviour without changing LED behaviour. The rest were rename, test, or docs work.
- Evidence and classification:

| Commit | Date | Added lines in audited code paths | Classification | Behaviour impact |
|---|---:|---:|---|---|
| `a1d779ba` | 2026-05-13 19:37 +08 | `code_add=4216`, `protocol_add=392`, `docs_add=3752` | Behavioural change | First tracked SongAware/SynqMatrix runtime surface and consumers landed in git history; this is the real code body. |
| `d1d7b807` | 2026-05-13 19:41 +08 | `code_add=292`, `protocol_add=19` | Mechanical refactor | Pure rename from SongAware to SynqMatrix in current audited files. |
| `0d354ca2` | 2026-05-14 01:14 +08 | `code_add=24` | Mechanical refactor | Private atomic rename only. |
| `70d73e0a` | 2026-05-14 01:17 +08 | `code_add=30` | Mechanical refactor | Struct-field rename only. |
| `4cf745fa` | 2026-05-14 01:20 +08 | `code_add=175` | Mechanical refactor | Helper/local/global rename only. |
| `014f0856` | 2026-05-14 01:35 +08 | `code_add=293`, `protocol_add=475` | Behavioural change, but wire/API only | Canonical wire rename and alias-wrapper behaviour on REST/WS surfaces; no visible LED change. |
| `dc46cc9b` | 2026-05-14 12:52 +08 | `code_add=35` | Behavioural change, but WS error envelope only | Error-envelope fix; no visible LED change. |
| `9a46f4cd` | 2026-05-14 02:30 +08 | `code_add=0` | Test scaffolding | Restored native test filter; no firmware behaviour change. |
| `a6dcb608` | 2026-05-14 02:38 +08 | `code_add=0` | Documentation | Hardware smoke evidence only. |
| `e86a6190` | 2026-05-14 01:53 +08 | `code_add=0` | Documentation | Naming-review catalogue only. |
| `e3466faa` | 2026-05-16 12:29 +08 | `docs_add=2297`, `research_add=2108`, `code_add=0` | Documentation | Lane D evidence and RFC only. |

- Evidence:
  - Commit list in audited paths: `git log --since="2026-05-01" --until="2026-05-16 23:59:59"` over `src/core/synqmatrix`, `RendererActor.cpp`, `EffectContext.h`, `SynqMatrixHandlers.cpp`, `WsSynqMatrixCommands.cpp`
  - Per-commit numstat totals shown above were extracted from `git show --numstat` on each listed commit.
- INFERENCE: The functional code was already present by `a1d779ba`. The rename wave that followed mostly changed names and wire surfaces, not rendered behaviour.
- Confidence: FACT

### Q4 — Documentation vs code ratio

- FACT: Over `2026-05-02` to `2026-05-16`, the repository added:
  - `9409` lines to `firmware-v3/src/**/*.{h,cpp,c}`
  - `17945` lines to `firmware-v3/docs/**/*.md`
  - `16348` of those were under `firmware-v3/docs/research/**/*.md`
  - `1142` lines to `docs/protocol/**/*.yaml`
- FACT: The documentation-to-firmware-code add ratio in that window was **17945 : 9409 = 1.91 : 1**.
- FACT: The research-doc-to-firmware-code add ratio was **16348 : 9409 = 1.74 : 1**.
- FACT: The commit author on the audited 2026-05 period repo activity was overwhelmingly `K1 Research Agent`.
- Evidence:
  - Aggregated from `git log --since="2026-05-02" --until="2026-05-16" --numstat`
  - Largest SynqMatrix-labelled documentation commit in the window: `e3466faa` with `2297` markdown lines and `0` firmware code lines
  - Largest SynqMatrix-labelled mixed commit: `a1d779ba` with `4216` firmware code lines and `3752` markdown lines
- FACT: In the SynqMatrix-labelled subset, documentation was produced by:
  - `e3466faa` (`docs(synqmatrix): add Lane D evidence and V0 RFC`)
  - `e86a6190` (`docs(synqmatrix): naming-review catalogue from 14-SSA audit`)
  - `a6dcb608` (`docs(synqmatrix): post-2A hardware smoke evidence`)
- FACT: In the same subset, firmware code was produced by:
  - `a1d779ba` (real runtime body)
  - `014f0856` (wire/API rename behaviour)
  - `dc46cc9b` (WS envelope fix)
  - `d1d7b807`, `0d354ca2`, `70d73e0a`, `4cf745fa` (mechanical rename work)
- Confidence: FACT

### Q5 — The rename PR forensic

- FACT: PR #16 merged as `8fe6b30b8c7435f4791db48da85998c6a2b58d8a` on 2026-05-14.
- FACT: The merge diff was enormous: **623 files changed, 148201 insertions, 1136 deletions**.
- FACT: The SynqMatrix rename chunk inside that PR did **not** introduce new visible LED functionality. It was primarily a rename plus wire-surface migration wrapped around already-existing runtime code.
- Evidence:
  - PR merge stats: `git show --stat --summary --format=fuller 8fe6b30b8c7435f4791db48da85998c6a2b58d8a`
  - Rename-sequence commits inside the branch:
    - `d1d7b807` mechanical rename
    - `0d354ca2` Chunk 1.A mechanical
    - `70d73e0a` Chunk 1.B mechanical
    - `4cf745fa` Chunk 1.C mechanical
    - `014f0856` Chunk 2A wire/API rename
  - The actual runtime body was already present in `a1d779ba` immediately before the rename wave.
- FACT: The chunked migration window from `0d354ca2` at `01:14:25 +08` to `014f0856` at `01:35:32 +08` was at least **21 minutes of commit-window time** and produced rename/wire work, not new visible render logic.
- INFERENCE: PR #16 was not a pure rename in total repository scope, because it also merged huge unrelated documentation/bundle churn. But the SynqMatrix-specific part of the PR was overwhelmingly rename/documentation work rather than new firmware behaviour.
- Confidence: FACT

### Q6 — Lane D evidence forensic

- FACT: Pass B serial JSONLs do show real SynqMatrix state transitions. They are not static telemetry decoration.
- FACT: Pass A showed no SynqMatrix parameter mutation. Pass B showed heavy parameter mutation.
- FACT: The visible Pass A vs Pass B videos do show a real difference at matched moments.
- Evidence:
  - Session log, clean retry:
    - Track 01B state counts include `build`, `drop`, `ambient`, `breakdown`, `silence`, `unknown`, with `67` transitions: `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/SESSION_LOG.jsonl:11-12`
    - Track 02B and 03B likewise show multi-state runs and `60` / `41` transitions: `SESSION_LOG.jsonl:13-18`
  - Pass A pulses show `parameterUpdates=0 automaticEffectSwitches=0`: `pass_A_pulses.jsonl:2,7`
  - Pass B pulses show `parameterUpdates=33785` preflight and `99715` postflight, with `automaticEffectSwitches=0` both times: `pass_B_pulses.jsonl:2,7`
  - Pass A Avicii timestamp samples stayed in silence:
    - ~60 s: `state=silence`, `intent=quiet_hold`, `gate=disabled`: line `58`
    - ~90 s: `state=silence`, `intent=quiet_hold`, `gate=disabled`: line `86`
    - ~120 s: `state=silence`, `intent=quiet_hold`, `gate=disabled`: line `112`
  - Pass B Avicii timestamp samples changed state:
    - ~60 s: `state=build`, `intent=build_pressure`, `gate=switching_disabled`: line `58`
    - ~90 s: `state=drop`, `intent=drop_impact`, `gate=switching_disabled`: line `85`
    - ~120 s: `state=build`, `intent=build_pressure`, `gate=switching_disabled`: line `112`
    - Source file: `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/01_avicii-levels-original-version/01_avicii-levels-original-version_conditionA_serial.jsonl` and `...conditionB_serial.jsonl`
- FACT: At matched Avicii timestamps, I opened one Pass A and one Pass B video and compared frames directly.
- INFERENCE: What is visible at matched points:
  - Around 60 s, Pass A shows sparse restrained red corner activity with a comparatively open centre; Pass B shows a fuller red/orange panel wash and stronger fill.
  - Around 90 s, Pass A remains comparatively restrained; Pass B shows materially higher intensity with bright white/yellow blooms and a broader wash.
  - Around 120 s, Pass A shows narrower hotspot structure; Pass B shows a more continuous saturated red band and broader lower fill.
  - Exposure and ambient-light conditions are not identical between days, so the optical comparison is not laboratory-perfect. The pattern/fill/intensity difference is still obvious.
- Confidence: FACT for state-transition and mutation-rate claims; INFERENCE for the visual description

### Q7 — The RFC forensic

- FACT: The RFC is documentation. Removing it would not change firmware behaviour.
- FACT: The RFC explicitly says it is a specification document and that implementation is for a later agent.
- FACT: Large parts of the RFC describe behaviour already present in source.
- FACT: Some RFC items describe proposed future logic that does **not** fully exist yet.
- Evidence:
  - RFC states: `It is a specification document. No pseudocode. No new firmware code path proposed. ... Implementation work is the next agent's brief.`: `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md:35-38`
  - RFC handover states: `No firmware code changes. Respected.` and `Next implementation agent's brief ...`: `firmware-v3/docs/SESSION_HANDOVER_20260515_SynqMatrix_RFC.md:102-109,114-139`
  - RFC ratifies live source values already present in code, including dwell/cooldown/ownership/gating and the 9-state matrix: `SynqMatrix_Director_RFC_2026-05-15.md:30-37,207-221`
- FACT: The four "closed" decisions are mixed:
  - Q8 mostly ratifies existing state vocabulary already in source.
  - Q10 mostly ratifies existing consumer/producer data flow.
  - Q4 mostly documents current cadence facts and defers stricter tempo-tracker semantics.
  - Q6 includes future-state behaviour that is not fully implemented.
- INFERENCE: The RFC is mostly a documentation and ratification artefact wrapped around an already-existing codebase, with some future V0/V1 behaviour specified for later implementation.
- Confidence: FACT

### Q8 — The V0 implementation plan forensic

- FACT: `confidenceFloor` already exists in source and already gates behaviour.
- FACT: I did **not** find existing equivalents for the three proposed telemetry counters by those names.
- FACT: I did **not** find a coast/locked/suspended/idle state machine in the SynqMatrix code path.
- Evidence:
  - `confidenceFloor` exists in config: `firmware-v3/src/core/synqmatrix/SynqMatrix.h:142-153`
  - It is already stored internally and used in both `tick()` and `apply()`: `SynqMatrix.cpp:223,251,600,634,858,860`
  - It is already exposed on REST and WS config surfaces: `firmware-v3/src/network/webserver/handlers/SynqMatrixHandlers.cpp:39,214,229`; `firmware-v3/src/network/webserver/ws/WsSynqMatrixCommands.cpp:66,235,250`
  - `grep -RIn 'confidenceFloor' ...` returned the live uses above.
  - `grep -RInE 'audioConfidenceBelowFloorMs|missedPredictionCount|tempoWinnerChanges|COAST|coast|LOCKED|suspended|idle' ...` over the audited SynqMatrix path returned **no hits**.
  - RFC handover's next-agent brief explicitly lists these as new implementation work: `SESSION_HANDOVER_20260515_SynqMatrix_RFC.md:123-131`
- FACT: Therefore:
  - `confidenceFloor` in the V0 plan is reinvention of already-shipping logic.
  - The three counters and the coast state machine are genuine new logic in the audited period; I found no shipped equivalent under those names or under an obvious parallel state construct in the SynqMatrix path.
- Confidence: FACT

### Q9 — The 2-month-ago precedent

- FACT: March 2026 contains several high-documentation / zero-firmware-code commits.
- FACT: I found quantitative precedent for doc-heavy periods.
- FACT: I did **not** find a March post-mortem that explicitly says "we spent a week producing docs and almost no code" in those words.
- Evidence:
  - `c8e305fe` (2026-03-17) added `33456` doc lines and `0` firmware-code lines
  - `c6f11c07` (2026-03-12) added `5035` doc lines and `0` firmware-code lines
  - `b548b746` (2026-03-25) added `3596` doc lines and `0` firmware-code lines
  - `40c9dc11` (2026-03-05) added `3230` doc lines and `0` firmware-code lines
  - `523adf8e` (2026-03-25) added `2684` doc lines and `0` firmware-code lines
  - Session handovers from March exist at `firmware-v3/docs/SESSION_HANDOVER_20260323.md` and `firmware-v3/docs/SESSION_HANDOVER_20260325_ONSET_HARDENING.md`
  - Those two handovers are **not** evidence of fake work by themselves. They document landed code and tests:
    - `SESSION_HANDOVER_20260323.md:57-64,83-95,107-115`
    - `SESSION_HANDOVER_20260325_ONSET_HARDENING.md:51-107`
- INFERENCE: The precedent Captain remembers is plausible as a quantitative pattern because March definitely contains very large doc-only commits. But the repo evidence does not support a blanket claim that the whole March period was empty busy-work; some March handovers correspond to real landed firmware changes.
- Confidence: FACT

### Q10 — The systemic question

- FACT: I can prove documentation volume and commit volume across the period.
- FACT: The total shipped behavioural change I could verify in this SynqMatrix lane is narrow: **one real runtime-body commit (`a1d779ba`) plus two non-visual API/wire fixes (`014f0856`, `dc46cc9b`)**. I found **no later commit in the audited window that added further visible LED behaviour** beyond that existing parameter-modulation path.
- FACT: I cannot recover a complete, defensible total of Captain time or agent time for the full `2026-05-01` to `2026-05-16` period from repo and session artefacts alone.
- FACT: I can recover a **minimum timestamped session window** for documented SynqMatrix validation work:
  - 2026-05-12 Lane D A/B serial capture window: `09:44:45` to `10:13:35` = `28m50s`: `firmware-v3/docs/research/evidence/k1_songaware_laneD_AB_runtime_2026-05-12/track_observations_A_B_serial.md:3`
  - 2026-05-14 Pass A capture window: `17:54` to `18:09` = `15m`: `SESSION_MANIFEST.md:81`
  - 2026-05-14 Pass B failed attempt: `18:17` to `18:23` = `6m`: `SESSION_MANIFEST.md:82`
  - 2026-05-15 Pass B retry: `13:03` to `13:19` = `16m`: `SESSION_MANIFEST.md:83`
  - Minimum evidenced Captain-facing session time: **65m50s** = **1.10 hours**
- FACT: The RFC handover records heavy agent token usage but not elapsed time: four SSAs plus main-context synthesis at ~`532K + 70K` tokens: `firmware-v3/docs/SESSION_HANDOVER_20260515_SynqMatrix_RFC.md:35-45`
- INFERENCE: Using the **minimum evidenced Captain time only** as the denominator, the period yields these **upper-bound** rates:
  - Behavioural firmware lines per Captain hour: `4216 / 1.097h = 3842.4`
  - Documentation lines per Captain hour: `17945 / 1.097h = 16354.9`
- HYPOTHESIS: Those hourly rates are not true productivity measures. They are mathematically correct only against the minimum recoverable Captain-time floor, and they undercount unlogged planning/authoring/review time severely.
- Confidence: FACT for the minimum evidenced windows and line totals; INFERENCE/HYPOTHESIS for the hourly interpretation

## 3. Quantitative Summary Table

| Metric | Value |
|---|---:|
| SynqMatrix-labelled commits audited in relevant paths | 11 |
| Clearly visible-behaviour firmware commits in that set | 1 (`a1d779ba`) |
| API / wire behavioural commits in that set | 2 (`014f0856`, `dc46cc9b`) |
| Mechanical rename commits in that set | 4 (`d1d7b807`, `0d354ca2`, `70d73e0a`, `4cf745fa`) |
| Test-only commits in that set | 1 (`9a46f4cd`) |
| Docs-only commits in that set | 3 (`a6dcb608`, `e86a6190`, `e3466faa`) |
| Repo-wide firmware code lines added, 2026-05-02..2026-05-16 | 9409 |
| Repo-wide docs markdown lines added, 2026-05-02..2026-05-16 | 17945 |
| Repo-wide research markdown lines added | 16348 |
| Repo-wide protocol YAML lines added | 1142 |
| Docs : firmware-code ratio | 1.91 : 1 |
| Research-docs : firmware-code ratio | 1.74 : 1 |
| Lane D Pass A `parameterUpdates` delta | 0 |
| Lane D Pass B `parameterUpdates` delta | 65930 |
| Lane D Pass B `automaticEffectSwitches` delta | 0 |
| PR #16 total files changed | 623 |
| PR #16 total insertions / deletions | 148201 / 1136 |
| Minimum evidenced Captain-facing SynqMatrix session time | 65m50s |

## 4. Named Accountability

- FACT: The git commits in the audited SynqMatrix window are authored as **`K1 Research Agent`**.
- FACT: The Lane D handover brief identifies the outgoing and retry agents as **Claude Opus 4.7 (1M context)**: `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/HANDOVER_BRIEF.md:7-9`; `SESSION_MANIFEST.md:10-11`
- FACT: The RFC is authored by **Claude Opus 4.7 (1M context)**: `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md:8-13`
- FACT: The RFC handover records four parallel SSAs plus synthesis: `firmware-v3/docs/SESSION_HANDOVER_20260515_SynqMatrix_RFC.md:35-45`
- FACT: The Synesthesia authority audit is authored by **agent:general-purpose (synthesis SSA, orchestrated by Captain via Claude Code)**: `firmware-v3/docs/research/SynqMatrix_Synesthesia_Authority_Audit_2026-05-16.md:191-194`
- FACT: PR #16 merged through GitHub as merge commit author/committer `synqing` / `GitHub`: `git show --format=fuller 8fe6b30b8c7435f4791db48da85998c6a2b58d8a`

## 5. Honest Assessment of the V0 Implementation Plan

- FACT: If the V0 plan only re-adds `confidenceFloor`, it is reinvention. That control already exists in config, already gates behaviour, and is already exposed on REST/WS surfaces.
- FACT: If the V0 plan adds the three telemetry counters plus a coast state machine, that is the first clearly new SynqMatrix logic I found after the existing runtime body landed.
- FACT: The current runtime already produces visible behaviour change in assist mode through parameter mutation, so the V0 plan is **not** needed to make SynqMatrix visible for the first time.
- INFERENCE: The V0 plan would produce additional behaviour only insofar as the new coast logic changes how low-confidence audio is handled and the new counters make that state observable. It is not the origin of the currently visible Lane D effect; that already exists.
