---
id: 2026-05-06--firmware-v3--controlbusframe-internal-dram-relocation
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Relocated the ControlBusFrame snapshot buffer payload to internal DRAM
files_changed:
  - BACKLOG.md
  - firmware-v3/src/audio/AudioActor.cpp
  - firmware-v3/src/audio/AudioActor.h
  - firmware-v3/src/audio/contracts/SnapshotBuffer.h
  - firmware-v3/src/core/actors/ActorSystem.cpp
  - firmware-v3/test/test_snapshot_buffer_diagnostics/test_snapshot_buffer_diagnostics.cpp
validation: pio test -e native_test_snapshot_buffer_diagnostics; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz; pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101; serial boot diagnostic on K1v2 MAC b4:3a:45:a5:87:f8
breaking_change: false
follow_ups:
  - Capture a fresh Tier 1 trace and compare audio_snapshot_read p99 against the baseline target.
---

## Details

Implemented the narrow 1B relocation path for `SnapshotBuffer<ControlBusFrame>`. `InternalSnapshotBufferOwner` now allocates the snapshot buffer object in `MALLOC_CAP_INTERNAL` memory on ESP targets, while preserving a native placement-storage path for host tests. `AudioActor` keeps the whole actor in its existing allocation region, and `ActorSystem` refuses startup if the snapshot buffer allocation fails.

K1v2 hardware verification on `/dev/cu.usbmodem2101` / MAC `b4:3a:45:a5:87:f8` confirmed the boot diagnostic changed from `actor=PSRAM payload=PSRAM` to `actor=PSRAM payload=DRAM`.
