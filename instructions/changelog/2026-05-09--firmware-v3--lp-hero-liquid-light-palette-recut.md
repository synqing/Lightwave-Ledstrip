# 2026-05-09 - firmware-v3 - LP hero liquid light palette recut

Superseded: Captain supplied the forward palette sequence later on 2026-05-09. Use `2026-05-09--firmware-v3--lp-hero-liquid-light-captain-palette.md` for the active palette decision.

## Summary

- Replaced the rejected Liquid Light palette sequence with a harder, candidate camera-facing Holographic recut: Nighttime, Bathy, Cool, Blue Magenta White, Blue Cyan Yellow, GR65 Hult.
- Updated `firmware-v3/tools/liquid_light_palette_sequence.py` to force a real `0x0201` re-init by switching through `0x0200` before selecting Holographic.
- Switched the runner default from palette quick-key stepping to direct SerialJSON `setPalette` / `setBrightness` / `setSpeed`, with quick keys retained behind `--quick-keys`.
- Recorded the corrected K1v2 run evidence in `firmware-v3/docs/research/lp_hero_liquid_light_palette_recut_2026-05-09.md`.

## Why

Captain rejected the first palette selection. During the correction pass, the runner also exposed that re-sending `effect 0x0201` does not reset Holographic state if the same effect is already active, leaving the long-running phase state capable of degrading timing. The recut fixes the operational capture path without changing firmware effect behaviour.

## Validation

- `~/.platformio/penv/bin/python -m py_compile firmware-v3/tools/liquid_light_palette_sequence.py`
- `~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py --port /dev/cu.usbmodem2101 --loops 1 --brightness 181 --speed 14 --dwell 7`
- post-run `vp stack`
- post-run `s`
- post-run `dbg memory`
