---
id: 2026-05-06--firmware-v3--controlbus-handoff-tier2-trace
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Added gated ControlBus handoff Tier 2 trace spans and captured K1v2 evidence
files_changed:
  - BACKLOG.md
  - firmware-v3/platformio.ini
  - firmware-v3/src/audio/contracts/SnapshotBuffer.h
  - firmware-v3/src/config/features.h
  - firmware-v3/src/core/actors/RendererActor.cpp
  - firmware-v3/docs/research/phase1b_runtime_evidence_2026-05-06/controlbus_dram_relocation_trace/README.md
validation: pio test -e native_test_snapshot_buffer_diagnostics; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_handoff; pio run -e esp32dev_audio_esv11_k1v2_32khz_trace_handoff -t upload --upload-port /dev/cu.usbmodem2101; python3 tools/capture_trace.py --port /dev/cu.usbmodem2101 --effect 0x2102 --soak 30 --timeout 25 --quiet; python3 tools/analyse_trace.py ... --strict; pio run -e esp32dev_audio_esv11_k1v2_32khz; pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101; serial s
breaking_change: false
follow_ups:
  - Reduce renderer-side ControlBusFrame/AudioContext copy count, or graduate the ControlBusFrame hot/cold split.
---

## Details

Added `FEATURE_TRACE_AUDIO_HANDOFF` and the `esp32dev_audio_esv11_k1v2_32khz_trace_handoff` env. The new gated spans split `audio_snapshot_read` into `bus_copy_memcpy`, retry markers/counters, `bus_copy_retry`, and `audio_ctx_populate_us`.

K1v2 evidence showed `audio_snapshot_read` remained above target (`687 us` p99), with `bus_copy_memcpy` at `247 us` p99, `audio_ctx_populate_us` at `302 us` p99, and only 2 retry events across 98 reads. The remaining issue is copy count / frame shape, not retry contention.
