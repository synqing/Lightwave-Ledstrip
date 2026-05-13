---
id: 2026-05-06--firmware-v3--audio-bench-toggle-wiring
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Wired Surface 7 audio bench toggles into ControlBus UpdateFromHop backends
files_changed:
  - BACKLOG.md
  - firmware-v3/src/audio/AudioActor.cpp
  - firmware-v3/src/audio/contracts/ControlBus.cpp
  - firmware-v3/src/audio/contracts/ControlBus.h
  - firmware-v3/src/utils/BenchRegistry.cpp
  - firmware-v3/src/utils/BenchRegistry.h
  - firmware-v3/test/test_control_bus_bench_toggles/test_control_bus_bench_toggles.cpp
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
validation: pio test -e native_test_control_bus_bench_toggles; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz; pio run -e esp32dev_audio_esv11_k1v2_32khz_trace
breaking_change: false
follow_ups:
  - ESV11 production adapter gates require a separate semantic decision because ESV11 bypasses ControlBus UpdateFromHop Stage A.
---

## Details

The existing `audio.lookahead`, `audio.zone_agc`, and `audio.chroma_zone_agc` BenchRegistry entries now feed the ControlBus `UpdateFromHop` path via an AudioActor per-hop observer. Defaults remain enabled, so canonical behaviour is unchanged unless Captain toggles a bench flag at runtime.
