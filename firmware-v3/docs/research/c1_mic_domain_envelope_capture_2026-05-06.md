# C-1 Mic-Domain Envelope Capture

**Date:** 2026-05-06
**RBDO label:** GROUNDED for the firmware-domain measurements below; DEGRADED-MODE for SPL/LUFS, cross-device, or production-acoustic claims.
**Scope:** Record the missing hardware evidence called out by `BACKLOG.md` C-1 using K1v2 serial telemetry and Captain-approved private playback material.

## Evidence Boundary

| Item | Evidence |
|---|---|
| Device | K1v2 on `/dev/cu.usbmodem2101`; upload verified against MAC `b4:3a:45:a5:87:f8` during the capture run. |
| Firmware profile | `esp32dev_audio_esv11_k1v2_32khz_c1_envelope`, defined in `firmware-v3/platformio.ini:389-396`. |
| Instrumentation | `AudioActor` emits `silentScale`, `isSilent`, `frame.rms`, waveform peak state, and compact `[C1]` serial lines at `firmware-v3/src/audio/AudioActor.cpp:1180-1210`. |
| Capture helper | `firmware-v3/tools/capture_c1_envelope.py` parses `[C1]` lines, owns `afplay` timing only when `--audio-manifest --armed` are both present, and computes post-hard-stop recovery (`firmware-v3/tools/capture_c1_envelope.py:39-59`, `382-427`, `553-566`, `670-677`). |
| Audio material | Captain-provided private local tracks. Source paths and clips are intentionally not committed per C-3 clip-private policy. |
| Capture mode | Process-owned `afplay` start/stop with serial `[C1]` samples; no REST, STA, or WiFi dependency. |

## Results

| Regime | Samples / raw lines | rawHopRms p50/p95/p99/max | frameRms p50/p95/max | confidence p50/min | silentScale p50/p95/max | isSilent mean | waveformPeak p50/p95/p99/max |
|---|---:|---:|---:|---:|---:|---:|---:|
| idle_room | 278 / 279 | 0.001591 / 0.003477 / 0.005916 / 0.021071 | 0.322308 / 0.777461 / 1.000000 | 0.918 / 0.127 | 0.001 / 0.285 / 0.730 | 0.942 | 0.000 / 0.021 / 0.374 / 1.183 |
| sparse_quiet | 389 / 390 | 0.005513 / 0.015687 / 0.027730 / 0.032373 | 0.510310 / 1.000000 / 1.000000 | 1.000 / 1.000 | 0.970 / 1.000 / 1.000 | 0.139 | 0.002 / 0.395 / 0.661 / 1.005 |
| normal_full_spectrum | 390 / 392 | 0.017600 / 0.036282 / 0.044053 / 0.061825 | 0.641935 / 1.000000 / 1.000000 | 1.000 / 1.000 | 1.000 / 1.000 / 1.000 | 0.010 | 0.364 / 0.623 / 0.833 / 1.090 |
| dense_bright_loud | 389 / 390 | 0.026247 / 0.044652 / 0.058309 / 0.064463 | 0.759867 / 1.000000 / 1.000000 | 1.000 / 0.995 | 1.000 / 1.000 / 1.000 | 0.026 | 0.563 / 0.775 / 0.932 / 1.010 |
| stop_recovery | 359 / 362 | 0.003325 / 0.033222 / 0.040718 / 0.044479 | 0.465803 / 0.931538 / 1.000000 | 1.000 / 0.088 | 0.414 / 1.000 / 1.000 | 0.499 | 0.081 / 0.576 / 0.756 / 0.932 |

## Stop-Recovery Finding

The stop-recovery track was hard-stopped 20.0 seconds after capture start. From that hard stop:

| Measurement | Result |
|---|---:|
| Post-stop samples | 186 |
| First `isSilent=true` | 0.266399 s after hard stop |
| First `silentScale < 0.2` | 1.128722 s after hard stop |

## Interpretation

C-1 is now measured for the current K1v2 firmware microphone-domain path:

- Idle raw-hop RMS sits around p50 `0.0016`, p95 `0.0035`, p99 `0.0059`. The p99 value straddles the source silence threshold family, so max outliers must not be used alone as the idle floor.
- Quiet music starts close to the silence boundary at p50 `0.0055`, but p95 rises to `0.0157` and `silentScale` holds near open (`0.970` p50). Quiet material therefore needs hysteresis/hold behaviour; a single raw-RMS cutoff is not enough.
- Normal/dense music occupies the stable working band: raw-hop RMS p50 `0.0176-0.0262`, p95 `0.0363-0.0447`, max `0.0645`.
- `frame.rms` saturates to `1.0` under normal music and can remain non-zero during idle, so it is useful as a perceptual energy signal but not as the primary mic-domain calibration axis.
- Waveform peak follower activity for normal/dense music sits around p50 `0.364-0.563`, p95 `0.623-0.775`, with maxima around `1.01-1.09`.
- Stop recovery is fast enough for current silence policy work: `isSilent` asserts within `0.27 s`; `silentScale` reaches a strong fade (`<0.2`) within `1.13 s`.

## Decision

Treat C-1 as **MEASURED-DEGRADED**:

- It no longer blocks firmware-domain tuning, AFS v2 silentScale validation, or current K1v2 tuned-regime sign-off work that uses the same ESV11 32 kHz profile and Captain-approved private playback chain.
- It still does not justify SPL, LUFS, cross-room, K1v1/K1v2 parity, or production-acoustic claims.
- Re-run this capture if microphone placement, enclosure acoustics, sample rate, silence-gate constants, playback chain, or source corpus materially changes.
