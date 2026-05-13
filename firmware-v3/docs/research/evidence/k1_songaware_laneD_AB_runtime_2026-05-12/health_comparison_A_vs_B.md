# Health Comparison A vs B

## Error counters

| Condition | Track | show_skips | failures | rmt_errors | underruns | automaticEffectSwitches |
|---|---|---:|---:|---:|---:|---:|
| A | ambient / low-energy intro-heavy | 0 | 0 | 0 | 0 | 0 |
| A | build/drop electronic | 0 | 0 | 0 | 0 | 0 |
| A | dense / high-energy | 0 | 0 | 0 | 0 | 0 |
| B | ambient / low-energy intro-heavy | 0 | 0 | 0 | 0 | 0 |
| B | build/drop electronic | 0 | 0 | 0 | 0 | 0 |
| B | dense / high-energy | 0 | 0 | 0 | 0 | 0 |

## Frame and LED show telemetry

`s` pre/end state provided FPS and frame time. Poll samples provided LED show time at approximately 1 Hz.

| Condition | Track | FPS pre/end | Frame avg us pre/end | Frame min/max us | LED show avg us | LED show p99 us | LED show max us |
|---|---|---|---|---|---:|---:|---:|
| A | ambient / low-energy intro-heavy | 119 / 115 | 8422 / 8661 | 8244 / 32996 | 6183.3 | 6217 | 7469 |
| A | build/drop electronic | 114 / 118 | 8758 / 8429 | 8244 / 32996 | 6185.8 | 6215 | 7469 |
| A | dense / high-energy | 119 / 119 | 8669 / 8389 | 8244 / 32996 | 6180.9 | 6212 | 7469 |
| B | ambient / low-energy intro-heavy | 118 / 119 | 8903 / 8394 | 8244 / 32996 | 6185.5 | 6215 | 7469 |
| B | build/drop electronic | 119 / 118 | 8378 / 8397 | 8244 / 32996 | 6187.1 | 6228 | 7469 |
| B | dense / high-energy | 118 / 119 | 8373 / 8398 | 8244 / 32996 | 6185.3 | 6219 | 7469 |

## Memory

Pre/end memory was stable across every A and B track:

| Source | Free | Minimum |
|---|---:|---:|
| `vp stack` heap | 8,088,103 bytes | 8,086,215 bytes |
| `dbg memory` free heap | 26,396 bytes | 24,572 bytes |
| `vp stack` SPIRAM free | 8,061,707 bytes | not reported |
| `vp stack` stack watermark | 11,324 words | not reported |

## Health verdict

B was not worse than A on the hard health counters. All hard counters stayed zero. LED show average/p99/max were materially equivalent between A and B.

Residual note: serial telemetry showed `LED show avg` around 6.18 ms and `max=7469 us`. That did not create skips or RMT failures during this run, but it remains a timing value to keep watching in later gates.

