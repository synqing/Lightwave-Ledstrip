# Perf Baseline Run (Phase 2 Extended)

- Timestamp: 2026-02-26 14:22:55
- Host: `http://192.168.1.101`
- WS: `ws://192.168.1.101/ws`
- Serial: `/dev/cu.usbmodem212401`
- Scenarios: REST-only (5m), WS-only (5m), Mixed REST+WS (15m)

| Scenario | Duration(s) | Shedding | Heap Min | Heap Max | Heap Avg | REST Calls | REST Errors | WS Connects | WS Msg | WS Recv | WS Errors |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rest_only_5m | 300 | 60 | 19484 | 24568 | 23979.5 | 1901 | 0 | 0 | 0 | 0 | 0 |
| ws_only_5m | 300 | 60 | 20480 | 25800 | 24329.7 | 0 | 0 | 1 | 2587 | 2587 | 0 |
| mixed_rest_ws_15m | 900 | 180 | 11252 | 24460 | 22348.0 | 4836 | 4131 | 1 | 975 | 936 | 2228 |

## Notes
- No AP/STA mode switching performed during this run.
- Compare against phase1 baseline and previous 10-minute stability run for drift assessment.
