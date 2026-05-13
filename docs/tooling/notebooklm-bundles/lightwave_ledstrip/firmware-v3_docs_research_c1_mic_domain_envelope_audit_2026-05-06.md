# C-1 Mic-Domain Envelope Audit

**Date:** 2026-05-06
**RBDO label:** GROUNDED
**Scope:** Audit whether `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md` already resolves `BACKLOG.md` C-1.
**Verdict:** It does not resolve C-1. It gives partial trace evidence, but not a stable tuned microphone-domain operating envelope.

## Question

`BACKLOG.md` C-1 asks:

> What mic-domain RMS / peak / silentScale-trip range was the firmware tuned against?

The revisit trigger allowed this audit to close the row only if the 2026-04-27 AFS v2 baseline already documented that envelope.

## Source Read

| Source | Finding |
|---|---|
| `BACKLOG.md:11-16` | C-1 blocks LUFS targets, AFS v2 silentScale validation, and tuned-regime sign-off claims. |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md:3-7` | The baseline is explicitly a failed/waived Phase 1B foundation report, not a passing calibration result. |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md:238` | One dense/bass asset recorded `audio_rms` p50/p95/max of `6250 / 9547 / 10000`. |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md:278` | One cymbal/hat asset recorded `audio_rms` p50/p95/max of `0 / 8569.4 / 9196`. |
| `firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md:325-347` | The baseline still failed timing/memory gates and explicitly says semantic expansion was not cleared. |
| `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/README.md:21-36` | The linked evidence has eight approved-corpus captures plus silence. |
| `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/README.md:73-86` | The runtime gate still failed; evidence must not be treated as a pass result. |
| `firmware-v3/src/audio/AudioActor.cpp:805-821` | `rawHopRms` is computed directly from the production ESV11 sample history before AGC-derived trigger logic. |
| `firmware-v3/src/audio/AudioActor.cpp:930-940` | `br_raw_rms` is trace-emitted as `rawHopRms * 1000000`. |
| `firmware-v3/src/audio/AudioActor.h:700-711` | The source contains an older K1v2-tuned raw RMS comment: silence p50 `0.003`, p95 `0.006`, max `0.009`; music p5 `0.015`, p50 `0.066`; BR gate `0.007`. |
| `firmware-v3/src/audio/AudioActor.cpp:616-627` | Production ESV11 silence gating uses threshold `0.005` and hysteresis `150 ms` unless `AUDIO_SILENCE_GATE_DISABLED` is defined. |
| `firmware-v3/src/audio/contracts/ControlBus.cpp:751-785` | `silentScale` trips after `rmsUngated < threshold` for the hysteresis period, then fades toward 0 with tau `0.19 s`. |
| `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:68-71` | `frame.rms` is not raw microphone RMS; it is `sqrt(es.vu_level) * 1.25` clipped to `[0,1]`. |
| `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp:237-285` | Waveform peak parity is a Sensory Bridge follower with a `750` sweet-spot subtraction, not a documented mic-domain peak envelope. |

## Extracted Evidence

Values below are from the committed `*.report.json` summaries under `firmware-v3/docs/research/phase1b_runtime_evidence_2026-04-27/`.

`br_raw_rms` values are divided by `1,000,000` because `AudioActor.cpp` emits `rawHopRms * 1000000`.

| Evidence set | Scenario | Raw hop RMS p50 | Raw hop RMS p95 | Raw hop RMS max | `frame.rms` p50 | `frame.rms` p95 | silentScale p50 | silentScale p95 | Read |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| original `reports/` | `silence_idle` | 0.003022 | 0.005119 | 0.005944 | 0.0000 | 0.0000 | 0.347 | 0.753 | Idle raw RMS sits near the source-comment silence envelope; silentScale is already fading. |
| original `reports/` | lowest music p50 (`satie_sparse_slow`) | 0.011894 | 0.022944 | 0.025478 | 0.6456 | 1.0000 | 0.999 | 0.999 | Approved music sits above the BR gate and keeps silentScale open. |
| original `reports/` | highest music p50 (`portishead_trip_hop_sparse`) | 0.068722 | 0.097541 | 0.122832 | 1.0000 | 1.0000 | 0.999 | 0.999 | Perceptual `frame.rms` saturates, so it cannot be the sole mic envelope. |
| lean trace rerun | `silence_idle` | 0.002169 | 0.003742 | 0.004387 | 0.4130 | 0.7121 | 0.000 | 0.001 | Raw silence is low, but `frame.rms` reports high due to ES VU mapping/path state; this is a scale-confound warning. |
| corrected timing rerun | `silence_idle` | 0.002861 | 0.008449 | 0.009879 | 0.3198 | 0.5640 | 0.809 | 0.993 | Silence evidence is not stable enough to define a final trip range. |
| reboot clean rerun | `silence_idle` | 0.002924 | 0.004730 | 0.006652 | 0.6250 | 0.9112 | 0.148 | 0.316 | SilentScale can be near black while `frame.rms` is high; raw and perceptual RMS must be separated in any future test. |

## What Is Known

1. Production ESV11 silence gate constants are known: `raw/rmsUngated` threshold `0.005`, hysteresis `150 ms`, fade tau `0.19 s`.
2. Band-ratio trigger tuning is source-documented against older K1v2 traces: silence p50 `0.003`, p95 `0.006`, max `0.009`; music p5 `0.015`, p50 `0.066`; BR gate `0.007`.
3. The 2026-04-27 evidence broadly supports that raw-hop RMS scale: silence is usually around `0.002-0.006`, while approved music spans roughly `0.012-0.069` p50 in the original capture set.
4. `frame.rms` is a perceptually expanded/clipped ES VU mapping, not mic-domain RMS. It saturates on normal music and can disagree with raw-hop RMS during silence.
5. `silentScale` behaviour is documented in source and trace counters, but the observed trip/fade range is inconsistent across reruns.

## What Is Not Known

1. No calibrated microphone-domain peak envelope was found. The only peak-like production path located here is the SB waveform peak follower with a `750` sweet-spot subtraction, which is a visual parity follower, not a calibrated mic peak report.
2. No LUFS/SPL/reference-playback level is tied to the captured traces.
3. No current 2026-05 hardware pass confirms the same raw-hop RMS ranges after the later render/transport fixes.
4. No stable silence-trip table exists showing `(rawHopRms, frame.rms, silentScale, isSilent, audioConfidence)` over enough time to prove trip/recovery boundaries.

## Decision

C-1 stays open. The audit narrows the unknowns, but the 2026-04-27 baseline does not document a complete microphone-domain operating envelope.

The next no-guesswork hardware pass should capture:

| Measurement | Required output |
|---|---|
| Idle room | raw hop RMS p50/p95/p99/max, `frame.rms`, `audioConfidence`, `silentScale`, `isSilent` over at least 30 s |
| Known quiet music | same fields, plus transition time until `silentScale=1.0` |
| Known normal music | same fields, plus clipping/saturation rate for `frame.rms` |
| Known loud music | same fields, plus raw-hop RMS max and waveform peak follower state |
| Stop playback | time from last audible music to `isSilent=true` and `silentScale < 0.2` |

Until that pass exists, outputs depending on C-1 must remain DEGRADED-MODE.
