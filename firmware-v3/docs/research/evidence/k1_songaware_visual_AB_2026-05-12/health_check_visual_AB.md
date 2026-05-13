# Health Check - Visual A/B Gate

RBDO label: GROUNDED

## Hard counters

| Run | Condition | Effect stable at `0x1302` | automaticEffectSwitches | show_skips | failures | rmt_errors | underruns | Result |
|---|---|---|---:|---:|---:|---:|---:|---|
| T1-A partial | A | yes | 0 | 0 | 0 | 0 | 0 | PASS |
| T1-B full | B | yes | 0 | 0 | 0 | 0 | 0 | PASS |
| Final disable | off | yes | 0 | 0 | 0 | 0 | 0 | PASS |

## Frame and LED show telemetry

| Run | FPS pre/end | Frame avg us pre/end | LED show avg us pre/end | LED show max us |
|---|---|---|---|---:|
| T1-A partial | 106 / 119 | 9244 / 8388 | 6190 / 6204 | 7469 |
| T1-B full | 107 / 118 | 9002 / 8421 | 6184 / 6196 | 7469 |
| Final disable | 115 | 8454 | 6197 | 7469 |

## Health verdict

No health regression was observed. The visual gate failed on product visible gain, not on runtime health or effect-id stability.

