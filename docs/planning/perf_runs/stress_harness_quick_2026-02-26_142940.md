# LightwaveOS Stress Harness Report

- Timestamp: 2026-02-26 14:29:40
- Host: `http://192.168.1.101`
- WS URL: `ws://192.168.1.101/ws`
- Serial: `/dev/cu.usbmodem212401`
- Profile: `quick`
- Overall: `FAIL`

| Scenario | Dur(s) | REST Calls | REST Err | WS Msg | WS Err | Heap Min | Heap Avg | Shedding | Uptime Resets |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| rest_quick | 30 | 75 | 75 | 0 | 0 | 22968 | 22968 | 6 | 0 |
| ws_quick | 30 | 0 | 0 | 0 | 80 | 22968 | 22968 | 7 | 0 |
| mixed_quick | 60 | 143 | 143 | 0 | 158 | 22616 | 22938.7 | 12 | 0 |

## Notes
- rest_quick: REST error rate too high (100.0%)
- mixed_quick: REST error rate too high (100.0%)
