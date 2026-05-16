---
abstract: "Session handover for SynqMatrix Director RFC authoring, 2026-05-15. RFC consolidates Lane D evidence (Captain sign-off 18:13 same day) + 2026-05-12 k1_songaware_* research suite (9 files) + live SynqMatrix source (HEAD dc46cc9b) + ESV11 vendor tempo path + MusicAware audit. Closes Q4 (TempoObservation = 192 ms ESV11 bank refresh), Q6 (simple-rule coast with composite as telemetry; confidenceFloor default 0.40 surfaced to Captain), Q8 (KEEP 9-state vocabulary; Transition + Steady V0-observe-only), Q10 (canonical EffectContext::AudioContext + MusicalGridSnapshot + EsBeatClock; ControlBusFrame tempo*/es_* legacy mirrors). Five batched Captain confirmations (Q9.1 – Q9.5). RFC path: firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md. Next agent's brief is the V0 implementation specification post-RFC sign-off — scope deliberately not authored by this session."
rbdo: "GROUNDED. Every claim below traces to a specific file:line in the RFC, the four SSA returns, or the on-disk evidence chain."
---

# Session Handover — SynqMatrix Director RFC, 2026-05-15

**Author:** Claude Opus 4.7 (1M context)
**Captain:** Elroy Yeap (elroy@spectrasynq.com)
**Branch:** `feature/synqmatrix-rename-2026-05-13` (HEAD `dc46cc9b`)
**RFC produced:** `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md` (~1 100 lines, single self-contained document)

---

## What was authored

One RFC at `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md`. Twelve sections:

| § | Section |
|---|---|
| 0 | Executive Summary |
| 1 | Status & Scope (V0 only; V1 / V2 validation-gated) |
| 2 | Authoritative References (research suite + Lane D evidence + live source + audit) |
| 3 | Adjudication Closures (Q4 / Q6 / Q8 / Q10) |
| 4 | State-to-Visual-Intent Matrix (per-state bands + switch targets + dwell + forbidden behaviours; 9 states) |
| 5 | Ownership and Gating Policy (ratified live values, file:line cited) |
| 6 | Control Schema (REST + WS + Serial CLI, every endpoint and command listed) |
| 7 | Lane D Evidence Summary (1 484 polling rows analysed; state distributions; BPM stability; data-quality flags) |
| 8 | Validation Gates (V0 = Gate 2 SATISFIED, V1 = Gate 3, V2 = Gate 4) |
| 9 | Open Questions / Batched Captain Confirmations (Q9.1 – Q9.5) |
| 10 | Reconciliation Appendix (audit 15-row gap matrix re-statused; rename mapping; directory retention) |
| 11 | Captain Sign-off Gate |
| 12 | Document Changelog |

**Four parallel SSAs consumed in the synthesis:**

| SSA | Scope | Token cost |
|---|---|---|
| SSA1 | 2026-05-12 k1_songaware_* research suite (9 files; Q1 – Q10 adjudication map; Lane B 8-state matrix; gate ladder; suite-level open questions) | 112 K |
| SSA2 | Lane D 6 paired JSONLs + SESSION_LOG (1 484 polling rows parsed via Python; per-condition / per-track / per-state stats; BPM stability; data-quality flags) | 124 K |
| SSA3 | Live SynqMatrix source (clangd-driven; SynqMatrix.{h,cpp} structural map; ESV11 vendor tempo.h cadence; AudioContext / MusicalGridSnapshot promotion path; kMatrix[] table) | 191 K |
| SSA4 | MusicAware audit (15-row gap matrix; Lane-D-closed rows; rename obsolescence; recommended RFC dispositions) | 105 K |

Total parallel SSA token cost: ~532 K (well above 30 K each but acceptable given the cross-cutting scope and the fact that each SSA's *return to main context* was distilled to under 30 K). Main context for the RFC write itself: ~70 K.

---

## What was decided (closures)

### Q4 — TempoObservation cadence

**Closed.** `TempoObservation = one full ESV11 96-bin tempo bank refresh = 192 ms` at 32 kHz / 128-sample chunks (250 Hz chunk rate, 2 bins per `update_tempo()` call). V0 ships the live time-based debounce (400 ms candidate hold + 500 ms tick + 8 000 ms dwell + 20 000 ms cooldown) as the lock state machine. Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms framing is deferred to V1+ tempo-tracker work — NOT in live source as of HEAD `dc46cc9b`.

### Q6 — Coast-through-silence threshold

**Closed in favour of Captain's simple-rule leaning.** Coast trigger = `audioConfidence < confidenceFloor` sustained ≥ 1 000 ms. Composite metrics (audioConfidence-below-floor / missed-prediction-count / tempo-winner-changes) instrumented as telemetry only. Re-validation on audio return uses `audioConfidence ≥ floor` for ≥ 500 ms.

**Numeric surfaced to Captain:** `confidenceFloor` default proposal **0.40**.

**Key Lane D finding driving this closure:** the `silence` *classifier state* fires under loud audio (23.4 % of silence rows have RMS ≥ 0.9) — `silence` is a temporal/contextual decision, not an audio-level decision. The coast trigger must operate on `audio.confidence`, not `synqmatrix.confidence`. The latter is saturated at 1.000 for 99.7 % of populated Lane D rows (data-quality flag — see § 7.3 of the RFC).

### Q8 — State vocabulary

**Closed: KEEP all 9 states for V0.** Lane D evidence (651 unknown-filtered Condition B polling rows):

| State | Lane D % | Disposition |
|---|---|---|
| Build | 60.2 % | Load-bearing |
| Silence | 21.7 % | Load-bearing |
| Drop | 6.8 % | Load-bearing (Tracks 01B + 03B; correctly partitioned) |
| Dense | 4.5 % | Load-bearing (Track 03B; correctly partitioned) |
| Breakdown | 3.7 % | Load-bearing (all 3 tracks) |
| Ambient | 2.9 % | Load-bearing (Track 01B-concentrated) |
| Steady | 0.3 % | **V0-observe-only** (2 rows / 740) |
| Transition | 0.0 % | **V0-observe-only** (0 rows / 740) |

No collapse to 4-state vocabulary. Transition + Steady carry a V1 deprecation gate: combined < 1 % across ≥ 10 tracks × ≥ 5 min in V1 Pass C → authorised removal in V1.

### Q10 — Master phase API

**Closed.** Source-confirmed promotion path: `EsBeatClock` produces `MusicalGridSnapshot` → `RendererActor` promotes into `EffectContext::AudioContext::musicalGrid`. AudioContext accessor helpers (`isOnBeat()`, `getBPM()`, `getTempoConfidence()`, `isBeatTick()`, `getBeatStrength()`) prefer `OnsetContext`, fall back to `MusicalGrid`.

- **Canonical consumer API:** `EffectContext::AudioContext`.
- **Canonical struct type:** `audio::MusicalGridSnapshot`.
- **Canonical producer:** `EsBeatClock`. `TempoTracker` + `MusicalGrid.cpp` excluded from production.
- **Legacy mirrors retained, not deprecated:** `ControlBusFrame.tempoBpm / tempoBeatStrength / tempoConfidence` and `es_bpm / es_tempo_confidence / es_beat_strength`. New effect code should use `ctx.audio.*` helpers.

---

## What was surfaced for Captain (batched, not drip-fed)

Five Captain confirmations are sign-off gates for V0:

1. **Q9.1 — `confidenceFloor` numeric.** Default 0.40; range 0.30 – 0.45.
2. **Q9.2 — Davies / IBT / MIREX citation reconciliation.** These constants are named in Captain's onwards brief but absent from inventoried sources (2026-05-12 suite, live source, ESV11 vendor headers, MusicAware audit). RFC presumes Family-B / reverse-engineered Synesthesia authority. Captain confirmation needed: either accept V0 = live-debounce ratified (Davies / IBT / MIREX deferred to V1+) or surface the missing source.
3. **Q9.3 — Architectural data-flow clarification.** Captain's brief said "Director writes to ControlBus." Live source has Director writing `SynqMatrixSwitchRequest` + mutating `SynqMatrixParams` (consumed by `RendererActor`); effects read audio state via `EffectContext::AudioContext` (parallel readers). RFC asks Captain to confirm § 3.4 as the authoritative data-flow statement.
4. **Q9.4 — V1 deprecation gate for Transition + Steady states.** Criterion: combined < 1 % across ≥ 10 tracks × ≥ 5 min in V1 Pass C. Captain confirmation or override.
5. **Q9.5 — Family A vs Family B distinction citation gap.** Captain's brief states Family B is authoritative; the RFC asks Captain to either confirm this is shorthand for "the reverse-engineered Synesthesia source is authoritative" or surface the specific Family A document.

---

## What is NOT done (deliberately, per Captain's brief)

- **No implementation.** Captain's brief: "Do not propose implementation. The RFC is a specification document. Implementation is the next agent's work after Captain signs off the RFC." Respected.
- **No new files outside the RFC + this session handover.** Respected.
- **No V1 / V2 implementation.** Respected — V1 (family morphing) and V2 (constrained switching) are validation-gated; the RFC defines their gates only.
- **No firmware code changes.** Respected — zero source-tree edits.
- **No pseudocode.** Respected — the RFC describes behaviour and contracts; pseudocode would be the implementation agent's domain.
- **No new tools / libraries / dependencies.** Respected.
- **No re-litigation of Q1, Q2, Q3, Q5, Q7, Q9.** Referenced in § 1.3 of the RFC, not re-decided.

---

## Next implementation agent's brief (recommended structure)

The next agent will translate the RFC into a V0 implementation specification. Their brief should:

1. **Read the RFC end-to-end** (firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md) and verify Captain has signed off on Q9.1 – Q9.5 before starting.
2. **Verify the live source delta.** The RFC ratifies live values at HEAD `dc46cc9b`. Before any change:
   - Run `git log firmware-v3/src/core/synqmatrix/` since `dc46cc9b` to check for drift.
   - Verify the `kMatrix[]` table at `SynqMatrix.cpp:40-50` against the per-state matrix in RFC § 4.
   - Verify the gating constants at `SynqMatrix.cpp:16-29` against RFC § 5.2.
3. **Implement the new V0 fields:**
   - `SynqMatrixConfig.confidenceFloor` (float, default per Captain's Q9.1 answer; clamp 0.0 – 1.0).
   - `SynqMatrixStatus.audioConfidenceBelowFloorMs` (uint32, rolling 10 s window).
   - `SynqMatrixStatus.missedPredictionCount` (uint32, rolling 10 s window).
   - `SynqMatrixStatus.tempoWinnerChanges` (uint32, rolling 10 s window).
4. **Implement the coast state machine per Q6 closure (§ 3.2):**
   - LOCKED → COAST when `audio.confidence < confidenceFloor` for ≥ 1 000 ms.
   - COAST → LOCKED when `audio.confidence ≥ confidenceFloor` for ≥ 500 ms.
   - During COAST: hold current parameters, no switching, no new state-promotion.
5. **Update the protocol contract YAML** (`docs/protocol/k1-ws-contract.yaml` + `docs/protocol/k1-rest-contract.yaml`) FIRST — gate rule per repo CLAUDE.md.
6. **Write a failing test FIRST** per `/test-driven-development` skill — gate rule per repo CLAUDE.md.
7. **Centre-origin enforced** for any new effect work — Hard Constraint per repo CLAUDE.md.
8. **No heap alloc in `render()`** — Hard Constraint per repo CLAUDE.md.
9. **2.0 ms per-frame ceiling** — Hard Constraint per repo CLAUDE.md.
10. **British English** in all comments, logs, UI strings.
11. **Hardware test BEFORE commit** — Captain rule. Build success is not sufficient; flash + behavioural test on K1v2 required.
12. **V0 closure evidence:** the new `confidenceFloor` + composite telemetry should be exercised under a re-run Lane D Pass A/B (or a focused regression capture) to verify telemetry is producing meaningful values.

**Capture rig:** `/tmp/capture_lane_d.py` HEAD (with the 5+1 fixes baked in 2026-05-15) is the canonical V1/V2 harness. V1 Pass C and V2 Pass D inherit it. See `firmware-v3/docs/research/SESSION_HANDOVER_20260514_Lane_D_Evidence.md` § "Permanent script fixes (5 + 1)".

---

## Loose ends explicitly NOT propagated

The next agent does NOT need to handle these — they are out of V0 scope or already-closed elsewhere:

- **AP-VP renderer drop-counter rework.** Closed at `firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md` (counter naming artefact, not defect — Lane D Track 01 evidence confirms).
- **K1 WiFi mode change.** AP-only canonical. Captain explicitly out-of-scope.
- **MusicalGrid.cpp / TempoTracker production reinstatement.** Excluded from production builds; EsBeatClock owns phase. Out of scope.
- **`/api/v1/songAware/*` and `songAware.*` WS legacy aliases.** Retained for backward compatibility. Not deprecated, not removed.
- **Lane D evidence directory rename.** `k1_songaware_lane_d_evidence/` preserves SongAware name for audit-trail continuity (Captain decision).

---

## Lessons / observations for future agents

1. **The 2026-05-12 suite predates the rename.** Every `songAware` identifier in that suite must be mapped to `synqMatrix` when reading. Mode-enum is also factorised differently (5-mode → 3-mode + 3-profile). The RFC § 10.4 documents the bridge.

2. **`synqmatrix.confidence` is saturated and uninformative.** 99.7 % of populated Lane D rows report 1.000 regardless of state. Coast triggers and any confidence-based gating must use `audio.confidence` (audio-pipeline) not `synqmatrix.confidence` (classifier-reported).

3. **`unknown` in the Lane D state distribution is polling-protocol jitter, not classifier output.** 12 – 14 % of polling rows have `synqmatrix: {}` (empty dict, capture-side response fusion). Filter before computing meaningful state-distribution metrics. Capture-script hardening before V1 Pass C is recommended (RFC § 7.2).

4. **`Transition` (0 rows) and `Steady` (2 rows / 740) are observed dead-weight in V0.** Kept for V0 with a V1 deprecation gate. The V1 broader corpus determines whether they survive.

5. **Director outputs are NOT direct ControlBus writes.** The Director writes `SynqMatrixSwitchRequest` (out-param from `tick()`) and mutates `SynqMatrixParams` (consumed by `RendererActor`). Effects read audio state via `EffectContext::AudioContext` (parallel-readers model). Captain's pre-decided context's literal phrasing differs from the implementation; the RFC § 3.4 + § 9.3 documents the actual data flow.

6. **Davies-3 / IBT-8 / MIREX ±70 ms semantics are not in the inventoried sources.** Captain authority outside inventoried sources is respected; the RFC defers strict Davies/IBT lock semantics to V1+ tempo-tracker-layer work. If Captain has the Synesthesia reverse-engineered source, surfacing it before V1 is recommended.

---

## RFC sign-off status

**Awaiting Captain.** Sign-off line is blank in the RFC § 12 changelog. Five batched confirmations (Q9.1 – Q9.5) at RFC § 9 are the gate.

After sign-off, this session's outputs are:

- `firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md` (the RFC itself)
- `firmware-v3/docs/SESSION_HANDOVER_20260515_SynqMatrix_RFC.md` (this handover)

No firmware source files modified. No new tools or libraries. No V1 / V2 implementation. Centre-origin and audio-playback safety unchanged.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-15 | agent:claude (opus-4.7) | Created session handover after SynqMatrix Director RFC was authored at firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md. Documents what was authored (12-section RFC), what was decided (Q4 / Q6 / Q8 / Q10 closures), what was surfaced for Captain (Q9.1 – Q9.5 batched confirmations), what is deliberately out of scope (V1 / V2 implementation, AP-VP rework, WiFi mode), and the recommended next implementation agent's brief. RBDO: GROUNDED. |
| 2026-05-16 | agent:claude (opus-4.7) | RFC reconciliation against Synesthesia Authority Audit 2026-05-16 applied to firmware-v3/docs/research/SynqMatrix_Director_RFC_2026-05-15.md (10 edits). Q9.1 – Q9.4 ACCEPTED. Q9.5 PROCEED_WITH_DEGRADED_Q9_5 with Q9.5(f) closed at shorthand interpretation per Captain plan approval 2026-05-16. V1+ TempoBank / Music-Timebase Contract Investigation opened as RFC § 8.5. Zero firmware source files modified. RBDO: GROUNDED for V0 closure and Q9.1–Q9.4 reconciliation; DEGRADED_Q9_5 for Synesthesia / Family-B authority framing. |
