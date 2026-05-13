---
id: 2026-05-08--firmware-v3--lp-hero-liquid-light-holographic-direction
date_utc: 2026-05-08
agent: codex
scope: firmware-v3
type: docs
summary: Pivot LP hero Liquid Light motion direction to controlled Holographic-family candidates
files_changed:
  - CHANGELOG.md
  - firmware-v3/docs/research/lp_hero_liquid_light_holographic_direction_2026-05-08.md
validation: Documentation-only change; validate with git diff --check.
breaking_change: false
follow_ups:
  - Capture K1v2 0x0201 Liquid Light palette candidates before LandingPage video processing.
---

## Details

Records Captain's correction that the landing-page first-contact Liquid Light source should start from `0x0201` LGP Holographic or a constrained `0x1000` Holo Auto-Cycle variant rather than Waveform Hybrid.

The note preserves the source distinction: `0x0201` is deterministic through the external palette path, while `0x1000` owns a shuffled internal playlist and should be narrowed before final hero capture.
