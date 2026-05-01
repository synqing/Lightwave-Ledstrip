---
abstract: "M2 ESV11 32 kHz pass/fail thresholds per signal class with operational definitions, baseline measurements from the 2026-04-27 dry run, and threshold-vs-firmware triage notes. Read before grading any M2 measurement run or before proposing changes to the ESV11 tempo selector based on M2 results."
---

# M2 — Expected results and pass/fail thresholds

Operational thresholds for the five adversarial signal classes defined in
`README.md`. Each rule has:

- a hard threshold (the gate),
- an operational definition (how the threshold is computed from the per-frame
  trajectory),
- a baseline measurement (what the current ESV11 32 kHz path actually does),
- a triage note (whether failure-against-threshold is a firmware finding or a
  threshold/rule artefact, in cases where this is currently ambiguous).

Source for the thresholds: `Topology_Reconciliation.md` §6 item 4 / Pass 4 §1
A-04 (ESV11 beat-tracking adversarial test set).

## 1. Ambient (Brian Eno class)

**Threshold.** `tempo_confidence < 0.3` in >= 90% of frames over a 60 s pink-noise drone.

**Operational definition.** Across all `frames[].conf` from the JSON trajectory,
count the fraction below 0.3. Pass requires `pct_low_conf >= 0.9`.

**Why this rule.** Ambient music has no metrical pulse. The correct ESV11
behaviour is to report low confidence — visual layers driven by `tempoConfidence`
should fade away rather than locking onto noise.

**Baseline (2026-04-27).** `pct_low_conf = 0.9996`, mean confidence 0.085. PASS.

## 2. Rubato (live recording class)

**Threshold.** Re-acquire correct phase within <= 4 bars of each tempo change.

**Operational definition.** "Re-acquire" = the dominant tempo bin lands within
+/- 5 BPM of the new target AND stays there for >= 8 consecutive bars (32 beats)
without dropping below `conf >= 0.1`. The 4-bar limit is computed against the
target BPM (e.g. at 80 BPM, 4 bars = 12.0 s).

The signal contains two tempo segments:
1. 0-30 s: 120 BPM linearly drifts to 80 BPM
2. 30-60 s: 80 BPM linearly drifts to 110 BPM

**Why this rule.** Live performance contains tempo rubato. The runtime must
re-lock without flickering between bins.

**Baseline (2026-04-27).** Both segments report `reacquire_s: None` (never
satisfies the 8-bar hold). FAIL against the literal threshold. **Triage: rule
caveat.** The rubato signal is a *continuously drifting* click track, so any
"hold for 8 bars at the target BPM" rule will fail by construction — the BPM
is moving the entire time. The rule needs one of two refinements before it's
firmware-actionable:
- **Refinement A:** evaluate against the *instantaneous* target BPM at each
  frame (track the ground-truth tempo curve).
- **Refinement B:** generate a stepped rubato signal (constant 120 BPM for
  10 s, then constant 80 BPM for 10 s, etc.) where the "hold for 8 bars" rule
  is well-defined.
- **Recommendation:** ship Refinement B as the primary fixture (`rubato_steps_60s.wav`),
  keep the drifting variant (`rubato_drift_60s.wav`) for visualisation. Captain
  to ratify before the next M2 run.

## 3. Heavy syncopation (Aphex Twin class)

**Threshold.** Phase coherence >= 75% over the run.

**Operational definition.** Count the fraction of consecutive frame pairs where:
- the dominant tempo bin did not change between the two frames, AND
- both frames have `conf >= 0.1`, AND
- the observed phase advance (modulo 1) is within +/- 10% of the expected
  step `step_expected = (top_bpm / 60) / REFERENCE_FPS`.

Pass requires `pct_coherent >= 0.75`.

**Why this rule.** Heavy syncopation must NOT cause phase tracking to glitch
between bins. The metric measures whether phase advances smoothly at the
locked tempo even when the audio surface is noisy.

**Baseline (2026-04-27).** `pct_coherent = 0.003`. FAIL against the literal
threshold. **Triage: likely measurement-method artefact, not a firmware
regression.** The expected-step formula assumes the per-bin phase advance is
`(top_bpm/60) / REFERENCE_FPS` per frame. Inspection of `vendor/tempo.h:269`
shows the actual advance is `phase_radians_per_reference_frame * delta` where
`delta` is computed from elapsed-microseconds-since-last-tick over the
ideal-100-Hz interval; it is not literally `bpm/60/fps`. The harness needs
to compute expected-step using the same formula the firmware uses (read
`tempi[top_bin].phase_radians_per_reference_frame` and compute step from
`delta`). Refinement plan:
- **Refinement A:** extend the harness to log `tempi[top_bin].phase_radians_per_reference_frame`
  per frame, then have the Python rule use `(per_ref_per_frame * delta) / (2*pi)`
  as the expected normalised step.
- **Refinement B:** replace the strict `+/- 10%` band with a wider tolerance
  (`+/- 30%`) and re-baseline.

Captain to ratify which refinement to apply before treating the syncopation
metric as a launch gate.

## 4. Sub-bass drone

**Threshold.** 0 false beats reported over 30 s of pure 40 Hz sine.

**Operational definition.** Count `tempi[top_bin].beat` zero-crossings
(negative -> positive). Pass requires `beat_tick_count == 0`. Beat-tick
counting EXCLUDES frames where the dominant bin changed between consecutive
frames (those phase discontinuities are not beats).

**Why this rule.** A sub-bass drone has no transients. The novelty curve
should be flat; the tempo Goertzel bank should report essentially zero
energy in every bin; tempo confidence should stay low. The runtime should
emit zero beat-tick events.

**Baseline (2026-04-27).** `beat_tick_count = 51`, BUT mean confidence is
0.078 and only 6 of 7500 frames cross 0.3. FAIL against the literal threshold.
**Triage: threshold needs gating on confidence.** The current rule counts
zero-crossings of `tempi[top_bin].beat` regardless of how confident the
runtime is in that bin. In practice the live system does not act on those
ticks because downstream consumers gate on `tempo_confidence`. The rule
should mirror that gating.

**Recommended refinement (Captain to ratify):**

> Pass requires `beat_tick_count_at_conf_geq_0.3 == 0`, i.e. count only beat
> ticks where the frame at the time of the tick had `conf >= 0.3`. The
> live-runtime gate is `0.30` (verified in
> `firmware-v3/src/audio/PipelineAdapter.cpp` and the contracts that consume
> `ControlBus.tempoConfidence`). Captain should confirm the threshold
> matches the runtime consumer before locking it in.

If the refined rule still fails, that is a real firmware finding — the
Goertzel bank or the `pick_top_tempo_bin_octave_aware` selector is reporting
spurious ticks at low confidence under DC-blocked sub-bass input.

## 5. Sine-wave-only

**Threshold.** `tempo_confidence < 0.2` in >= 95% of frames over 30 s of pure
1 kHz sine.

**Operational definition.** Same as Ambient class but with stricter thresholds
(0.2 instead of 0.3, 95% instead of 90%).

**Why this rule.** A pure tone has no transients and no spectral envelope
variation. ESV11 must report essentially zero tempo confidence — anything
above 0.2 means the tempo Goertzel is responding to numerical noise or
DC-blocker transient artefacts rather than music.

**Baseline (2026-04-27).** `pct_low_conf = 0.999`, mean confidence 0.10.
PASS.

## Summary table — baseline 2026-04-27

| Signal | Threshold | Baseline | Outcome | Triage |
|---|---|---|---|---|
| Ambient | conf<0.3 in >=90% | 99.96% | PASS | clean |
| Rubato | reacquire <=4 bars | never reacquires | FAIL | rule caveat (drifting signal) |
| Syncopation | coherence >=75% | 0.3% | FAIL | rule formula mismatch |
| Subbass | 0 beat ticks | 51 ticks | FAIL | rule needs conf-gating |
| Sine 1 kHz | conf<0.2 in >=95% | 99.85% | PASS | clean |

## Baseline observations and threshold caveats

The 2026-04-27 dry run shows **two clean PASSes (Ambient, Sine)** and **three
rule-related FAILs (Rubato, Syncopation, Subbass)**. None of the failures
are believed to be firmware regressions; all three are pre-launch
threshold-tuning items.

**Recommended next step before declaring M2 a launch gate:**

1. Captain to ratify the three refinements above (stepped-rubato fixture,
   per-bin phase-step formula in coherence rule, conf-gated subbass rule).
2. Re-baseline with the refined rules.
3. If any class still fails after refinement, that result IS a firmware finding
   and should be filed as an ESV11 regression item before Phase 2 doctrine
   ratifies the tempo selector.

## Future work — folding into `pio test`

If Captain later wants the M2 sweep to run as part of the regular `pio test`
gate (rather than via stand-alone Python orchestration), the minimal recipe is:

1. Add a new env to `platformio.ini` mirroring `[env:native_test_esv11_music_corpus_32khz]`
   but with `test_filter = test_esv11_adversarial`.
2. Add `firmware-v3/test/test_esv11_adversarial/test_esv11_adversarial.cpp` —
   a Unity test that loops over the same 5 WAVs in
   `tools/m2_adversarial/signals/`, calls the same `runEsv11()` driver,
   and TEST_ASSERTs against the thresholds documented above.
3. Move `m2_harness.cpp`'s rule-evaluation into a small library
   `tools/m2_adversarial/scripts/m2_rules.h` so both the Python orchestrator
   and the new Unity test can call the same predicates.

Estimated effort: half a day. Out of scope for the M2 measurement campaign
itself; appropriate as a follow-up once Captain has ratified the thresholds.

## Document Changelog

| Date | Author | Change |
|------|--------|--------|
| 2026-04-27 | agent:ssa-4 | Created. Documented per-class thresholds + 2026-04-27 baseline measurements + threshold-vs-firmware triage. |
