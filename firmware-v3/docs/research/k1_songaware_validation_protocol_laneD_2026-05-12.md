# Lane D — Song-Aware Validation Protocol

**Target decision:** prove or negate whether K1 is selecting effects in response to song structure beyond standard parameter adaptation.

## 0) Scope note

Keep the evidence set and fixed runtime controls unchanged. Extend the protocol to compare:

1. Baseline fixed effect
2. Song-aware parameter mode
3. Family morphing
4. Constrained switching (only if constrained switching is enabled in runtime)

Use this as a comparison protocol only; no firmware edits, no timing work, no medium-layer changes.

## 1) Required evidence set

- `firmware-v3/docs/research/k1_songaware_protocol_precheck_2026-05-12_serial.md`
- `firmware-v3/docs/research/k1_songaware_observation_<track>_<date>_serial.md` (one per music track)
- `firmware-v3/docs/research/k1_songaware_transition_matrix_<date>_analysis.md`
- `firmware-v3/docs/research/k1_songaware_stability_metrics_<date>.md`
- `firmware-v3/docs/research/k1_songaware_userfit_table_<date>.md`

## 2) Fixed controls

- brightness 160
- speed 27
- intensity 128
- saturation 128
- complexity 128
- variation 0
- palette 10 / Vintage 01
- EdgeMixer MIRROR

All controls are runtime setters only (no NVS writes).

## 3) Test design (single-pass, evidence-only)

- Build a ground-truth segment CSV for each track:
  - columns: `track_id, t_start_s, t_end_s, expected_state`  
    (`ambient, build, drop, breakdown, dense, melody, silence`, etc.)
- Run 3 tracks minimum, each 2–4 minutes.
- Comparison order is fixed:
  1. baseline fixed effect
  2. song-aware parameter mode
  3. family morphing
  4. constrained switching (if the flag is enabled)

### 3.1 Protocol comparison table

| ID | Condition | Core flags | Expected effect movement | Comparison target |
|---|---|---|---|---|
| A | Baseline fixed effect | `songAware.mode=off`; `songAware.familyMorphing=off`; `songAware.constrainedSwitching=off` | No automatic effect-ID switching | Anchor baseline |
| B | Song-aware parameter mode | `songAware.mode=on`; `songAware.familyMorphing=off`; `songAware.constrainedSwitching=off` | No automatic effect-ID switching; parameter adaptation allowed | A baseline comparison |
| C | Family morphing | `songAware.mode=on`; `songAware.familyMorphing=on`; `songAware.constrainedSwitching=off` | Family-level movement inside allowed map only | A baseline + B comparison |
| D | Constrained switching | `songAware.mode=on`; `songAware.familyMorphing=on`; `songAware.constrainedSwitching=on` | Effect-ID movement constrained by family map, cooldown, hold | A baseline + B + C comparison |

If constrained switching is unavailable, run A/B/C only and mark D as "not run / not enabled".

## 4) Per-track capture workflow

At each run:
1. Capture pre-state:
   - `vp stack`, `s`, `dbg memory`, `adbg status`, `dbg status`, `edbg status` (if available)
2. Set fixed controls and select the run condition.
3. Run steady for the full track.
4. Poll 1 Hz (minimum) with timestamp:
   - `vp stack`, `s`, `adbg status`, `dbg status`
5. Record all effect transitions and runtime control-change events.
6. End-state capture: same commands as pre-state.

## 5) Telemetry fields to extract

- From `vp stack`:
  - effect id/name, authored/correction/output surface, mismatch
  - colour-correction toggle+skip
  - tone-map status
  - silence-policy active/bypass/hard_gate/silent_scale
  - EdgeMixer mode/spread/strength/spatial/temporal
  - `songAware` flags in force
- From `s`:
  - effect id/name, fps, frame time avg/min/max, drops
  - frame timing, led show time, stack watermark
  - show skips/failures/RMT errors/underruns (or absence thereof)
- From `dbg memory`:
  - free/min free heap
- From audio:
  - RMS/flux/onset/BPM/confidence to align with music events
- From show/command surface:
  - mode flags and whether switching constraints are active

## 6) Metrics and pass/fail

### A) Wrong switch rate (objective)

- Define `expected_state(t)` from segment annotation.
- Define `family(effect_id)` from the internal mapping.
- A transition is a wrong switch if automatic and `family(effect_id)` is not allowed for `expected_state(t)`.
- `wrong_switch_rate = wrong_switches / total_automatic_switches`.
- Pass expectations:
  - A/B: automatic switches must be zero.
  - C/D: `wrong_switch_rate <= 0.20`.

### B) Stability

- Compute:
  - `switch_rate_per_min = automatic_switches / run_minutes`
  - `min_dwell_sec` = minimum time between automatic switches
  - `thrash_count` = switches back-and-forth between same pair in 10 seconds
  - `stability_score = 1 if min_dwell_sec >= 4 and thrash_count == 0 else 0`
- Fail if `switch_rate_per_min > 2.0` or `stability_score == 0` when switching is permitted.

### C) Runtime health (hard stop)

- Pass: `show_skips==0`, `failures==0`, `RMT errors==0`, `underruns==0`.
- Fail: any non-zero value in either condition of a tracked metric.

### D) Parameter activity

- Compute:
  - `auto_param_changes_per_min` (automatic global + effect-parameter value changes)
  - `manual_param_change_count` (manual command count)
- Gate:
  - B/C/D only: if claimed as adaptive, `auto_param_changes_per_min` must be > 0 in at least 50% of non-silent windows.
- No-change here is a fail only for modes claiming adaptation.

### E) Comparison-everything-given-no-degradation (anchor A)

Use Condition A as the anchor for every other condition.

For each condition `m ∈ {B,C,D}` and each metric below, calculate delta versus baseline (`m - A`):

| Metric direction | Baseline rule | Gate for condition m |
|---|---:|---|
| Lower is better | smaller is better | `metric_m <= metric_A` |
| Higher is better | larger is better | `metric_m >= metric_A` |
| Binary | success states | no movement from `1 -> 0` |

Metrics in scope:
- Runtime health counters: show_skips, failures, RMT errors, underruns
- Frame timing: fps, frame_time_{avg,min,max}
- Show timing: show_time_p50/p95/p99
- Stability: wrong_switch_rate, switch_rate_per_min, min_dwell_sec, thrash_count, stability_score
- Parameter activity where mode expects adaptation (B/C/D)

If any condition fails this rule, mark it **FAIL** even if other metrics pass.

### F) User-visible fit (retained for blind review)

- Per track and segment:
  - transition deliberate? (Y/N)
  - family match intent? (Y/N)
  - perceptual discontinuity/jarring? (Y/N)
  - motion clarity (1–5)
  - overall fit (1–5)
- This is not the primary gate; it is evidence for Captain review and traceability.

## 7) Decision rule (final)

- **True song-aware selection proven** only if:
  - D is run and C/D both pass
  - wrong-switch and stability pass for switching conditions
  - all no-degradation checks pass
  - health gates pass
  - user-visible fit has no clear regressions versus A
- **Parameter mode preferred outcome** if:
  - B passes all health and no-degradation checks
  - B has no automatic effect-ID switching
  - C/D either not available or fail without improvement
- **Constrained switching disabled/absent:** D is marked "not run". Do not fail the protocol on D.
- **Negated / no claim** if any hard stop or no-degradation check fails for a running condition.

## 8) Command/telemetry capture standard format

### 8.1 Per timestamped sample

`ts | condition | command | response_excerpt | active_effect_id | active_effect_name | track_t | segment_label | vp_metrics_json | s_metrics_json | audio_metrics_json | heap_metrics_json | timing_metrics_json | notes`

### 8.2 Exact report format

Use one report file per run and one session summary file.

```text
Title:
Protocol:
Lane:
Session date:
Track:
Track duration:
Build env:
Baseline commit:
Candidate commit:
Device:
MAC:
Port:
Fixed controls:
Run condition:
Constrained switching:
Song-aware flags:
Pre-state checks:
  vp stack:
  s:
  dbg memory:
  debug:
Poll interval:
Evidence files:
  precheck:
  observation:
  transition_matrix:
  stability_metrics:
  userfit:
Run table:
  (append 9.1 row)
No-degradation result:
  condition:
  status:
Health result:
  show_skips:
  failures:
  rmt_errors:
  underruns:
Decision:
Captain follow-up:
```

### 9.1 Run matrix template (append once per condition)

| Track | Condition | A vs Baseline status | Total switches | wrong_switch_rate | switch_rate_per_min | min_dwell_sec | thrash_count | stability_score | show_skips | failures | rmt_errors | underruns | auto_param_changes/min | fps | frame_time_p99_us | show_time_p99_us | no_degradation | decision |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| `track_id` | A/B/C/D | PASS/FAIL | integer | 0.000 | 0.00 | 0.0 | 0 | 1/0 | 0 | 0 | 0 | 0 | 0.0 | 120.0 | 4500 | 4700 | PASS/FAIL | PASS/FAIL |

## 9) Notes

- Keep blind review fields separate from parser output.
- Mark sample invalid only for transport faults (missed poll windows, command retries, serial timeout).
- Do not change production defaults or effect IDs outside controlled runtime setters.
