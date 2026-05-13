---
id: 2026-05-07--firmware-v3--waveform-characterisation-followup
date_utc: 2026-05-07
agent: codex
scope: firmware-v3
type: docs
summary: Refresh waveform characterisation docs after post-Work-Block K1v2 baseline
files_changed:
  - firmware-v3/docs/research/k1_visual_characterisation_database.md
  - firmware-v3/docs/research/k1_waveform_hybrid_serial_evidence_2026-05-07.md
  - firmware-v3/docs/debugging/VP_STACK_INTROSPECTION_COMMAND_SPEC.md
  - CHANGELOG.md
validation: K1v2 serial vp stack/s/dbg memory on 0x1313 and 0x1302; clangd hover on both waveform renderEffect methods; git diff --check.
breaking_change: false
follow_ups: []
---

## Details

Records the resumed waveform-family baseline after Work Blocks WB-1 and WB-2 were logged and assigned out. K1v2 stayed AP-only, both `0x1313` and `0x1302` remained unified/no-output-fault effects, and the device was returned to `0x1313`.

Updates stale wording that treated `vp stack` as only a future draft and records the current source-mechanism anchors for the native speed floor, Hybrid colour budget, loiter state, and runtime timing pressure.
