# Health Precheck And Runtime Health

RBDO label: GROUNDED

## Precheck

Before Director enablement:

```text
effect: 0x1302 K1 Waveform
brightness=160 speed=27 intensity=128 saturation=128 complexity=128 variation=0
palette=10 Vintage 01
edge_mixer: mode=mirror
songAware: enabled=false mode=off
show_skips=0 failures=0 rmt_errors=0 underruns=0
```

`s` output before Director run:

```text
Effect: 4866 (K1 Waveform)
Brightness: 160
Speed: 27
FPS: 119
Frame time: avg=8375, min=8244, max=33032 us
LED show: avg=6196, max=8630 us, skips=0
Heap: 8092667 / min 8086335 bytes
```

## Post-switch Runtime Health

After corrected Director switch:

```text
effect: 0x0407 LGP Photonic Crystal
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
LED show: avg=6196, max=8630 us, skips=0
Heap: 8092667 / min 8086335 bytes
```

## Final Restored Health

After `songaware off` and restore to `0x1302`:

```text
effect: 0x1302 K1 Waveform
songAware: enabled=false mode=off constrainedSwitching=false
songAware_health: show_skips=0 failures=0 rmt_errors=0 underruns=0
LED show: avg=6193, max=8630 us, skips=0
Heap: 8088023 / min 8086335 bytes
```

## Health Decision

PASS for hard runtime counters:

- `show_skips=0`
- `failures=0`
- `rmt_errors=0`
- `underruns=0`

Residual note: renderer `drops` counters were already high in `s` output and are tracked separately from the hard counters above. This runtime gate did not classify them as a hard failure because the requested hard fail list was `show_skips`, `failures`, `rmt_errors`, and `underruns`.
