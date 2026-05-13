---
id: 2026-05-06--firmware-v3--phase5-pvf-bps-candidate-checkpoint
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Checkpoint Phase 5 PVF and BPS candidate repairs with native regressions and evidence notes
files_changed:
  - CHANGELOG.md
  - firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.cpp
  - firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.cpp
  - firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.h
  - firmware-v3/test/test_native/test_attack_only_pitch_velocity.cpp
  - firmware-v3/test/test_native/test_beat_parity_sprite.cpp
  - firmware-v3/test/test_native_phase5/test_main.cpp
  - firmware-v3/test/test_native_phase5/test_beat_parity_sprite.cpp
  - firmware-v3/docs/research/phase5_effects_visual_resume_run_2026-05-06.md
  - firmware-v3/docs/research/lgp_beat_emotiscope_architecture_review_2026-05-06.md
  - firmware-v3/docs/research/phase5_effects_resume_prompt_2026-05-06.md
validation: Native Phase 5 harness, K1v2 production build, and git diff hygiene; hardware visual status remains degraded pending next K1v2 effect pass.
breaking_change: false
follow_ups:
  - Reflash K1v2 and rerun focused PVF/BPS visual checks before any PASS or ship-gate claim.
---

## Details

Records the Phase 5 PVF/BPS candidate boundary so it cannot be swept into unrelated SRAM or tooling commits. PVF now snapshots chroma structure only on onset hops and removes raw-RMS bed ownership. BPS removes the always-alive RMS/gHue bed so sustained amplitude without a launch event stays dark.

Native tests cover the production render-path regressions for sustained non-event silence. The research notes preserve Captain's PVF visual failure, the BPS silence confirmation, and the remaining degraded BPS musical responsiveness/timing concerns.
