# K1 Song-Aware Lane D A/B Health Comparison - 2026-05-12

## Comparison Status

Health comparison result: not comparable because Condition B could not be activated or observed.

The validation stopped before track playback. Therefore B cannot be shown equal-or-better than A on health metrics, and the correct decision is a runtime-support block rather than a health pass or health fail.

## Precheck Health Evidence

| Metric | Precheck Value | Source |
| --- | ---: | --- |
| Active effect | `0x1302 K1 Waveform` | serial `s` / `vp stack` |
| FPS | `119` | serial `s` / `vp stack` |
| Frame avg | `8380-8391 us` | serial `s` / `vp stack` |
| Frame min | `8251 us` | serial `s` / `vp stack` |
| Frame max | `32947 us` | serial `s` / `vp stack` |
| LED show avg | `6205-6362 us` | serial `s` / timing stack |
| LED show skips | `0` | serial `s` / `vp stack` |
| Failures | `0` | serial `vp stack` |
| RMT errors | `0` | serial `vp stack` |
| Underruns | `0` | serial `vp stack` |
| Free heap | `8089399` actor-system snapshot; `27284` memory command snapshot | serial precheck |
| Min free heap | `8089267` actor-system snapshot; `26508` memory command snapshot | serial precheck |

The early boot snapshot included frame drops:

```text
Frames: 184
Drops: 50
```

These were observed immediately after the serial open/boot sequence and were not counted as an A/B comparison result.

## A/B Health Table

| Metric | Condition A Baseline | Condition B Parameter Mode | Gate Result |
| --- | --- | --- | --- |
| Active effect id/name | Not captured in full-track run | Not captured | Not comparable |
| FPS | Not captured | Not captured | Not comparable |
| Frame avg/min/max/p99 | Not captured | Not captured | Not comparable |
| Show skips | Not captured | Not captured | Not comparable |
| Failures | Not captured | Not captured | Not comparable |
| RMT errors | Not captured | Not captured | Not comparable |
| Underruns | Not captured | Not captured | Not comparable |
| Free/min heap | Not captured | Not captured | Not comparable |
| Audio RMS/flux/BPM/confidence | Precheck only | Precheck only | Not comparable |
| Automatic effect-ID switches | Not run | Not run | No switch observed because no run occurred |

## Hard-Fail Trace

| Hard Fail Gate | Result |
| --- | --- |
| `show_skips > 0` | Not tested in A/B; precheck showed `0` |
| `failures > 0` | Not tested in A/B; precheck showed `0` |
| `RMT errors > 0` | Not tested in A/B; precheck showed `0` |
| `underruns > 0` | Not tested in A/B; precheck showed `0` |
| Automatic effect-ID switches in A or B | Not tested; no run |
| Parameter mode cannot be activated or observed | Failed |
| B is not equal-or-better than A on health metrics | Not comparable because B unavailable |

Final health classification: `BLOCKED_RUNTIME_SUPPORT`, not `FAIL_PARAMETER_MODE_HEALTH`.

