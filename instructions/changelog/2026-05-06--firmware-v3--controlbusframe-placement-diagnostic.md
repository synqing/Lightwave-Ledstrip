---
id: 2026-05-06--firmware-v3--controlbusframe-placement-diagnostic
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Added verify-first ControlBusFrame snapshot placement diagnostics
files_changed:
  - BACKLOG.md
  - firmware-v3/src/audio/AudioActor.cpp
  - firmware-v3/src/audio/AudioActor.h
  - firmware-v3/src/audio/contracts/SnapshotBuffer.h
  - firmware-v3/src/core/actors/ActorSystem.cpp
  - firmware-v3/test/test_snapshot_buffer_diagnostics/test_snapshot_buffer_diagnostics.cpp
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
validation: pio test -e native_test_snapshot_buffer_diagnostics; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz
breaking_change: false
follow_ups:
  - Flash K1v2 and read the ControlBusFrame snapshot storage boot line before deciding whether relocation is needed.
---

## Details

Implemented the 1C verify-first path for the ControlBusFrame internal-DRAM relocation decision. `SnapshotBuffer` now exposes diagnostic-only payload/object size accessors, and `ActorSystem` asks `AudioActor` to emit one-shot memory-region logs plus trace counters for actor storage, snapshot payload storage, and snapshot payload bytes immediately after allocation. This does not relocate any buffer and does not change render or audio behaviour.
