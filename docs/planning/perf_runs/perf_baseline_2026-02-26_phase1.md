# Perf Baseline Run (Phase 1 Start)

- Timestamp: 2026-02-26 13:56:18
- Host: `http://192.168.1.101`
- Serial: `/dev/cu.usbmodem212401`
- Scope: initial profiling slice (`idle`, `REST load`)

| Scenario | Duration(s) | Serial Lines | Shedding Lines | WS Diag Lines | REST Errors | Heap Samples | Heap Min | Heap Max | Heap Avg |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| idle | 90 | 31 | 18 | 9 | 0 | 18 | 27484 | 27484 | 27484 |
| rest_load_device_status | 90 | 30 | 18 | 9 | 0 | 18 | 22548 | 24568 | 24238.7 |

## Notes
- This is the first execution slice for the perf roadmap phase.
- Next slice should add WS-client load scenario and 15-30 minute windows.
