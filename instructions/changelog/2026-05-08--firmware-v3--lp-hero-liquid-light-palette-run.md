# 2026-05-08 - firmware-v3 - LP hero liquid light palette run

## Summary

- Added `firmware-v3/tools/liquid_light_palette_sequence.py`, a reproducible SerialCLI helper for driving the existing `0x0201 LGP Holographic` Liquid Light palette audition without changing firmware effect behaviour.
- Recorded the first K1v2 execution evidence for the LP hero Liquid Light direction in `firmware-v3/docs/research/lp_hero_liquid_light_palette_run_2026-05-08.md`.

## Why

The LP hero production track needs a controlled, repeatable K1v2 source-motion pass before any landing-page asset is promoted. The current direction is to reuse the restrained Holographic family first, not add a new effect or return to Waveform Hybrid for hero capture.

## Validation

- `~/.platformio/penv/bin/python firmware-v3/tools/liquid_light_palette_sequence.py --port /dev/cu.usbmodem2101 --loops 1 --brightness 208 --speed 14 --echo`
- post-run `vp stack`
- post-run `s`
- post-run `dbg memory`

## Notes

- Device path: `/dev/cu.usbmodem2101`
- Observed K1v2 serial: `B4:3A:45:A5:87:F8`
- Final state: `0x0201 LGP Holographic`, palette `1 Rivendell`, brightness `213`, speed `14`
- Visual sign-off remains pending.
