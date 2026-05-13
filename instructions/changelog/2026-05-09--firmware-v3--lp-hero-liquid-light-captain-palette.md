# 2026-05-09 - firmware-v3 - LP hero liquid light Captain palette

## Summary

- Replaced the Liquid Light runner sequence with Captain's selected palette order: Red Magenta Yellow, Autumn 19, Fire, Emerald Dragon, Vintage 57, GR64 Hult, Vintage 01.
- Recorded the K1v2 run evidence in `firmware-v3/docs/research/lp_hero_liquid_light_captain_palette_2026-05-09.md`.
- Marked the prior agent-selected recut as superseded.

## Why

Captain rejected the agent-selected palette choices and supplied the forward palette list directly. The runner should preserve that decision so future agents do not reintroduce the rejected cool/soft sequences.

## Validation

- `~/.platformio/penv/bin/python -m py_compile firmware-v3/tools/liquid_light_palette_sequence.py`
- `~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py --port /dev/cu.usbmodem2101 --loops 1 --brightness 181 --speed 14 --dwell 7`
- post-run `vp stack`
- post-run `s`
- post-run `dbg memory`
