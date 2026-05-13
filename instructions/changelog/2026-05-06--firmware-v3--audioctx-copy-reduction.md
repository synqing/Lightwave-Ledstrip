---
id: 2026-05-06--firmware-v3--audioctx-copy-reduction
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Reduced renderer-side audio context copy count after ControlBus handoff tracing
files_changed:
  - BACKLOG.md
  - firmware-v3/src/core/actors/RendererActor.cpp
  - firmware-v3/src/core/actors/RendererActor.h
  - firmware-v3/test/test_snapshot_buffer_diagnostics/test_snapshot_buffer_diagnostics.cpp
  - firmware-v3/docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/README.md
validation: pio test -e native_test_snapshot_buffer_diagnostics; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz; pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_handoff -t upload --upload-port /dev/cu.usbmodem2101; python3 tools/capture_trace.py --port /dev/cu.usbmodem2101 --effect 0x2102 --soak 30 --timeout 25 --quiet; python3 tools/analyse_trace.py ... --strict; pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101; serial s
breaking_change: false
follow_ups:
  - Treat ControlBusFrame hot/cold split as the next lever only if sub-300 us snapshot reads become a hard requirement.
---

## Details

Single-effect and independent-strip render paths now populate `EffectContext.audio` directly from the renderer-owned `ControlBusFrame` instead of first copying through `m_sharedAudioCtx`. The zone path keeps the compatibility context because `ZoneComposer` owns its own reusable context.

The follow-up K1v2 `0x2102` handoff trace improved `audio_snapshot_read` p99 from 687 us to 460 us and `render_frame` p99 from 3072 us to 2755 us in the comparable trace run. Retry stayed at 2 events, so retry policy is still not the root cause.
