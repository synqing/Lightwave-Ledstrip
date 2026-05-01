# Trace Baselines

Canonical mabutrace JSON captures committed for use as the `--baseline`
input to `firmware-v3/tools/analyse_trace.py`. Each file is a trace soak
captured on real hardware with the trace-instrumented build env.

## Naming

```
k1v2_0xNNNN_YYYY-MM-DD.json            — clean capture via capture_trace.py
k1v2_0xNNNN_serial_dump_YYYY-MM-DD.json — raw serial dump (HH:MM:SS:NNN -> prefixes intact)
```

`0xNNNN` is the effect ID. The dated suffix is the capture date in ISO-8601.

## Current baselines

| File | Effect | Captured | Build env | Hardware | Soak | Events |
|---|---|---|---|---|---|---|
| `k1v2_0x2100_2026-04-27.json` | RadialTimeScopeEffect (RTS) | 2026-04-27 15:36 | `esp32dev_audio_esv11_k1v2_32khz_trace` | K1 V2 MAC `b4:3a:45:a5:87:f8` | ~30 s | 5,470 |
| `k1v2_0x2101_2026-04-27.json` | AttackOnlyPitchVelocityFieldEffect (PVF) | 2026-04-27 15:36 | same | same | ~30 s | 5,470 |
| `k1v2_0x2102_2026-04-27.json` | BeatParitySpriteEffect (BPS) | 2026-04-27 15:37 | same | same | ~30 s | 5,477 |
| `k1v2_0x2102_serial_dump_2026-04-27.json` | BPS (raw serial) | 2026-04-27 15:25 | same | same | n/a | 5,484 |

Each baseline contains the full canonical Surface 1/2/3/4/5/7 counter set
plus the effect-specific markers:
- `rts_*` family (10 counters) for 0x2100
- `pvf_*` family (6 counters + 2 instants) for 0x2101
- `bps_*` family (8 counters + 6 instants) for 0x2102

## Usage

Compare a fresh capture against a baseline:

```bash
~/.platformio/penv/bin/python3 firmware-v3/tools/capture_trace.py \
    --port /dev/cu.usbmodem2101 --effect 0x2102 --soak 30 \
    --output /tmp/k1v2_0x2102_$(date +%Y-%m-%d).json --open

python3 firmware-v3/tools/analyse_trace.py \
    /tmp/k1v2_0x2102_$(date +%Y-%m-%d).json \
    --baseline firmware-v3/tools/baselines/k1v2_0x2102_2026-04-27.json \
    --strict
```

`--strict` exits non-zero on contract regression (per `TRACE_INSTRUMENTATION_SPEC.md` §8 exit codes).

## When to add a new baseline

After a major change to a measured surface — e.g. DRAM relocation of
`ControlBusFrame`, RMT pacing change, render-path refactor — capture a
fresh baseline AS PART of the same commit (or a sibling chore commit).
Replace the old baseline only if the change is intentional; otherwise
keep both for regression diffing.

## Raw serial dump caveat

`*_serial_dump_*.json` files contain `HH:MM:SS:NNN -> ` prefixes from the
serial monitor. Plain `json.load()` will fail. Use `capture_trace.py
--strip-only <file>` to clean them, or strip with:

```python
import re, json
text = open(path).read()
clean = '\n'.join(re.sub(r'^\d\d:\d\d:\d\d:\d\d\d -> ', '', line) for line in text.splitlines())
events = json.loads(clean)
```
