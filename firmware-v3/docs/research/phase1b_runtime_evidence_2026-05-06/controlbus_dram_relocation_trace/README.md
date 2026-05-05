# ControlBusFrame DRAM Relocation Trace Evidence — 2026-05-06

## Scope

K1v2 on `/dev/cu.usbmodem2101`, MAC `b4:3a:45:a5:87:f8`, effect `0x2102`.

This folder records the post-relocation trace gate for the ControlBusFrame internal-DRAM move and the follow-up Tier 2 handoff decomposition trace.

## Files

| File | Purpose |
|---|---|
| `k1v2_0x2102_controlbus_dram_2026-05-06.json` | Post-relocation Tier 1 capture |
| `k1v2_0x2102_controlbus_dram_2026-05-06.report.md` | Tier 1 analyser report |
| `k1v2_0x2102_controlbus_dram_2026-05-06.report.json` | Tier 1 machine-readable report |
| `k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.json` | Tier 2 handoff decomposition capture |
| `k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.report.md` | Tier 2 analyser report |
| `k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.report.json` | Tier 2 machine-readable report |

## Result

The DRAM relocation changed placement correctly, but did not meet the `<200 us` `audio_snapshot_read` target.

| Metric | Tier 1 p99 | Tier 2 p99 | Interpretation |
|---|---:|---:|---|
| `audio_snapshot_read` | 647 us | 687 us | Still above the 300 us diagnostic gate |
| `bus_copy_memcpy` | n/a | 247 us | The direct snapshot payload copy is no longer >300 us |
| `audio_ctx_populate_us` | n/a | 302 us | Renderer-side context population is a co-dominant cost |
| `bus_retry_check` | n/a | 2/98 reads | Retry contention is not the root cause |

## Interpretation

The remaining handoff cost is copy shape, not retry policy:

- `ReadLatest()` copies the `ControlBusFrame` into `RendererActor::m_lastControlBus`.
- `RendererActor` then copies the same large frame into `m_sharedAudioCtx.controlBus`.
- Single-effect and independent-strip paths copy `m_sharedAudioCtx` into `EffectContext.audio` before render.

The next optimisation should reduce renderer-side by-value `ControlBusFrame` / `AudioContext` copies, or graduate the parked hot/cold split. Do not change retry behaviour unless a later trace shows retry frequency rising.

`motion_engine_tick_us` and `motion_shaper_tick_us` were not added because `RendererActor` has no live `m_motionEngine.update()` / `m_motionShaper.update()` call site. Adding those calls would activate effect-facing motion semantics and change visible behaviour.

## Validation

- `pio test -e native_test_snapshot_buffer_diagnostics`
- `python3 scripts/check_native_harness_routes.py`
- `python3 scripts/native_harness_matrix.py`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_handoff`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_handoff -t upload --upload-port /dev/cu.usbmodem2101`
- `python3 tools/capture_trace.py --port /dev/cu.usbmodem2101 --effect 0x2102 --soak 30 --timeout 25 --quiet --output docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.json`
- `python3 tools/analyse_trace.py docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.json --baseline tools/baselines/k1v2_0x2102_2026-04-27.json --strict --output docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.report.md --json docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/k1v2_0x2102_controlbus_handoff_tier2_2026-05-06.report.json`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz`
- `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101`
- Serial `s` after production restore: no panic/RMT errors observed, `LED show avg=6182 us`, `max=7684 us`, `skips=0`; internal heap was low but stable across two samples (`9072` bytes free, `8644` min free).
