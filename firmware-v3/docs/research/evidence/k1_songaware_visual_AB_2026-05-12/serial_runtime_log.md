# Serial Runtime Log - Visual A/B Gate

RBDO label: GROUNDED

Device: `/dev/cu.usbmodem1101` at 115200 baud  
Control path: USB serial only

## T1-A partial baseline

Condition: `songaware off`, fixed effect and parameters.  
Runtime: stopped at about 62.7 s after Captain challenged repeated baseline playback.

| Metric | Value |
|---|---:|
| parameter update delta | 0 |
| observed effect IDs | `0x1302` |
| automaticEffectSwitches | 0 |
| show_skips | 0 |
| failures | 0 |
| rmt_errors | 0 |
| underruns | 0 |
| pre state | `enabled=false mode=off effectiveMode=off owner=none suppressed=disabled` |
| end state | `enabled=false mode=off effectiveMode=off owner=none suppressed=disabled` |

## T1-B full visual run

Condition: `songaware on`, `songaware mode balanced`, family morphing false, constrained switching false.  
Runtime: full available ambient/intro-heavy track.

| Metric | Value |
|---|---:|
| parameter update delta | 25,604 |
| observed effect IDs | `0x1302` |
| automaticEffectSwitches | 0 |
| show_skips | 0 |
| failures | 0 |
| rmt_errors | 0 |
| underruns | 0 |
| pre state | `enabled=true mode=balanced effectiveMode=balanced owner=director suppressed=none state=build lastAction=parameter_update` |
| end state | `enabled=true mode=balanced effectiveMode=balanced owner=director suppressed=none state=build lastAction=parameter_update` |

## Final disable check

After Captain rejected the visual result, serial command `songaware off` was sent and confirmed:

```text
songAware: enabled=false mode=off familyMorphing=false constrainedSwitching=false
songAware_status: effectiveMode=off owner=none suppressed=disabled state=silence confidence=0.000 lastAction=none activeEffect=0x1302 parameterUpdates=441579 automaticEffectSwitches=0
```

Final health excerpt:

```text
effect: 0x1302 K1 Waveform
controls: brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
led_show: show_skips=0 failures=0 rmt_errors=0 underruns=0
LED show: avg=6197, max=7469 us, skips=0
```

