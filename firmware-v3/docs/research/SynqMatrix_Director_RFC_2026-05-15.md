---
abstract: "SynqMatrix Director V0 specification. Consumes Lane D evidence (sign-off 2026-05-15 UTC+8 18:13), the 2026-05-12 k1_songaware_* research suite, live source at firmware-v3/src/core/synqmatrix/ (HEAD dc46cc9b on feature/synqmatrix-rename-2026-05-13), the ESV11 vendor tempo path, and the MusicAware audit. Closes Q4 (TempoObservation cadence = 192 ms ESV11 96-bin bank refresh), Q6 (coast = simple audioConfidence floor rule with composite as telemetry), Q8 (KEEP 9-state vocabulary for V0; Transition + Steady observe-only with V1 deprecation gate), Q10 (canonical phase API = EffectContext::AudioContext with MusicalGridSnapshot as canonical struct; ControlBusFrame tempo*/es_* are legacy mirrors). Documents the state-to-visual-intent matrix, ownership and gating policy, public control schema, Lane D empirical summary, and V0→V1→V2 validation gates. Reconciles SongAware→SynqMatrix rename. Reconciled 2026-05-16 against the Synesthesia Authority Audit (firmware-v3/docs/research/SynqMatrix_Synesthesia_Authority_Audit_2026-05-16.md): Q9.1 – Q9.4 ACCEPTED; Q9.5 status PROCEED_WITH_DEGRADED_Q9_5 with shorthand interpretation confirmed by Captain plan approval 2026-05-16; Davies/IBT/MIREX re-attributed to Tier-2 academic compass-artifact and deferred to V1+ tempo-tracker work; Synesthesia and AutoBPM acknowledged as Tier-1 reference bodies, not implementation authorities; V1+ TempoBank / Music-Timebase Contract Investigation opened as a separate lane (§ 8.5)."
rbdo: "GROUNDED for V0 closure and Q9.1–Q9.4 reconciliation; DEGRADED_Q9_5 for Synesthesia / Family-B authority framing. Reconciled 2026-05-16 against the Synesthesia Authority Audit; Davies/IBT/MIREX re-attributed to Tier-2 academic compass-artifact per MusicAware_Audit_And_Gap_Analysis.md:61-65 and deferred to V1+ tempo-tracker work; the 'reference-grade authoritative' framing for Synesthesia is withdrawn; Q9.5(f) confirmed at shorthand interpretation by Captain plan approval 2026-05-16."
---

# SynqMatrix Director — RFC, 2026-05-15

**Author:** Claude Opus 4.7 (1M context)
**Captain:** Elroy Yeap (elroy@spectrasynq.com)
**Branch:** `feature/synqmatrix-rename-2026-05-13` (HEAD `dc46cc9b`)
**Production target:** `esp32dev_audio_esv11_k1v2_32khz` (canonical AP-only K1v2)
**Status:** awaiting Captain sign-off
**Scope:** **V0 (parameter mode) only.** V1 (family morphing) and V2 (constrained switching) are validation-gated; this RFC defines the gates but does not specify their implementation.

---

## 0. Executive Summary

This RFC consolidates the SongAware → SynqMatrix workstream into a single coherent V0 specification. It consumes:

- The 2026-05-12 k1_songaware_* research suite (nine files; Lane A/B/C/D/E lane outputs + decision record + multi-SSA execution plan).
- The 2026-05-14 Lane D evidence capture (six paired Condition A / Condition B serial JSONL + MOV captures across three tracks, totalling 1,484 polling rows; Captain sign-off 2026-05-15 UTC+8 18:13).
- Live source at `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}` (513 + 1,637 lines) and the ESV11 vendor tempo path at `firmware-v3/src/audio/backends/esv11/vendor/tempo.h` (443 lines).
- The MusicAware audit at `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md` (181 lines, 15-row gap matrix).

It closes four open decisions left open by the 2026-05-12 suite:

| ID | Decision | Closure |
|---|---|---|
| **Q4** | TempoObservation cadence | **One full ESV11 96-bin tempo bank refresh ≈ 192 ms** at 32 kHz / 128-sample chunks. SynqMatrix consumes audio frames at its 500 ms tick + 400 ms candidate hold for V0. Davies-3-promote / IBT-8-bad-demote at the tempo-tracker layer is a V1+ design target. |
| **Q6** | Coast-through-silence rule | **Simple rule** — `audioConfidence < confidenceFloor` sustained ≥ 1 000 ms triggers demotion from LOCKED → COAST. Composite metrics (audioConfidence + missed-prediction counter + tempo-winner-change events) instrumented as telemetry only. Numeric `confidenceFloor` surfaced to Captain (default proposal **0.40**). |
| **Q8** | State vocabulary | **KEEP all 9 states for V0.** Lane D evidence shows Build / Drop / Breakdown / Dense / Ambient / Silence all fire with track-specific perceptual intent. Transition (0 / 740 polling rows) and Steady (2 / 740) flagged V0-observe-only with V1 deprecation gate if combined frequency stays < 1 % across a broader corpus. No 4-state collapse. |
| **Q10** | Master phase API | **Canonical consumer API = `EffectContext::AudioContext`** with helpers `isOnBeat()`, `getBPM()`, `getTempoConfidence()`, `isBeatTick()`, `getBeatStrength()`. **Canonical struct type = `audio::MusicalGridSnapshot`**. **Canonical producer = `EsBeatClock`**. `ControlBusFrame.tempoBpm / tempoBeatStrength / tempoConfidence` and `es_bpm / es_tempo_confidence / es_beat_strength` are legacy mirrors retained for backward compatibility, not preferred for new code. |

It ratifies the existing implementation of dwell, cooldown, hysteresis, ownership precedence, manual-suppression, show-suppression, and health-recovery (all values cited from source). It documents the public control schema (REST + WebSocket + Serial CLI) field-by-field. It tabulates the Lane D evidence statistics. It defines V0 → V1 → V2 promotion criteria mapped to the 2026-05-12 Gate 2 / 3 / 4 ladder.

It is a specification document. **No pseudocode. No new firmware code path proposed. No new tools or libraries.** Implementation work is the next agent's brief, downstream of Captain's sign-off.

---

## 1. Status & Scope

### 1.1 What is being decided here

**FACT.** The four Q-decisions named above. Each is closed inside this RFC with evidence-grounded reasoning. Captain confirmation of the closures (plus one numeric, `confidenceFloor`) is the sign-off gate.

### 1.2 What is **not** being decided here

- **V1 (family morphing).** Validation-gated on Lane D Pass C. Out of scope.
- **V2 (constrained switching).** Validation-gated on Lane D Pass D + C/D combined gates. Out of scope.
- **Director RFC implementation.** Captain's brief explicitly forbids implementation proposals. The next agent's brief is what gets specified post sign-off.
- **AP-VP renderer drop-counter rework.** Closed elsewhere at `firmware-v3/docs/research/ap_vp_contract_frame_drop_investigation_2026-05-14.md`. Out of scope.
- **K1 WiFi mode change.** AP-only is the canonical shipping mode (`WIFI_AP_ONLY`). Out of scope.

### 1.3 Six Q-decisions already adjudicated (referenced, not re-decided)

**FACT** — these are recorded in the 2026-05-12 suite and remain authoritative:

| ID | Decision | Adjudication source |
|---|---|---|
| Q1 | Is "song-aware" today caused by automatic effect-ID switching? | **CLOSED — NO.** `k1_songaware_translation_research_task_2026-05-12.md:9`; reinforced at `k1_songaware_decision_record_2026-05-12.md:40-44`. |
| Q2 | Strategic priority order (parameter → family morph → constrained switching)? | **CLOSED — confirmed.** `k1_songaware_decision_record_2026-05-12.md:15` and `:17, :55-59`. |
| Q3 | Lane D validation gates and PASS/FAIL semantics? | **CLOSED.** `k1_songaware_validation_protocol_laneD_2026-05-12.md:37-58` and `:92-171`. |
| Q5 | Ownership precedence? | **CLOSED — show > manual > director.** `k1_songaware_control_surface_schema_2026-05-12.md:65-69`; `k1_songaware_rollout_safety_2026-05-12.md:25-31`; `k1_songaware_director_mode_matrix_2026-05-12_analysis.md:47`. |
| Q7 | Dwell / cooldown / thrash thresholds? | **CLOSED.** Dwell ≥ 4 s (now **8 s** per live source); cooldown ≥ 8 s (now **20 s** per live source); thrash zero; switch rate ≤ 2/min. Suite cites at `k1_songaware_director_mode_matrix_2026-05-12_analysis.md:48`; `k1_songaware_validation_protocol_laneD_2026-05-12.md:104-112`; `k1_songaware_rollout_safety_2026-05-12.md:59`. Live source values cited in § 5. |
| Q9 | Implementation now, or evidence-only? | **CLOSED — evidence-only (2026-05-12).** `k1_songaware_decision_record_2026-05-12.md:84-86`. Note: the live source at HEAD `dc46cc9b` shows the rename has shipped post-evidence-gate, so this Q is historically closed; the present RFC is the implementation gate. |

---

## 2. Authoritative References

The RFC is reconciled with — and overrides nothing in — the following inputs. Where this RFC closes a question the inputs left open, that closure is the RFC's contribution; where this RFC ratifies an input, that ratification is explicit.

| Layer | Reference | RFC relationship |
|---|---|---|
| **Research suite** | `firmware-v3/docs/research/k1_songaware_*` (9 files, 2026-05-12) | Consumed; Q4/Q6/Q8/Q10 closures fill suite gaps |
| **Evidence capture** | `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/` | Consumed; 1,484 polling rows + 6 MOVs + per-track manifests + SESSION_HANDOVER + SESSION_MANIFEST |
| **Live source** | `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}` | Ratified; every behavioural value cited file:line |
| **ESV11 vendor** | `firmware-v3/src/audio/backends/esv11/vendor/tempo.h` and `EsBeatClock.cpp` | Ratified; cadence math for Q4 closure |
| **Audio contracts** | `firmware-v3/src/audio/contracts/ControlBus.h`, `MusicalGrid.h`, and `EffectContext.h` | Ratified; canonical-vs-mirror status for Q10 |
| **MusicAware audit** | `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md` | Consumed; 15-row gap matrix re-statused in § 10 |
| **Captain pre-decided context** | This RFC's brief (operational handover) | Respected. Davies-3-promote / IBT-8-bad-demote / MIREX-±70 ms / Family A / Family B citations come from Captain authority outside inventoried sources; see § 9 surface for citation reconciliation |

**FACT.** Production target is `esp32dev_audio_esv11_k1v2_32khz`. ESV11 is the production audio backend. `TempoTracker` and `MusicalGrid.cpp` are **excluded** from production builds — `EsBeatClock` owns phase.

**FACT.** SynqMatrix is the canonical module name post-rename. SongAware is the deprecated alias. Source-tree path is `firmware-v3/src/core/synqmatrix/`. Public surfaces expose canonical `synqMatrix.*` plus legacy `songAware.*` aliases.

---

## 3. Adjudication Closures (Q4 / Q6 / Q8 / Q10)

### 3.1 Q4 — TempoObservation cadence

**Captain's pre-decided framing:** define a `TempoObservation` as one full 96-bin tempo bank refresh; the lock criterion is three consecutive observations with unchanged winning bin and no octave flip (Davies-three-consecutive-promote / IBT-eight-bad-demote, IBT asymmetric tolerance ±46.4 ms inner with MIREX ±70 ms fallback).

**FACT (source-grounded cadence math).** From `firmware-v3/src/audio/backends/esv11/vendor/tempo.h` and `EsV11_32kHz_Shim.h`:

| Parameter | Value (32 kHz shim) | Source |
|---|---|---|
| `SAMPLE_RATE` | 32 000 Hz | `EsV11_32kHz_Shim.h:19` |
| `CHUNK_SIZE` | 128 samples | `EsV11_32kHz_Shim.h:20` |
| `NOVELTY_LOG_HZ` | 50 Hz (NOT overridden by shim) | `global_defines.h:21` and `tempo.h:239-265` |
| `NUM_TEMPI` | 96 bins (BPM 48 → 143 inclusive, 1 BPM resolution) | `global_defines.h:28` |
| Bins per `update_tempo()` call | 2 | `tempo.h:161-182` (`iter % 2 == 0` branch at `:171-175`; `calc_bin += 2` at `:177`) |
| `update_tempo()` call cadence | once per chunk | `EsV11Backend.cpp:119` |
| Chunk rate | 128 / 32 000 = 4 ms per chunk → **250 Hz** | computed |
| **One full 96-bin bank refresh** | 96 / 2 × 4 ms = **192 ms (≈ 5.2 refreshes/sec)** | computed |

**FACT (live source debounce values).** The SynqMatrix tick is rate-limited and uses time-based debounce, **not** count-based promote/demote. From `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp` anonymous-namespace constants at `:16-29`:

| Constant | Value | Purpose |
|---|---|---|
| `kEvaluationPeriodMs` | **500 ms** | `tick()` rate-limit |
| `kStableStateHoldMs` | **400 ms** | Candidate-hold for state promotion (general) |
| `kDropStateHoldMs` | **250 ms** | Candidate-hold for Drop when `audioConfidence > 0.60` (faster lock for transient strikes) |
| `kMinimumDwellMs` | **8 000 ms** | Post-promotion lock-in before next switch |
| `kSwitchCooldownMs` | **20 000 ms** | After any director-mode switch |

**INFERENCE.** At 500 ms tick × 400 ms candidate hold, the live SynqMatrix state machine promotes after approximately **one** confirmed observation at its own tick layer. This is **not** Davies-3-promote semantics at the tempo-tracker layer. At the bank-refresh layer (192 ms), three consecutive observations would equal 576 ms — less than one SynqMatrix tick interval. The two layers run at incompatible cadences for the literal Davies-3 framing to apply directly.

**HYPOTHESIS (citation gap).** Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms inner / MIREX ±70 ms fallback are named in Captain's onwards brief and are presumed to live in a reverse-engineered Synesthesia source (Family B authoritative). These constants are **not** in the inventoried sources (2026-05-12 suite, live SynqMatrix source, ESV11 vendor headers, MusicAware audit). The RFC respects Captain's authority but flags the citation gap in § 9.

**Closure.**

1. **`TempoObservation` is defined as one full ESV11 96-bin tempo bank refresh: 192 ms at 32 kHz / 128-sample chunks (250 Hz chunk rate).** This grounds Captain's framing in source-confirmed cadence.

2. **V0 ships the existing live debounce as the lock state machine:**
   - Candidate-hold 400 ms (Drop: 250 ms when `audioConfidence > 0.60`)
   - Tick period 500 ms
   - Minimum dwell post-promotion 8 000 ms
   - Switch cooldown 20 000 ms
   - Anti-thrash window 45 000 ms (A→B→A guard, `kAntiThrashWindowMs` at `SynqMatrix.cpp:26`)
   - Max switches per window 2 in 60 000 ms (`kMaxSwitchesPerWindow` at `:25` and `kSwitchWindowMs` at `:24`)

3. **Strict Davies-3-promote and IBT-8-bad-demote semantics at the tempo-tracker layer are a V1+ design target.** If Captain wants tighter lock semantics, the work belongs in `EsBeatClock` (the canonical phase producer per § 3.4) — not in `SynqMatrix.cpp`. The RFC does not propose a design here.

4. **IBT asymmetric tolerance ±46.4 ms inner / MIREX ±70 ms fallback** are similarly V1+ tempo-tracker-layer concerns, not SynqMatrix-layer concerns. V0's beat-phase consumption uses the boundary gate at `SynqMatrix.cpp:1019-1041` with a ±0.06 phase fallback (i.e. ±6 % of a beat at the consumer side).

**RBDO label for this closure: DEGRADED-MODE (residual; citation gap closed by re-attribution 2026-05-16).**

- **Unresolved assumption (re-attributed 2026-05-16).** Davies-3 / IBT-8 / MIREX ±70 ms framing originates from Tier-2 academic compass-artifact per `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61-65`, NOT from Synesthesia / Family B. The 2026-05-16 Synesthesia Authority Audit explicitly refutes Synesthesia-derivation of these constants (audit § 4 ledger rows 3-5; audit § 5 claim C6 status CONTRADICTED).
- **Risk if wrong.** If the Tier-2 academic attribution is later contested, V1+ design over-cites compass-artifact; V0 ship state remains the live debounce regardless.
- **Fallback.** Treat the closure as: V0 = live debounce ratified; Davies/IBT/MIREX framing deferred to V1+ tempo-tracker work citing Tier-2 academic compass-artifact directly.
- **Revisit trigger.** V1+ tempo-tracker work formally cites the compass-artifact academic source in its own RFC.
- **Debt count.** 1 output (this Q4 closure). Affects: § 3.1 + § 9 + § 11.

Reconciled 2026-05-16 against the Synesthesia Authority Audit; citation gap closed by re-attribution.

---

### 3.2 Q6 — Coast-through-silence threshold (simple vs composite)

**Captain's pre-decided leaning (per onwards brief):** simple rule — `audioConfidence < floor` for ≥ 1 s triggers demotion from LOCKED; composite (`audioConfidence + 8-bad-predictions + tempo-winner-change`) instrumented as observability metrics in parallel for later A/B-driven refinement.

**FACT (source-grounded).** The 2026-05-12 suite documents:

- A three-valued `silencePolicy` enum: `hold | fade_to_ambient | manual_hold` (`k1_songaware_control_surface_schema_2026-05-12.md:39`).
- Silence gate: "No automatic movement below the calibrated floor. Floor not yet calibrated; default research fallback is hold/manual" (`k1_songaware_rollout_safety_2026-05-12.md:57`).
- Lane B silence-state visual: "Enter only after sustained low confidence; exit through ambient or build" (`k1_songaware_director_mode_matrix_2026-05-12_analysis.md:34`).

**FACT (Lane D evidence).** From SSA2 analysis of 740 Condition B polling rows:

| State | Mean RMS | p50 RMS | p90 RMS | Mean `audio.confidence` |
|---|---|---|---|---|
| silence | 0.544 | 0.625 | 1.000 | 0.741 |
| build | 0.591 | 0.668 | 1.000 | 0.780 |
| drop | 0.689 | 0.789 | 0.999 | 0.769 |
| ambient | 0.443 | 0.233 | 1.000 | 0.572 |
| breakdown | 0.666 | 0.805 | 1.000 | 0.674 |

**33 of 141 `silence`-state rows (23.4 %) have RMS ≥ 0.9.** The classifier's `silence` state fires under loud audio.

**INFERENCE.** The `silence` classifier state is decoupled from instantaneous audio loudness. It is a temporal/contextual decision (recently silent, low-flux, pre-onset, low confidence) — not an audio-level signal. Therefore the **coast trigger (which is about audio-pipeline confidence, not classifier output) lives at a layer distinct from the `silence` state**. Captain's simple-rule leaning is sound: the trigger is `audioConfidence < floor`, not `state == silence`.

**FACT (live source).** `SynqMatrix.cpp:592-597` already short-circuits the entire `tick()` to `NoAudio` suppression when `audioAvailable == false`. The "missing audio" case is handled. The remaining design question is what to do when audio is present but confidence is low.

**Closure.**

1. **Coast trigger (V0):** `audioConfidence < confidenceFloor` sustained for ≥ 1 000 ms causes demotion from `LOCKED` to `COAST` (i.e. parameter-hold, no switching, no new state-promotion attempts). Recovery: when `audioConfidence ≥ confidenceFloor` for ≥ 500 ms, return to normal tick.

2. **`confidenceFloor` numeric value — SURFACED TO CAPTAIN.** Default proposal: **0.40**. Range: 0.30 (loose, accepts marginal audio) to 0.45 (strict, demotes earlier). The 2026-05-12 suite documented this as uncalibrated. Lane D evidence shows `synqmatrix.confidence` is saturated at 1.000 for 99.7 % of populated rows (§ 7.3) — so the floor must operate on `audio.confidence` (the audio-pipeline confidence), not `synqmatrix.confidence` (the classifier confidence). Captain confirmation required for the numeric.

3. **Composite metrics (telemetry only, V0):** instrument and surface in `synqMatrixTelemetry` payload:
   - `audioConfidenceBelowFloorMs` — running counter of ms below floor in last 10 s window.
   - `missedPredictionCount` — running count of beat-phase predictions that did not coincide with an `onset.beat.fired` event.
   - `tempoWinnerChanges` — count of ESV11 winning-bin changes in last 10 s window.
   These do not gate behaviour in V0; they exist for V1 A/B refinement.

4. **silencePolicy enum (unchanged from suite proposal):** `hold | fade_to_ambient | manual_hold`. V0 default = `hold`. Captain can override per-config.

5. **Re-validation on audio return** (closes audit Row 10 — the only `ABSENT` row in the gap matrix): same `audioConfidence ≥ floor` for ≥ 500 ms criterion. No separate composite trigger for re-validation in V0.

**RBDO label for this closure: GROUNDED** — apart from the `confidenceFloor` numeric, which is DEGRADED-MODE pending Captain confirmation:

- **Unresolved assumption.** `confidenceFloor = 0.40` is a default proposal, not a calibrated value.
- **Risk if wrong.** Too high → director coasts excessively, parameter mode under-activates. Too low → director thrashes during marginal audio.
- **Fallback.** Adopt Captain's chosen numeric. If no override, ship 0.40 and instrument the three composite metrics for V1 A/B tightening.
- **Revisit trigger.** First V1 evidence cycle (Lane D Pass C) — re-tune from composite-metric distributions across a broader corpus.
- **Debt count.** 1 numeric.

---

### 3.3 Q8 — State vocabulary (KEEP 9 vs collapse to 4)

**Captain's pre-decided framing:** the current nine-state enum is load-bearing on the `kPolicies[]` table at SynqMatrix.cpp; the shrunk-vocabulary option (Silence / Ambient / Steady / Eventful) demotes Build / Drop / Breakdown to derived signals on ControlBus that effects read directly. Lane D evidence informs the call.

**FACT (live source — 9 states + 9 policy rows).** From `SynqMatrix.h:67-77` and `SynqMatrix.cpp:40-50`:

| State (enum value) | `kMatrix[]` row (state, effectId, family, visual language, switch reason, minConfidence) |
|---|---|
| `Unknown=0` | `{Unknown, EID_SB_K1_WAVEFORM, "baseline", "k1_waveform_restore_baseline", SwitchReason::None, 1.0f}` |
| `Silence=1` | row exists at `SynqMatrix.cpp:42` |
| `Ambient=2` | row exists at `:43` |
| `Steady=3` | row exists at `:44` |
| `Build=4` | row exists at `:45` |
| `Drop=5` | row exists at `:46` |
| `Breakdown=6` | row exists at `:47` |
| `Dense=7` | row exists at `:48` |
| `Transition=8` | `{Transition, EID_LGP_CHROMATIC_PULSE, "advanced_optical", "chromatic_transition_pulse", SwitchReason::TransitionBridge, 0.45f}` |

`kPolicyCount = 9`. All nine canonical states are present; no extras, no missing.

**FACT (Lane D empirical distribution, unknown-filtered).** From SSA2 analysis of 651 valid Condition B polling rows (after filtering 89 `unknown` rows that are polling-protocol artefacts — see § 7.2):

| State | Rows | % | Per-track presence |
|---|---|---|---|
| build | 392 | 60.2 % | All three tracks (01B 174, 02B 156, 03B 62) |
| silence | 141 | 21.7 % | All three tracks (heavy on 02B with 92) |
| drop | 44 | 6.8 % | **01B + 03B only** (none on 02B breakdown track) |
| dense | 29 | 4.5 % | **03B only** |
| breakdown | 24 | 3.7 % | All three tracks (01B 10, 02B 4, 03B 10) |
| ambient | 19 | 2.9 % | **01B-concentrated** (18 / 19) |
| steady | 2 | 0.3 % | **02B only (2 rows)** |
| **transition** | **0** | **0.0 %** | **NEVER OBSERVED** |

**INFERENCE.**

1. **Build, Silence, Drop, Dense, Breakdown, Ambient all carry distinct perceptual intent that the 4-state collapse (Silence / Ambient / Steady / Eventful) would lose:**
   - `Drop` is the impact pulse — distinct visual treatment per Lane B matrix at `k1_songaware_director_mode_matrix_2026-05-12_analysis.md:38`.
   - `Breakdown` is the release / clean-decay posture — distinct from `Steady` per matrix `:39`.
   - `Dense` is high-fill while retaining centre-origin — distinct from `Build` per matrix `:40`.
   - `Ambient` is drifting low-contrast — distinct from `Silence` per matrix `:35`.
   - Collapsing these to "Eventful" loses the Build / Drop / Breakdown perceptual triad that the Lane B visual-intent matrix is built around.

2. **Drop / Dense / Ambient are track-specific accents.** They do not fire on every track (Drop misses the breakdown track, Dense is Pjanoo-only, Ambient is Levels-only). This is not a problem — it is the state machine correctly partitioning by structural content. A broader corpus is needed to confirm rates.

3. **Transition (0 / 740 rows) and Steady (2 / 740 rows) are observed dead-weight on this corpus.** Two possibilities:
   - The classifier triggers for these states are too strict and they never fire.
   - These states are genuinely rare in EDM corpus (the Lane D tracks are all electronic; Transition / Steady might fire more on transitional / instrumental material).

**Closure.**

1. **V0 ships the full 9-state vocabulary unchanged.** All 9 enum values and all 9 `kMatrix[]` rows remain load-bearing.

2. **No collapse to 4-state vocabulary in V0.** The collapse would lose Build / Drop / Breakdown / Dense / Ambient perceptual distinctions that Lane D evidence confirms the classifier produces.

3. **Transition and Steady flagged `V0-observe-only`.** They are functional in the enum, dispatched through `kMatrix[]`, and counted in telemetry, but the RFC formally records that:
   - Transition has 0 Lane D occurrences.
   - Steady has 2 Lane D occurrences (0.3 %).
   - **V1 deprecation gate:** if the combined Transition + Steady frequency stays below **1 %** across a corpus of ≥ 10 tracks × ≥ 5 minutes (i.e. ≥ 3 000 polling rows total) during V1 evidence runs, the RFC authorises their removal in V1 with a follow-up RFC.

4. **`Unknown` is retained.** It is the baseline / boot / `kPostEnableGraceMs` state and the safe fallback for `parseSynqMatrixState()`. It does NOT mean "polling-protocol artefact" — that confusion comes from Lane D capture-script labelling of empty `synqmatrix` dicts as `unknown` (see § 7.2). The classifier itself emits `Unknown` legitimately at boot and during health-recovery; this is correct.

**RBDO label for this closure: GROUNDED.**

---

### 3.4 Q10 — Master phase API (canonical surface confirmation)

**Captain's pre-decided framing:** the implicit decision is that `MusicalGridSnapshot` promoted through `EffectContext::AudioContext` is the public contract, with parallel surfaces (`OnsetContext`, `ControlBusFrame` beat/tempo fields) deprecated as render-domain mirrors only.

**FACT (live source — promotion path).**

- `EsBeatClock` produces `MusicalGridSnapshot m_snap`. Initialised at `EsBeatClock.cpp:44`, populated each tick from `:79` onward.
- `RendererActor` consumes the grid: `RendererActor.cpp:284` and `:302` take `const audio::MusicalGridSnapshot& grid` parameters. Render-loop promotes the snapshot into `EffectContext::AudioContext::musicalGrid` at `EffectContext.h:85`.
- `EffectContext::AudioContext` accessor helpers (`EffectContext.h:138-178`) compute via OR-fallback: `isOnBeat()` is `onset.beat.fired || musicalGrid.beat_tick`. Same pattern for `getBPM()`, `getTempoConfidence()`, `isBeatTick()`, `getBeatStrength()`. **The accessors prefer `OnsetContext`, fall back to `MusicalGridSnapshot`.**

**FACT.** `TempoTracker` and `MusicalGrid.cpp` (the legacy snapshot producer) are **excluded from production builds.** `EsBeatClock` is the sole production phase producer.

**FACT (legacy mirrors).** `ControlBus.h` carries duplicate fields:
- `tempoConfidence` at `ControlBus.h:108, :209` — documented as PipelineCore/TempoTracker-era field
- `tempoBpm` at `:111, :212` (default 120.0)
- `tempoBeatStrength` at `:112, :213`
- `es_bpm` at `:218`, `es_tempo_confidence` at `:219`, `es_beat_strength` at `:221` — ESV11-native equivalents
- `onsetEvent` at `:81, :173`

`SynqMatrix.cpp:1187` and `:996-1007` read both families and take `max(es_*, *)` in `updateAudioSummary()` and `buildFeatureSnapshot()`.

**INFERENCE.** The architecture is correct as-is. The accessor helpers in `EffectContext::AudioContext` are the single point of truth for *consumer code*. Behind them, `OnsetContext` (render-tick-aligned beat events) and `MusicalGridSnapshot` (continuous beat phase + grid) cover the two complementary timing needs. The `ControlBusFrame.tempo*` and `es_*` fields are *internal* mirrors that the audio chain populates and the Director reads directly (via `updateAudioSummary()`), but new effect code should consume via `ctx.audio.*` accessors.

**Closure.**

1. **Canonical consumer API: `EffectContext::AudioContext`.** Helpers `isOnBeat()`, `getBPM()`, `getTempoConfidence()`, `isBeatTick()`, `getBeatStrength()` (and any future helpers added there) are the public contract. New effect code SHALL consume via these accessors.

2. **Canonical struct type: `audio::MusicalGridSnapshot`** (defined in `firmware-v3/src/audio/contracts/MusicalGrid.h`). This is the canonical representation of beat / tempo / phase / grid state.

3. **Canonical producer: `EsBeatClock`** (in production builds). `TempoTracker` and `MusicalGrid.cpp` are non-production and out of scope.

4. **`OnsetContext` is canonical for render-tick-aligned beat events.** It is not a mirror; it is the discrete-event surface that complements the continuous `MusicalGridSnapshot`. Both are part of the canonical AudioContext bundle.

5. **Legacy mirrors retained, not deprecated:**
   - `ControlBusFrame.tempoBpm`, `tempoBeatStrength`, `tempoConfidence`
   - `ControlBusFrame.es_bpm`, `es_tempo_confidence`, `es_beat_strength`
   - `ControlBusFrame.onsetEvent`

   These remain readable for backward compatibility (the Director itself uses them via `updateAudioSummary()`). New effect code SHOULD prefer `ctx.audio.*` helpers. The RFC does not mark these fields as removable in V1 — they are part of the audio chain's internal contract and removing them would require a separate audio-chain RFC.

6. **Architectural clarification (not a Q-decision closure, but a load-bearing correction):** The Director does **not** write to `ControlBus`. It writes to:
   - `SynqMatrixSwitchRequest` — an out-parameter from `tick()` consumed by `RendererActor.cpp:2144`.
   - `SynqMatrixParams` — mutated by `apply()`, consumed by `RendererActor.cpp:2284`.
   - Internal `std::atomic<*>` snapshot fields surfaced via `getStatus()` and `getDebugSnapshot()`.

   Effects do not read from a Director-written ControlBus stream. The Director and effects are *parallel readers* of audio state via `EffectContext::AudioContext`; the Director gates *which effect* is active and *with what parameters*, while effects render against the same audio surface independently. Captain's onwards brief said "Director writes to ControlBus; all effects including Kuramoto consume from ControlBus generically with no dedicated contract" — the spirit is correct (no per-effect Director→effect contract) but the literal data path is Director→RendererActor (switch + params) and AudioPipeline→ControlBus→AudioContext→effects (audio state).

**RBDO label for this closure: GROUNDED.**

---

## 4. State-to-Visual-Intent Matrix

**Purpose.** This is the load-bearing artefact of the RFC for implementation agents. For every state in the chosen vocabulary, it specifies what the Director writes to `SynqMatrixParams`, what effect-switch target it nominates (in Director mode), what audio features gate entry, what dwell / cooldown applies, and what is forbidden.

**Sources.** Lane B band-table at `k1_songaware_director_mode_matrix_2026-05-12_analysis.md:32-41` (visual intent + band ranges); `SynqMatrix.cpp:40-50` (`kMatrix[]` effectId + family + visual language + minConfidence); `SynqMatrix.cpp:1050-1111` (classifyState entry conditions); `SynqMatrix.cpp:1113-1174` (updateStableState debounce); § 5 (gating policy).

**Bands convention.** All bands in the matrix below are 0-255 unsigned-byte ranges. "Intent" means the SynqMatrixParams modulation envelope; effects consume the final parameter and treat the band as the target setpoint while their own range and curve apply.

### 4.1 Per-state matrix

#### `Unknown (=0)` — baseline / boot / health-recovery

- **Entry conditions.** Boot (during `kBootGraceMs = 3 000 ms`), post-enable grace (`kPostEnableGraceMs = 4 000 ms` after `setConfig(enabled=true)`), health-degraded fallback, parse fallback.
- **Director output (SynqMatrixParams):** brightness ~120, speed ~24, intensity ~128, saturation ~128, complexity ~96, palette = configured default.
- **Switch target (Director mode):** `EID_SB_K1_WAVEFORM` ("k1_waveform_restore_baseline" — `SynqMatrix.cpp:41`).
- **minConfidence:** `1.0f` (i.e. Unknown is always allowed as a baseline restore).
- **Forbidden:** None — Unknown is the safe fallback.
- **Dwell / debounce:** `kStableStateHoldMs = 400 ms`. Exits when classifier emits any other state with sustained confidence.
- **Lane D presence:** **0 polling rows** (capture-script `unknown` labels are polling artefacts, not classifier output; see § 7.2). FACT.

#### `Silence (=1)` — narrow centre glow

- **Entry conditions.** Sustained low confidence with `isSilent` or `silentScale` low. Entry hysteresis: 0.10 sticky, 0.06 cold (per `SynqMatrix.cpp:1059`).
- **Visual intent (Lane B band).** Brightness 56-88, speed 8-16, intensity 32-72, complexity 24-64.
- **Director output:** brightness ~72, speed ~12, intensity ~52, saturation ~96 (cool), complexity ~44, slow breathing.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:42` — refer to source for current effectId.
- **Forbidden:** hard black (unless explicitly selected); rainbow; any high-energy effect.
- **Dwell / debounce:** standard `kStableStateHoldMs = 400 ms` candidate, then `kMinimumDwellMs = 8 000 ms` lock; suite recommended ≥ 8 s ambient/silence (which the live 8 000 ms minimum dwell satisfies).
- **Lane D presence:** 141 rows / 21.7 % aggregated CondB. **Note:** the classifier `silence` state fires under loud audio (23.4 % of silence rows have RMS ≥ 0.9). The state is a temporal/contextual decision, not an audio-level decision. § 7.

#### `Ambient (=2)` — soft drifting low-contrast

- **Entry conditions.** Low-to-moderate RMS with smoothed saliency; entry through Silence or Steady, not direct from Drop / Dense.
- **Visual intent (Lane B band).** Brightness 88-128, speed 12-22, intensity 56-96, complexity 48-88.
- **Director output:** brightness ~108, speed ~17, intensity ~76, saturation ~104, complexity ~68, centre-out spread.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:43`.
- **Forbidden:** sharp onset response; rainbow; full-strip sweep.
- **Dwell / debounce:** minimum dwell 8 000 ms (per Lane B recommendation of ≥ 8 s).
- **Lane D presence:** 19 rows / 2.9 % aggregated CondB. 18 of 19 on Track 01 (Avicii Levels). FACT.

#### `Steady (=3)` — balanced field with readable pulse

- **Entry conditions.** Stable RMS, moderate flux, decent confidence; beat-aware pulse permitted.
- **Visual intent (Lane B band).** Brightness 128-160, speed 20-32, intensity 96-144, complexity 96-144.
- **Director output:** brightness ~144, speed ~26, intensity ~120, saturation ~128, complexity ~120.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:44`.
- **Forbidden:** full hue-wheel; aggressive Drop-style impact.
- **Dwell / debounce:** ≥ 4 000 ms (Lane B), now ≥ 8 000 ms via live dwell.
- **Lane D presence:** **2 rows / 0.3 %** aggregated CondB. Both on Track 02 (Sun & Moon breakdown). FACT — observe-only with V1 deprecation gate per § 3.3.

#### `Build (=4)` — widening centre-out, increasing density

- **Entry conditions.** Rising flux with rising RMS slope; hysteresis 0.32 sticky / 0.38 cold (per `SynqMatrix.cpp:1084`).
- **Visual intent (Lane B band).** Brightness 144-176, speed 28-44, intensity 128-184, complexity 128-176.
- **Director output:** brightness ~160, speed ~36, intensity ~156, saturation ~136, complexity ~152, ramp-only — no effect-ID change during build, only parameter modulation.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:45`.
- **Forbidden:** stepwise jumps; back-stepping; effect-ID switches mid-ramp.
- **Dwell / debounce:** standard 400 ms candidate; minimum dwell 8 000 ms.
- **Lane D presence:** **392 rows / 60.2 %** aggregated CondB — dominant. All three tracks. FACT.

#### `Drop (=5)` — full centre-out impact

- **Entry conditions.** High RMS + strong onset; hysteresis 0.52 sticky / 0.62 cold (per `SynqMatrix.cpp:1083`); faster candidate hold `kDropStateHoldMs = 250 ms` when `audioConfidence > 0.60`.
- **Visual intent (Lane B band).** Brightness 168-200, speed 36-60, intensity 168-224, complexity 144-200.
- **Director output:** brightness ~184, speed ~48, intensity ~196, saturation ~140, complexity ~172, decisive pulse / collision / bloom.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:46`.
- **minConfidence (`kMatrix[]`):** higher than baseline — Drop only fires on high confidence.
- **Forbidden:** repeating drops within `kSwitchCooldownMs = 20 000 ms`; double-drop without intervening breakdown.
- **Dwell / debounce:** 250 ms candidate hold (faster lock); cooldown after switch 20 000 ms.
- **Lane D presence:** 44 rows / 6.8 % aggregated CondB. Track 01 (Avicii) + Track 03 (Pjanoo) only. **Not on Track 02 (Sun & Moon breakdown)** — confirms Drop is correctly partitioned by structural intent. FACT.

#### `Breakdown (=6)` — reduced density, exposed centre

- **Entry conditions.** RMS decay + tempo confidence dip; entry from Drop / Dense, not from Build.
- **Visual intent (Lane B band).** Brightness 96-136, speed 12-26, intensity 64-112, complexity 48-96.
- **Director output:** brightness ~116, speed ~19, intensity ~88, saturation ~112, complexity ~72, suspended clean decay.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:47`.
- **Forbidden:** immediate re-Drop (must traverse Build first); aggressive sweep.
- **Dwell / debounce:** standard 400 ms candidate; minimum dwell 8 000 ms; anti-thrash `kAntiThrashWindowMs = 45 000 ms` prevents Breakdown↔Drop ping-pong.
- **Lane D presence:** 24 rows / 3.7 % aggregated CondB. Spread thinly across all three tracks (01B 10, 02B 4, 03B 10). FACT.

#### `Dense (=7)` — high fill with centre-origin structure

- **Entry conditions.** High RMS + high flux + structural-density signal; hysteresis 0.60 sticky / 0.68 cold (per `SynqMatrix.cpp:1085`).
- **Visual intent (Lane B band).** Brightness 152-184, speed 32-50, intensity 152-208, complexity 176-224.
- **Director output:** brightness ~168, speed ~40, intensity ~180, saturation ~144, complexity ~200, layered but legible; cap complexity if FPS / show / health regress.
- **Switch target (Director mode):** row at `SynqMatrix.cpp:48`.
- **Forbidden:** full-frame solid colour; centre-bypassed linear sweeps (centre-origin enforced — see § 5 invariants).
- **Dwell / debounce:** standard 400 ms candidate; minimum dwell 8 000 ms.
- **Lane D presence:** 29 rows / 4.5 % aggregated CondB. **Track 03 (Pjanoo) only.** Confirms Dense fires on continuous high-energy material as expected. FACT.

#### `Transition (=8)` — bridge posture, not a destination

- **Entry conditions.** Used as interstitial during cross-state movement; not a stable destination.
- **Visual intent (Lane B band).** Brightness 112-168, speed 18-36, intensity 80-160, complexity 80-144.
- **Director output:** smoothing / crossfade; temporary; must resolve under dwell / cooldown.
- **Switch target (Director mode):** `EID_LGP_CHROMATIC_PULSE` ("chromatic_transition_pulse" — `SynqMatrix.cpp:49`).
- **minConfidence:** 0.45 (per `SynqMatrix.cpp:49`).
- **Forbidden:** holding Transition as a stable state.
- **Dwell / debounce:** very short — Transition is by design an interstitial.
- **Lane D presence:** **0 rows aggregated CondB.** Never observed. FACT — observe-only with V1 deprecation gate per § 3.3.

### 4.2 Allowed-transitions table

**FACT.** Cross-state movement rules from `SynqMatrix.cpp:1247-1260` (`transitionIsAllowed()`) plus Lane B matrix `:43-53`:

- Ownership precedence applies BEFORE any state transition: Show > Manual > Director (see § 5).
- Centre origin enforced (Hard Constraint from repo CLAUDE.md): all effect renders originate at LED 79 / 80 outward; no linear sweeps.
- No rainbow / full hue-wheel: explicitly forbidden across every state.
- Hue palette: palette 10 (Vintage 01) is the Lane D capture baseline; production default may differ.
- Confidence: low-confidence (`audioConfidence < confidenceFloor`) → hold / coast / fall back to Ambient, never switch.
- Health-degraded (any non-zero showSkips / failures / rmtErrors / underruns): suppress all transitions; surface `SuppressedReason::Health`.
- Anti-thrash 45 000 ms (`kAntiThrashWindowMs`): A→B→A within window is rejected.

Per-state transition rules (Lane B `director_mode_matrix:43-53`, ratified by source):

- **Silence ↔ Ambient ↔ Steady:** allowed in either direction, subject to dwell.
- **Silence → Build:** allowed only with sustained rising RMS / flux.
- **Build → Drop:** allowed only with high audioConfidence + onset event; the canonical transition.
- **Drop → Breakdown:** preferred release path; cooldown ≥ 20 000 ms.
- **Drop → Drop:** forbidden within cooldown (no double-drop).
- **Build → Dense:** allowed; alternative to Drop for sustained high-energy material.
- **Dense → Breakdown:** allowed release.
- **Breakdown → Build:** allowed (re-energising); not direct to Drop.
- **Transition:** entered briefly as bridge; must resolve to another state within dwell.

---

## 5. Ownership and Gating Policy

**Source.** Every value below is cited from `firmware-v3/src/core/synqmatrix/SynqMatrix.cpp` at HEAD `dc46cc9b`. The RFC ratifies the live values.

### 5.1 Ownership precedence (Show > Manual > Director)

**FACT.** `markShowControl()` at `SynqMatrix.cpp:524-526` and `markManualControl()` at `:520-522`. Show owns 1 000 ms (`kShowSuppressMs`) after each effect change; Manual owns 15 000 ms (`kManualSuppressMs`). The Director is gated off whenever either is active.

Suppression reasons surfaced in `SynqMatrixStatus.suppressedReason` (`SynqMatrix.h:41-65`):

- `ShowOwner` (Show is currently active)
- `ManualOwner` (Manual is currently active)
- `Disabled` (synqMatrix.enabled = false)
- `LowConfidence` (`audioConfidence < confidenceFloor`)
- `NoAudio` (`audioAvailable == false`)
- `BootGrace` / `EnableGrace` (within `kBootGraceMs = 3 000 ms` or `kPostEnableGraceMs = 4 000 ms`)
- `MinimumDwell` (within `kMinimumDwellMs = 8 000 ms` post-promotion)
- `SwitchCooldown` (within `kSwitchCooldownMs = 20 000 ms` post-switch)
- `AntiThrash` (would create A→B→A within `kAntiThrashWindowMs = 45 000 ms`)
- `Health` (any non-zero degradation counter)
- `HealthRecovering` (within `kHealthCleanWindowMs = 3 000 ms` of degradation clearing)
- `SwitchingDisabled` (Mode = Assist, no switching permitted)
- `PolicyForbidden` (state's policy bit is disabled in allowlist)
- + 10 additional reasons in the enum

### 5.2 Dwell / cooldown / hysteresis / thrash (ratified values)

| Constant | Value | Source | Purpose |
|---|---|---|---|
| `kSmoothTauSeconds` | 0.18 s | `:16` | Feature smoother tau |
| `kBootGraceMs` | 3 000 ms | `:17` | Director silenced during boot |
| `kPostEnableGraceMs` | 4 000 ms | `:18` | Director silenced after `enabled=true` |
| `kEvaluationPeriodMs` | 500 ms | `:19` | `tick()` rate-limit |
| `kStableStateHoldMs` | 400 ms | `:20` | Candidate-hold for promotion |
| `kDropStateHoldMs` | 250 ms | `:21` | Faster Drop lock when `audioConfidence > 0.60` |
| `kMinimumDwellMs` | 8 000 ms | `:22` | Post-promotion lock-in |
| `kSwitchCooldownMs` | 20 000 ms | `:23` | Director-mode switch cooldown |
| `kSwitchWindowMs` | 60 000 ms | `:24` | Rolling window for rate-limit |
| `kMaxSwitchesPerWindow` | 2 | `:25` | Max switches in window |
| `kAntiThrashWindowMs` | 45 000 ms | `:26` | A→B→A guard |
| `kHealthCleanWindowMs` | 3 000 ms | `:27` | Health-recovery dwell |
| `kManualSuppressMs` | 15 000 ms | `:28` | Manual ownership window |
| `kShowSuppressMs` | 1 000 ms | `:29` | Show ownership window |

Hysteresis thresholds (`classifyState()` at `SynqMatrix.cpp:1050-1111`):

| State | "in-state" threshold (sticky) | "not-yet-in-state" threshold (cold) | Line |
|---|---|---|---|
| Silence | 0.10 | 0.06 | `:1059` |
| Drop | 0.52 | 0.62 | `:1083` |
| Build | 0.32 | 0.38 | `:1084` |
| Dense | 0.60 | 0.68 | `:1085` |

The hysteresis pattern is classical: the threshold to leave a state is lower than the threshold to enter it.

### 5.3 Health-recovery

**FACT.** `healthIsDegraded()` at `SynqMatrix.cpp:1384-1387` is strict: ANY non-zero `showSkips`, `failures`, `rmtErrors`, or `underruns` counter marks the system degraded. Gates:

- While degraded → `SuppressedReason::Health`. Director frozen.
- After counters clear → `SuppressedReason::HealthRecovering` for `kHealthCleanWindowMs = 3 000 ms`. Director still frozen.
- After 3 000 ms of clean counters → normal tick resumes from current candidate.

This is the correct "ratchet" behaviour: any blip back into degraded resets the 3 s clean-window.

### 5.4 Kill-switch / disable behaviour

**FACT.** From `k1_songaware_rollout_safety_2026-05-12.md:33-39`:

- Set `effectiveMode = off`.
- Set `familyMorphing = false`.
- Set `constrainedSwitching = false`.
- Preserve current active effect / manual state.
- **No NVS save.** Disable is volatile by design — reboot returns to pre-disable state unless Captain explicitly re-saves config.

**INFERENCE.** This protects against a deployed kill-switch becoming permanent after a hardware power-cycle; recovery requires explicit re-enable.

---

## 6. Control Schema (synqMatrix.* surfaces)

**Source.** `V1ApiRoutes.cpp:263-380`, `SynqMatrixHandlers.cpp`, `WsSynqMatrixCommands.cpp:418-461+`, `SerialCLI.cpp:727-932`.

### 6.1 REST endpoints

| Method | Path | Handler |
|---|---|---|
| GET | `/api/v1/synqmatrix/config` | `SynqMatrixHandlers.cpp:238` |
| POST | `/api/v1/synqmatrix/config` | `:245` |
| PATCH | `/api/v1/synqmatrix/config` | `:245` (shared handler) |
| GET | `/api/v1/synqmatrix/status` | `:314` |
| GET | `/api/v1/synqmatrix/allowlist` | `:368` |
| PATCH | `/api/v1/synqmatrix/allowlist` | `:375` |
| POST | `/api/v1/synqmatrix/allowlist/reset` | `:403` |

Legacy `/api/v1/songAware/*` aliases coexist for backward compatibility (`V1ApiRoutes.cpp:263` and onward).

### 6.2 WebSocket envelopes

| Envelope type (canonical) | Handler |
|---|---|
| `synqMatrix.config.get` / `.set` | `WsSynqMatrixCommands.cpp:418, :421` |
| `synqMatrix.status` | `:424` |
| `synqMatrix.reset` | `:427` |
| `synqMatrix.restore` | `:430` |
| `synqMatrix.debug` | `:433` |
| `synqMatrix.policy` | `:436` |
| `synqMatrix.allowlist.get` / `.set` / `.reset` | `:439, :442, :445` |
| `synqMatrix.health` | `:448` |
| `synqMatrix.counters.reset` | `:451` |

Legacy `songAware.*` envelope-type aliases reuse the same `Impl` functions with different envelopeType strings (`:457-461+`).

### 6.3 Serial CLI commands

| Command | Action | Line |
|---|---|---|
| `synqmatrix` / `synqmatrix status` | print full status | `SerialCLI.cpp:727` |
| `synqmatrix on` | enable | `:733` |
| `synqmatrix off` | disable | `:747` |
| `synqmatrix mode <off\|assist\|director>` | set mode | `:761` |
| `synqmatrix profile <subtle\|balanced\|high>` | set profile | `:798` |
| `synqmatrix switch on/off` (alias `synqmatrix switching`) | toggle switching gate | `:816, :833` |
| `synqmatrix wipe` / `synqmatrix reset` | full reset | `:845` |
| `synqmatrix restore` | restore from saved runtime state | `:857` |
| `synqmatrix dbg [N]` / `synqmatrix debug` | debug snapshot | `:864` |
| `synqmatrix policy` | print policy table | `:879` |
| `synqmatrix allowlist` | print allowlist | `:885` |
| `synqmatrix allow <state>` | toggle policy bit | `:891` |
| `synqmatrix allow reset` | restore full allowlist | `:919` |
| `synqmatrix health` | health counters | `:926` |
| `synqmatrix counters reset` | reset perf counters | `:932` |

Each command also has a `sa <verb>` short alias.

### 6.4 Public payload schema

**Config (`SynqMatrixConfig` struct at `SynqMatrix.h:142-153`):**

```yaml
enabled: bool
mode: enum (off | assist | director)         # post-rename — no "balanced" mode
profile: enum (subtle | balanced | high)     # orthogonal axis
silencePolicy: enum (hold | fade_to_ambient | manual_hold)
sensitivity: float 0.0-1.0
intensityScalar: float 0.0-1.0
motionScalar: float 0.0-1.0
minDwellMs: uint32                            # ratified at 8 000 ms
switchCooldownMs: uint32                      # ratified at 20 000 ms
confidenceFloor: float 0.0-1.0                # NEW — surfaces Captain numeric (proposed default 0.40)
familyMorphing: bool                          # V0: false; V1 may set true
constrainedSwitching: bool                    # V0: false; V2 may set true
```

**Status (`SynqMatrixStatus` struct at `SynqMatrix.h:155-218`):** ~63 fields including `effectiveMode`, `owner`, `suppressedReason`, `previousSuppressedReason`, `classificationReason`, `rawState`, `previousState`, `currentState`, `candidateState`, `intent`, `actionPlan`, `boundaryGate`, `boundaryReady`, `waitingForBoundary`, `boundaryConfidence`, `confidence`, `selectionScore`, `lastAction`, plus all counters / ms-remainders / health / BPM.

**New telemetry fields (V0 instrumentation per § 3.2):**

```yaml
audioConfidenceBelowFloorMs: uint32       # running counter, last 10s window
missedPredictionCount: uint32             # last 10s window
tempoWinnerChanges: uint32                # last 10s window
```

These are observability-only in V0; they do not gate behaviour.

**Telemetry update cadence.** Full status is published on every WebSocket subscription tick (existing `status.subscribe` cadence, no change). Per-field cadence is determined by the consumer; the Director updates internal snapshot fields each `tick()` (500 ms) plus each `apply()` (per-frame).

---

## 7. Lane D Evidence Summary

**Source.** `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/` — six paired serial JSONLs (three tracks × two conditions) + 1 SESSION_LOG + 1 SESSION_MANIFEST + 1 HANDOVER_BRIEF + 1 SESSION_HANDOVER. Captain sign-off 2026-05-15 UTC+8 18:13 (`SESSION_MANIFEST.md` § Captain sign-off; mirrored to all three per-track manifests + the session handover report).

### 7.1 Aggregate state distribution

**FACT.** Across 1,484 polling rows (740 Condition B + 744 Condition A):

| State | All rows | % | Condition A | % A | Condition B | % B |
|---|---|---|---|---|---|---|
| silence | 790 | 53.2 % | 649 | **87.2 %** | 141 | 19.1 % |
| build | 392 | 26.4 % | 0 | 0.0 % | **392** | 53.0 % |
| unknown | 184 | 12.4 % | 95 | 12.8 % | 89 | 12.0 % |
| drop | 44 | 3.0 % | 0 | 0.0 % | 44 | 5.9 % |
| dense | 29 | 2.0 % | 0 | 0.0 % | 29 | 3.9 % |
| breakdown | 24 | 1.6 % | 0 | 0.0 % | 24 | 3.2 % |
| ambient | 19 | 1.3 % | 0 | 0.0 % | 19 | 2.6 % |
| steady | 2 | 0.1 % | 0 | 0.0 % | 2 | 0.3 % |
| **transition** | **0** | **0.0 %** | 0 | 0.0 % | **0** | **0.0 %** |

**FACT (null control).** Condition A (`mode = off`) produces only `silence` + `unknown`. No state vocabulary fires under Director-off. The Director-off baseline is clean.

### 7.2 `unknown` is polling-protocol jitter, not classifier output

**FACT.** 12 – 14 % of polling rows in **both** Condition A and Condition B have `synqmatrix: {}` (empty dict). Investigation: this occurs when the `synqmatrix status` CLI response and the `dbg status` CLI response fuse on the serial line, and the capture script ends up parsing the SA payload into the `dbg` response field instead of the `synqmatrix` field. The capture script labels these rows as `state = unknown`.

**INFERENCE.** `unknown` rows are missing observations, not Director output. Of the 89 Condition B `unknown` rows, 62 are immediately sandwiched between two identical non-`unknown` neighbours (mean run-length 1.19; max 3). These should be filtered out of any meaningful state-distribution metric.

**Unknown-filtered Condition B (651 valid rows):**

| State | Rows | % |
|---|---|---|
| build | 392 | 60.2 % |
| silence | 141 | 21.7 % |
| drop | 44 | 6.8 % |
| dense | 29 | 4.5 % |
| breakdown | 24 | 3.7 % |
| ambient | 19 | 2.9 % |
| steady | 2 | 0.3 % |
| transition | 0 | 0.0 % |

**ACTION (V0 instrumentation).** Capture-script `synqmatrix status` / `dbg status` parsing should be hardened against fused responses before V1 Lane D Pass C. See § 10 NEW-3 (capture-rig reproducibility gap).

### 7.3 `synqmatrix.confidence` is saturated

**FACT.** Of 651 valid Condition B polling rows, 649 (99.7 %) report `synqmatrix.confidence ≥ 0.7`. Per-state median is exactly 1.000 for every named state. Two outliers at 0.430 (one Build, one Silence) are isolated.

**INFERENCE.** `synqmatrix.confidence` is not behaving as a probabilistic state-classifier certainty. It is either (a) clipped / saturated, (b) reporting something other than per-row classification confidence (long-window accumulator), or (c) defaulting to 1.0 absent a real estimator. The signal carries no information across states. **This is the data-quality flag the Q6 closure addresses:** the coast trigger must operate on `audio.confidence` (audio-pipeline confidence, which Lane D shows varies meaningfully per state — § 3.2 table), not `synqmatrix.confidence`.

### 7.4 BPM stability correlates with track density

**FACT (per-track Condition B BPM):**

| Track | n | mean | stdev | min | max | unique BPMs |
|---|---|---|---|---|---|---|
| 01B (Avicii Levels, build-drop) | 305 | 125.28 | 11.94 | 90.0 | 142.0 | 12 |
| 02B (Sun & Moon, breakdown) | 293 | 126.77 | **16.74** | 81.0 | 142.0 | 8 |
| 03B (Pjanoo, steady) | 142 | 125.25 | **4.15** | 98.0 | 129.0 | **4** |

**INFERENCE.** Track 03 (Pjanoo) locks tightly (stdev 4.15, 80 %+ at 126 BPM — the song's actual tempo). Track 02 (Sun & Moon) shows half-time oscillation (stdev 16.74; p10 at 86 BPM vs p50 at 134 BPM — the BPM tracker oscillates between halved and base tempo during breakdown sections). Track 01 (Levels) sits in between. This is expected behaviour for the ESV11 octave-aware selector under sparse breakdown material (see SSA3 findings on `esv11_pick_top_tempo_bin_octave_aware()` at `tempo.h:372-443`).

### 7.5 Match between chosen vocabulary and observed behaviour

**FACT.**

- Build, Silence, Drop, Dense, Breakdown, Ambient: all fire under track-appropriate conditions with sensible distributions. Vocabulary is well-supported by evidence.
- Drop / Dense / Ambient are correctly track-partitioned (Drop misses the breakdown track; Dense is steady-track-only; Ambient is build-drop-track-concentrated).
- Transition: 0 occurrences. Vocabulary may be dead-weight.
- Steady: 2 occurrences. Vocabulary may be dead-weight.

**INFERENCE.** The 9-state vocabulary is mostly load-bearing on Lane D evidence. Transition and Steady are the only suspect cases; their V0 retention is conditional on V1 broader-corpus testing (§ 3.3).

### 7.6 Lane D Pass A vs Pass B — comparative observation (no interpretation)

The Condition A captures show the Director-off baseline behaviour: clean `silence` + polling-jitter `unknown`. Condition B captures show Director-on parameter mode firing the full vocabulary except Transition. This is the substrate for downstream Gate 2 evaluation. **The RFC does not score the A/B comparison against any external benchmark — that work is Gate 2 evaluation, downstream of RFC sign-off.**

---

## 8. Validation Gates (V0 → V1 → V2)

The 2026-05-12 suite defines a four-gate ladder at `k1_songaware_rollout_safety_2026-05-12.md:86-93`. This RFC adopts the V0 / V1 / V2 naming from Captain's onwards brief, with the following mapping:

| Brief term | Suite gate | What opens | Evidence required |
|---|---|---|---|
| **V0** | Gate 1 + Gate 2 | Parameter mode shipped | Director design readiness + Lane D Pass A/B (Condition A baseline + Condition B parameter mode) passes health / no-degradation + parameter activity |
| **V1** | Gate 3 | Family morphing enabled | V0 already passes + Lane D Pass C (family morphing material fit without chaos or health regression) |
| **V2** | Gate 4 | Constrained switching enabled | V1 already passes + Lane D Pass D (constrained switching wrong-switch ≤ 20 %, stability + health pass, switching strictly beats parameter / family) |

### 8.1 V0 status (this RFC's primary closure)

**FACT.** V0 ships in the current source at HEAD `dc46cc9b`. Lane D Pass A (Condition A, 2026-05-14) provides the baseline; Lane D Pass B (Condition B `mode = assist`, 2026-05-15 retry, Captain sign-off 18:13) provides the parameter-mode evidence. Per the Gate 2 criteria at suite `:86-93`:

- **Health / no-degradation:** Pass B SESSION_LOG.jsonl shows 0 abort events, 0 heartbeat triggers, 0 orphan-cleaned events, 0 smoke-failed events. Per-track manifests confirm all six captures completed full music duration ± 4 s. PASS.
- **Useful parameter activity:** state distribution evidence (§ 7.1) confirms 6 of 8 non-Unknown states fire (Build / Silence / Drop / Dense / Breakdown / Ambient) with track-appropriate distributions. PASS.
- **Director vs baseline behavioural difference:** Pass A null control is clean `silence` only; Pass B shows the full vocabulary firing under matched audio. Distributional separation is unambiguous. PASS.

**V0 promotion criterion: SATISFIED.** This RFC, on Captain sign-off, ratifies V0.

### 8.2 V1 promotion criteria (NOT IN SCOPE FOR THIS RFC)

V1 enables `familyMorphing` (configuration field). Evidence required:

- Lane D Pass C run on a corpus of ≥ 3 tracks (recommendation: extend to ≥ 5 tracks × ≥ 5 min for broader sampling).
- Family map fit per Lane D protocol § 5.3: no chaos transitions, no thrash, health metrics within Pass A range.
- V0-deferred-items revisit:
  - `confidenceFloor` re-calibrated from V0 telemetry composite metrics (§ 3.2).
  - Transition + Steady deprecation evaluated against the broader corpus (§ 3.3).
  - Davies-3-promote / IBT-8-bad-demote semantics at the tempo-tracker layer evaluated (§ 3.1).
  - Audit Row 5 + Row 9 + Row 14 (flywheel coast / re-sync, parameter-mode half) revisited with V1 evidence.

V1 ships under its own subsequent RFC after Lane D Pass C runs.

### 8.3 V2 promotion criteria (NOT IN SCOPE FOR THIS RFC)

V2 enables `constrainedSwitching` (configuration field). Evidence required:

- Lane D Pass D run on the same corpus, with director-mode active.
- Wrong-switch rate ≤ 20 % per suite protocol `:104-112`.
- Stability gate: dwell ≥ 4 s minimum, switch rate ≤ 2 / minute, zero A→B→A within 45 s.
- Health gate: zero non-zero counters across the full Pass D run.
- Switching must strictly beat parameter-mode + family-morph on the same corpus (suite `:113-117`).

V2 ships under its own subsequent RFC after Lane D Pass D runs.

### 8.4 Validation infrastructure (post-Lane-D)

The Lane D capture rig (`/tmp/capture_lane_d.py` HEAD) has been hardened with five permanent fixes + one AVFoundation device-binding correction during the 2026-05-15 retry. This rig is the canonical V1 / V2 capture harness. See `firmware-v3/docs/research/SESSION_HANDOVER_20260514_Lane_D_Evidence.md` § "Permanent script fixes (5 + 1)" for full inventory.

**FACT.** Capture-rig hardening status (already shipped):

1. Session-start ffmpeg smoke gate (`Session.smoke_ffmpeg`).
2. ffmpeg output-file 10 s heartbeat (abort on no-growth).
3. Pre-spawn `pkill -9 -x ffmpeg` + post-stop `proc.poll()` SIGKILL fallback.
4. Pass-boundary AVFoundation release.
5. Spotify play-state heartbeat (existing — confirmed).
6. AVFoundation device binding by name (`"EiP Camera:EiP Microphone"`) — robust to enumeration drift.

V1 Pass C and V2 Pass D inherit this rig.

### 8.5 V1+ follow-up — TempoBank / Music-Timebase Contract Investigation

**Scope.** RFC follow-up only. **NOT a design proposal. NOT an implementation specification. Out of V0 scope.**

**Context.** The 2026-05-16 Synesthesia Authority Audit, together with the AutoBPM forensic clarification, reframes the V1+ architectural question. Every serious reference body that V1+ might draw on hits the same wall:

| Reference body | What it consumes | What K1 V0 currently exposes |
|---|---|---|
| AutoBPM / OctaveScorer | candidate tempo bank, octave scoring, confidence over multiple tempo hypotheses | collapsed single-bin facts |
| Emotiscope Beat Tunnel | 96-bin tempo-resonator field with phase and smoothed intensity per bin | collapsed onset / beat facts |
| Synesthesia confidence shape | richer confidence / beat-state semantics | upstream confidence (saturated; see § 7.3) + SynqMatrix policy layer |

K1 V0 publishes `es_bpm`, `tempoBpm`, `beat_strength`, `tempoConfidence`, `beat_tick`, `downbeat_tick`, `beat_phase01`, plus collapsed onset triggers (`ControlBusFrame` + `MusicalGridSnapshot` per Q10). This surface is sufficient for V0 parameter mode but **not** for the richer-state contracts named in the reference bodies above.

**Investigation question.** Does K1 need an effect-consumable per-bin tempo-phase / tempo-resonator surface before attempting faithful AutoBPM, Emotiscope Beat Tunnel, or Synesthesia-derived behaviour?

**Where the work belongs.** This is a `ControlBus` / `EffectContext` contract question, **not** a SynqMatrix V0 policy question. The TempoBank / Music-Timebase Contract Investigation is a V1+ lane. **Do not design or implement that surface in V0 scope.** V0 ships against the existing surface.

**Reference handling for V1+.** Davies, IBT, MIREX semantics in any V1+ tempo-tracker / contract design cite Tier-2 academic compass-artifact directly per `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61-65`. Synesthesia and AutoBPM inform the design as Tier-1 reference bodies but do not gate it; neither is an implementation authority for SynqMatrix.

---

## 9. Open Questions / Surfaced for Captain (batched, not drip-fed)

This RFC closes Q4 / Q6 / Q8 / Q10 with evidence-grounded recommendations. The following items require Captain confirmation before V0 RFC sign-off. None are blocking; all have defensible defaults if Captain does not override.

### Q9.1 — `confidenceFloor` numeric value (the one numeric Q6 surfaces)

**Status (2026-05-16 reconciliation): ACCEPTED — `confidenceFloor = 0.40`.**

**Default proposal:** `0.40`.
**Range:** 0.30 (loose, accepts marginal audio) to 0.45 (strict, demotes earlier).
**Why default 0.40:** mid-range between the suite-documented "research fallback" (uncalibrated, conservative hold/manual at `rollout_safety:57`) and the Lane D-observed `audio.confidence` distribution per state (§ 3.2 table — most named states sit at mean 0.741 – 0.851, so 0.40 catches the lower-confidence tail without false-firing).

### Q9.2 — Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms (citation reconciliation)

**Status (2026-05-16 reconciliation): ACCEPTED.** V0 = live-debounce ratified. Davies / IBT / MIREX semantics deferred to V1+ tempo-tracker work in `EsBeatClock` and `firmware-v3/src/audio/backends/esv11/vendor/tempo.h`, citing Tier-2 academic compass-artifact directly when V1+ begins. **No Synesthesia authority claim survives this section.**

**FACT (re-attributed 2026-05-16).** Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms inner / MIREX ±70 ms fallback are attributed by `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61-65` to **Tier-2 academic compass-artifact**, NOT to Synesthesia. The 2026-05-16 Synesthesia Authority Audit explicitly refutes Synesthesia-derivation of these constants (audit § 4 ledger rows 3-5; audit § 5 claim C6 status CONTRADICTED).

**RFC observation.** These constants are named in Captain's onwards brief as the lock state machine for V0, but are not present in:

- 2026-05-12 k1_songaware_* research suite (SSA1 confirmed exhaustive read).
- Live `firmware-v3/src/core/synqmatrix/SynqMatrix.{h,cpp}` (SSA3 confirmed exhaustive read).
- ESV11 vendor headers at `firmware-v3/src/audio/backends/esv11/vendor/tempo.h` (SSA3 confirmed).
- `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md` for SynqMatrix V0 (the document attributes them to Tier-2 academic compass-artifact at `:61-65`, not to Synesthesia Tier-1).

**Closed disposition.** V0 = live-debounce ratified (per § 3.1 closure). Davies / IBT / MIREX semantics are V1+ tempo-tracker work; V1+ design cites Tier-2 academic compass-artifact directly per the MusicAware audit attribution. Synesthesia is acknowledged as a Tier-1 reference body in § 9.5 but is not the origin of these specific constants.

### Q9.3 — Architectural data-flow correction

**Status (2026-05-16 reconciliation): ACCEPTED.** § 3.4 + Q9.3 architectural clarification is confirmed as the RFC's authoritative data-flow statement. § 3.4 remains unchanged.

**RFC observation.** Captain's onwards brief states "Director writes to ControlBus; all effects including Kuramoto consume from ControlBus generically with no dedicated contract." The live implementation differs from the literal reading:

- Director writes `SynqMatrixSwitchRequest` (out-param consumed by `RendererActor.cpp:2144`) and mutates `SynqMatrixParams` (consumed by `RendererActor.cpp:2284`).
- Effects do not read a Director-written ControlBus stream. They read `EffectContext::AudioContext` (which carries `MusicalGridSnapshot` + `OnsetContext` + `controlBus`).
- The Director and the effects are parallel readers of audio state via `AudioContext`. The Director's *outputs* are switch-requests + parameter mutations, mediated by `RendererActor`.

**INFERENCE.** The spirit of Captain's framing is correct (no per-effect Director→effect contract — effects do consume audio generically). The literal data path is what § 3.4 documents.

**Closed disposition.** § 3.4 + Q9.3 is the authoritative data-flow statement. Any subsequent proposal for effects to read a Director-written ControlBus stream (parallel-readers → mediated-stream model) would be a V1+ architectural change requiring a separate RFC.

### Q9.4 — V0 deprecation of Transition + Steady states (V1 gate)

**Status (2026-05-16 reconciliation): ACCEPTED.** V1 deprecation gate accepted — combined Transition + Steady < 1 % across ≥ 10 tracks × ≥ 5 min in V1 Pass C → authorised removal in V1 follow-up RFC.

**RFC observation.** Lane D evidence shows Transition 0 / 740 rows and Steady 2 / 740 rows under Condition B. § 3.3 keeps both for V0 with a V1 deprecation gate (combined < 1 % across ≥ 10 tracks × ≥ 5 min in V1 Pass C → remove in V1).

**Closed disposition.** V0 retains the 9-state vocabulary unchanged. V1 Pass C evidence triggers the deprecation gate; removal lands in V1's own RFC, not in any V0 follow-up.

### Q9.5 — Family A vs Family B distinction (citation gap)

**Status (2026-05-16 reconciliation): PROCEED_WITH_DEGRADED_Q9_5.** Captain plan approval 2026-05-16 confirms Q9.5(f) at the shorthand interpretation — "Family B canonical" in the onwards brief = "the Synesthesia RE corpus is one of two Tier-1 reference bodies", NOT a literal source-native Family A/B taxonomy. No further Captain action required for V0 sign-off.

The phrase "reference-grade authoritative for SynqMatrix" is **withdrawn** from this RFC. The six lettered statements below replace the prior framing in full.

**(a) Captain authority ratified.** Captain authority stands over the scope and direction of SynqMatrix V0. The Director's V0 visual-policy matrix, debounce constants, and live state classifier ship as currently implemented at HEAD `dc46cc9b` on `feature/synqmatrix-rename-2026-05-13`.

**(b) Synesthesia acknowledged as a Tier-1 reference body.** Synesthesia is real, dated (8-9 January 2026), durable, reverse-engineered, and lives off-repo at `~/Workspace_Management/Software/Synesthesia/`. `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md` treats it as one of two Tier-1 reference bodies alongside Auto BPM. **Synesthesia is a reference body, not an implementation authority for SynqMatrix.**

**(c) "Reference-grade authoritative" framing withdrawn.** The 2026-05-16 Synesthesia Authority Audit found: zero written Captain decision designating Synesthesia or "Family B" as authoritative; no occurrence of "Family A" or "Family B" strings in the original Synesthesia RE source documents (SSA-4 grep-verified); the only inventoried "Family B" reference is bundling-layer NotebookLM metadata at `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md:64` and the hybrid-beat-tracker MANIFEST.md cross-reference. The chain "Synesthesia → Family B → reference-grade → authoritative for SynqMatrix" collapses after the first link.

**(d) V0 sign-off stands on Lane D evidence and live source — not Synesthesia evidence.** The Synesthesia authority debate does not gate V0. **AutoBPM is acknowledged as a Tier-1 reference body, not current firmware logic — no AutoBPM port is implied or authorised by V0.**

**(e) Davies / IBT / MIREX deferred to V1+, cited from compass-artifact directly.** Davies-3-promote / IBT-8-bad-demote / IBT ±46.4 ms / MIREX ±70 ms semantics are deferred to V1+ work in `EsBeatClock` and the tempo-tracker layer, **NOT in `SynqMatrix.cpp`**. `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md:61-65` attributes these constants to Tier-2 academic compass-artifact, NOT to Synesthesia; V1+ design cites the academic source directly.

**(f) CLOSED by Captain plan approval 2026-05-16.** "Family B canonical" in the onwards brief was shorthand for "the Synesthesia RE corpus is one of two Tier-1 reference bodies" — **not** a literal taxonomy distinguishing two named Synesthesia source-document families. This revised Q9.5 stands as written; V0 ships per § 3.3.

**Default action.** Ship V0 closure exactly as described in § 3.3; fold this revised Q9.5 into the RFC sign-off; open a V1 ticket for tempo-tracker semantics citing Tier-2 academic compass-artifact directly. V1+ TempoBank / Music-Timebase Contract Investigation tracked at § 8.5.

---

## 10. Reconciliation Appendix

### 10.1 MusicAware audit (15-row gap matrix) post-Lane-D status

**Source.** `firmware-v3/docs/MusicAware_Audit_And_Gap_Analysis.md` (audit document, 181 lines, ~2026-05-05 to 2026-05-12 era, branch `feature/synergy-topology-resume-2026-05-05`, commit `627b0b14`).

| Row | Gap | Lane D? | RFC disposition |
|---|---|---|---|
| 1 | Causal onset detection function | N | V0 retains band-ratio live + FFT telemetry split (deferred to V1) |
| 2 | Tempo induction (ACF or comb-bank) | N | V0 binds to ESV11 (canonical per § 3.4); PipelineCore / TempoTracker excluded |
| 3 | Tempo prior (shape, peak, width) | N | V0 accepts ESV11 selection as-is (deferred to V1) |
| 4 | Tempo lock criterion | N | V0 uses live debounce per Q4 closure (§ 3.1); Davies/IBT deferred to V1+ |
| 5 | Beat-phase flywheel | Partial | Pass B silence-heavy Track 02 implicitly exercises coast path; explicit flywheel contract deferred to V1 |
| 6 | Beat tolerance window | N | V0 uses ±0.06 phase fallback per `buildFeatureSnapshot()`; IBT asymmetric tolerance deferred to V1+ |
| 7 | Confidence scoring | N | V0 documents existing field ownership (audio.confidence vs synqmatrix.confidence — see § 7.3); no new fields |
| 8 | Octave-error handling | N | V0 keeps ESV11 alias guards (`esv11_pick_top_tempo_bin_octave_aware()` at `tempo.h:372-443`) |
| 9 | Coast-through-silence | Partial | Q6 closure (§ 3.2) — simple rule + composite telemetry |
| 10 | Re-validation on audio return | N | Q6 closure — same `audioConfidence ≥ floor` for ≥ 500 ms criterion (the only ABSENT row, now closed) |
| 11 | Downbeat detection | N | V0 keeps existing `grid.downbeat_tick`; no reference-backed detector (deferred to V1) |
| 12 | Phrase-grid (8/16/32 bar) | N | V0 keeps TranslationEngine 4/8-bar heuristics; 16/32-bar deferred indefinitely |
| 13 | Structural-section detection | **Closed (Lane D)** | Lane D Pass B confirms classifier produces 6 of 8 non-Unknown states with track-appropriate intent; § 7.5 |
| 14 | Aesthetic intent / Kuramoto handoff | Partial | Pass B `mode = assist` tests parameter-mutation path; dedicated Kuramoto surface deferred to V1 |
| 15 | Master phase API exposure | N → Closed (RFC) | Q10 closure (§ 3.4) |

**Rows fully closed by this RFC + Lane D:** 5 (Rows 10, 13, 4 partial-via-Q4, 6 partial-via-Q4, 15).
**Rows partially closed (V1 evidence required):** 5, 9, 14.
**Rows V0-retained / V1-deferred:** 1, 2, 3, 7, 8, 11, 12.

### 10.2 New gaps surfaced by Lane D (audit was ~2026-05-12 dated, pre-rename)

| ID | Description | RFC disposition |
|---|---|---|
| NEW-1 | SynqMatrix mode-enum coercion path documented but undefined | RFC § 10.4 documents enum coercion table |
| NEW-2 | Mode-flip vs effect-switching separation | RFC § 8 splits V0 (parameter mode) from V1/V2 (switching) |
| NEW-3 | Capture-rig reproducibility hardening | RFC § 8.4 documents 5+1 fixes; future passes inherit `/tmp/capture_lane_d.py` HEAD |
| NEW-4 | Audit-trail directory retention vs rename | RFC § 10.5 documents retention policy |
| NEW-5 (RFC-surfaced) | `synqmatrix.confidence` saturated at 1.0 — data-quality flag | RFC § 7.3 documents; Q6 closure routes coast trigger via `audio.confidence` instead |
| NEW-6 (RFC-surfaced) | Capture-script `unknown` polling artefact (response fusion) | RFC § 7.2 documents; capture-script hardening before V1 Pass C |

### 10.3 Audit-document rename status

**FACT.** The audit document uses SongAware-era terminology throughout (`SongAwareDirector`, `src/core/songaware/`, REST `/api/v1/songAware/`, WS `songAware.*`, class names `SongAware*`). The audit has NOT been refreshed for the rename. This RFC does not rewrite the audit in place; the rename mapping (below) is the bridge.

### 10.4 Mode-enum rename mapping (suite 5-mode → post-rename 3-mode + 3-profile)

| 2026-05-12 suite mode | Post-rename mode | Post-rename profile | Behaviour |
|---|---|---|---|
| `off` | `off` | (any) | Director disabled |
| `subtle` | `assist` | `subtle` | Parameter mode, mild modulation |
| `balanced` | `assist` | `balanced` | Parameter mode, moderate modulation (parser coerces `"balanced"` → `Assist` — `SynqMatrix.cpp:1602`) |
| `high` | `assist` | `high` | Parameter mode, strong modulation |
| `director` | `director` | (any profile) | Full state machine + switching (V2 ships this) |

**FACT.** The post-rename `parseSynqMatrixMode()` parser (`SynqMatrix.cpp:1595-1610`) accepts the legacy strings `"subtle"`, `"balanced"`, `"high"`, `"high_energy"`, `"parameter"`, `"on"`, `"assist"` and folds them all to `SynqMatrixMode::Assist`. Only `"director"` selects switching mode. Profile is a separate axis preserved through the rename.

### 10.5 Directory / artefact retention policy

**FACT.** The Lane D evidence directory `firmware-v3/docs/research/k1_songaware_lane_d_evidence/2026-05-14/` retains its SongAware-era name post-rename for audit-trail continuity. Captain's directive (per session-start anchor #51619).

**Policy.** Existing evidence directories preserve historical names. Future evidence directories created post-rename use SynqMatrix naming.

---

## 11. Captain Sign-off Gate

Following the 2026-05-16 Synesthesia Authority Audit reconciliation (with Captain plan approval of the same date), the sign-off gate items below are closed at the statuses shown:

1. **Q4 closure ACCEPTED** — TempoObservation = 192 ms ESV11 bank refresh; V0 = live debounce; Davies/IBT/MIREX deferred to V1+ tempo-tracker work citing Tier-2 academic compass-artifact directly.
2. **Q6 closure ACCEPTED** + **Q9.1 ACCEPTED** — `confidenceFloor = 0.40` (per Q9.1).
3. **Q8 closure ACCEPTED** — KEEP 9 states for V0; Transition + Steady V0-observe-only with V1 deprecation gate per Q9.4.
4. **Q10 closure ACCEPTED** — canonical `EffectContext::AudioContext` + `MusicalGridSnapshot` + `EsBeatClock`; legacy mirrors retained.
5. **Q9.2 ACCEPTED** — Davies / IBT / MIREX re-attributed to Tier-2 academic compass-artifact per `MusicAware_Audit_And_Gap_Analysis.md:61-65`; deferred to V1+ tempo-tracker work; Synesthesia authority claim withdrawn.
6. **Q9.3 ACCEPTED** — architectural data-flow per § 3.4 confirmed as authoritative.
7. **Q9.4 ACCEPTED** — V1 deprecation gate for Transition + Steady (< 1 % across ≥ 10 tracks × ≥ 5 min in V1 Pass C → removal authorised in V1 follow-up RFC).
8. **Q9.5 PROCEED_WITH_DEGRADED_Q9_5** — "reference-grade authoritative" withdrawn; Synesthesia + AutoBPM acknowledged as Tier-1 reference bodies, not implementation authorities; Q9.5(f) closed at shorthand interpretation per Captain plan approval 2026-05-16.

Captain sign-off line should be appended at the top of the document changelog when satisfied. The next implementation agent's brief will be authored against this RFC.

---

## 12. Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-05-15 | agent:claude (opus-4.7) | Created RFC consolidating Lane D evidence (Captain sign-off 18:13) + 2026-05-12 k1_songaware_* research suite (9 files) + live SynqMatrix source at HEAD `dc46cc9b` + ESV11 vendor tempo path + MusicAware audit (15-row gap matrix). Closes Q4 (TempoObservation = 192 ms ESV11 bank refresh), Q6 (coast = simple `audioConfidence < confidenceFloor` ≥ 1 000 ms; composite metrics as telemetry; numeric surfaced to Captain default 0.40), Q8 (KEEP 9-state vocabulary; Transition + Steady V0-observe-only with V1 deprecation gate), Q10 (canonical = EffectContext::AudioContext + MusicalGridSnapshot + EsBeatClock; ControlBusFrame tempo*/es_* legacy mirrors). Documents state-to-visual-intent matrix, ownership and gating policy (ratified live values), control schema (REST + WS + Serial), Lane D evidence summary (unknown-filtered distributions, BPM stability per track, data-quality flags on `synqmatrix.confidence`), and V0 → V1 → V2 validation gates mapped to suite Gates 2 / 3 / 4. Reconciles MusicAware audit (15 rows re-statused: 5 closed, 5 partial, 7 V1-deferred). Documents SongAware → SynqMatrix rename mapping (5-mode suite → 3-mode + 3-profile factorisation). Batches five Q-decisions for Captain confirmation (Q9.1–Q9.5). RBDO: DEGRADED-MODE for Davies / IBT / MIREX framing (citation gap — Captain authority outside inventoried sources); GROUNDED for everything else. |
| 2026-05-16 | agent:claude (opus-4.7) | Reconciliation pass against Synesthesia Authority Audit 2026-05-16 (`firmware-v3/docs/research/SynqMatrix_Synesthesia_Authority_Audit_2026-05-16.md`, AMBER verdict). 10 edits applied: frontmatter rbdo split, § 3.1 Q4 RBDO re-attribution, new § 8.5 TempoBank / Music-Timebase Contract Investigation V1+ follow-up, Q9.1 ACCEPTED, Q9.2 ACCEPTED (Davies/IBT/MIREX re-attributed to Tier-2 academic compass-artifact per `MusicAware_Audit_And_Gap_Analysis.md:61-65`), Q9.3 ACCEPTED (§ 3.4 confirmed), Q9.4 ACCEPTED (V1 deprecation gate), Q9.5 PROCEED_WITH_DEGRADED_Q9_5 ("reference-grade authoritative" withdrawn; Q9.5(f) closed at shorthand interpretation per Captain plan approval), § 11 sign-off gate updated. § 3.4 NOT edited (already factually accurate). Zero firmware source files modified. RBDO: GROUNDED for V0 closure and Q9.1–Q9.4 reconciliation; DEGRADED_Q9_5 for Synesthesia / Family-B authority framing. |

---

**Captain sign-off:** Q9.1 – Q9.4 ACCEPTED + Q9.5 PROCEED_WITH_DEGRADED_Q9_5 (shorthand interpretation) via Captain plan approval 2026-05-16. Physical sign-off line awaiting Captain's mark at his discretion.
