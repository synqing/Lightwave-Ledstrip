# K1 Song-Aware Lane D A/B Decision - 2026-05-12

## Final Decision

`BLOCKED_RUNTIME_SUPPORT`

## Why

The K1 device was reachable over USB serial and reported healthy precheck counters for LED show skips, failures, RMT errors, and underruns. However, the requested Condition B could not be activated or observed through an existing song-aware parameter-only runtime path.

The source/contract scan found no runtime `songAware.mode`, no `familyMorphing`, no `constrainedSwitching`, and no named parameter-only director mode. The only adjacent runtime controls found were:

- manual merge-layer parameter injection,
- narrative auto-play,
- audio mapping endpoints that were not reachable from the host network during this run,
- EdgeMixer controls.

None of those is valid evidence for autonomous song-aware parameter mode under the requested boundary.

## Decision Logic

| Candidate Decision | Applies? | Reason |
| --- | --- | --- |
| `PASS_PARAMETER_MODE_VALIDATION` | No | No A/B playback and no visible-gain evidence |
| `FAIL_PARAMETER_MODE_HEALTH` | No | Condition B did not run, so no B health degradation was measured |
| `FAIL_PARAMETER_MODE_NO_VISIBLE_GAIN` | No | No valid B run existed to judge visible gain |
| `BLOCKED_RUNTIME_SUPPORT` | Yes | Parameter mode cannot be activated or observed through existing runtime support |
| `BLOCKED_DEVICE_OR_CAPTURE` | No | Serial device was reachable; the blocker was runtime support, not USB capture |

## What Was Proven

- USB serial access to K1 was available at `/dev/cu.usbmodem1101`.
- The device booted AP-only and ESV11 audio initialised.
- Audio debug values were exposed through serial debug commands.
- Precheck `vp stack` showed `show_skips=0`, `failures=0`, `rmt_errors=0`, and `underruns=0`.
- HTTP/REST at `192.168.4.1` was not reachable from the host during this run.
- No runtime `songAware` control surface was found in source or protocol contracts.

## What Was Not Proven

- No proof that song-aware parameter mode improves perceived song fit over baseline.
- No proof that song-aware parameter mode is equal-or-better than baseline on health metrics.
- No full-track A/B serial polling evidence.
- No parameter changes/min evidence for autonomous song-aware adaptation.
- No visible section-change timestamps.

## Stop Condition

Stopped after A/B decision as requested. No family morphing, constrained switching, implementation handoff, NVS save, firmware edit, or production default change was performed.

