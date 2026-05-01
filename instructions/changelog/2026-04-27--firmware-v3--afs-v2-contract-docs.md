---
id: 2026-04-27--firmware-v3--afs-v2-contract-docs
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: docs
summary: Document the Audio Feature Surface v2 contract and measurement baseline.
files_changed:
  - firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md
  - firmware-v3/docs/audio-visual/audio-visual-contract-surface.md
  - firmware-v3/docs/research/audio_feature_surface_v2_baseline_2026-04-27.md
  - firmware-v3/docs/debugging/MABUTRACE_GUIDE.md
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py
breaking_change: false
follow_ups: []
---

## Details

Records the raw-substrate containment policy, helper/API direction, measurement gates, and AFS-specific trace counters needed before new semantic audio fields are promoted.
