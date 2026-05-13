# Effect ID Stability A vs B

Fixed effect: `0x1302 K1 Waveform`

## Stability table

| Condition | Track | Start effect | End effect | Observed effect IDs | automaticEffectSwitches max | Result |
|---|---|---|---|---|---:|---|
| A | ambient / low-energy intro-heavy | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |
| A | build/drop electronic | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |
| A | dense / high-energy | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |
| B | ambient / low-energy intro-heavy | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |
| B | build/drop electronic | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |
| B | dense / high-energy | `0x1302` | `0x1302` | `0x1302` | 0 | PASS |

## Hard-fail checks

| Check | Result |
|---|---|
| Any automatic effect-id switch in A | PASS, none observed |
| Any automatic effect-id switch in B | PASS, none observed |
| `automaticEffectSwitches > 0` | PASS, all observed values were 0 |
| Active effect changed during 60-second preflight smoke | PASS, `0x1302` first and last |

## Source support

`SongAwareDirector.cpp:94-96` stores the supplied active effect id and resets `automaticEffectSwitches` to zero. No code path in the audited runtime block selects or writes a new effect id.

