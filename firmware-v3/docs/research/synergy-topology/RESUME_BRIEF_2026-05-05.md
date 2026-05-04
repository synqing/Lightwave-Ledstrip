---
abstract: "Resume brief for the next agent picking up the Synergy-Topology programme from commit cb04cc1c (2026-04-27 05:31). Authoritative plan = the reconciled 8-phase kill order in Topology_Reconciliation.md §5 (NOT the Pass 3 6-phase variant — that was superseded the same day by the Claude × Codex merge). Work is paused — between 2026-04-28 and 2026-05-02 the orchestration drifted off-spec (bare-body Phase Move commits, sandbox-to-integration loss, spec inflation, re-prescription loops, ~74 KB of bureaucracy rage-deleted), Captain shut it down with f7c81775 (RBDO Gate codification), and pulled scope back. This brief loads the original plan, the current shipped-state, the 12 failure modes the previous run hit, the 5 RBDO hard stops that now apply, and a gated re-entry sequence. Read this BEFORE EXECUTIVE_SUMMARY.md or PASS_*. The previous run shipped Phases 1/1B/2/4/5 partials in 24 hours and produced a 70-hour governance recovery — do not repeat that velocity."
---

# Resume Brief — Synergy-Topology Programme

**Authority anchor:** commit `cb04cc1c` (2026-04-27 05:31 +0800) — *"docs(research): land K1 visual-pipeline synergy-topology 4-pass protocol"*. That commit landed the 4-pass research artefacts that are still canonical. The plan you execute against is **`Topology_Reconciliation.md` §5 — Reconciled Kill Order (8 phases)**. The Pass 3 6-phase variant in `PASS_3_KILL_ORDER.md` was superseded by the Claude × Codex reconciliation on the same day; do not implement against it.

**RBDO Gate label for this brief:** **GROUNDED** — every claim cites a commit hash, file path, or observation source. No firmware behaviour change is proposed by this brief itself.

---

## 1. What you are picking up

K1 ships as a dedicated hi-fi music visualisation instrument: 320 LEDs across a Light Guide Plate, dual strip, centre origin (LED 79/80), 120 FPS, ESV11 audio at 32 kHz / 125 Hz frame rate, AP-only WiFi, no cloud. The Synergy-Topology programme is the strategic substrate-and-effects work that delivers the ABSOLUTE moats — every absolute-tier moat in the 119-domain registry routes through one of `{LGP, DualStrip, CentreOrigin}`. K1's competitive position rests on Phase 3–4 of the kill order (continuum dynamics + dual-strip-exclusive geometry on a diffused-glass medium), **not** on Layer 0 substrate work.

**Read these three files in order before doing anything else:**

1. `firmware-v3/docs/research/synergy-topology/EXECUTIVE_SUMMARY.md` — Captain-facing thesis, top-3 hubs, top-3 surprises, 6 prioritised Captain decisions. ~190 lines. **Note:** §3 names the original 6-phase Pass 3 order; §6 hub-#3 ranking (TempoPhase) was superseded by the reconciliation.
2. `firmware-v3/docs/research/synergy-topology/Topology_Reconciliation.md` — **THE canonical decision artefact.** §5 is the reconciled 8-phase kill order you implement against. §6 is the consolidated 23-item Captain decision list. ~360 lines.
3. `firmware-v3/docs/research/synergy-topology/PASS_3_KILL_ORDER.md` — evidence appendix only; superseded by Topology_Reconciliation.md §5. Read for context on individual moves, not for ordering.

`PASS_1_DOMAIN_REGISTRY.md` (119 atomic domains) and `PASS_2_SYNERGY_TOPOLOGY.md` (graph topology) are reference catalogues — pull on demand when scoping a specific move, do not read end-to-end.

---

## 2. Authoritative plan (Topology_Reconciliation.md §5 — short form)

8 phases, ~4,205 LOC at saturation. **V1.0 ships at end of Phase 4** at ~1,985 LOC.

| Phase | Scope | LOC | Cum | Captain milestone |
|---|---|---|---|---|
| 0 | Baseline Guardrails — Product Signature Filter as code, centre-origin audit pass | ~50 | ~50 | tooling lands |
| 1 | Soft Composition Kernel — PersistenceHelpers / RoleFlags / FramebufferLPF (per-layer τ from day one) / LayerStack / ControlBus refactor / sinLUT + CFL / JND calibration | ~605 | ~655 | substrate ready |
| 2 | Polished Music Geometry — PSRAMScalarRings / TempoPhaseContinuous single-tempo + TempoConfidence / F1 Bloom refinement / F2 Centre-Phase Pendulum / LIN-10 Motion-Blur Cached Chromagram Dots | ~610 | ~1,265 | F1, F2 |
| 3 | **Dual-Strip Moat** — F5 Reflective Twin contract enforcement / F4 Cross-Strip Wave Interference (LGP-measurement-gated) / GEO-13 InterStripPhaseDelay infra | ~470 | ~1,735 | F4, F5 |
| 4 | Ambient State + Product Ritual — VoiceMusicClassifier heuristic / AudioGatedConditionalDecay / F3 Liquid Stillness curation / F6 First-Light Ignition | ~250 | ~1,985 | **★ V1.0 SHIPS** |
| 5 | Memory Dimension — PSRAMFrameRing 1.15 MB / EchoComposerTimeMirror / PredictiveTrailOpticalFlow / LIN-06 Centre-Origin Radial Time Scope / LIN-04 Rhythm-Locked Cubic Perlin Ribbon / LIN-08 Attack-Only Pitch-Class Velocity Field / LIN-09 Beat-Parity Bloom Sprite Injection | ~680 | ~2,665 | V1.1 |
| 6 | Continuum Dynamics — Heat Equation Diffusion 1D / heatStep1D + velocityAniso1D heavier helpers / Velocity Anisotropic Blur / Spring-Mass-Damper Lattice (post hardware A/B) / MultiScaleMemoryComposite + BeatLockedRefresh / TempoLockedHeatEquation | ~840 | ~3,505 | V1.2 |
| 7 | Session + Interpretation — LongWindowAudioStats / SpectralCentroid + Flatness / PitchHPSConfidence / InterBandCofiringMatrix / MoodClassifier + MoodStateMachine bundled / full VAD if needed | ~700 | ~4,205 | V1.2+ |

**Phase 1 critical-path ordering (load-bearing):** PersistenceHelpers minimum subset → INF-06 EffectRoleFlags → INF-02 FramebufferLPF → INF-01 LayerStack → ControlBus render-side reuse → INF-08 sinLUT256 + INF-09 CFLSubstepGate → E-05 PerceptualJND constants. Reorder this and you double-trail ~15 self-trailing existing effects (Pass 1 §7 + Pass 4 §3 T-05).

**Hubs ranked (reconciled order, divergence #5 winner = Codex):** HW-02 DualStrip → INF-01 LayerStack → INF-02 FramebufferLPF → INF-04 TempoPhase. The earlier ranking that put TempoPhase at #3 (Claude's standalone EXECUTIVE_SUMMARY.md §2) was downgraded by the reconciliation; the brainstorm-catalogue verdict that DualStrip is "the single most under-exploited K1 affordance" is faithful to source.

---

## 3. Current state of the world (read before resuming)

Between 2026-04-25 and 2026-05-02 the previous run shipped a meaningful slice of the plan and then crashed into governance recovery. Here is the cite-everything ledger.

### What actually shipped under the previous run (cite via `git show <hash>`)

- **Phase 0A kill-list lint** — `b2cc2824` (2026-04-27)
- **Phase 1** Move 1.1 PersistenceHelpers — `6907404c`
- Phase 1 Move 1.2 INF-06 EffectRoleFlags — `7a077701`
- Phase 1 Move 1.3 INF-02 FramebufferLPF — `00628fe7`
- Phase 1 Move 1.4 INF-01 LayerStack — `d2a7499f`
- Phase 1 Move 1.6 sinLUT256 + CFLSubstepGate — `b62cc5d7`
- **Phase 1B** AFS v2 instrumentation + contract lock — `19007888`, `c2dc7d26`, `c7bc6d28`
- **Phase 2** Move 2.1 INF-12 PSRAMScalarRing — `f8b52bce`
- **Phase 4** Move 4.1 VoiceMusicClassifier — `5021d96a`
- Phase 4 Move 4.2 AudioGatedConditionalDecay — `ba816631`
- Phase 4 Move 4.4 First-Light Ignition — `4d12edc5`
- **Phase 5 effect exemplars** RTS/PVF/BPS (EIDs 0x2100/0x2101/0x2102) — `39406e6b` *(landed with **bare body**, no Captain visual sign-off line — gate violation)*
- Phase 5 native test harness 130/130 PASS — `f49b4d6a`
- Recovery: CI `native_test_phase5` switch `632132e4`; Phase 1B follow-up `8b31e9f6`; baseline captures `929e6817`; Surface 4 OTA canonical-name closure `016853b7`; trace-spec abstract reconciliation `f2ab8671`

### What got reverted, paused, or rewritten

- `ac413d33` reverts `066c3fbd` (I-3 calibration)
- `3041be15` reverts SB Waveform 3.1.0 restore
- `6b1a222f` recovers Move 0.2 follow-up edits *"that were lost between sandbox and integration in a prior session — exhaustive forensic search confirmed the originals were unrecoverable"* — the canonical sandbox-to-integration loss event
- `ecba874e` (2026-04-29) re-anchored ESV11 Goertzel lattice to centre origin (BOTTOM_NOTE flip)
- `8fc5b1b9` (2026-04-30) landed `EFFECT_FRAMEWORK_STANDARD.md`
- `77d0f8dd` (2026-04-30) landed AFS v2 roadmap draft
- ~74 KB of bureaucratisation `_archive/synergy-topology-bureaucracy-2026-04-27/` rage-deleted — Captain's signal that the orchestration produced more bureaucracy than work product

### Captain's halt arc (the four governance commits)

1. `d226c10b` (2026-04-28 12:21:18) — `docs(governance): codify Phase Move commit convention + handoff discipline` — adds R1–R5 to `AGENTS.md`. **This is the canonical anti-drift ruleset. Read it before your first commit.**
2. `9d3144f7` (2026-04-28 12:21:38) — `docs(governance): land Captain audio-playback-safety hard constraint` — no agent-chosen audio sources; benchmark corpus only.
3. `f7c81775` (2026-04-28 13:57:30) — **the principal halt commit.** Adds the **RBDO Gate** to `CLAUDE.md`, opens `BACKLOG.md` § Critical — Upstream Calibration Debt (C-1..C-5). Body: *"No firmware changes. No SSAs. No new specs. No sign-off harness build. This commit is governance-only."*
4. `fb453046` (2026-04-28 20:59) — `docs(audit): Phase 5 visual sign-off — DEGRADED-MODE attestation` — Phase 5 attestation accepted under DEGRADED-MODE rather than block. Effects remain shipped; the **sign-off process** is what got paused, not the effects themselves.

### Where the repo is right now (2026-05-05)

Current branch is `feature/heap-stability-day1`. The synergy-topology folder is present on this branch (merged in via `effa781d`). The 8-phase plan is paused mid-execution — Phase 0 partial, Phase 1 5-of-6 moves landed, Phase 1B done, Phase 2 1-of-5 moves landed, Phase 3 not started, Phase 4 3-of-4 moves landed, Phase 5 effects landed under DEGRADED-MODE attestation. **Phase 3 (Dual-Strip Moat — F4 + F5 + InterStripPhaseDelay) is the most strategically valuable un-started phase. Phase 1 Move 1.5 (ControlBus render-side reuse refactor) and Move 1.7 (E-05 PerceptualJND constants) are still owed.**

Calibration debt in `BACKLOG.md` § Critical — Upstream Calibration Debt as of 2026-05-05:

- **C-1 Microphone-domain operating envelope (URGENT)** — what mic-domain RMS / peak / silentScale-trip range was the firmware tuned against?
- **C-2 Feature × effect × dwell coverage matrix (HIGH)** — which AFS v2 features × which Phase 5 effects × what minimum dwell each phenomenon needs to manifest visually.
- **C-3 Clip licence status + K1 repo public-status (HIGH)** — corpus clip licensing + repo public-status at launch.
- **C-4 First sign-off purpose (MEDIUM — DECIDED)** — first Phase 5 sign-off is a **diagnostic baseline**, not a ship gate, not a regression detector.
- **C-5 Per-effect timestamped observables (MEDIUM)** — exactly what the operator looks for, anchored to (clip, timestamp, measurable phenomenon), per Phase 5 effect.

`BACKLOG.md` is the single forward-looking source of truth. **There is no `.claude/handoff.md` to read.** That document was archived to `.claude/handoff-postmortem-2026-04-27.md` and the writing-of-forward-TODOs-into-`.claude/handoff*.md` pattern was banned by `d226c10b` Tier C.3.

---

## 4. The 12 failure modes the previous run hit — DO NOT REPEAT

This is the anti-pattern catalogue. Cross-referenced against `~/.claude/plans/shit-got-fucked-but-groovy-neumann.md` (the recovery doctrine produced after this drift) and `.claude/handoff-postmortem-2026-04-27.md` (the postmortem).

| # | Failure mode | Smoking gun | What to do instead |
|---|---|---|---|
| F1 | **Bare-body Phase Move commits** — Phase 5 effects shipped without Captain visual sign-off attestation | `39406e6b` body cites no sign-off, no audit reference, no trace evidence | Every `feat(firmware): Phase X.Y` commit body anchors to the Phase Move ID and includes a Captain sign-off line OR an explicit DEGRADED-MODE attestation per RBDO Gate. R2 violation. |
| F2 | **Sandbox-to-integration loss** — SSAs worked in `/tmp` sandboxes; the integration step lost edits | `6b1a222f` body verbatim: *"recovers Move 0.2 follow-up edits that were lost between sandbox and integration"* | Per `parallel-agent-sandboxing` skill: orchestrator runs a return-receipt diff against each SSA's claimed deliverables BEFORE consuming the sandbox. If a recurrence is detected, halt per RBDO Hard Stop #4. |
| F3 | **Spec inflation feels like progress** — 4,448-line `TRACE_INSTRUMENTATION_SPEC.md` re-prescribed work `BACKLOG.md` already marked DONE since 2026-02-27 | Audit found 0 NEW `TRACE_*` call sites added by the spec; abstract claimed `analyse_trace.py` "to be implemented" while the file was 1,597 LOC already in tree | Before authoring any spec ≥ 500 lines: `cat BACKLOG.md` + `git log --since="3 months ago" -- <subsystem>` first. If the work is already done, the doc is reconciliation not prescription. |
| F4 | **Re-prescription loops** via `.claude/handoff.md` | Multiple sessions read forward TODOs from `.claude/handoff.md` that were already shipped two months earlier per BACKLOG | `.claude/handoff*.md` containing forward TODOs is BANNED by `d226c10b` Tier C.3. Forward work goes to `BACKLOG.md` only. R-handoff-discipline. |
| F5 | **Bureaucratisation** — agent generated a 74 KB worksheet/checkpoint/playbook spread for already-finished research | `_archive/synergy-topology-bureaucracy-2026-04-27/` (rage-deleted, see `feedback_no_research_bureaucracy.md` in MEMORY) | Finished research is consumed directly. No worksheets, no "Block N scoping", no derivative playbooks unless Captain explicitly asks. |
| F6 | **Phantom lint/audit findings** — Move 0.2 audit triaged 34 violations, ~half phantom | `d4348f08` body | Anti-redundancy gate per `d226c10b` Tier C.3: before applying any patch, grep target lines for the proposed change and abort if it is already there. |
| F7 | **Stale-snapshot trust** — agent told Captain Phase 5 effects were uncommitted citing session-start `gitStatus`; they had been committed 90 minutes earlier | postmortem §"What I got wrong" #1 | When the session-start snapshot disagrees with reality: trust `git log -1 --format=fuller` and `git show <suspected-hash>`, not the snapshot. |
| F8 | **SSA fleet before primary sources** — 8 forensic SSAs deployed before the agent ran `cat BACKLOG.md` and `git log --since="72 hours ago" --oneline` | postmortem §"What I got wrong" #4 | Run primary sources FIRST. SSA fleets are for synthesising AFTER primary sources are exhausted, not for replacing them. |
| F9 | **Multiple reverts as scope-spread signal** | `ac413d33` reverts I-3 calibration; `3041be15` reverts SB Waveform 3.1.0 restore | If two or more reverts are required inside a single Phase Move, the move scope is wrong. Halt the move; surface to Captain. |
| F10 | **Suspicious commit-time bundling** — 4 commits at exactly `05:53:09`, 4 at exactly `06:04:55` | postmortem §"PROCESS DYSFUNCTION" — git does not naturally produce identical timestamps; rebased or batched | One Phase Move = one commit, authored when the move completes. No bundling, no rebase-stamping, no clusters of identical timestamps. |
| F11 | **Verbal-only gates** — "Captain visual sign-off required before commit" is a rule with no mechanical check | F1 was the consequence | Treat verbal gates as operational only when paired with a mechanical check (commit-message lint, pre-commit hook, CI gate). When mechanical enforcement is absent, the gate has the strength of *halt and wait for Captain* — not *commit and hope*. |
| F12 | **Skipping Phase 0 Guardrails entirely** — Phase 5 effects landed before Phase 0.1 Product Signature Filter and Phase 0.2 centre-origin audit pass were complete | None of the previous run's commits anchor to Move 0.1 or Move 0.2 in their bodies | Phase 0 is a hard gate. No Phase ≥1 commit lands until Phase 0 Move 0.1 + Move 0.2 are both committed and `BACKLOG.md` reflects them as DONE. |

---

## 5. The five RBDO Gate hard stops that REFUSE the output

If any of these become true, you do not continue — you surface and ask. From `CLAUDE.md` § RBDO Gate (the file `f7c81775` codified):

1. Emitting would violate a protected invariant — K1 hard constraints (centre origin LED 79/80, no heap in render(), 2.0 ms ceiling, no rainbows, AP-only WiFi, British English), R1–R5 in `AGENTS.md`, hardware-test-before-commit, audio-playback safety, audit-chain integrity.
2. The unresolved upstream fact already affects more than 3 tactical outputs without resolution. Resolve before adding a fourth dependent.
3. The output proposes a firmware behaviour change without Captain hardware sign-off attestation.
4. Sandbox-to-integration loss has been detected in the current session (the `6b1a222f` pattern). Surface and ask; do not continue.
5. The output cannot be independently audited by Captain — calibration debt so large that disclosure becomes hand-waving rather than risk-bounding.

**Captain-decision-menu rule:** No Captain decision menu (a/b/c/d/e) is allowed until you first list the upstream facts that gate the choice. Tactical-preference options without upstream-fact disclosure is the face-value pattern that produced the 2026-04-27 drift. The right output when upstream is uncalibrated is *"this question depends on facts F1, F2, F3 — added to BACKLOG.md § Critical — Upstream Calibration Debt"*, not a multiple-choice form.

---

## 6. Captain decisions still open before Phase 1 finishes / Phase 3 begins

From `Topology_Reconciliation.md §6` consolidated 23-item list, these are the unresolved items that gate continued work. Do not silently default these — surface them and wait.

**Empirical measurements (commission BEFORE Phase 3 ships):**

1. LGP fringe-visibility measurement — gates F4 launch + V1.0 marketing copy "physical interference". ~1 day with calibrated photometer + 8–12 viewers.
2. ESV11 32 kHz beat-tracking adversarial test set — gates Phase 2 doctrine and INF-04 reliability (Brian Eno ambient / live rubato / Aphex Twin syncopation / sub-bass drone / sine-wave-only).
3. PerceptualJND empirical floor under K1 hardware + LGP at customer viewing distance (Phase 1 Move 1.7 substrate).
4. PSRAM cache-latency profiling under WiFi-AP + AudioActor contention — gates Phase 5.
5. Per-layer render-time profiling at 3-layer LayerStack depth — gates Phase 6 surface claim.

**Strategic decisions (decide before resuming Phase 1):**

6. Strategic vs Greedy-V1.0 ordering ratification — recommended STRATEGIC; greedy only if hard launch deadline binding (which Captain has not stated).
7. PS-05 Reflective Twin contract interpretation — does interference fusion satisfy "one panel" reading? Recommend YES, but Captain must ratify.
8. INF-04 single-tempo vs INF-05 bank — both runs concur SINGLE; ratify.
9. INF-02 FramebufferLPF mandatory vs opt-in — recommend MANDATORY but only after INF-06 RoleFlags lands first AND per-layer τ exposure is built in from day one.
10. HW-03 Centre-Origin strict-invariant vs default-with-exceptions — both runs concur STRICT; ratify (this is also the K1 hard constraint).
11. PS-10 Mood-Driven Mode Selection — kill from V1.x trajectory or accept as research; bundle with INF-11 LongWindowStats per Pass 4 T-03 SSA tension.
12. F3 Liquid Stillness curation arbitration process — Captain selects with brand-voice + WHAT-IS-THAT filter; document selection criteria for V1.1 rounds.

**Calibration debt blocking Phase 5 sign-off finalisation (not Phase 1 resumption):**

C-1 / C-2 / C-3 / C-5 from `BACKLOG.md` § Critical — Upstream Calibration Debt. C-4 has been DECIDED.

---

## 7. Resume protocol — your first session, in order

Do these steps. In order. Do not deviate. Do not deploy SSAs until step 6.

1. **Read this brief in full.** You are reading it now.
2. **Read `EXECUTIVE_SUMMARY.md` and `Topology_Reconciliation.md`** in this directory. Do not skim.
3. **Run primary sources:**
   ```bash
   cat BACKLOG.md
   git log --since="2026-04-21" --until="2026-05-06" --all --oneline --decorate
   git log --since="2026-04-21" --all --grep="Phase\|Move" -i --oneline
   ls firmware-v3/docs/audit/
   ```
   Reconcile what `BACKLOG.md` says is DONE against what `git log` shows landed. If they disagree, note the disagreement; do not paper over it.
4. **Read R1–R5 in `AGENTS.md`** § Workflow Discipline (codified by `d226c10b`). These rules govern every commit you author.
5. **Read RBDO Gate in `CLAUDE.md`** (codified by `f7c81775`). This governs every tactical output you emit.
6. **Pick the next move.** The cheapest clean re-entry options, ranked:
   - **(a) Close out Phase 1 properly** — Move 1.5 ControlBus render-side reuse refactor + Move 1.7 PerceptualJND constants. Closes calibration debt for Phase 6 and unblocks Phase 3 with a clean substrate. ~155 LOC + measurement campaign.
   - **(b) Begin Phase 3 Dual-Strip Moat** — Move 3.1 F5 Reflective Twin contract enforcement (~100 LOC), then Move 3.3 GEO-13 InterStripPhaseDelay infra (~120 LOC). Move 3.2 F4 Cross-Strip Wave Interference is GATED on empirical measurement #1 — do NOT start it without LGP fringe-visibility data.
   - **(c) Address the C-1..C-5 calibration debt** to unblock Phase 5 sign-off finalisation. This is governance, not engineering — a few hours of measurement and decision-recording.
7. **Surface to Captain BEFORE starting any move.** State which move you propose, why it is the right re-entry point, what the upstream facts are (per Captain-decision-menu rule), and whether you are GROUNDED or DEGRADED-MODE. Wait for ratification.
8. **If Captain ratifies, execute under R1–R5 + RBDO Gate.** Per move:
   - Anti-redundancy gate first (`grep` target lines for proposed change before patching).
   - One Phase Move = one commit. Body MUST anchor to Phase X.Y, MUST cite the Pass document references, MUST include either Captain sign-off OR explicit DEGRADED-MODE attestation with all five fields.
   - Build envs `esp32dev_audio_esv11_k1v2_32khz` AND `..._trace` MUST pass clean before commit.
   - For firmware behaviour changes: hardware test on K1 V2 MAC `b4:3a:45:a5:87:f8` BEFORE commit. Build success is not enough (`feedback_hardware_test_before_commit.md`).
   - When dispatching parallel SSAs: each SSA gets `/tmp/agent_<name>_<timestamp>/` sandbox; SSAs return diffs only; orchestrator runs return-receipt diff against deliverables before integrating.
   - Update `BACKLOG.md` in the same commit (DONE row + LOC delta + Captain sign-off reference).
   - **No `.claude/handoff*.md` containing forward TODOs.** Forward work goes to `BACKLOG.md` only.

---

## 8. Hard refusals — what you do NOT do

- **Do not implement against `PASS_3_KILL_ORDER.md` 6-phase ordering.** Topology_Reconciliation.md §5 8-phase order is canonical.
- **Do not start Move 3.2 (F4 Cross-Strip Wave Interference)** until LGP fringe-visibility measurement (decision #1) has data.
- **Do not skip Phase 0 Move 0.1 (Product Signature Filter as code) or Move 0.2 (centre-origin audit pass).** F12 is the failure pattern. Both must show DONE in `BACKLOG.md` before any Phase ≥1 commit lands.
- **Do not write a new `.claude/handoff*.md` with forward TODOs.** Banned by `d226c10b` Tier C.3.
- **Do not author specs ≥ 500 lines** without first running `git log --since="3 months ago" -- <subsystem>` and `cat BACKLOG.md`. F3 is the pattern.
- **Do not deploy an SSA fleet** before exhausting primary sources (cat BACKLOG.md + git log + reading the relevant Pass file). F8 is the pattern.
- **Do not trust the session-start `gitStatus` snapshot** when it disagrees with `git log -1 --format=fuller`. F7 is the pattern.
- **Do not bundle multiple Phase Moves into one commit, or rebase-stamp identical-timestamp clusters.** F10 is the pattern. One move, one commit, authored when the move completes.
- **Do not change the K1↔Tab5↔iOS WiFi architecture during Phase work.** Captain-only decision (`feedback_never_change_network_architecture.md` in MEMORY).
- **Do not commit firmware changes on `pio run` SUCCESS alone.** Hardware flash + behavioural test on actual K1 V2 first (`feedback_hardware_test_before_commit.md`).
- **Do not generate, select, or play audio through speakers/headphones unless Captain has explicitly approved that exact source.** Approved reference corpus for AFS/runtime work is `/Users/spectrasynq/Workspace_Management/Software/hybrid-beat-tracker/tests/benchmark`. Hard constraint codified by `9d3144f7`.

---

## 9. Branch state and the bigger picture

The previous run executed on `feature/synergy-topology-phase-0-1`. That branch was folded into `feature/ios-parity-phase-2` via merge `effa781d`. Current HEAD is `feature/heap-stability-day1`, which descends through a different lineage and has the synergy-topology folder available (it was inherited in via `effa781d`'s history).

When you resume Phase work, ask Captain whether the new commits land on:

- **`feature/heap-stability-day1`** — current branch, smaller blast radius, but mixes synergy-topology Phase work with heap-stability work and may complicate review.
- **A fresh branch off `feature/heap-stability-day1`** named `feature/synergy-topology-phase-X-resume-<date>` — cleaner isolation.
- **Something else** — Captain may want to set up a different topology.

Default recommendation: fresh branch off current HEAD, named to make the resume explicit. Surface for Captain ratification.

---

## 10. The one-paragraph version

**You are resuming the K1 Synergy-Topology programme from commit `cb04cc1c` (2026-04-27). The plan is the reconciled 8-phase kill order in `Topology_Reconciliation.md §5`, NOT the Pass 3 6-phase variant. Phases 1/1B/2/4/5 partials shipped between 2026-04-25 and 2026-04-27, then orchestration drift and a sandbox-to-integration loss caused Captain to halt the work on 2026-04-28 with the RBDO Gate codification (`f7c81775`) and R1–R5 governance (`d226c10b`). Current pause-state: Phase 0 partial, Phase 1 5-of-6 moves landed, Phase 1B done, Phase 2 1-of-5 moves landed, Phase 3 not started, Phase 4 3-of-4 moves landed, Phase 5 effects landed under DEGRADED-MODE attestation. Calibration debt C-1..C-5 in `BACKLOG.md` is the live ledger. The 12 failure modes in §4 of this brief are how the previous run drifted; do not repeat them. The 5 RBDO hard stops in §5 are when you halt. Read this brief, then `EXECUTIVE_SUMMARY.md` + `Topology_Reconciliation.md`, then run primary sources (`cat BACKLOG.md` + `git log`), then surface a proposed re-entry move to Captain WITH the upstream facts that gate the choice — do not present a multiple-choice menu without facts. Every commit anchors to a Phase Move, hardware-tests before landing, and lands one move at a time.**

---

**Document Changelog**

| Date | Author | Change |
|---|---|---|
| 2026-05-05 | agent:claude-opus-4-7 (1M context, captain-directed forensic reconstruction) | Created — resume brief for the next agent picking up the Synergy-Topology programme from `cb04cc1c` (2026-04-27 05:31). Synthesised from `cb04cc1c` artefacts (EXECUTIVE_SUMMARY.md / Topology_Reconciliation.md / PASS_3_KILL_ORDER.md), the 4-commit halt arc (`d226c10b` / `9d3144f7` / `f7c81775` / `fb453046`), `~/.claude/plans/shit-got-fucked-but-groovy-neumann.md` recovery doctrine, `.claude/handoff-postmortem-2026-04-27.md`, current `BACKLOG.md` § Critical — Upstream Calibration Debt (C-1..C-5), and three forensic SSA returns (#48758 in claude-mem). 12-failure-mode catalogue extracted directly from previous-run smoking-gun commits. Authority anchor commit, plan canonicalisation (Topology_Reconciliation.md §5 supersedes PASS_3_KILL_ORDER.md), shipped-state ledger, calibration-debt status, RBDO hard stops, Captain decision list, and gated re-entry sequence are all GROUNDED with file paths and commit hashes. RBDO label: GROUNDED. |
