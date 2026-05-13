# K1 Song-Aware Validation Protocol Lane D Runlog - 2026-05-12

RBDO label: GROUNDED.

## Evidence Status

Live comparison run executed: **NO**.

No hardware commands were run for this handoff. This runlog is a PASS/FAIL template and evidence-status record. It must not be used to claim visual sign-off or runtime improvement.

Required Lane D evidence set from protocol (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:16-23`):

| Evidence file | Status |
|---|---|
| `k1_songaware_protocol_precheck_2026-05-12_serial.md` | MISSING |
| `k1_songaware_observation_<track>_<date>_serial.md` | MISSING |
| `k1_songaware_transition_matrix_<date>_analysis.md` | MISSING |
| `k1_songaware_stability_metrics_<date>.md` | MISSING |
| `k1_songaware_userfit_table_<date>.md` | MISSING |

## Fixed Controls

Required runtime controls for any future capture:

| Control | Value |
|---|---|
| brightness | 160 |
| speed | 27 |
| intensity | 128 |
| saturation | 128 |
| complexity | 128 |
| variation | 0 |
| palette | 10 / Vintage 01 |
| EdgeMixer | MIRROR |

These are runtime setters only. Do not save to NVS (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:24-36`).

## Conditions

| ID | Condition | Flags | Expected movement | Gate |
|---|---|---|---|---|
| A | Baseline fixed effect | `songAware.mode=off`; `familyMorphing=off`; `constrainedSwitching=off` | No automatic effect-ID switching. | Anchor baseline. |
| B | Song-aware parameter mode | `songAware.mode=on`; `familyMorphing=off`; `constrainedSwitching=off` | No automatic effect-ID switching; parameter adaptation allowed. | Must beat or equal A without health regression. |
| C | Family morphing | `songAware.mode=on`; `familyMorphing=on`; `constrainedSwitching=off` | Family-level movement only inside allowed map. | Must beat or equal B without health or stability regression. |
| D | Constrained switching | `songAware.mode=on`; `familyMorphing=on`; `constrainedSwitching=on` | Effect-ID movement constrained by family map, cooldown, and hold. | Run only if runtime support is proven; otherwise NOT RUN / NOT ENABLED. |

The protocol explicitly says D is not a failure if unavailable; mark D as not run when constrained switching is not enabled in runtime (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:58`, `:158-171`).

## Per-Track Capture Template

```text
Title:
Protocol: K1 Song-Aware Lane D
Lane: D
Session date:
Track:
Track duration:
Ground-truth segment CSV:
Build env:
Baseline commit:
Candidate commit:
Device:
MAC:
Port:
Fixed controls:
Run condition: A / B / C / D
Constrained switching support evidence:
Pre-state checks:
  vp stack:
  s:
  dbg memory:
  adbg status:
  dbg status:
  edbg status:
Poll interval: 1 Hz minimum
Observation rows:
End-state checks:
No-degradation result:
Health result:
Decision:
Captain follow-up:
```

## Timestamped Sample Row Format

`ts | condition | command | response_excerpt | active_effect_id | active_effect_name | track_t | segment_label | vp_metrics_json | s_metrics_json | audio_metrics_json | heap_metrics_json | timing_metrics_json | notes`

This matches the protocol capture format (`firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:173-224`).

## PASS/FAIL Run Matrix

| Track | Condition | A-vs-baseline | Total switches | wrong_switch_rate | switch_rate_per_min | min_dwell_sec | thrash_count | stability_score | show_skips | failures | rmt_errors | underruns | auto_param_changes/min | fps | frame_time_p99_us | show_time_p99_us | no_degradation | decision |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| NOT RUN | A | NOT RUN | 0 | 0.000 | 0.00 | n/a | 0 | n/a | n/a | n/a | n/a | n/a | 0.0 | n/a | n/a | n/a | NOT RUN | NO CLAIM |
| NOT RUN | B | NOT RUN | 0 | 0.000 | 0.00 | n/a | 0 | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | NOT RUN | NO CLAIM |
| NOT RUN | C | NOT RUN | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | NOT RUN | NO CLAIM |
| NOT RUN | D | NOT RUN | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | NOT RUN | NOT ENABLED / NO CLAIM |

## Gate Definitions

| Gate | PASS rule | Evidence |
|---|---|---|
| Wrong switch rate | A/B automatic switches must be zero; C/D wrong-switch rate <= 0.20. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:94-103` |
| Stability | `switch_rate_per_min <= 2.0`, `min_dwell_sec >= 4`, `thrash_count == 0`. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:104-112` |
| Runtime health | `show_skips==0`, `failures==0`, `RMT errors==0`, `underruns==0`. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:113-117` |
| Parameter activity | B/C/D must show automatic parameter activity in at least 50% of non-silent windows if claimed adaptive. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:118-126` |
| No degradation | Each B/C/D metric must be equal or better than A according to metric direction. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:127-147` |
| User-visible fit | Blind review is retained for Captain review but is not the primary health gate. | `firmware-v3/docs/research/k1_songaware_validation_protocol_laneD_2026-05-12.md:148-157` |

## Current Lane D Decision

- True song-aware selection proven: **NO**.
- Parameter mode preferred: **NOT PROVEN BY LIVE RUN**, but remains the only supportable next validation condition.
- Family morphing: **NOT RUN**.
- Constrained switching: **NOT RUN / NOT ENABLED**.
- Final claim: **NO RUNTIME CLAIM**.
